#include "test_scene.h"
#include "render_window.h"
#include "../render/render.h"
#include "../render/scene.h"

#include "thorin_runtime.h"

void render_test_scene() {      
    using namespace imba;
    
    using StateType = imba::PTState;
    using IntegratorType = imba::PathTracer;
    
    constexpr int width = 1024;
    constexpr int height = 1024;
    constexpr int n_samples = 8;    
    // sponza
    imba::PerspectiveCamera<StateType> cam(width, height, n_samples, float3(-184.0f, 193.f, -4.5f), normalize(float3(-171.081f, 186.426f, -4.96049f) - float3(-184.244f, 193.221f, -4.445f)), float3(0.0f, 1.0f, 0.0f), 60.0f);
    // cornell
    //imba::PerspectiveCamera<StateType> cam(width, height, n_samples, float3(0.0f, 0.9f, 2.5f), float3(0.0f, 0.0f, -1.0f), float3(0.0f, 1.0f, 0.0f), 60.0f);
    // sponza parts
    //imba::PerspectiveCamera<StateType> cam(width, height, n_samples, float3(-5, 0.0f, 0.0f), normalize(float3(1.0f, 0.0f, 0.0f)), float3(0.0f, 1.0f, 0.0f), 60.0f);
    //imba::PerspectiveCamera<StateType> cam(width, height, n_samples, float3(10, 0.0f, 0.0f), normalize(float3(-1.0f, 0.0f, 0.0f)), float3(0.0f, 1.0f, 0.0f), 60.0f);

    imba::Scene scene;
    scene.camera = &cam;
    std::vector<Node> nodes;
    std::vector<Vec4> tris;
    imba::build_test_scene(nodes, tris, scene);
    
    IntegratorType integrator(scene);
    //imba::Renderer<StateType> render(node_array, tri_array, integrator, width, height);
    
    imba::RenderWindow wnd(width, height, n_samples, integrator);
    wnd.render();
}

int main(int argc, char** argv) {
    render_test_scene();   
    return 0;
}
