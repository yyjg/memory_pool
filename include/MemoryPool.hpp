#pragma once
#include "ThreadCache.hpp"


namespace Memory_Pool {
    class MemoryPool {

    public:
        static void* allocate(size_t size) {
            return ThreadCache::getInstance()->allocate(size);
        }
        static void deallocate(void* ptr, size_t size) {
            return ThreadCache::getInstance()->deallocate(ptr, size);
        }
    };
}; //namespace Memory_Pool