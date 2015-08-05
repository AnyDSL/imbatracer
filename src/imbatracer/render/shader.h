#ifndef IMBA_SHADER_H
#define IMBA_SHADER_H

#include "traversal.h"
#include "image.h"

namespace imba {

class Shader {
public:
    virtual void operator()(Ray* rays, Hit* hits, int ray_count, Image& out) = 0;
};

class BasicPathTracer : public Shader {
public:
    virtual void operator()(Ray* rays, Hit* hits, int ray_count, Image& out) override;
};

}

#endif
