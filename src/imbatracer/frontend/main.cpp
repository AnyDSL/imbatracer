#include "test_scene.h"
#include "SDL_device.h"
#include "../render/render.h"
#include "../core/allocator.h"

#include "thorin_runtime.h"

void render_test_scene() {    
    using imba::float3;    
    
    const int width = 512;
    const int height = 512;
    
    //imba::OrthographicCamera cam(width, height);
    imba::PerspectiveCamera cam(width, height, float3(0.0f, 0.5f, 2.5f), float3(0.0f, 0.0f, -1.0f), float3(0.0f, 1.0f, 0.0f), 60.0f);
    
    std::vector<imba::AreaLight> lights;
    imba::testSceneLights(lights);
    
    imba::BasicPathTracer shader(lights);

    imba::ThorinVector<Node> nodes;
    imba::ThorinVector<Vec4> tris;
    imba::buildTestScene(nodes, tris);
    
    imba::Render render(cam, nodes, tris, shader, width, height);
    
    imba::SDLDevice device(width, height, render);
    device.render();
}

int main(int argc, char** argv) {
    thorin_init();
    
    render_test_scene();   
    
    return 0;
}
