#define _X86INTRIN_H_INCLUDED
#include <common/scene_triangle_mesh.h>
#include <common/accelinstance.h>
#include <common/scene.h>
#include <geometry/triangle1.h>
#include <bvh4/bvh4.h>

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
    : Bench("bench_bvh4_build_embree")
    , scene_(new embree::Scene(RTC_SCENE_DYNAMIC, RTC_INTERSECT1)) {
    mesh_id_ = scene_->newTriangleMesh(RTC_GEOMETRY_DYNAMIC, mesh->triangle_count(), mesh->vertex_count(), 1);
    embree::TriangleMesh* embree_mesh = scene_->getTriangleMesh(mesh_id_);

    char* vptr = reinterpret_cast<char*>(embree_mesh->map(RTC_VERTEX_BUFFER0));
    int vstride = embree_mesh->getVertexBufferStride();

    for (int i = 0; i < mesh->vertex_count(); i++) {
        const imba::Vec3& v = mesh->vertices()[i];
        float* u = reinterpret_cast<float*>(vptr + vstride * i);

        u[0] = v[0];
        u[1] = v[1];
        u[2] = v[2];
    }

    embree_mesh->unmap(RTC_VERTEX_BUFFER);

    char* iptr = reinterpret_cast<char*>(embree_mesh->map(RTC_INDEX_BUFFER));
    int istride = embree_mesh->getTriangleBufferStride();

    for (int i = 0; i < mesh->triangle_count(); i++) {
        const imba::TriangleMesh::Triangle& t = mesh->triangles()[i];
        int* u = reinterpret_cast<int*>(iptr + istride * i);

        u[0] = t[0];
        u[1] = t[1];
        u[2] = t[2];
    }

    embree_mesh->unmap(RTC_INDEX_BUFFER);
    scene_->build(0, 1);
}

void BenchBvh4BuildEmbree::iteration() {
    embree::TriangleMesh* embree_mesh = scene_->getTriangleMesh(mesh_id_);
    embree::Accel* accel = embree::BVH4::BVH4Triangle1ObjectSplit(embree_mesh);
    accel->build(0, 1);
    delete accel;
}

} // namespace bench

