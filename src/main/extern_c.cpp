#include <iostream>
#include <float.h>
#include <limits.h>
#include <core/util.h>

extern "C"
{
    void print_ii(int i, int x)
    {
      std::cout << "Impala print [" << i << "] " << x << std::endl;
    }
    void print_if(int i, float x)
    {
      std::cout << "Impala print [" << i << "] " << x << std::endl;
    }
    void print_ifff(int i, float x, float y, float z)
    {
      std::cout << "Impala print [" << i << "] " << x << ", " << y << ", " << z << std::endl;
    }

    unsigned char *HACK_NULL()
    {
        return nullptr;
    }

    float FLT_MAX_fn()
    {
        return FLT_MAX;
    }

    void c_assert(bool cond, int i)
    {
        if(cond)
            return;
        std::cerr << "IMBA ASSERTION FAILED [" << i << "]" << std::endl;
        rt::debugAbort();
    }
}
