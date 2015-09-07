#include "test_scene.h"
#include "SDL_device.h"
#include "../render/render.h"
#include "../core/allocator.h"

#include "thorin_runtime.h"

void render_test_scene() {    
    using imba::float3;    
    
    const int width = 1024;
    const int height = 1024;
    
    //imba::OrthographicCamera cam(width, height);
    imba::PerspectiveCamera cam(width, height, float3(0.0f, 0.5f, 2.5f), float3(0.0f, 0.0f, -1.0f), float3(0.0f, 1.0f, 0.0f), 60.0f);
    
    std::vector<imba::AreaLight> lights;
    imba::testSceneLights(lights);

    imba::ThorinVector<Node> nodes;
    imba::ThorinVector<Vec4> tris;
    imba::buildTestScene(nodes, tris);
    
    std::vector<float3> normals;
    normals.reserve(tris.size() / 3);
    for (int i = 0; i < tris.size() / 3; ++i) {
        Vec4 v0_v = tris[i * 3];
        Vec4 v1_v = tris[i * 3 + 1];
        Vec4 v2_v = tris[i * 3 + 2];
        float3 v0(v0_v.x, v0_v.y, v0_v.z);
        float3 v1(v1_v.x, v1_v.y, v1_v.z);
        float3 v2(v2_v.x, v2_v.y, v2_v.z);
        float3 normal = normalize(cross(v1 - v0, v2 - v0));
        normals.push_back(normal);
    }
    
    imba::BasicPathTracer shader(lights, tris, normals);
    imba::Render render(cam, nodes, tris, shader, width, height);
    
    imba::SDLDevice device(width, height, render);
    device.render();
}

int main(int argc, char** argv) {
    thorin_init();
    
    render_test_scene();   
    
    return 0;
}
