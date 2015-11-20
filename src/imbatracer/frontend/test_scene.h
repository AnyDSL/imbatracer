#include <cstring>
#include <cmath>
#include <cassert>
#include <iostream>

#include "thorin_runtime.h"

#include "../render/render.h"
#include "../core/mesh.h"
#include "../core/adapter.h"
#include "../core/common.h"
#include "build_scene.h"

namespace imba {

void buildTestScene(ThorinVector<Node>& nodes, ThorinVector<Vec4>& tris, Mesh& m, TextureContainer& textures, MaterialContainer& materials,
                    std::vector<int>& material_ids, std::vector<float2>& texcoords, LightContainer& lights) {

    //const std::string file_name = "../test/sanMiguel.obj";
    const std::string file_name = "../test/sponza_light_large.obj";
    //const std::string file_name = "../test/sibenik.obj";
    //const std::string file_name = "../test/CornellBox-Original.obj";
    //const std::string file_name = "../test/sponza_curtain.obj";
    //const std::string file_name = "../test/sponza_vase_multi.obj";

    if (!build_scene(file_name, m, materials, textures, material_ids, texcoords, lights)) {
        std::cerr << "ERROR: Cannot load scene" << std::endl;
        exit(1);
    }

    if (m.triangle_count() == 0) {
        std::cerr << "ERROR: No triangle in the scene" << std::endl;
        exit(1);
    }
    
    if (lights.empty()) {
        std::cerr << "ERROR: There are no lights in the scene." << std::endl;
        exit(1);
    }

    std::unique_ptr<Adapter> adapter = new_adapter(nodes, tris);
    adapter->build_accel(m);

    assert(!nodes.empty());
}

} // namespace imba
