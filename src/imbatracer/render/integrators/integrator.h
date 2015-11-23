#ifndef IMBA_INTEGRATOR_H
#define IMBA_INTEGRATOR_H

#include "../ray_queue.h"
#include "../camera.h"
#include "../../core/image.h"
#include "../light.h"
#include "../random.h"
#include "../scene.h"

#include "../../core/mesh.h"

namespace imba {

class Integrator {
public:
    Integrator(Scene& scene)  
        : scene_(scene)
    {
    }

    virtual void render(Image& out) = 0;
    
protected:
    Scene& scene_;
};

}

#endif
