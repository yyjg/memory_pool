#include "../include/CentralCache.hpp"
#include"../include/PageCache.hpp"
#include <cassert>
#include <thread>

namespace Memory_Pool {

    // 每次从PageCache获取span大小（以页为单位）
    static const size_t SPAN_PAGES = 8;

    // index 内存卡大小  batchNum 数量
    void* CentralCache::fetchRange(size_t index, size_t& batchNum) {

        //1.申请单块内存过大，超过256KB|| 申请0块内存，在线程缓存做过预防，不可能出现
        if (index >= FREE_LIST_SIZE || batchNum == 0) {
            return nullptr;
        }
        // 批量，申请小块内存 ，最大不超过256KB
        //自旋锁
        while (locks_[index].test_and_set(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        void* result = nullptr;
        try {
            // 检查中心缓存空余0
            result = centralFreeList_[index].load(std::memory_order_relaxed);

            if (!result) {
                // 根据索引计算内存块大小，0号8字节，1号16字节
                size_t size = (index + 1) * ALIGNMENT;
                result = fetchFromPageCache(size * batchNum);
                if (!result) {
                    locks_[index].clear(std::memory_order_relaxed);
                    return nullptr;
                }

                // 将从PageCache 获取的内存块切分为小块
                char* start = static_cast<char*>(result);
                // 8页的切分
                size_t totalBlocks = (SPAN_PAGES * PageCache::PAGE_SIZE) / size;
                size_t allocBlocks = std::min(batchNum, totalBlocks);
                batchNum = allocBlocks;

                // 构建链表
                if (allocBlocks > 1) {
                    for (size_t i = 1; i < allocBlocks; i++) {
                        void* current = start + (i - 1) * size;
                        void* next = start + i * size;
                        *reinterpret_cast<void**>(current) = next;
                    }
                    *reinterpret_cast<void**>(start + (allocBlocks - 1) * size) = nullptr;
                }
                if (totalBlocks > allocBlocks) {
                    void* remianStart = start + allocBlocks * size;
                    for (size_t i = allocBlocks + 1;i < totalBlocks;i++) {
                        void* current = start + (i - 1) * size;
                        void* next = start + i * size;
                        *reinterpret_cast<void**>(current) = next;
                    }
                    *reinterpret_cast<void**>(start + (totalBlocks - 1) * size) = nullptr;
                    centralFreeList_[index].store(remianStart, std::memory_order_relaxed);
                }

            } else { //中心缓存有对应大小的内存块
                void* current = result;
                void* prev = nullptr;
                size_t count = 0;
                while (current && count < batchNum) {
                    count++;
                    prev = current;
                    current = *reinterpret_cast<void**>(current);
                }
                batchNum = count;
                if (prev) {
                    *reinterpret_cast<void**>(prev) = nullptr;
                }
                centralFreeList_[index].store(current, std::memory_order_relaxed);
            }
        }
        catch (...) {
            locks_[index].clear(std::memory_order_relaxed);
            throw;
        }
        locks_[index].clear(std::memory_order_relaxed);
        return result;
    }



    void CentralCache::returnRange(void* start, size_t size, size_t index) {
        // 传入空指针或内存过大，在线程缓冲已经判断过
        if (!start || index >= FREE_LIST_SIZE)return;

        //对对应大小链表加锁
        while (locks_[index].test_and_set(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        try {
            void* end = start;
            size_t count = 1;
            // 遍历得到尾部
            while (*reinterpret_cast<void**>(end) != nullptr && count < size) {
                end = *reinterpret_cast<void**>(end);
                count++;
            }
            // 将归还的内存链表尾，连接
            void* current = centralFreeList_[index].load(std::memory_order_relaxed);
            *reinterpret_cast<void**>(end) = current;
            centralFreeList_[index].store(start, std::memory_order_relaxed);
        }
        catch (...) {
            locks_[index].clear(std::memory_order_release);
            throw;
        }
        locks_[index].clear(std::memory_order_release);
    }


    void* CentralCache::fetchFromPageCache(size_t size) {

        //  小于等于32KB的请求，使用固定8页
        if (size <= SPAN_PAGES * PageCache::PAGE_SIZE) {
            return PageCache::getInstance().allocateSpan(SPAN_PAGES);
        } else {
            //取到整页
            size_t numPages = (size + PageCache::PAGE_SIZE - 1) / PageCache::PAGE_SIZE;
            return PageCache::getInstance().allocateSpan(numPages);
        }
    }
}//namespace Memory_Pool