#include <iostream>
#include <cfloat>

#include <embree2/rtcore.h>
#include <embree2/rtcore_ray.h> 

#include "bench_ray_bvh.hpp"

namespace bench {

void BenchRayBvhImpala::iteration() {
    bench_ray_bvh(nrays_, &build_->bvh, (Vec3*)mesh_->vertices(), (int*)mesh_->triangles(), result_.get());
    tmin_ = result_->tmin;
    tmax_ = result_->tmax;
    icount_ = result_->intr_count;
}

void BenchRayBvhImpala::display() {
    std::cout << tmin_ << " " << tmax_ << " " << icount_ << std::endl;
}

void BenchRay4BvhImpala::iteration() {
    bench_ray4_bvh(nrays_, &build_->bvh, (Vec3*)mesh_->vertices(), (int*)mesh_->triangles(), result_.get());
    tmin_ = result_->tmin;
    tmax_ = result_->tmax;
    icount_ = result_->intr_count;
}

void BenchRay4BvhImpala::display() {
    std::cout << tmin_ << " " << tmax_ << " " << icount_ << std::endl;
}

BenchRayBvh4Embree::BenchRayBvh4Embree(const imba::TriangleMesh* mesh, int nrays)
    : Bench("bench_ray_bvh4_embree")
    , nrays_(nrays) {
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

BenchRayBvh4Embree::~BenchRayBvh4Embree() {
    rtcDeleteGeometry(scene_, mesh_id_);
    rtcDeleteScene(scene_);
}

void BenchRayBvh4Embree::iteration() {
    tmin_ = FLT_MAX;
    tmax_ = -FLT_MAX;
    icount_ = 0;
    for (int i = 0; i < nrays_; i++) {
        RTCRay ray;

        const float pi = 3.14159265359;
        const float theta = (i % 1000) * pi / 1000.0f;
        const float phi = i * 2.0f * pi / 1000.0f;

        const float sin_theta = sinf(theta);

        ray.org[0] = 0.3f;
        ray.org[1] = -0.1f;
        ray.org[2] = 0.2f;

        ray.dir[0] = sin_theta * cosf(phi);
        ray.dir[1] = sin_theta * sinf(phi);
        ray.dir[2] = cosf(theta);

        ray.tnear = 0.0f;
        ray.tfar = FLT_MAX;
        ray.time = 0.0f;
        ray.mask = 0xFFFFFFFF;

        ray.primID = RTC_INVALID_GEOMETRY_ID;
        ray.geomID = RTC_INVALID_GEOMETRY_ID;
        ray.instID = RTC_INVALID_GEOMETRY_ID;

        rtcIntersect(scene_, ray);

        if (ray.primID >= 0) {
            if (tmin_ > ray.tfar) {
                tmin_ = ray.tfar;
            }

            if (tmax_ < ray.tfar) {
                tmax_ = ray.tfar;
            }
            icount_++;
        }
    }
}

void BenchRayBvh4Embree::display() {
    std::cout << tmin_ << " " << tmax_ << " " << icount_ << std::endl;
}

BenchRay4Bvh4Embree::BenchRay4Bvh4Embree(const imba::TriangleMesh* mesh, int nray4s)
    : Bench("bench_ray4_bvh4_embree")
    , nray4s_(nray4s) {
    scene_ = rtcNewScene(RTC_SCENE_DYNAMIC | RTC_SCENE_HIGH_QUALITY, RTC_INTERSECT4);
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

BenchRay4Bvh4Embree::~BenchRay4Bvh4Embree() {
    rtcDeleteGeometry(scene_, mesh_id_);
    rtcDeleteScene(scene_);
}

void BenchRay4Bvh4Embree::iteration() {
    tmin_ = FLT_MAX;
    tmax_ = -FLT_MAX;
    icount_ = 0;
    for (int i = 0; i < nray4s_; i++) {
        RTCRay4 ray;

        const float pi = 3.14159265359;
        
        int valid[4] = {-1, -1, -1, -1};

        ray.orgx[0] = 0.3f;
        ray.orgx[1] = 0.3f;
        ray.orgx[2] = 0.3f;
        ray.orgx[3] = 0.3f;

        ray.orgy[0] = -0.1f;
        ray.orgy[1] = -0.1f;
        ray.orgy[2] = -0.1f;
        ray.orgy[3] = -0.1f;

        ray.orgz[0] = 0.2f;
        ray.orgz[1] = 0.2f;
        ray.orgz[2] = 0.2f;
        ray.orgz[3] = 0.2f;

        for (int j = 0; j < 4; j++) {
            const int k = i * 4 + j;
            const float theta = (k % 1000) * pi / 1000.0f;
            const float phi = k * 2.0f * pi / 1000.0f;
            const float sin_theta = sinf(theta);

            ray.dirx[j] = sin_theta * cosf(phi);
            ray.diry[j] = sin_theta * sinf(phi);
            ray.dirz[j] = cosf(theta);
        }

        ray.tnear[0] = 0.0f;
        ray.tnear[1] = 0.0f;
        ray.tnear[2] = 0.0f;
        ray.tnear[3] = 0.0f;

        ray.tfar[0] = FLT_MAX;
        ray.tfar[1] = FLT_MAX;
        ray.tfar[2] = FLT_MAX;
        ray.tfar[3] = FLT_MAX;

        ray.time[0] = 0.0f;
        ray.time[1] = 0.0f;
        ray.time[2] = 0.0f;
        ray.time[3] = 0.0f;

        ray.mask[0] = 0xFFFFFFFF;
        ray.mask[1] = 0xFFFFFFFF;
        ray.mask[2] = 0xFFFFFFFF;
        ray.mask[3] = 0xFFFFFFFF;

        ray.primID[0] = RTC_INVALID_GEOMETRY_ID;
        ray.primID[1] = RTC_INVALID_GEOMETRY_ID;
        ray.primID[2] = RTC_INVALID_GEOMETRY_ID;
        ray.primID[3] = RTC_INVALID_GEOMETRY_ID;

        ray.geomID[0] = RTC_INVALID_GEOMETRY_ID;
        ray.geomID[1] = RTC_INVALID_GEOMETRY_ID;
        ray.geomID[2] = RTC_INVALID_GEOMETRY_ID;
        ray.geomID[3] = RTC_INVALID_GEOMETRY_ID;

        ray.instID[0] = RTC_INVALID_GEOMETRY_ID;
        ray.instID[1] = RTC_INVALID_GEOMETRY_ID;
        ray.instID[2] = RTC_INVALID_GEOMETRY_ID;
        ray.instID[3] = RTC_INVALID_GEOMETRY_ID;

        rtcIntersect4(&valid, scene_, ray);

        for (int j = 0; j < 4; j++) {
            if (ray.primID[j] >= 0) {
                if (tmin_ > ray.tfar[j]) {
                    tmin_ = ray.tfar[j];
                }

                if (tmax_ < ray.tfar[j]) {
                    tmax_ = ray.tfar[j];
                }
                icount_++;
            }
        }
    }
}

void BenchRay4Bvh4Embree::display() {
    std::cout << tmin_ << " " << tmax_ << " " << icount_ << std::endl;
}

} // namespace bench

