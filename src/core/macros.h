#ifndef CG1RAYTRACER_MACROS_HEADER
#define CG1RAYTRACER_MACROS_HEADER

#include <stdlib.h>

#if defined(_MSC_VER)
#  define ALIGN(n) __declspec(align(n))
#  define THREADLOCAL __declspec(thread)
#  define NO_RETURN  __declspec (noreturn)
#  pragma warning( disable : 4786 )    // identifier was truncated to '255' characters in the debug information
#  pragma warning( disable : 4800 )    // conversion to bool, performance warning
#  pragma warning( disable : 4996 )    // disable warning for "too old" functions (VC80)
#  pragma warning( disable : 4355 )    // 'this' : used in base member initializer list
#  pragma warning( disable : 4723 )    // potential divide by 0
#  if (_MSC_VER >= 1200)
#    define FORCE_INLINE __forceinline
#  else
#    define FORCE_INLINE __inline
#  endif

#elif defined(__GNUC__)
#  define ALIGN(n) __attribute__((aligned(n)))
#  define THREADLOCAL __thread
#  define NO_RETURN  __attribute__ ((noreturn))
#  ifdef NDEBUG
#    define FORCE_INLINE inline __attribute__((always_inline))
#  else
#    define FORCE_INLINE inline
#endif

#else
# warning unkown compiler; fix this
# define ALIGN(n)
# define THREADLOCAL
# define NO_RETURN
# define FORCE_INLINE inline
#endif

#define UNUSED(x)   ((void)(x))

#define ISALIGNED(x, aln) ((((intptr_t)(x)) % aln) == 0)
#define IS_SSE_ALIGNED(x) ISALIGNED(x, 16)


#endif
