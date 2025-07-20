#pragma once
#include"Common.hpp"
#include<mutex>

namespace Memory_Pool {
    class CentralCache {
    public:
        static CentralCache& getInstance() {
            static CentralCache instance;
            return instance;
        }

        void* fetchRange(size_t index, size_t& batchNum);
        void returnRange(void* start, size_t size, size_t bytes);
    private:
        // 初始化两个数组
        CentralCache() {
            for (auto& ptr : centralFreeList_) {
                //std::memory_order_relaxed 是 C++ 内存模型（C++11 引入）
                //一种内存顺序约束，用于原子操作（std::atomic）。
                //它提供最弱的内存顺序保证，仅确保原子操作的原子性，不提供任何同步或顺序约束。
                ptr.store(nullptr, std::memory_order_relaxed);
            }
            for (auto& lock : locks_) {
                lock.clear();
            }
        }
        // 从页缓存获得内存
        void* fetchFromPageCache(size_t size);
    private:
        // 使用std::atomic 保证操作的原子性
        // 中心缓存的自由链表
        // 根据索引计算内存块大小，0号8字节，1号16字节，...，最大256KB
        std::array<std::atomic<void*>, FREE_LIST_SIZE> centralFreeList_;
        // 同步的自旋锁
        std::array<std::atomic_flag, FREE_LIST_SIZE> locks_;
    };
};//namespace Memory_Pool