#include <iostream>
#include <float.h>
#include <limits.h>
#include <core/util.h>

extern "C"
{
    void print_ints(int x, int y)
    {
      std::cout << "Impala print " << x << " " << y << std::endl;
    }

    unsigned char *HACK_NULL()
    {
        return nullptr;
    }

    float FLT_MAX_fn()
    {
        return FLT_MAX;
    }

    void c_assert(bool cond)
    {
        if(cond)
            return;
        std::cerr << "IMBA ASSERTION FAILED" << std::endl;
        rt::debugAbort();
    }
}
