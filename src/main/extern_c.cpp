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

    void assert_failed(int i)
    {
        std::cerr << "Impala assertion failed [" << i << "]" << std::endl;
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

    void scene_add_file(impala::State *state, int id)
    {
        state->sceneMgr->add(rt::FileObject("models/" + std::to_string(id) + ".obj",
                                            rt::FileObject::IgnoreNormals | rt::FileObject::IgnoreTexCoord));
    }

    void scene_build(impala::State *state)
    {
        state->sceneMgr->build();
    }
}
