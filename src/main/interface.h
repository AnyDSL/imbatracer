#ifndef INTERFACE_H
#define INTERFACE_H

#include <math.h>
#include <iostream>
#include <float.h>

namespace impala {
    // C-side of the Impala structs: Needed to create textures and materials in the object loader
    struct Color
    {
        float r, g, b;
    };

    struct Noise
    {
        int ty;
        unsigned octaves;
        float amplitude;
        float freq;
        float persistence;
    };

    struct Image
    {
        Color *buf;
        unsigned w;
        unsigned h;
        unsigned stride;
        unsigned _unused_padding; // for alignment
    };

    struct Texture
    {
        int ty;
        Color color1;
        Color color2;
        Noise noise;
        Image img;
        float imgW;
        float imgH;
        unsigned char imgFilter;
        unsigned char imgBorder;
        unsigned char imgMipmap;
        unsigned char _unused_padding;
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
        unsigned nSamples;
        unsigned refract;
    };

    struct State; // some opaque datatype
    struct Tris; // some other opaque datatype
    struct Scene; // and another one

    extern "C" {
        // functiosn working on the state
        State *impala_init();
        void impala_event(void *, State *, bool grabbed, unsigned evt, bool down, int key, float x, float y);
        void impala_update(State *, float dt);

        State *impala_init_bench1();
        State *impala_init_bench2();

        void impala_render(unsigned *buf, int w, int h, bool measureTime, State*);

        void impala_finish(State *);

        // functions working on materials and textures
        unsigned impala_noIdx();
        void impala_dummyMaterial(Material *mat);
        void impala_constantTexture(Texture *tex, float r, float g, float b);
        unsigned impala_sceneAddTexture(Scene *scene, Texture *tex);
        unsigned impala_sceneAddMaterial(Scene *scene, Material *mat);

        // functions working on triangle bunches
        unsigned impala_trisAppendVertex(Tris *tris, float x, float y, float z);
        unsigned impala_trisNumVertices(Tris *tris);
        unsigned impala_trisAppendNormal(Tris *tris, float x, float y, float z);
        unsigned impala_trisNumNormals(Tris *tris);
        unsigned impala_trisAppendTexCoord(Tris *tris, float x, float y);
        unsigned impala_trisNumTexCoords(Tris *tris);
        void impala_trisAppendTriangle(Tris *tris,
                                       unsigned p1, unsigned p2, unsigned p3,
                                       unsigned n1, unsigned n2, unsigned n3,
                                       unsigned t1, unsigned t2, unsigned t3,
                                       unsigned mat);
    }

    // test that these are all POD
    inline static void test_for_pod()
    {
        static_assert(std::is_pod<impala::Color>::value, "impala::Color must be a POD");
        static_assert(std::is_pod<impala::Texture>::value, "impala::Texture must be a POD");
        static_assert(std::is_pod<impala::Noise>::value, "impala::Noise must be a POD");
        static_assert(std::is_pod<impala::Material>::value, "impala::Material must be a POD");
    }

}

#endif
