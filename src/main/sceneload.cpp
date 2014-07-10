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
}
