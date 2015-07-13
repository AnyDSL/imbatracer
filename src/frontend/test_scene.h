#include "render.h"
#include "thorin_runtime.h"
#include <cstring>

enum {
    BUFFER_I32 = 1,
    BUFFER_F32 = 3
};

namespace imba {

    void createTestScene(Scene& s_out) {
        // creates a very basic test scene (single triangle)

        float vb[9] = {-1.f, -1.f, 1.f, 1.f, -1.f, 1.f, 0.f, 1.f, 1.f};
        int ib[3] = {0,1,2};

        s_out.models = thorin_new<Model>(1);
        s_out.model_count = 1;
        
        // create the test model
        s_out.models->tex_ref = nullptr;
        s_out.models->tex_count = 0;
        s_out.models->vertex_buf = 0;
        s_out.models->index_buf = 1;
        s_out.models->buf_refs = nullptr;
        s_out.models->buf_count = 0;
        s_out.models->shader = 0;
        
        s_out.textures = nullptr;
        s_out.tex_count = 0;

        // create vertex and index buffer
        s_out.buffers = thorin_new<Buffer>(2);
        
        // vertex
        s_out.buffers[0].format = BUFFER_F32;
        s_out.buffers[0].data = reinterpret_cast<unsigned char*>(thorin_new<float>(9));
        s_out.buffers[0].size = sizeof(float) * 9;
        std::memcpy(s_out.buffers[0].data, vb, s_out.buffers[0].size);
        
        // index
        s_out.buffers[1].format = BUFFER_I32;
        s_out.buffers[1].data = reinterpret_cast<unsigned char*>(thorin_new<int>(3));
        s_out.buffers[1].size = sizeof(int) * 3;
        std::memcpy(s_out.buffers[1].data, ib, s_out.buffers[1].size);
        
        s_out.buf_count = 2;
        
        s_out.hemi_lights = nullptr;
        s_out.area_lights = nullptr;
        s_out.hemi_light_count = 0;
        s_out.area_light_count = 0;
    }

    void buildTestSceneAccel(const Scene& scene, Accel& a_out) {
        // builds the acceleration structure for our basic test scene
        
        a_out.nodes = thorin_new<BvhNode>(1);
        a_out.indices = reinterpret_cast<int*>(scene.buffers[1].data);
        a_out.vertices = reinterpret_cast<float*>(scene.buffers[0].data);
        a_out.root = 0;
        
        a_out.nodes->min[0] = -1.f;
        a_out.nodes->min[0] = -1.f;
        a_out.nodes->min[0] = 1.f;
        
        a_out.nodes->max[0] = 1.f;
        a_out.nodes->max[0] = 1.f;
        a_out.nodes->max[0] = 1.f;
        
        a_out.nodes->child_tri = 0;
        a_out.nodes->prim_count = 1;
    }
    
    void freeTestScene(const Scene& scene, const Accel& accel) {
        thorin_free(accel.nodes);
        thorin_free(scene.models);
        thorin_free(scene.buffers[0].data);
        thorin_free(scene.buffers[1].data);
        thorin_free(scene.buffers);
    }

} // namespace imba
