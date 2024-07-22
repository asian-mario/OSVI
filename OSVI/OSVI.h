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
#include <stdexcept>

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
            if (currentSession) {
                EndSession();
            }
            LoadPreviousSession(filepath);
            outputStream.open(filepath);
            if (!outputStream.is_open()) {
                throw std::runtime_error("Could not open output file.");
            }
            WriteHeader();
            currentSession = new Session{ name };
        }

        void EndSession() {
            std::lock_guard<std::mutex> lock(mutex);
            if (currentSession) {
                WriteFooter();
                outputStream.close();
                delete currentSession;
                currentSession = nullptr;
            }
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

            // Store the result for comparison
            currentResults[result.Name] = result.End - result.Start;
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

        void LoadPreviousSession(const std::string& filepath) {
            std::ifstream infile(filepath);
            if (infile.good()) {
                std::string line;
                while (std::getline(infile, line)) {
                    auto pos = line.find("\"dur\":");
                    if (pos != std::string::npos) {
                        auto name_start = line.find("\"name\":\"") + 8;
                        auto name_end = line.find("\"", name_start);
                        std::string name = line.substr(name_start, name_end - name_start);

                        auto dur_start = pos + 6;
                        auto dur_end = line.find(",", dur_start);
                        long long duration = std::stoll(line.substr(dur_start, dur_end - dur_start));

                        previousResults[name] = duration;
                    }
                }
            }
        }

        void CompareSessions() {
            std::lock_guard<std::mutex> lock(mutex);

            std::ofstream comparisonStream("comparison.json");
            if (!comparisonStream.is_open()) {
                throw std::runtime_error("Could not open comparison file.");
            }
            comparisonStream << "{\n\"comparison\": [\n";

            bool first = true;
            for (const auto& entry : currentResults) {
                const std::string& name = entry.first;
                long long currentDuration = entry.second;

                if (!first) {
                    comparisonStream << ",\n";
                }
                first = false;

                comparisonStream << "{\n";
                comparisonStream << "\"name\": \"" << name << "\",\n";
                comparisonStream << "\"current_duration\": " << currentDuration << ",\n";

                if (previousResults.find(name) != previousResults.end()) {
                    comparisonStream << "\"previous_duration\": " << previousResults[name] << ",\n";
                    comparisonStream << "\"difference\": " << (currentDuration - previousResults[name]) << "\n";
                }
                else {
                    comparisonStream << "\"previous_duration\": \"N/A\",\n";
                    comparisonStream << "\"difference\": \"N/A\"\n";
                }

                comparisonStream << "}";
            }

            comparisonStream << "\n]\n}";
            comparisonStream.close();
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
            CompareSessions();
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

        std::map<std::string, long long> previousResults;
        std::map<std::string, long long> currentResults;
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