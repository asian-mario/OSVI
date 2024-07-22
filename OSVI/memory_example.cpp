#include "OSVI.h"
#include <iostream>
#include <thread>

void work() {
    OSVI_PROFILE_FUNCTION();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

int main() {
    try {
        OSVI_PROFILE_BEGIN_SESSION("Current Session", "CurrentSession.json");

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
    }
    catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    return 0;
}
