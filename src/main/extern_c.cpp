#include <iostream>
#include <float.h>
#include <limits.h>
#include <core/util.h>
#include <core/assert.h>
#include "objloader.h"
#include <io/sdlgui.h>
#include <io/image.h>

extern "C"
{
    // Debugging
    void print_s(const char *s)
    {
      std::cout << "Impala print: " << s << std::endl;
    }
    void print_sptr(const char *s, void *p)
    {
      std::cout << "Impala print: " << s << " " << p << std::endl;
    }
    void print_si(const char *s, int x)
    {
      std::cout << "Impala print: " << s << " " << x << std::endl;
    }
    void print_ssi(const char *s, const char *t, int x)
    {
      std::cout << "Impala print: " << s << " " << t << " " << x << std::endl;
    }
    void print_sii(const char *s, int x, int y)
    {
      std::cout << "Impala print: " << s << " " << x << ", " << y << std::endl;
    }
    void print_siii(const char *s, int x, int y, int z)
    {
      std::cout << "Impala print: " << s << " " << x << ", " << y << ", " << z << std::endl;
    }
    void print_sif(const char *s, int x, float y)
    {
      std::cout << "Impala print: " << s << " " << x << ", " << y << std::endl;
    }
    void print_sf(const char *s, float x)
    {
      std::cout << "Impala print: " << s << " " << x << std::endl;
    }
    void print_sff(const char *s, float x, float y)
    {
      std::cout << "Impala print: " << s << " " << x << ", " << y << std::endl;
    }
    void print_sfff(const char *s, float x, float y, float z)
    {
      std::cout << "Impala print: " << s << " " << x << ", " << y << ", " << z << std::endl;
    }
    void print_sffff(const char *s, float x, float y, float z, float w)
    {
      std::cout << "Impala print: " << s << " " << x << ", " << y << ", " << z << ", " << w << std::endl;
    }

    void assert_failed(const char *str)
    {
        std::cerr << "Impala assertion failed: " << str << std::endl;
        rt::debugAbort();
    }

    void set_pixelscale(rt::SDLGui *gui, float s)
    {
        gui->setPixelScale(s);
    }

    float get_pixelscale(rt::SDLGui *gui)
    {
        return gui->getPixelScale();
    }

    unsigned *image_loadraw(const char *fn, unsigned *w, unsigned *h)
    {
        size_t ws = 0, hs = 0;
        unsigned *buf = rt::Image::loadPNGBuf(fn, ws, hs);
        *w = (unsigned)ws;
        *h = (unsigned)hs;
        return buf;
    }
}
