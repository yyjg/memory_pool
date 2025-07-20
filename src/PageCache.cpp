#include"../include/PageCache.hpp"
#include<sys/mman.h>
#include<cstring>

namespace Memory_Pool {
    // 分配指定页数的span
    void* PageCache::allocateSpan(size_t numPages) {
        //加锁
        std::lock_guard<std::mutex> lock(mutex_);
 
        // 查找合适的空闲span
        // lower_bound函数返回第一个大于等于numPages的元素的迭代器
        auto it = freeSpans_.lower_bound(numPages);
        if (it != freeSpans_.end()) {

            Span* span = it->second;
            if (span->next) {
                freeSpans_[it->first] = span->next;
            } else {
                freeSpans_.erase(it);
            }
            // 获得的内存块页数大,分割成两块
            if (span->numPages > numPages) {
                Span* newSpan = new Span;
                //多余内存块的地址
                newSpan->pageAddr = static_cast<char*>(span->pageAddr) + numPages * PAGE_SIZE;
                newSpan->numPages = span->numPages - numPages;
                // newSpan->next = nullptr;
                // // 是否空闲链表有同等大小块，有则返回，无则nullptr
                // auto& list = freeSpans_[newSpan->numPages];
                // newSpan->next = list;
                newSpan->next = freeSpans_[newSpan->numPages];
                //记录块
                spanMap_[newSpan->pageAddr] = newSpan;

                //更新使用块的大小
                span->numPages = numPages;

            }
            spanMap_[span->pageAddr] = span;
            return span->pageAddr;

        }
        //没有合适的，新申请
        void* memory = systemAlloc(numPages);
        if (!memory) return nullptr;

        // 初始化内存块
        Span* span = new Span;
        span->pageAddr = memory;
        span->numPages = numPages;
        span->next = nullptr;

        //记录span用于回收
        spanMap_[memory] = span;
        return memory;
    }
    // 释放对应内存
    void PageCache::deallocateSpan(void* ptr, size_t numPages) {
        std::lock_guard<std::mutex> lock(mutex_);
        // 检查该内存是否这里分配
        auto it = spanMap_.find(ptr);
        if (it == spanMap_.end())return;

        Span* span = it->second;

        //尝试向后合并
        void* nextAddr = static_cast<char*> (ptr) + numPages * PAGE_SIZE;
        auto nextIt = spanMap_.find(nextAddr);
        if (nextIt != spanMap_.end()) {
            Span* nextSpan = nextIt->second;
            bool found = false;
            auto& nextlist = freeSpans_[nextSpan->numPages];
            if (nextlist == nextSpan) {
                nextlist = nextSpan->next;
                found = true;
            } else if (nextlist) {
                Span* prev = nextlist;
                while (prev->next) {
                    if (prev->next == nextSpan) {
                        prev->next = nextSpan->next;
                        found = true;
                        break;
                    }
                    prev = prev->next;
                }
            }
            if (found) {
                span->numPages = span->numPages + nextSpan->numPages;
                spanMap_.erase(nextAddr);
                delete nextSpan;
            }
        }

        //更新 空闲数组链表
        span->next = freeSpans_[span->numPages];
        freeSpans_[span->numPages] = span;

    }
    // 申请系统内存
    void* PageCache::systemAlloc(size_t numPages) {
        size_t size = numPages * PAGE_SIZE;
        //     mmap分配内存
        //     void* mmap(
        //     void* addr,      // 映射的首选地址（通常设为 nullptr，由内核决定）
        //     size_t length,   // 映射区域的长度（字节数）
        //     int prot,        // 内存保护标志
        //     int flags,       // 映射选项
        //     int fd,          // 文件描述符（匿名映射时设为 -1）
        //     off_t offset     // 文件偏移量（匿名映射时设为 0）
        // );
        void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        // 清零
        memset(ptr, 0, size);
        return ptr;
    }
}//namespace Memory_Pool