#include "../render/render.h"
#include "thorin_runtime.h"
#include <cstring>
#include <cmath>
#include <assert.h>
#include "../core/mesh.h"
#include "../core/adapter.h"
#include "obj_loader.h"

#include <iostream>

namespace imba {
    
    void testSceneLights(std::vector<AreaLight>& lights) {
        AreaLight l(float3(-0.24f, 1.98f, -0.22f), float3(0.47f, 0.0f, 0.0f), float3(0.0f, 0.0f, 0.38f), float4(1000.0f));

        lights.push_back(l);
    }
    
    void buildTestSceneObj(ThorinVector<Node>& nodes, ThorinVector<Vec4>& tris) {
        ObjLoader l;
        Mesh m;
        l.load_file(Path("../test/cornell.obj"), m);
        
        std::unique_ptr<Adapter> adapter = new_adapter(nodes, tris);
        adapter->build_accel(m);
        
        assert(nodes.size() && "Nodes are empty");
        assert(tris.size() && "No tris");
    }
    
    void buildTestSceneSimple(ThorinVector<Node>& nodes, ThorinVector<Vec4>& tris) {
        Mesh m;
        m.set_vertex_count(4);
        m.set_index_count(6);
        
        //////////////////////////////////////////
        // 3 triangles
        //////////////////////////////////////////
        /*m.vertices()[0].x = -2.5f;
        m.vertices()[0].y = -1.0f;
        m.vertices()[0].z = 5.0f;
        
        m.vertices()[1].x = -1.5f;
        m.vertices()[1].y = 1.0f;
        m.vertices()[1 * 4 + 2] = 5.0f;
        
        m.vertices()[2 * 4 + 0] = -0.5f;
        m.vertices()[2 * 4 + 1] = -1.0f;
        m.vertices()[2 * 4 + 2] = 5.0f;
        
        ////////////////////////////////
        
        m.vertices()[3 * 4 + 0] = 0.5f;
        m.vertices()[3 * 4 + 1] = -1.0f;
        m.vertices()[3 * 4 + 2] = 5.0f;
        
        m.vertices()[4 * 4 + 0] = 1.5f;
        m.vertices()[4 * 4 + 1] = 1.0f;
        m.vertices()[4 * 4 + 2] = 5.0f;
        
        m.vertices()[5 * 4 + 0] = 2.5f;
        m.vertices()[5 * 4 + 1] = -1.0f;
        m.vertices()[5 * 4 + 2] = 5.0f;
        
        //////////////////////////////
        
        m.vertices()[6 * 4 + 0] = -1.0f;
        m.vertices()[6 * 4 + 1] = 0.0f;
        m.vertices()[6 * 4 + 2] = 1.0f;
        
        m.vertices()[7 * 4 + 0] = 1.0f;
        m.vertices()[7 * 4 + 1] = 0.0f;
        m.vertices()[7 * 4 + 2] = 1.0f;
        
        m.vertices()[8 * 4 + 0] = 0.0f;
        m.vertices()[8 * 4 + 1] = 2.0f;
        m.vertices()[8 * 4 + 2] = 1.0f;
        
        /////////////////////////////
        
        m.indices()[0] = 0;
        m.indices()[1] = 1;
        m.indices()[2] = 2;
        m.indices()[3] = 3;
        m.indices()[4] = 4;
        m.indices()[5] = 5;
        m.indices()[6] = 6;
        m.indices()[7] = 7;
        m.indices()[8] = 8;*/
        
        //////////////////////////////////////////
        // back plane
        //////////////////////////////////////////
        m.vertices()[0].x = -3.0f;
        m.vertices()[0].y = -3.0f;
        m.vertices()[0].z = 20.0f;
        
        m.vertices()[1].x = -3.0f;
        m.vertices()[1].y = 3.0f;
        m.vertices()[1].z = 20.0f;
        
        m.vertices()[2].x = 3.0f;
        m.vertices()[2].y = -3.0f;
        m.vertices()[2].z = 20.0f;
        
        m.vertices()[3].x = 3.0f;
        m.vertices()[3].y = 3.0f;
        m.vertices()[3].z = 20.0f;
        
        m.indices()[0] = 0;
        m.indices()[1] = 1;
        m.indices()[2] = 2;
        m.indices()[3] = 1;
        m.indices()[4] = 2;
        m.indices()[5] = 3;
        
        //////////////////////////////////////////
        // box
        //////////////////////////////////////////
        /*m.vertices()[13 * 4 + 0] = 0.0f;
        m.vertices()[13 * 4 + 1] = 1.5f;
        m.vertices()[13 * 4 + 2] = 5.0f;
        
        m.vertices()[14 * 4 + 0] = 2.0f;
        m.vertices()[14 * 4 + 1] = 1.5f;
        m.vertices()[14 * 4 + 2] = 10.0f;
        
        m.vertices()[15 * 4 + 0] = 0.0f;
        m.vertices()[15 * 4 + 1] = 1.5f;
        m.vertices()[15 * 4 + 2] = 15.0f;
        
        m.vertices()[16 * 4 + 0] = -2.0f;
        m.vertices()[16 * 4 + 1] = 1.5f;
        m.vertices()[16 * 4 + 2] = 10.0f;
        
        ///////////////////////
        
        m.vertices()[17 * 4 + 0] = 0.0f;
        m.vertices()[17 * 4 + 1] = -1.5f;
        m.vertices()[17 * 4 + 2] = 5.0f;
        
        m.vertices()[18 * 4 + 0] = 2.0f;
        m.vertices()[18 * 4 + 1] = -1.5f;
        m.vertices()[18 * 4 + 2] = 10.0f;
        
        m.vertices()[19 * 4 + 0] = 0.0f;
        m.vertices()[19 * 4 + 1] = -1.5f;
        m.vertices()[19 * 4 + 2] = 15.0f;
        
        m.vertices()[20 * 4 + 0] = -2.0f;
        m.vertices()[20 * 4 + 1] = -1.5f;
        m.vertices()[20 * 4 + 2] = 10.0f;
        
        ////////////////////////
        
        m.indices()[15] = 13;
        m.indices()[16] = 14;
        m.indices()[17] = 15;
        m.indices()[18] = 15;
        m.indices()[19] = 16;
        m.indices()[20] = 13;
        
        m.indices()[21] = 17;
        m.indices()[22] = 18;
        m.indices()[23] = 19;
        m.indices()[24] = 19;
        m.indices()[25] = 20;
        m.indices()[26] = 17;
        
        m.indices()[27] = 17;
        m.indices()[28] = 18;
        m.indices()[29] = 13;
        m.indices()[30] = 13;
        m.indices()[31] = 18;
        m.indices()[32] = 14;
        
        m.indices()[33] = 14;
        m.indices()[34] = 18;
        m.indices()[35] = 19;
        m.indices()[36] = 19;
        m.indices()[37] = 14;
        m.indices()[38] = 15;
        
        m.indices()[39] = 19;
        m.indices()[40] = 20;
        m.indices()[41] = 15;
        m.indices()[42] = 15;
        m.indices()[43] = 16;
        m.indices()[44] = 20;
        
        m.indices()[45] = 20;
        m.indices()[46] = 16;
        m.indices()[47] = 13;
        m.indices()[48] = 20;
        m.indices()[49] = 13;
        m.indices()[50] = 17;*/
        
        std::unique_ptr<Adapter> adapter = new_adapter(nodes, tris);
        adapter->build_accel(m);
        
        assert(nodes.size() && "Nodes are empty");
        assert(tris.size() && "No tris");
    }
    
    void buildTestScene(ThorinVector<Node>& nodes, ThorinVector<Vec4>& tris) {
        buildTestSceneObj(nodes, tris);
        //buildTestSceneSimple(nodes, tris);
    }

} // namespace imba
