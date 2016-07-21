#include <cstring>

#include "mesh.h"

namespace imba {

void Mesh::compute_normals(int normal_attr) {
    auto normals = attribute<float3>(normal_attr);

    for (int i = 0; i < vertex_count(); ++i) {
        normals[i] = float3(0.0f);
    }

    for (int i = 0; i < triangle_count(); ++i) {
        auto t = triangle(i);
        float3 e0 = t[1] - t[0];
        float3 e1 = t[2] - t[0];
        float3 n = cross(e0, e1);
        normals[indices_[i * 4 + 0]] += n;
        normals[indices_[i * 4 + 1]] += n;
        normals[indices_[i * 4 + 2]] += n;
    }

    for (int i = 0; i < vertex_count(); ++i) {
        normals[i] = normalize(normals[i]);
    }
}

void Mesh::compute_bounding_box() {
    bbox_ = BBox::empty();
    for (int i = 0; i < vertex_count(); ++i) {
        bbox_.extend(float3(vertices_[i]));
    }
}

} // namespace imba
