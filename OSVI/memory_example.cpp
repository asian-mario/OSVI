#include "OSVI.h"
#include <iostream>
#include <thread>
#include <mutex>

int main() {
    OSVI_PROFILE_BEGIN_SESSION("Memory Profiling Session", "MemoryProfiling.json");

    {
        OSVI_PROFILE_SCOPE("Main Scope");

        void* memory = malloc(1024);
        OSVI_PROFILE_MEMORY_ALLOC(1024);
        // Simulate some work
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        free(memory);
        OSVI_PROFILE_MEMORY_FREE(memory);
    }

    OSVI_PROFILE_END_SESSION();
    return 0;
}
