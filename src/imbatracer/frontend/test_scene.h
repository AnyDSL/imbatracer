#include "../render/render.h"
#include "thorin_runtime.h"
#include <cstring>
#include <cmath>
#include <assert.h>
#include "../core/mesh.h"
#include "../core/adapter.h"
#include "../core/common.h"
#include "obj_loader.h"

#include <iostream>

namespace imba {
    /*int count_tris(int i, const Vec4* tris) {
        int c = 1;
        while (float_as_int(tris[i + 2].w) != 0x80000000) {
            i += 3;
            c++;
        }
        return c;
    }

    BBox make_bb(const ::BBox& bb) {
        return BBox(float3(bb.lo_x, bb.lo_y, bb.lo_z),
                    float3(bb.hi_x, bb.hi_y, bb.hi_z));
    }

    double evaluate_sah(int n, double ci, double ct, const Node* nodes, const Vec4* tris) {
        float left_cost;
        if (nodes[n].left >= 0) {
            left_cost = evaluate_sah(nodes[n].left, ci, ct, nodes, tris);
        } else {
            left_cost = ci * count_tris(~nodes[n].left, tris);
        }

        float right_cost;
        if (nodes[n].right >= 0) {
            right_cost = evaluate_sah(nodes[n].right, ci, ct, nodes, tris);
        } else {
            right_cost = ci * count_tris(~nodes[n].right, tris);
        }

        const BBox& left_bb = make_bb(nodes[n].left_bb);
        const BBox& right_bb = make_bb(nodes[n].right_bb);
        const BBox& parent_bb = BBox(left_bb).extend(right_bb);

        return ct + (left_bb.half_area() * left_cost + right_bb.half_area() * right_cost) / parent_bb.half_area();
    }*/
    
    void buildTestScene(ThorinVector<Node>& nodes, ThorinVector<Vec4>& tris, Mesh& m, TextureContainer& textures, MaterialContainer& materials,
                        std::vector<int>& material_ids, std::vector<float2>& texcoords, LightContainer& lights) {
        ObjLoader l;
        
        //l.load_file(Path("../test/sponza_light_large.obj"), m, materials, textures, material_ids, texcoords, lights);
        //l.load_file(Path("../test/sibenik.obj"), m, materials, textures, material_ids, texcoords, lights);
        //l.load_file(Path("../test/CornellBox-Original.obj"), m, materials, textures, material_ids, texcoords, lights);
        l.load_file(Path("../test/sponza_curtain.obj"), m, materials, textures, material_ids, texcoords, lights);
        
        std::unique_ptr<Adapter> adapter = new_adapter(nodes, tris);
        adapter->build_accel(m);

        //printf("Mesh tri. count : %d\nBVH tri. count : %d\nSAH: %lf\n",
        //    m.triangle_count(), tris.size() / 3, evaluate_sah(0, 1, 1, nodes.data(), tris.data()));
        
        assert(nodes.size() && "Nodes are empty");
        assert(tris.size() && "No tris");
        
        if (lights.size() == 0) {
            printf("ERROR: There are no lights in the scene.\n");
            exit(1);
        }
    }
} // namespace imba
