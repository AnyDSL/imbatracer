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

        Point() = default;
        Point(float x, float y, float z) : x(x), y(y), z(z) {}

        float &operator[](unsigned i) {
            switch (i) {
            case 0:  return x;
            case 1:  return y;
            default: return z;
            }
        }
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

        Vec() = default;
        Vec(float x, float y, float z) : x(x), y(y), z(z) {}

        float &operator[](unsigned i) {
            switch (i) {
            case 0:  return x;
            case 1:  return y;
            default: return z;
            }
        }
        float operator[](unsigned i) const {
            switch (i) {
            case 0:  return x;
            case 1:  return y;
            default: return z;
            }
        }
        float len() const { return sqrtf(x*x + y*y + z*z); }
        Vec normal() const { if(!x && !y && !z) return Vec(0,0,0); float il=1/len(); return Vec(il*x, il*y, il*z); }
    };
    inline std::ostream &operator<<(std::ostream &o, const Vec &v)
    {
        return o << "(" << v.x << ", " << v.y << ", " << v.z << ")";
    }

    struct Float4
    {
        float x, y, z, w;
    };

    struct Matrix
    {
        Float4 rows[4];
    };

    struct Color
    {
        float r, g, b;

        Color() = default;
        Color(float r, float g, float b) : r(r), g(g), b(b) {}

        float &operator[](unsigned i) {
            switch (i) {
            case 0:  return r;
            case 1:  return g;
            default: return b;
            }
        }
        float operator[](unsigned i) const {
            switch (i) {
            case 0:  return r;
            case 1:  return g;
            default: return b;
            }
        }
    };

    struct TexCoord
    {
        float u, v;

        TexCoord() = default;
        TexCoord(float u, float v) : u(u), v(v) {}
    };

    struct Noise
    {
        int ty;
        unsigned octaves;
        float amplitude;
        float freq;
        float persistence;
    };

    struct Texture
    {
        int ty;
        Color color1;
        Color color2;
        Noise noise;

        static Texture constant(const Color& c)
        {
            return (Texture) {
                .ty = -1,
                .color1 = c,
                .color2 = c,
            };
        }
    };

    struct Material
    {
        // diffuse
        unsigned diffuse;
        // specular (phong)
        unsigned specular;
        float specExp;
        // ambient / emissive
        unsigned emissive;
        // mirror
        unsigned sampling;
        float eta;
        float etaSqrKappaSqr;

        static Material dummy()
        {
            return (Material) {
                .diffuse = 1,
                .specular = 0,
                .specExp = -1.0f,
                .emissive = 0,
                .sampling = 0, // SAMPLING_NOT_NEEDED
                .eta = 0.0f,
                .etaSqrKappaSqr = 0.0f,
            };
        }
    };


    extern "C" {
        void impala_object_init(Object *obj, unsigned rootIdx);

        void *impala_init();
        void impala_update(void *state, float dt);

        void *impala_init_bench1();
        void *impala_init_bench2();

        void impala_render(unsigned *buf, int w, int h, bool measureTime, void *state);
    }

    // test that these are all POD
    inline static void test_for_pod()
    {
        static_assert(std::is_pod<impala::Point>::value, "impala::Point must be a POD");
        static_assert(std::is_pod<impala::Vec>::value, "impala::Vec must be a POD");
        static_assert(std::is_pod<impala::Float4>::value, "impala::Float4 must be a POD");
        static_assert(std::is_pod<impala::Matrix>::value, "impala::Matrix must be a POD");
        static_assert(std::is_pod<impala::Color>::value, "impala::Color must be a POD");
        static_assert(std::is_pod<impala::TexCoord>::value, "impala::TexCoord must be a POD");
        static_assert(std::is_pod<impala::Texture>::value, "impala::Texture must be a POD");
        static_assert(std::is_pod<impala::Noise>::value, "impala::Noise must be a POD");
        static_assert(std::is_pod<impala::Material>::value, "impala::Material must be a POD");
    }

}

#endif
