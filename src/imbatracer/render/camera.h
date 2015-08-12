#ifndef IMBA_CAMERA_H
#define IMBA_CAMERA_H

#include "traversal.h"

namespace imba {

class Camera {
public:
    Camera(int width, int height) : width_(width), height_(height) { aspect_ = width / height; }

    virtual void operator()(Ray* rays, int ray_count) = 0;
    
protected:
    int width_;
    int height_;
    float aspect_;
};

class OrthographicCamera : public Camera {
public:
    OrthographicCamera(int w, int h) : Camera(w, h) {}

    virtual void operator()(Ray* rays, int ray_count) override;
};

}

#endif
