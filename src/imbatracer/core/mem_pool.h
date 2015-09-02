#ifndef IMBA_MEM_POOL_H
#define IMBA_MEM_POOL_H

#include <cstdint>
#include <vector>
#include "allocator.h"

namespace imba {

template <typename Allocator = std::allocator<uint8_t> >
class MemoryPool {
public:
    ~MemoryPool() {
        cleanup();
    }

    template <typename T>
    T* alloc(size_t count) {
        size_t size = count * sizeof(T);
        chunks_.emplace_back(alloc_.allocate(size), size);
        return reinterpret_cast<T*>(chunks_.back().first);
    }

    void cleanup() {
        for (auto chunk: chunks_)
            alloc_.deallocate(chunk.first, chunk.second);
    }

private:
    typedef std::pair<uint8_t*, size_t> Chunk;

    std::vector<Chunk> chunks_;
    Allocator alloc_;
};

} // namespace imba

#endif // IMBA_MEM_POOL_H
