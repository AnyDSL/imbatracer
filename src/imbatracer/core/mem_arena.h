#ifndef IMBA_MEM_ARENA_H
#define IMBA_MEM_ARENA_H

#include <cstdint>
#include <vector>

namespace imba {

/// Allocates large blocks of memory that can be used for "allocation" of many smaller chunks
/// for example to allocate memory for BSDF objects.
/// The blocks are kept in memory until the entire MemoryArena is destroyed.
class MemoryArena {
public:
    MemoryArena(size_t block_size = 32768)
        : block_size_(block_size), current_block_(0)
    {
        allocate_block();

#ifdef DEBUG_MEM_ARENA
        printf("Memory arena created.\n");
#endif
    }

    ~MemoryArena() {
        for (auto& b : blocks_) {
            delete[] b;
        }
        blocks_.clear();

#ifdef DEBUG_MEM_ARENA
        printf("Memory arena destroyed, all blocks deleted.\n");
#endif
    }

    // Copying and moving are not supported.
    MemoryArena& operator= (MemoryArena&) = delete;
    MemoryArena(MemoryArena&) = delete;
    MemoryArena(MemoryArena&&) = delete;

    void allocate_block() {
        blocks_.emplace_back(new char[block_size]);

#ifdef DEBUG_MEM_ARENA
        printf("Memory arena allocated a new block of size %d.\n", block_size);
#endif
    }

    /// Releases all chunks and makes all memory in all blocks available for reusing.
    /// Does not actually free any memory.
    void free_all() {
        cur_block_ = 0;
        cur_block_offset_ = 0;
    }

    /// Creates a new object of type T, using the memory blocks that were already allocated.
    /// Memory is only allocated if there is not enough room in the last block. In that case, an entire new block is allocated and added.
    template <typename T>
    T* alloc() {
        // Round up chunk size for alignment
        size = sizeof(T);
        size = (size + 0xf) & (~0xf); // 16 byte alignment

        if (cur_block_offset_ + size > block_size_) {
            // Use the next block, allocate a new one if there are none left.
            ++cur_block_;
            if (cur_block_ >= blocks_.size())
                allocate_block();

            cur_block_offset_ = 0;
            cur_block_ = blocks_.size() - 1;
        }

        current_block_ += size;

        T* res = reinterpret_cast<T*>(blocks_[cur_block_]);
        new (res) T; // Use the placement operator new to call the constructor of T
        return res;
    }

private:
    size_t block_size_;
    size_t cur_block_;
    size_t cur_block_offset_;
    std::vector<char *> blocks_;
};

}

#endif