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
template<typename T, typename... Args>
inline void errprint(const T &a, Args... args)
{
    std::cerr << a;
    errprint(args...);
}

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
