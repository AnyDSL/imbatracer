#ifndef IMBA_ALLOCATOR_HPP
#define IMBA_ALLOCATOR_HPP

#include <memory>
#include <vector>
#include <cstdint>

#include <thorin_runtime.h>

namespace imba {

/// Thorin-aware memory allocator for STL containers
template <typename T>
class ThorinAllocator : public std::allocator<T> {
public:
    ThorinAllocator() throw() : std::allocator<T>() {}
    ThorinAllocator(const ThorinAllocator& a) throw() : std::allocator<T>(a) { }

    template <class U>
    ThorinAllocator(const ThorinAllocator<U>& a) throw() : std::allocator<T>(a) { }

    ~ThorinAllocator() throw() {}

    typedef size_t size_type;
    typedef T* pointer;
    typedef const T* const_pointer;

    template<typename U>
    struct rebind
    {
        typedef ThorinAllocator<U> other;
    };

    pointer allocate(size_type n, const void* hint = nullptr) {
        return reinterpret_cast<pointer>(thorin_malloc(n * sizeof(T)));
    }

    void deallocate(pointer ptr, size_type n) {
        thorin_free(reinterpret_cast<void*>(ptr));
    }
};

template <typename T>
struct ThorinDeleter {
    void operator () (T* ptr) {
        return thorin_free(reinterpret_cast<void*>(ptr));
    }
};

template <typename T>
struct ThorinDeleter<T[]> {
    void operator () (T* ptr) {
        return thorin_free(reinterpret_cast<void*>(ptr));
    }
};

template <typename T>
using ThorinUniquePtr = std::unique_ptr<T, ThorinDeleter<T> >;

template <typename T, typename... Args>
inline ThorinUniquePtr<T> thorin_make_unique(Args... args) {
    T* ptr = new (reinterpret_cast<T*>(thorin_malloc(sizeof(T)))) T(args...);
    return std::unique_ptr<T, ThorinDeleter<T> >(ptr);
}

template <typename T>
using ThorinVector = std::vector<T, ThorinAllocator<T> >;

} // namespace imba

#endif // IMBA_ALLOCATOR_HPP

