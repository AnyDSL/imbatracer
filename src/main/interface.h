#ifndef INTERFACE_H
#define INTERFACE_H

#include <math.h>
#include <iostream>

namespace impala {
    // C-side of the Impala structs
    struct Point
    {
        Point() {}
        Point(float x, float y, float z) : x(x), y(y), z(z) {}

        float x, y, z;
    };
    inline std::ostream &operator<<(std::ostream &o, const Point &p)
    {
        return o << "(" << p.x << ", " << p.y << ", " << p.z << ")";
    }

    struct Vec
    {
        Vec() {}
        Vec(float x, float y, float z) : x(x), y(y), z(z) {}

        float x, y, z;
    };
    inline std::ostream &operator<<(std::ostream &o, const Vec &v)
    {
        return o << "(" << v.x << ", " << v.y << ", " << v.z << ")";
    }

    struct Scene
    {
        unsigned nTris;
        Point *verts;
        unsigned *triVerts;
    };

    struct View
    {
        Point origin;
        Vec forward, up, right, originalUp;
        float rightFactor, upFactor;
    };

    struct Cam
    {
        View view;
        float param1, param2;
        int camtype;
    };

    struct Integrator
    {
        float minDist, maxDist, maxRecDepth;
        int mode;
        int itype;
    };

    struct State
    {
        float time;
        Cam cam;
        Integrator integrator;
        Scene scene;
    };


    extern "C" {
        void impala_render(unsigned *buf, int w, int h, State *state);
        void impala_camInitPerspectiveLookAt(Cam *cam,
                                             float cx, float cy, float cz, float atx, float aty, float atz,
                                             float upx, float upy, float upz, float verticalOpeningAngle, float horizontalOpeningAngle);
    }

    // provide perspectiveCam the way we would want to use it...
    inline Cam perspectiveCam(Point center, Point at, Vec up, float verticalOpeningAngle, float horizontalOpeningAngle)
    {
        Cam c;
        impala_camInitPerspectiveLookAt(&c, center.x, center.y, center.z, at.x, at.y, at.z, up.x, up.y, up.z, verticalOpeningAngle, horizontalOpeningAngle);
        return c;
    }
}

#endif
