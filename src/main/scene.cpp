#include <thorin_ext_runtime.h>
#include <iostream>
#include "scene.h"

namespace rt {

    /** Scene */
    Scene::Scene(impala::Scene *scene) : scene(scene)
    {
        scene->verts = nullptr;
        scene->nTris = 0;
        scene->triVerts = nullptr;
    }
    
    Scene::~Scene(void)
    {
        thorin_free(scene->verts);
        thorin_free(scene->triVerts);
    }
    
    void Scene::build()
    {
        thorin_free(scene->verts);
        thorin_free(scene->triVerts);
        // copy points
        scene->verts = (impala::Point*)thorin_malloc(verts.size()*sizeof(impala::Point));
        std::copy(verts.begin(), verts.end(), scene->verts);
        // copy tris
        scene->nTris = tris.size();
        scene->triVerts = (unsigned*)thorin_malloc(tris.size()*3*sizeof(unsigned));
        unsigned i = 0;
        for (auto& tri : tris) {
            //std::cout << tri.p1 << ", " << tri.p2 << ", " << tri.p3 << std::endl;
            scene->triVerts[i++] = tri.p1;
            scene->triVerts[i++] = tri.p2;
            scene->triVerts[i++] = tri.p3;
        }
        //std::cout << tris.size() << ", " << i << std::endl;
    }
    
    /** CubeScene */
    CubeScene::CubeScene(impala::Scene *scene) : Scene(scene)
    {
        verts.push_back(impala::Point(-1, -1, -1));
        verts.push_back(impala::Point( 1, -1, -1));
        verts.push_back(impala::Point(-1,  1, -1));
        verts.push_back(impala::Point(-1, -1,  1));
        verts.push_back(impala::Point(-1,  1,  1));
        verts.push_back(impala::Point( 1, -1,  1));
        verts.push_back(impala::Point( 1,  1, -1));
        verts.push_back(impala::Point( 1,  1,  1));
        
        tris.push_back(Tri(0, 1, 2));
        tris.push_back(Tri(6, 1, 2));
        
        tris.push_back(Tri(0, 1, 3));
        tris.push_back(Tri(5, 1, 3));
        
        tris.push_back(Tri(0, 2, 3));
        tris.push_back(Tri(4, 2, 3));
        
        tris.push_back(Tri(7, 6, 5));
        tris.push_back(Tri(1, 6, 5));
        
        tris.push_back(Tri(7, 6, 4));
        tris.push_back(Tri(2, 6, 4));
        
        tris.push_back(Tri(7, 5, 4));
        tris.push_back(Tri(3, 5, 4));
        
        build();
    }

}

