#pragma once
#include<cstddef>
#include<atomic>
#include<array>

namespace Memory_Pool {

    // 对齐数大小定义
    // C++11 编译时常量 constexpr
    constexpr size_t ALIGNMENT = 8;
    //  256KB;
    constexpr size_t MAX_BYTES = 256 * 1024;
    //  8B,16B,...,256KB
    constexpr size_t FREE_LIST_SIZE = MAX_BYTES / ALIGNMENT;

    // 内存块头部信息
    struct BlockHeader {
        size_t size;
        bool isUse;
        BlockHeader* next;
    };

    // 大小类管理
    class SizeClass {
    public:
        // 大小对齐函数，将bytes对齐到ALIGNMENT的倍数
        static size_t roundUp(size_t bytes) {
            return (bytes + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
        }
        // 计算给定大小 bytes 在自由链表数组中的索引位置
        static size_t getIndex(size_t bytes) {
            bytes = std::max(bytes, ALIGNMENT);
            return (bytes + ALIGNMENT - 1) / ALIGNMENT - 1;
        }
    };
};  // namespace Memory_Pool