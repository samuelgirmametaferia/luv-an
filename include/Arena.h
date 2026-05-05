#pragma once
#include <vector>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <algorithm>

namespace luv {

class Arena {
public:
    Arena(size_t blockSize = 4096) : blockSize_(blockSize) {
        currentBlock_ = allocateBlock(blockSize_);
    }

    ~Arena() {
        for (auto& cleanup : cleanups_) {
            cleanup.fn(cleanup.ptr);
        }
        for (void* block : blocks_) {
            std::free(block);
        }
    }

    template <typename T, typename... Args>
    T* alloc(Args&&... args) {
        void* ptr = allocate(sizeof(T), alignof(T));
        T* obj = new (ptr) T(std::forward<Args>(args)...);
        if constexpr (!std::is_trivially_destructible_v<T>) {
            cleanups_.push_back({[](void* p) { static_cast<T*>(p)->~T(); }, ptr});
        }
        return obj;
    }

    void* allocate(size_t size, size_t alignment) {
        size_t padding = (alignment - (reinterpret_cast<uintptr_t>(currentPtr_) % alignment)) % alignment;
        
        if (currentOffset_ + padding + size > currentBlockSize_) {
            blockSize_ = std::max(blockSize_, size + alignment);
            currentBlock_ = allocateBlock(blockSize_);
            padding = 0; // New block is aligned
        }

        currentOffset_ += padding;
        void* ptr = static_cast<char*>(currentBlock_) + currentOffset_;
        currentOffset_ += size;
        currentPtr_ = static_cast<char*>(ptr) + size;
        return ptr;
    }

private:
    void* allocateBlock(size_t size) {
        void* block = std::malloc(size);
        blocks_.push_back(block);
        currentBlockSize_ = size;
        currentOffset_ = 0;
        currentPtr_ = block;
        return block;
    }

    struct Cleanup {
        void (*fn)(void*);
        void* ptr;
    };

    size_t blockSize_;
    size_t currentBlockSize_;
    size_t currentOffset_;
    void* currentBlock_;
    void* currentPtr_;
    std::vector<void*> blocks_;
    std::vector<Cleanup> cleanups_;
};

} // namespace luv
