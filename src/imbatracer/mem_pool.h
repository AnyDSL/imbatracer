#ifndef MEM_POOL_H
#define MEM_POOL_H

#include <cstdint>
#include <vector>

namespace imba {

class MemoryPool {
public:
    MemoryPool(size_t init = 1 << 16) {
        chunks_.emplace_back(new uint8_t[init], init);
        cur = 0;
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
    typedef std::pair<uint8_t, size_t> Chunk;

    uint8_t* find_chunk(size_t size) {
        if (chunks.back().second - cur_ < size) {
            // Allocate new chunk
            size_t new_size = chunks.back().second;
            while (new_size < size) new_size = new_size * 2 + 1;
            chunks.emplace_back(new uint8_t[new_size], new_size);
            cur = 0;
        }

        uint8_t* ptr = chunks.back().first + cur;
        cur += size;
        return ptr;
    }

    std::vector<Chunk> chunks_;
    int cur_;
};

} // namespace imba

#endif
