#ifndef IMBA_MEM_POOL_H
#define IMBA_MEM_POOL_H

#include <cstdint>
#include <vector>
#include "allocator.h"

namespace imba {

template <typename Allocator>
class MemoryPool {
public:
    MemoryPool(size_t init = 1 << 16) {
        chunks_.emplace_back(alloc_.allocate(init), init);
        cur_ = 0;
    }

    ~MemoryPool() {
        for (auto chunk: chunks_)
            delete[] chunk.first;
    }

    template <typename T>
    T* alloc(size_t count) {
        return reinterpret_cast<T*>(find_chunk(count * sizeof(T)));
    }

private:
    typedef std::pair<uint8_t*, size_t> Chunk;

    uint8_t* find_chunk(size_t size) {
        if (chunks_.back().second - cur_ < size) {
            // Allocate new chunk
            size_t new_size = chunks_.back().second;
            while (new_size < size) new_size = new_size * 2 + 1;
            chunks_.emplace_back(alloc_.allocate(new_size), new_size);
            cur_ = 0;
        }

        uint8_t* ptr = chunks_.back().first + cur_;
        cur_ += size;
        return ptr;
    }

    std::vector<Chunk> chunks_;
    Allocator alloc_;
    int cur_;
};

typedef MemoryPool<ThorinAllocator<uint8_t> > ThorinMemoryPool;
typedef MemoryPool<std::allocator<uint8_t> > StdMemoryPool;

} // namespace imba

#endif // IMBA_MEM_POOL_H
