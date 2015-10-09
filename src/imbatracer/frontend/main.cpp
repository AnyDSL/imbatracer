#include "test_scene.h"
#include "SDL_device.h"
#include "../render/render.h"
#include "../core/allocator.h"
#include "../render/scene.h"

#include "thorin_runtime.h"

void render_test_scene() {    
    using imba::float3;    
    
    constexpr int width = 512;
    constexpr int height = 512;
    constexpr int n_samples = 16;
    
    // sponza
    imba::PerspectiveCamera cam(width, height, n_samples, float3(-184.0f, 193.f, -4.5f), normalize(float3(-171.081f, 186.426f, -4.96049f) - float3(-184.244f, 193.221f, -4.445f)), float3(0.0f, 1.0f, 0.0f), 60.0f);
    // cornell
    //imba::PerspectiveCamera cam(width, height, 16, float3(0.0f, 0.5f, 2.5f), float3(0.0f, 0.0f, -1.0f), float3(0.0f, 1.0f, 0.0f), 60.0f);
    
    std::vector<imba::AreaLight> lights;
    imba::testSceneLights(lights);

    imba::ThorinVector<Node> nodes;
    imba::ThorinVector<Vec4> tris;
    imba::Mesh mesh;
    imba::MaterialContainer materials;
    std::vector<int> material_ids;
    imba::buildTestScene(nodes, tris, mesh, materials, material_ids);
    
    std::vector<float3> normals;
    normals.reserve(mesh.triangle_count());
    for (int i = 0; i < mesh.triangle_count(); ++i) {
        imba::Tri t = mesh.triangle(i);
        float3 normal = normalize(cross(t.v1 - t.v0, t.v2 - t.v0));
        normals.push_back(normal);
    }
    
    imba::BasicPathTracer shader(lights, tris, normals, materials, material_ids);
    imba::Render render(cam, nodes, tris, shader, width, height);
    
    imba::SDLDevice device(width, height, n_samples, render);
    device.render();
}

int main(int argc, char** argv) {
    thorin_init();
    
    render_test_scene();   
    
    return 0;
}
