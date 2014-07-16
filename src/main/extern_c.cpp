#include <iostream>
#include <float.h>
#include <limits.h>
#include <core/util.h>
#include <core/assert.h>
#include "scene.h"
#include "objloader.h"

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

    float FLT_MAX_fn()
    {
        return FLT_MAX;
    }

    void assert_failed(int i)
    {
        std::cerr << "Impala assertion failed [" << i << "]" << std::endl;
        rt::debugAbort();
    }

    void scene_init_cube(impala::Scene *scene)
    {
        delete scene->sceneMgr;
        scene->sceneMgr = new rt::CubeScene(scene);
    }

    void scene_init_objloader(impala::Scene *scene)
    {
        delete scene->sceneMgr;
        scene->sceneMgr = new rt::ObjLoader(scene);
    }

    bool scene_add_obj(impala::Scene *scene, int id)
    {
        rt::ObjLoader *objloader = dynamic_cast<rt::ObjLoader*>(scene->sceneMgr);
        assert(objloader, "ObjLoader is b0rked");
        return objloader->addObj("models/" + std::to_string(id) + ".obj");
    }

    void scene_build(impala::Scene *scene)
    {
        scene->sceneMgr->build();
    }

    void scene_free(impala::Scene *scene)
    {
        delete scene->sceneMgr;
    }
}
