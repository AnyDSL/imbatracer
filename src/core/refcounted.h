#ifndef CG_CORE_REFCOUNTED_H
#define CG_CORE_REFCOUNTED_H

#include <algorithm>
#include <atomic>
#include <core/macros.h>
#include <core/assert.h>

namespace rt {

class Refcounted
{
public:
    Refcounted() : _refcount(0)
    {}

    virtual ~Refcounted()
    {
        assert(_refcount == 0, "Object was deleted with refcount ", (unsigned)_refcount);
    }

    FORCE_INLINE void incref()
    {
        _refcount.fetch_add(1);
    }
    FORCE_INLINE void decref()
    {
        if (_refcount.fetch_sub(1) == 1) {
            // if the refcount was 1, so it is now zero, it will stay zero forever as nobody has a reference anymore
            delete this;
        }
    }

private:
    std::atomic_uint _refcount;
};


template<typename T> class CountedPtr
{
public:
    FORCE_INLINE ~CountedPtr()
    {
        if(_p)
            _p->decref();
    }
    FORCE_INLINE CountedPtr() : _p(nullptr)
    {}
    FORCE_INLINE CountedPtr(T* p) : _p(p)
    {
        if(p)
            p->incref();
    }
    FORCE_INLINE CountedPtr(const CountedPtr& ref) : _p(ref._p)
    {
        if (_p)
            _p->incref();
    }

    // C++11 move constructor
    CountedPtr(CountedPtr&& ref) : CountedPtr() // initialize via default constructor
    {
        CountedPtr::swap(*this, ref);
    }

    // intentionally not a reference -- see http://stackoverflow.com/questions/3279543/what-is-the-copy-and-swap-idiom
    CountedPtr& operator=(CountedPtr ref)
    {
        CountedPtr::swap(*this, ref);
        return *this;
    }

    const T* operator->() const  { return _p; }
        T* operator->()        { return _p; }

    bool operator!() const { return !_p; }

    // Safe for use in if statements
    operator bool() const  { return _p != nullptr; }

    // if you use these, make sure you also keep a counted reference to the object!
        T* content ()       { return _p; }
    const T* content () const { return _p; }

    bool operator<(const CountedPtr& ref) const { return _p < ref._p; }
    bool operator<=(const CountedPtr& ref) const { return _p <= ref._p; }
    bool operator==(const CountedPtr& ref) const { return _p == ref._p; }
    bool operator!=(const CountedPtr& ref) const { return _p != ref._p; }
    bool operator>=(const CountedPtr& ref) const { return _p >= ref._p; }
    bool operator>(const CountedPtr& ref) const { return _p > ref._p; }

    FORCE_INLINE static void swap(CountedPtr& a, CountedPtr& b)
    {
        std::swap(a._p, b._p);
    }

private:

    T *_p;
};

}

#endif
