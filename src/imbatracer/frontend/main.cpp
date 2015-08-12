#include "test_scene.h"
#include "SDL_device.h"
#include "../render/render.h"
#include "../core/allocator.h"

#include "thorin_runtime.h"

void render_test_scene() {    
    const int width = 512;
    const int height = 512;
    
    imba::OrthographicCamera cam(width, height);
    imba::BasicPathTracer shader;

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
