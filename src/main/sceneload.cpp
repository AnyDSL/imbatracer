#include <thorin_ext_runtime.h>
#include "sceneload.h"

void loadScene(impala::Scene *scene)
{
    scene->verts = thorin_malloc(8*sizeof(impala::Point));
    
}
