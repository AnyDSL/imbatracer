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
	std::vector<int> material_ids;
	
	TextureContainer textures;
	
	Mesh mesh;
	
	TraversalData traversal_data;
};

}

#endif
