#ifndef CG1RAYTRACER_ASSERT_HEADER
#define CG1RAYTRACER_ASSERT_HEADER

#include <iostream>
#include <cstdlib>
#include <core/util.h>
#include <core/macros.h>



// Printing stuff to std::cerr
inline void errprint()
{
    std::cerr << std::endl;
}
#ifdef HAVE_CXX_11
template<typename T, typename... Args>
inline void errprint(const T &a, Args... args)
{
    std::cerr << a;
    errprint(args...);
}
#else
template<typename T1>
inline void errprint(const T1& a1)
{
    std::cerr << a1 << std::endl;
}
template<typename T1, typename T2>
inline void errprint(const T1& a1, const T2& a2)
{
    std::cerr << a1 << a2 << std::endl;
}
template<typename T1, typename T2, typename T3>
inline void errprint(const T1& a1, const T2& a2, const T3& a3)
{
    std::cerr << a1 << a2 << a3 << std::endl;
}
template<typename T1, typename T2, typename T3, typename T4>
inline void errprint(const T1& a1, const T2& a2, const T3& a3, const T4& a4)
{
    std::cerr << a1 << a2 << a3 << a4 << std::endl;
}
template<typename T1, typename T2, typename T3, typename T4, typename T5>
inline void errprint(const T1& a1, const T2& a2, const T3& a3, const T4& a4, const T5& a5)
{
    std::cerr << a1 << a2 << a3 << a4 << a5 << std::endl;
}
template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
inline void errprint(const T1& a1, const T2& a2, const T3& a3, const T4& a4, const T5& a5, const T6& a6)
{
    std::cerr << a1 << a2 << a3 << a4 << a5 << a6 << std::endl;
}
template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>
inline void errprint(const T1& a1, const T2& a2, const T3& a3, const T4& a4, const T5& a5, const T6& a6, const T7& a7)
{
    std::cerr << a1 << a2 << a3 << a4 << a5 << a6 << a7 << std::endl;
}
template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>
inline void errprint(const T1& a1, const T2& a2, const T3& a3, const T4& a4, const T5& a5, const T6& a6, const T7& a7, const T8& a8)
{
    std::cerr << a1 << a2 << a3 << a4 << a5 << a6 << a7 << a8 << std::endl;
}
template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9>
inline void errprint(const T1& a1, const T2& a2, const T3& a3, const T4& a4, const T5& a5, const T6& a6, const T7& a7, const T8& a8, const T9& a9)
{
    std::cerr << a1 << a2 << a3 << a4 << a5 << a6 << a7 << a8 << a9 << std::endl;
}
template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10>
inline void errprint(const T1& a1, const T2& a2, const T3& a3, const T4& a4, const T5& a5, const T6& a6, const T7& a7, const T8& a8, const T9 &a9, const T10& a10)
{
    std::cerr << a1 << a2 << a3 << a4 << a5 << a6 << a7 << a8 << a9 << a10 <<std::endl;
}
template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10, typename T11>
inline void errprint(const T1& a1, const T2& a2, const T3& a3, const T4& a4, const T5& a5, const T6& a6, const T7& a7, const T8& a8, const T9& a9, const T10& a10, const T11& a11)
{
    std::cerr << a1 << a2 << a3 << a4 << a5 << a6 << a7 << a8 << a9 << a10 << a11 <<std::endl;
}
#endif

// Install it as assertion macro
#ifdef assert
#  undef assert
#endif
#ifdef release_assert
#  undef release_assert
#endif

#define release_assert(cond, ...) do { if (!(cond)) { errprint("Release assertion failure at " __FILE__ ":", __LINE__,  " -- ", __VA_ARGS__); rt::debugAbort(); } } while(false)

#ifndef NDEBUG
#define assert(cond, ...) do { if (!(cond)) { errprint("Assertion failure at " __FILE__ ":", __LINE__,  " -- ", __VA_ARGS__); rt::debugAbort(); } } while(false)
#else
#define assert(cond, ...) do {} while(false)
#endif

#define UNREACHABLE do { std::cerr << "UNREACHABLE reached at " __FILE__ ":" << __LINE__ << std::endl; rt::debugAbort(); } while(false)
#define NOT_IMPLEMENTED do { std::cerr << "NOT_IMPLEMENTED reached at " __FILE__ ":" << __LINE__ << std::endl; rt::debugAbort(); } while(false)

#define DIE(...) do { errprint(__VA_ARGS__); exit(1); } while(false)


#endif
