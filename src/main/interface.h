#ifndef INTERFACE_H
#define INTERFACE_H

#include <math.h>
#include <iostream>
#include <float.h>

namespace rt {
    class Scene;
}

namespace impala {
    // C-side of the Impala structs
    struct Point
    {
        float x, y, z;

        Point() {}
        Point(float x, float y, float z) : x(x), y(y), z(z) {}

        float operator[](unsigned i) const {
            switch (i) {
            case 0:  return x;
            case 1:  return y;
            default: return z;
            }
        }
    };
    inline std::ostream &operator<<(std::ostream &o, const Point &p)
    {
        return o << "(" << p.x << ", " << p.y << ", " << p.z << ")";
    }

    struct Vec
    {
        float x, y, z;

        Vec() {}
        Vec(float x, float y, float z) : x(x), y(y), z(z) {}

        float operator[](unsigned i) const {
            switch (i) {
            case 0:  return x;
            case 1:  return y;
            default: return z;
            }
        }
    };
    inline std::ostream &operator<<(std::ostream &o, const Vec &v)
    {
        return o << "(" << v.x << ", " << v.y << ", " << v.z << ")";
    }

    struct TexCoord
    {
        float u, v;

        TexCoord(float u, float v) : u(u), v(v) {}
    };

    struct Object
    {
        unsigned bvhRoot;
    };

    struct BBox
    {
        Point cmin, cmax;

        BBox() : cmin(Point(FLT_MAX, FLT_MAX, FLT_MAX)), cmax(Point(FLT_MIN, FLT_MIN, FLT_MIN)) {}
        BBox(const Point &p) : cmin(p), cmax(p) {}

        Point centroid() const
        {
            return Point(0.5*cmin.x + 0.5*cmax.x, 0.5*cmin.y + 0.5*cmax.y, 0.5*cmin.z + 0.5*cmax.z);
        }
        BBox &extend(const Point &p)
        {
            cmin = Point(std::min(cmin.x, p.x), std::min(cmin.y, p.y), std::min(cmin.z, p.z));
            cmax = Point(std::max(cmax.x, p.x), std::max(cmax.y, p.y), std::max(cmax.z, p.z));
            return *this;
        }
        BBox &extend(const BBox &b)
        {
            return extend(b.cmin).extend(b.cmax);
        }
        static BBox unite(const BBox &b1, const BBox &b2)
        {
            return BBox(b1).extend(b2);
        }
        unsigned longestAxis() const
        {
            float xlen = cmax.x-cmin.x;
            float ylen = cmax.y-cmin.y;
            float zlen = cmax.z-cmin.z;
            if (xlen > ylen) {
                return xlen > zlen ? 0 : 2;
            }
            else {
                return ylen > zlen ? 1 : 2;
            }
        }
    };

    struct BVHNode
    {
        BBox bbox;
        unsigned sndChildFirstPrim;
        uint16_t nPrim, axis;

        BVHNode() {}
        BVHNode(const BBox &bbox) : bbox(bbox) {}
    };

    struct Scene
    {
        Point *verts;
        unsigned *triVerts;
        BVHNode *bvhNodes;
        Object *objs;
        unsigned nObjs;
        rt::Scene *sceneMgr;

        Scene() : sceneMgr(nullptr) {}
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
        void impala_render(unsigned *buf, int w, int h, State *state, float dt);
    }

}

#endif
