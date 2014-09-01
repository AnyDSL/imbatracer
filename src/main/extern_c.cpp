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

    void assert_failed(const char *str)
    {
        std::cerr << "Impala assertion failed: " << str;
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
