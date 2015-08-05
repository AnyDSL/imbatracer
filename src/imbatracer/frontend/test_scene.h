#include "../render/render.h"
#include "thorin_runtime.h"
#include <cstring>
#include <cmath>
#include "../core/mesh.h"
#include "../core/adapter.h"

namespace imba {
/*
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
        
        s_out.hemi_lights = thorin_new<HemiLight>(1);
        s_out.area_lights = nullptr;
        s_out.hemi_light_count = 1;
        s_out.area_light_count = 0;
        
        s_out.hemi_lights[0].intensity.values[0] = 1.f * 10.0f;
        s_out.hemi_lights[0].intensity.values[1] = 0.6f * 10.0f;
        s_out.hemi_lights[0].intensity.values[2] = 0.f * 10.0f;
        
        s_out.hemi_lights[0].shader = 0;
        
        s_out.hemi_lights[0].pos.values[0] = 0.0f;
        s_out.hemi_lights[0].pos.values[1] = 10.0f;
        s_out.hemi_lights[0].pos.values[2] = 0.0f;
                
        s_out.hemi_lights[0].cutoff = 2.0f * 3.14159f;
        
        s_out.hemi_lights[0].dir.values[0] = 0.0f;
        s_out.hemi_lights[0].dir.values[1] = 0.0f;
        s_out.hemi_lights[0].dir.values[2] = 1.0f;
    }*/

    void buildTestScene(ThorinVector<Node>& nodes, ThorinVector<Vec4>& tris) {
        Mesh m;
        m.set_vertex_count(9 * 4);
        m.set_index_count(3 * 3);
        
        m.vertices()[0 * 4 + 0] = -5.0f;
        m.vertices()[0 * 4 + 1] = -5.0f;
        m.vertices()[0 * 4 + 2] = 5.0f;
        
        m.vertices()[1 * 4 + 0] = 0.0f;
        m.vertices()[1 * 4 + 1] = -5.0f;
        m.vertices()[1 * 4 + 2] = 5.0f;
        
        m.vertices()[2 * 4 + 0] = -2.5f;
        m.vertices()[2 * 4 + 1] = 5.0f;
        m.vertices()[2 * 4 + 2] = 5.0f;
        
        ////////////////////////////////
        
        m.vertices()[3 * 4 + 0] = 0.0f;
        m.vertices()[3 * 4 + 1] = -5.0f;
        m.vertices()[3 * 4 + 2] = 5.0f;
        
        m.vertices()[4 * 4 + 0] = 5.0f;
        m.vertices()[4 * 4 + 1] = -5.0f;
        m.vertices()[4 * 4 + 2] = 5.0f;
        
        m.vertices()[5 * 4 + 0] = 2.5f;
        m.vertices()[5 * 4 + 1] = 5.0f;
        m.vertices()[5 * 4 + 2] = 5.0f;
        
        //////////////////////////////
        
        m.vertices()[6 * 4 + 0] = -1.0f;
        m.vertices()[6 * 4 + 1] = -1.0f;
        m.vertices()[6 * 4 + 2] = 1.0f;
        
        m.vertices()[7 * 4 + 0] = 1.0f;
        m.vertices()[7 * 4 + 1] = -1.0f;
        m.vertices()[7 * 4 + 2] = 1.0f;
        
        m.vertices()[8 * 4 + 0] = 0.0f;
        m.vertices()[8 * 4 + 1] = 1.0f;
        m.vertices()[8 * 4 + 2] = 1.0f;
        
        /////////////////////////////////////////////
        
        m.indices()[0] = 0;
        m.indices()[1] = 1;
        m.indices()[2] = 2;
        m.indices()[3] = 3;
        m.indices()[4] = 4;
        m.indices()[5] = 5;
        m.indices()[6] = 6;
        m.indices()[7] = 7;
        m.indices()[8] = 8; 
        
        std::unique_ptr<Adapter> adapter = new_adapter(nodes, tris);
        adapter->build_accel(m);
    }

} // namespace imba
