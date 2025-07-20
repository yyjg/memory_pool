#pragma once
#include "Common.hpp"
#include<map>
#include<mutex>


namespace Memory_Pool {
    class PageCache {
    public:
        // 4k页
        static const size_t PAGE_SIZE = 4096;
        static PageCache& getInstance() {
            static PageCache instance;
            return instance;
        }
        // 分配指定页数的span
        void* allocateSpan(size_t numPages);
        // 释放对应内存
        void deallocateSpan(void* ptr, size_t numPages);
    private:
        PageCache() = default;
        // 申请系统内存
        void* systemAlloc(size_t numPages);
    private:
        // 内存块结构
        struct Span {
            // 起始地址
            void* pageAddr;
            //页数
            size_t numPages;
            // 下一个指针
            Span* next;
        };
        //按页数管理空闲span,不同页数对应不同链表
        std::map<size_t, Span*> freeSpans_;
        //映射页地址到Span, 用于回收
        std::map<void*, Span*> spanMap_;
        std::mutex mutex_;
    };
};//namespace Memory_Pool