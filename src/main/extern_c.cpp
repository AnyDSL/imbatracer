#include <iostream>
#include <float.h>
#include <limits.h>
#include <core/util.h>
#include <core/assert.h>
#include "scene.h"
#include "objloader.h"

extern "C"
{
    // Debugging
    void print_si(const char *s, int x)
    {
      std::cout << "Impala print: " << s << " " << x << std::endl;
    }
    void print_sii(const char *s, int x, int y)
    {
      std::cout << "Impala print: " << s << " " << x << ", " << y << std::endl;
    }
    void print_sf(const char *s, float x)
    {
      std::cout << "Impala print: " << s << " " << x << std::endl;
    }
    void print_sfff(const char *s, float x, float y, float z)
    {
      std::cout << "Impala print: " << s << " " << x << ", " << y << ", " << z << std::endl;
    }

    void assert_failed(const char *str)
    {
        std::cerr << "Impala assertion failed: " << str << std::endl;
        rt::debugAbort();
    }

    // Scene interface
    void scene_clear(impala::State *state)
    {
        state->sceneMgr->clear();
    }

    void scene_add_cube(impala::State *state, float size)
    {
        state->sceneMgr->add(rt::Cube(size));
    }

    void scene_add_file_mat(impala::State *state, int id, int flags, impala::Material *overrideMats, unsigned nMats)
    {
        state->sceneMgr->add(rt::FileObject("models/", std::to_string(id) + ".obj", state->sceneMgr, flags, overrideMats, nMats));
    }

    void scene_add_file(impala::State *state, int id, int flags)
    {
        scene_add_file_mat(state, id, flags, nullptr, 0);
    }

    void scene_build(impala::State *state)
    {
        state->sceneMgr->build();
    }
}
