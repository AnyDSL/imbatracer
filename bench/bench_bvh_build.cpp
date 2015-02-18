#include <iostream>
#include "bench_bvh_build.hpp"

namespace bench {

void BenchBvhBuildImpala::iteration() {
    bench_bvh_build((Vec3*)mesh_->vertices(),
                    (int*)mesh_->triangles(),
                    mesh_->triangle_count(),
                    result_.get());

    nodes_ = result_->bvh.node_count;

    thorin_free(result_->bvh.nodes);
    thorin_free(result_->bvh.prim_ids);
    thorin_free(result_->boxes);
    thorin_free(result_->centers);
}

void BenchBvhBuildImpala::display() {
    std::cout << nodes_ << std::endl;
}

BenchBvh4BuildEmbree::BenchBvh4BuildEmbree(const imba::TriangleMesh* mesh)
    : Bench("bench_bvh4_build_embree") {
    scene_ = rtcNewScene(RTC_SCENE_DYNAMIC | RTC_SCENE_HIGH_QUALITY, RTC_INTERSECT1);
    mesh_id_ = rtcNewTriangleMesh(scene_, RTC_GEOMETRY_DYNAMIC, mesh->triangle_count(), mesh->vertex_count(), 1);

    float* vptr = reinterpret_cast<float*>(rtcMapBuffer(scene_, mesh_id_, RTC_VERTEX_BUFFER0));
    for (int i = 0; i < mesh->vertex_count(); i++) {
        const imba::Vec3& v = mesh->vertices()[i];
        vptr[i * 4 + 0] = v[0];
        vptr[i * 4 + 1] = v[1];
        vptr[i * 4 + 2] = v[2];
    }
    rtcUnmapBuffer(scene_, mesh_id_, RTC_VERTEX_BUFFER0);

    int* iptr = reinterpret_cast<int*>(rtcMapBuffer(scene_, mesh_id_, RTC_INDEX_BUFFER));
    for (int i = 0; i < mesh->triangle_count(); i++) {
        const imba::TriangleMesh::Triangle& t = mesh->triangles()[i];

        iptr[i * 3 + 0] = t[0];
        iptr[i * 3 + 1] = t[1];
        iptr[i * 3 + 2] = t[2];
    }    
    rtcUnmapBuffer(scene_, mesh_id_, RTC_INDEX_BUFFER);

    rtcCommit(scene_);
}

BenchBvh4BuildEmbree::~BenchBvh4BuildEmbree() {
    rtcDeleteGeometry(scene_, mesh_id_);
    rtcDeleteScene(scene_);
}

void BenchBvh4BuildEmbree::iteration() {
    rtcUpdate(scene_, mesh_id_);
    rtcCommit(scene_);
}

} // namespace bench

