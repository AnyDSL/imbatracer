#ifndef IMBA_SCENE_H
#define IMBA_SCENE_H

#include "materials/materials.h"
#include "light.h"
#include "thorin_mem.h"

#include "../core/mesh.h"
#include "../core/mask.h"
#include "traversal.h"

namespace imba {

class RayGen;

struct MeshAttributes {
    enum {
        texcoords = 0,
        normals = 1
    };
};

/// Stores all data required to render a scene.
struct Scene {
    LightContainer lights;
    TextureContainer textures;
    MaterialContainer materials;

    std::vector<float3> geom_normals;

    ThorinArray<::Node> nodes;
    ThorinArray<::Vec4> tris;
    ThorinArray<::Vec2> texcoords;
    ThorinArray<int> indices;
    ThorinArray<::TransparencyMask> masks;
    ThorinArray<char> mask_buffer;

    Mesh mesh;

    BoundingSphere sphere;
};

}

#endif
