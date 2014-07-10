#ifndef INTERFACE_H
#define INTERFACE_H

#include <math.h>

namespace impala {
    // C-side of the Impala structs
    struct Point
    {
        float x, y, z;
    };
    
    inline Point point(float x, float y, float z)
    {
        return { .x = x, .y = y, .z = z };
    }

    struct Vec
    {
        float x, y, z;
    };
    
    inline Vec vec(float x, float y, float z)
    {
        return { .x = x, .y = y, .z = z };
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

    struct Camera
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
        Camera cam;
        Integrator integrator;
        Scene scene;
    };


    extern "C" {
        void impala_render(unsigned *buf, int w, int h, State *state);
        Camera perspectiveCam(Point center, Point at, Vec up = vec(0, 1, 0), float verticalOpeningAngle = M_PI / 4, float horizontalOpeningAngle = M_PI / 3);
    }
}

#endif
