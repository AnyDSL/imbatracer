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
        Camera perspectiveCam(float cx, float cy, float cz, float atx, float aty, float atz,
                              float upx, float upy, float upz, float verticalOpeningAngl, float horizontalOpeningAngle);
        void imp_print_stuff(Point p, float x, float y, float z, Vec v, float a, float b, float c);
    }
}

#endif