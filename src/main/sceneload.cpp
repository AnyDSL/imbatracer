#include <thorin_ext_runtime.h>
#include "sceneload.h"

void loadScene(impala::Scene *scene)
{
    scene->verts = (impala::Point*)thorin_malloc(8*sizeof(impala::Point));
    scene->verts[0] = impala::point(-1, -1, -1);
    scene->verts[1] = impala::point( 1, -1, -1);
    scene->verts[2] = impala::point(-1,  1, -1);
    scene->verts[3] = impala::point(-1, -1,  1);
    scene->verts[4] = impala::point(-1,  1,  1);
    scene->verts[5] = impala::point( 1, -1,  1);
    scene->verts[6] = impala::point( 1,  1, -1);
    scene->verts[7] = impala::point( 1,  1,  1);
    
    scene->triVerts = (unsigned*)thorin_malloc(12*3*sizeof(unsigned));
    unsigned i = 0;
    scene->triVerts[i++] = 0; scene->triVerts[i++] = 1; scene->triVerts[i++] = 2;
    scene->triVerts[i++] = 6; scene->triVerts[i++] = 1; scene->triVerts[i++] = 2;
    
    scene->triVerts[i++] = 0; scene->triVerts[i++] = 1; scene->triVerts[i++] = 3;
    scene->triVerts[i++] = 5; scene->triVerts[i++] = 1; scene->triVerts[i++] = 3;
    
    scene->triVerts[i++] = 0; scene->triVerts[i++] = 2; scene->triVerts[i++] = 3;
    scene->triVerts[i++] = 4; scene->triVerts[i++] = 2; scene->triVerts[i++] = 3;
    
    scene->triVerts[i++] = 7; scene->triVerts[i++] = 6; scene->triVerts[i++] = 5;
    scene->triVerts[i++] = 1; scene->triVerts[i++] = 6; scene->triVerts[i++] = 5;
    
    scene->triVerts[i++] = 7; scene->triVerts[i++] = 6; scene->triVerts[i++] = 4;
    scene->triVerts[i++] = 2; scene->triVerts[i++] = 6; scene->triVerts[i++] = 4;
    
    scene->triVerts[i++] = 7; scene->triVerts[i++] = 5; scene->triVerts[i++] = 4;
    scene->triVerts[i++] = 3; scene->triVerts[i++] = 5; scene->triVerts[i++] = 4;
}
