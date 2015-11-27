#include <cstring>

#include "mesh.h"

namespace imba {

void Mesh::compute_normals(bool smooth, int normal_attr) {
    if (smooth) {
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
    } else {
        // Allocate memory for the new vertex data, index data and other attributes.
        std::vector<uint32_t> new_indices(triangle_count() * 3);
        std::vector<float4> new_vertices(triangle_count() * 3);
        std::vector<Attribute> new_attrs(attribute_count());

        for (int i = 0; i < attribute_count(); ++i) {
            new_attrs.emplace_back(attrs_[i].type, triangle_count() * 3);
        }

        auto normals = AttributeProxy<float3>(new_attrs[normal_attr].data.data(),
                                              new_attrs[normal_attr].stride);
        uint32_t vertex_offset = 0;
        for (int i = 0; i < triangle_count(); ++i) {
            const auto t = triangle(i);

            // Create new vertices for this face
            new_vertices[vertex_offset + 0] = float4(t[0], 1.0f);
            new_vertices[vertex_offset + 1] = float4(t[1], 1.0f);
            new_vertices[vertex_offset + 2] = float4(t[2], 1.0f);

            // Compute and set the normal
            float3 e0 = t[1] - t[0];
            float3 e1 = t[2] - t[0];
            float3 n = normalize(cross(e0, e1));

            normals[vertex_offset + 0] = n;
            normals[vertex_offset + 1] = n;
            normals[vertex_offset + 2] = n;

            // Set the new indices
            new_indices[i * 4 + 0] = vertex_offset++;
            new_indices[i * 4 + 1] = vertex_offset++;
            new_indices[i * 4 + 2] = vertex_offset++;
        }

        // Copy all other attributes
        for (int k = 0; k < attribute_count(); ++k) {
            if (k == normal_attr) continue;

            auto new_attr = new_attrs[k].data.data();
            auto old_attr = attrs_[k].data.data();
            const auto stride = attrs_[k].stride;

            for (int i = 0; i < triangle_count(); ++i) {
                const int i0 = indices_[i * 4 + 0];
                const int i1 = indices_[i * 4 + 1];
                const int i2 = indices_[i * 4 + 2];
                memcpy(new_attr + (i * 3 + 0) * stride, old_attr + i0 * stride, stride);
                memcpy(new_attr + (i * 3 + 1) * stride, old_attr + i1 * stride, stride);
                memcpy(new_attr + (i * 3 + 2) * stride, old_attr + i2 * stride, stride);
            }
        }

        std::swap(vertices_, new_vertices);
        std::swap(indices_, new_indices);
        std::swap(attrs_, new_attrs);
    }
}

} // namespace imba
