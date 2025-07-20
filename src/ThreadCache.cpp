#include "../include/CentralCache.hpp"
#include "../include/ThreadCache.hpp"
#include <malloc.h>

namespace Memory_Pool {
    // 申请size大小内存，过大由系统分配，小优先从缓存分配，没有向中心缓存申请一些缓存
    void* ThreadCache::allocate(size_t size) {
        if (size == 0) {
            size = ALIGNMENT;
        }
        // 超过256KB,由系统分配，不由中心内存分配
        if (size > MAX_BYTES) {
            return malloc(size);
        }
        size_t index = SizeClass::getIndex(size);

        //// 检查本地线程链表是否有对应空缓存，没有则申请新内存
        if (freeListSize_[index] == 0) {
            fetchFromCentralCache(index);
        }
        // ptr 存储了下个内存块的地址
        // 将freeList_[index]指向的内存块的下一个内存块地址（取决于内存块的实现）
        void* ptr = freeList_[index];
        freeList_[index] = *reinterpret_cast<void**>(ptr);

        //减一，本地缓存
        freeListSize_[index]--;
        return ptr;
    }
    //  销毁
    void ThreadCache::deallocate(void* ptr, size_t size) {
        if (size > MAX_BYTES) {
            free(ptr);
            return;
        }
        size_t index = SizeClass::getIndex(size);
        *reinterpret_cast<void**>(ptr) = freeList_[index];
        freeListSize_[index]++;

        if (shouldReturnToCentralCache(index)) {
            returnToCentralCache(index);
        }
    }
    // 从中心缓存获取缓存index大小，批量获取
    void* ThreadCache::fetchFromCentralCache(size_t index) {
        size_t size = (index + 1) * ALIGNMENT;
        size_t batchNum = getBatchNum(size);
        // 传batchNum 引用进去，获取实际数量
        void* start = CentralCache::getInstance().fetchRange(index, batchNum);
        // 获取不到
        if (!start)return nullptr;
        //  加上新获得的数量
        freeListSize_[index] += batchNum;
        freeList_[index] = start;
    }
    // 归还到中心缓存
    void ThreadCache::returnToCentralCache(size_t index) {

        size_t batchNum = freeListSize_[index];
        if (batchNum <= 1) return; // 如果只有一个块，则不归还

        // 保留一部分在ThreadCache中（比如保留1/4）
        size_t keepNum = std::max(batchNum / 4, size_t(1));
        size_t returnNum = batchNum - keepNum;

        // 将内存块串成链表
        void* current = freeList_[index];
        // 使用对齐后的大小计算分割点
        void* splitNode = current;
        for (size_t i = 0; i < keepNum - 1; ++i) {
            splitNode = *reinterpret_cast<void**>(splitNode);
        }

        if (splitNode != nullptr) {
            // 将要返回的部分和要保留的部分断开
            void* nextNode = *reinterpret_cast<void**>(splitNode);
            *reinterpret_cast<void**>(splitNode) = nullptr; // 断开连接

            // 更新ThreadCache的空闲链表
            freeList_[index] = current;

            // 更新自由链表大小
            freeListSize_[index] = keepNum;

            // 将剩余部分返回给CentralCache
            if (returnNum > 0 && nextNode != nullptr) {
                CentralCache::getInstance().returnRange(nextNode, returnNum, index);
            }
        }
    }
    // 判断是否归还内存给中心缓存
    bool ThreadCache::shouldReturnToCentralCache(size_t index) {
        // 设定阈值，例如：当自由链表的大小超过一定数量时
        size_t threshold = 64; // 例如，64个内存块
        return (freeListSize_[index] > threshold);
    }

    // 计算批量获取内存块的数量
    size_t ThreadCache::getBatchNum(size_t size) {
        // 批量获取不超过4KB大小
        constexpr size_t MAX_BATCH_SIZE = 4096;

        //4KB一下批量获取，4KB以上只取一个
        // 根据对象大小设置合理的基准批量数
        size_t baseNum;
        if (size <= 64) baseNum = 64;    // 64 * 64 = 4KB
        else if (size <= 128) baseNum = 32;  // 32 * 128 = 4KB
        else if (size <= 256) baseNum = 16; // 16 * 256 = 4KB
        else if (size <= 512) baseNum = 8;  // 8 * 512 = 4KB
        else if (size <= 1024) baseNum = 4;  // 4 * 1024 = 4KB
        else if (size <= 2048) baseNum = 2; // 2 * 2048 = 4KB
        else baseNum = 1;                   // 大于1024的对象每次只从中心缓存取1个

        size_t maxNum = std::max(size_t(1), MAX_BATCH_SIZE / size);
        return std::min(baseNum, maxNum);
    }
}//namespace Memory_Pool