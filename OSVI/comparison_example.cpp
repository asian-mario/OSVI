#include "OSVI.h"
#include <iostream>
#include <thread>

void work() {
    OSVI_PROFILE_FUNCTION();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

int main() {
    try {
        // First session
        OSVI_PROFILE_BEGIN_SESSION("Previous Session", "PreviousSession.json");

        {
            OSVI_PROFILE_SCOPE("Main Scope");

            void* memory = malloc(1024);
            OSVI_PROFILE_MEMORY_ALLOC(1024);
            // Simulate some work
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            free(memory);
            OSVI_PROFILE_MEMORY_FREE(memory);
        }

        // Additional threading work
        std::thread t1(work);
        std::thread t2(work);

        t1.join();
        t2.join();

        OSVI_PROFILE_END_SESSION();

        // Modify the code/environment here to produce different results
        // Second session
        OSVI_PROFILE_BEGIN_SESSION("Current Session", "CurrentSession.json");

        {
            OSVI_PROFILE_SCOPE("Main Scope");

            void* memory = malloc(2048);
            OSVI_PROFILE_MEMORY_ALLOC(2048);
            // Simulate some work
            std::this_thread::sleep_for(std::chrono::milliseconds(150));

            free(memory);
            OSVI_PROFILE_MEMORY_FREE(memory);
        }

        // Additional threading work
        std::thread t3(work);
        std::thread t4(work);

        t3.join();
        t4.join();

        OSVI_PROFILE_END_SESSION();

        // The comparison will automatically be generated in comparison.json
    }
    catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    return 0;
}
