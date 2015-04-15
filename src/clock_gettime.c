#ifdef __cplusplus
extern "C" {
#endif

#include <time.h>

asm (".symver clock_gettime, clock_gettime@GLIBC_2.2.5");

int __wrap_clock_gettime(clockid_t clk_id, struct timespec *tp) {
    return clock_gettime(clk_id, tp);
}

#ifdef __cplusplus
} // extern "C"
#endif

