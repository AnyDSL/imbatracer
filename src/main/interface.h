#ifndef INTERFACE_H
#define INTERFACE_H

namespace impala {
    // C-side of the Impala structs
    struct Point
    {
        float x, y, z;
    };

    struct Vec
    {
        float x, y, z;
    };

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
        float maxDist, maxRecDepth;
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


    extern "C" void impala_render(unsigned *buf, int w, int h, State *state);
}

#endif
