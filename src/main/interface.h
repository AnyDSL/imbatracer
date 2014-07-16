#ifndef INTERFACE_H
#define INTERFACE_H

#include <math.h>
#include <iostream>

namespace rt {
    class Scene;
}

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
        Scene() : sceneMgr(nullptr) {}

        unsigned nTris;
        Point *verts;
        unsigned *triVerts;
        rt::Scene *sceneMgr;
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
        void impala_init(State *state);
        void impala_deinit(State *state);
        void impala_render(unsigned *buf, int w, int h, State *state, float dt);
    }

}

#endif
