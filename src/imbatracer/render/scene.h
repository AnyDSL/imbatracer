#ifndef IMBA_SCENE_H
#define IMBA_SCENE_H

#include "material.h"
#include "ray_gen.h"
#include "light.h"

#include "../core/mesh.h"

namespace imba {

struct MeshAttributes {
	enum {
		texcoords,
		normals
	};
};

/// Stores all data required to render a scene.
struct Scene {
	RayGen* camera;
	
	LightContainer lights;
	MaterialContainer materials;	
	TextureContainer textures;
	
	Mesh mesh;
	std::vector<float3> geometry_normals;
	std::vector<int> material_ids;
	
	TraversalData traversal_data;
};

}

#endif
