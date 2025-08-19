#ifndef MEMORY_MONITOR_H
#define MEMORY_MONITOR_H

#include <Arduino.h>
#include <esp_heap_caps.h>

class MemoryMonitor {
public:
    static void printMemoryInfo(const char* context = "") {
        Serial.printf("[Memory] %s - Free heap: %d bytes, Largest free block: %d bytes, Fragmentation: %d%%\n", 
                     context, 
                     ESP.getFreeHeap(),
                     heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
                     getFragmentation());
    }
    
    static bool checkMemoryThreshold(size_t required = 4096) {
        size_t freeHeap = ESP.getFreeHeap();
        size_t largestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
        
        if (freeHeap < required * 2 || largestBlock < required) {
            Serial.printf("[Memory] WARNING: Low memory! Free: %d, Largest: %d, Required: %d\n", 
                         freeHeap, largestBlock, required);
            return false;
        }
        return true;
    }
    
    static void logMemoryLeak(const char* location, void* ptr) {
        Serial.printf("[Memory] LEAK DETECTED at %s: ptr=%p, heap=%d\n", 
                     location, ptr, ESP.getFreeHeap());
    }
    
    static int getFragmentation() {
        size_t freeHeap = ESP.getFreeHeap();
        size_t largestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
        if (freeHeap == 0) return 0;
        return ((freeHeap - largestBlock) * 100) / freeHeap;
    }
};

#endif // MEMORY_MONITOR_H