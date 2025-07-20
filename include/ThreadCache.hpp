#pragma once
#include "Common.hpp"


namespace Memory_Pool {
    //单例模式，线程本地缓存
    class ThreadCache {
    public:
        // 局部静态变量，第一次调用生成初始化，手动则二次检查，一次判断，加锁再判断
        static ThreadCache* getInstance() {
            static thread_local ThreadCache instance;
            return &instance;
        }
        //  获得对应内存
        void* allocate(size_t size);
        //  销毁
        void deallocate(void* ptr, size_t size);
    private:
        // c++11默认初始化
        ThreadCache() = default;
        // 从中心缓存获取
        void* fetchFromCentralCache(size_t index);
        // 归还到中心缓存
        void returnToCentralCache(size_t index);
        // 计算批量获取内存块的数量
        size_t getBatchNum(size_t size);
        // 判断是否归还内存给中心缓存
        bool shouldReturnToCentralCache(size_t index);

    private:
        //  线程维护成员
        //  内存指针数组
        std::array<void*, FREE_LIST_SIZE>freeList_;
        //  内存大小数组
        std::array<size_t, FREE_LIST_SIZE>freeListSize_;
    };
};// namespace Memory_Pool