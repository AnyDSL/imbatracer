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

void build_test_scene(std::vector<Node>& nodes, std::vector<Vec4>& tris, Scene& scene) {
    //const std::string file_name = "../test/sanMiguel.obj";
    //const std::string file_name = "../test/sponza_light_large.obj";
    //const std::string file_name = "../test/sibenik.obj";
    const std::string file_name = "../test/CornellBox-Original.obj";
    //const std::string file_name = "../test/sponza_curtain.obj";
    //const std::string file_name = "../test/sponza_vase_multi.obj";

    if (!build_scene(file_name, scene)) {
        std::cerr << "ERROR: Cannot load scene" << std::endl;
        exit(1);
    }

    if (scene.mesh.triangle_count() == 0) {
        std::cerr << "ERROR: No triangle in the scene" << std::endl;
        exit(1);
    }
    
    //scene.lights.clear();
    //scene.lights.emplace_back(new DirectionalLight(normalize(float3(0.7f, -1.0f, -0.001f)), float4(2.5f)));
    //scene.lights.emplace_back(new PointLight(float3(-10.0f, 193.f, -4.5f), float4(500000.5f)));
    
    if (scene.lights.empty()) {
        std::cerr << "ERROR: There are no lights in the scene." << std::endl;
        exit(1);
    }

    std::unique_ptr<Adapter> adapter = new_adapter(nodes, tris);
    adapter->build_accel(scene.mesh);

    assert(!nodes.empty());
    
    scene.traversal_data.nodes = nodes;
    scene.traversal_data.tris = tris;
    scene.traversal_data.nodes.upload();
    scene.traversal_data.tris.upload();
}

} // namespace imba
