#pragma once

#include <chrono>
#include <string>
#include <fstream>
#include <thread>
#include <mutex>
#include <sstream>
#include <iostream>
#include <map>
#include <cstdlib>

// Profiling macros
#define OSVI_PROFILE_BEGIN_SESSION(name, filepath) ::OSVI::Profiler::Get().BeginSession(name, filepath)
#define OSVI_PROFILE_END_SESSION() ::OSVI::Profiler::Get().EndSession()
#define OSVI_PROFILE_SCOPE(name) ::OSVI::ProfileTimer timer##__LINE__(name)
#define OSVI_PROFILE_FUNCTION() OSVI_PROFILE_SCOPE(__FUNCTION__)

#define OSVI_PROFILE_MEMORY_ALLOC(size) ::OSVI::Profiler::Get().ProfileMemoryAlloc(size, __FILE__, __LINE__)
#define OSVI_PROFILE_MEMORY_FREE(ptr) ::OSVI::Profiler::Get().ProfileMemoryFree(ptr, __FILE__, __LINE__)

namespace OSVI {

    struct ProfileResult {
        std::string Name;
        long long Start, End;
        uint32_t ThreadID;
    };

    struct MemoryProfileResult {
        size_t Size;
        const char* File;
        int Line;
        std::string Operation;
        uint32_t ThreadID;
    };

    class Profiler {
    public:
        static Profiler& Get() {
            static Profiler instance;
            return instance;
        }

        void BeginSession(const std::string& name, const std::string& filepath) {
            std::lock_guard<std::mutex> lock(mutex);
            outputStream.open(filepath);
            WriteHeader();
            currentSession = new Session{ name };
        }

        void EndSession() {
            std::lock_guard<std::mutex> lock(mutex);
            WriteFooter();
            outputStream.close();
            delete currentSession;
            currentSession = nullptr;
        }

        void WriteProfile(const ProfileResult& result) {
            std::lock_guard<std::mutex> lock(mutex);

            if (profileCount++ > 0)
                outputStream << ",";

            std::stringstream json;
            json << "{";
            json << "\"cat\":\"function\",";
            json << "\"dur\":" << (result.End - result.Start) << ',';
            json << "\"name\":\"" << result.Name << "\",";
            json << "\"ph\":\"X\",";
            json << "\"pid\":0,";
            json << "\"tid\":" << result.ThreadID << ",";
            json << "\"ts\":" << result.Start;
            json << "}";

            outputStream << json.str();
            outputStream.flush();
        }

        void WriteMemoryProfile(const MemoryProfileResult& result) {
            std::lock_guard<std::mutex> lock(mutex);

            if (profileCount++ > 0)
                outputStream << ",";

            std::stringstream json;
            json << "{";
            json << "\"cat\":\"memory\",";
            json << "\"name\":\"" << result.Operation << "\",";
            json << "\"ph\":\"M\",";
            json << "\"pid\":0,";
            json << "\"tid\":" << result.ThreadID << ",";
            json << "\"ts\":" << GetTimestamp() << ",";
            json << "\"args\":{\"size\":" << result.Size << ",\"file\":\"" << EscapeJsonString(result.File) << "\",\"line\":" << result.Line << "}";
            json << "}";

            outputStream << json.str();
            outputStream.flush();
        }

        void ProfileMemoryAlloc(size_t size, const char* file, int line) {
            MemoryProfileResult result{ size, file, line, "malloc", std::hash<std::thread::id>{}(std::this_thread::get_id()) };
            WriteMemoryProfile(result);
        }

        void ProfileMemoryFree(void* ptr, const char* file, int line) {
            MemoryProfileResult result{ 0, file, line, "free", std::hash<std::thread::id>{}(std::this_thread::get_id()) };
            WriteMemoryProfile(result);
        }

        long long GetTimestamp() {
            auto now = std::chrono::high_resolution_clock::now();
            auto time = std::chrono::time_point_cast<std::chrono::microseconds>(now).time_since_epoch();
            return time.count();
        }

    private:
        struct Session {
            std::string Name;
        };

        Profiler() : currentSession(nullptr), profileCount(0) {}

        void WriteHeader() {
            outputStream << "{\"otherData\": {},\"traceEvents\":[";
            outputStream.flush();
        }

        void WriteFooter() {
            outputStream << "]}";
            outputStream.flush();
        }

        std::string EscapeJsonString(const std::string& str) {
            std::ostringstream oss;
            for (char c : str) {
                switch (c) {
                case '"':  oss << "\\\""; break;
                case '\\': oss << "\\\\"; break;
                case '\b': oss << "\\b"; break;
                case '\f': oss << "\\f"; break;
                case '\n': oss << "\\n"; break;
                case '\r': oss << "\\r"; break;
                case '\t': oss << "\\t"; break;
                default:   oss << c; break;
                }
            }
            return oss.str();
        }

        std::mutex mutex;
        std::ofstream outputStream;
        Session* currentSession;
        int profileCount;
    };

    class ProfileTimer {
    public:
        ProfileTimer(const char* name) : name(name), stopped(false) {
            startTimepoint = std::chrono::high_resolution_clock::now();
        }

        ~ProfileTimer() {
            if (!stopped)
                Stop();
        }

        void Stop() {
            auto endTimepoint = std::chrono::high_resolution_clock::now();
            auto start = std::chrono::time_point_cast<std::chrono::microseconds>(startTimepoint).time_since_epoch();
            auto end = std::chrono::time_point_cast<std::chrono::microseconds>(endTimepoint).time_since_epoch();

            ProfileResult result{ name, start.count(), end.count(), std::hash<std::thread::id>{}(std::this_thread::get_id()) };
            Profiler::Get().WriteProfile(result);

            stopped = true;
        }

    private:
        const char* name;
        std::chrono::time_point<std::chrono::high_resolution_clock> startTimepoint;
        bool stopped;
    };

} 