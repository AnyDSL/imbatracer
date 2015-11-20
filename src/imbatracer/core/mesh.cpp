#include "mesh.h"

#include <cstring>

namespace imba {
	
void Mesh::compute_normals(bool smooth, int normal_attr) {	
	if (smooth) {
		for (int i = 0; i < vertex_count(); ++i) {
			auto& n = get_attr_data<float3>(attrs_[normal_attr], i);
			n = float3(0.0f);
		}

		for (int i = 0; i < triangle_count(); ++i) {
			auto t = triangle(i);
			float3 e0 = t[1] - t[0];
			float3 e1 = t[2] - t[0];
			float3 n = cross(e0, e1);
			get_attr_data<float3>(attrs_[normal_attr], i * 3 + 0) += n;
			get_attr_data<float3>(attrs_[normal_attr], i * 3 + 1) += n;
			get_attr_data<float3>(attrs_[normal_attr], i * 3 + 2) += n;
		}

		for (int i = 0; i < vertex_count(); ++i) {
			auto& n = get_attr_data<float3>(attrs_[normal_attr], i);
			n = normalize(n);
		}
	} else {
		// Allocate memory for the new vertex data, index data and other attributes.
		std::vector<uint32_t> new_indices(triangle_count() * 3);
		std::vector<float4> new_vertices(triangle_count() * 3);
		std::vector<Attribute> new_attrs(attribute_count());
		
		for (int i = 0; i < attribute_count(); ++i) {
			new_attrs.emplace_back(attrs_[i].type, triangle_count() * 3);
		}
		
		uint32_t vertex_offset = 0;
		for (int i = 0; i < triangle_count(); ++i) {
			auto t = triangle(i);

			// Create new vertices for this face
			new_vertices[vertex_offset + 0] = float4(t[0], 0.0f);
			new_vertices[vertex_offset + 1] = float4(t[1], 0.0f);
			new_vertices[vertex_offset + 2] = float4(t[2], 0.0f);
			
			// Compute and set the normal
			float3 e0 = t[1] - t[0];
			float3 e1 = t[2] - t[0];
			float3 n = normalize(cross(e0, e1));
			
			get_attr_data<float3>(attrs_[normal_attr], vertex_offset + 0) = n;
			get_attr_data<float3>(attrs_[normal_attr], vertex_offset + 1) = n;
			get_attr_data<float3>(attrs_[normal_attr], vertex_offset + 2) = n;
			
			// Copy all other attributes
			for (int k = 0; k < attribute_count(); ++k) {
				if (k == normal_attr) continue;
				
				auto new_attr = new_attrs[k].data.data();
				auto old_attr = attrs_[k].data.data();
				auto stride = attrs_[k].stride;
				
				for (int m = 0; m < 3; ++m) {
					int idx = indices_[i * 3 + m];
					memcpy(new_attr + (vertex_offset + m) * stride,
							old_attr + idx * stride,
							stride);
				}
			}

			// Set the new indices
			new_indices[i * 3 + 0] = vertex_offset++;
			new_indices[i * 3 + 1] = vertex_offset++;
			new_indices[i * 3 + 2] = vertex_offset++;
		}

		std::swap(vertices_, new_vertices);
		std::swap(indices_, new_indices);
		std::swap(attrs_, new_attrs);
	}
}
	
} // namespace imba