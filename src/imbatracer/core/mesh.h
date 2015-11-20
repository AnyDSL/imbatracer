#ifndef IMBA_MESH_H
#define IMBA_MESH_H

#include <cstdint>
#include "allocator.h"
#include "float4.h"
#include "tri.h"

namespace imba {

/// Triangle mesh represented as a list of indices, a list of vertices
/// and a collection of attributes.
class Mesh {
public:
    enum AttributeType {
        ATTR_FLOAT,
        ATTR_FLOAT2,
        ATTR_FLOAT3,
        ATTR_FLOAT4,
        ATTR_INT,
        ATTR_INT2,
        ATTR_INT3,
        ATTR_INT4
    };

    const uint32_t* indices() const { return indices_.data(); }
    const float4* vertices() const { return vertices_.data(); }
    const float2* texcoords() const { return texcoords_.data(); }
    uint32_t* indices() { return indices_.data(); }
    float4* vertices() { return vertices_.data(); }
    float2* texcoords() { return texcoords_.data(); }

    size_t index_count() const { return indices_.size(); }
    void set_index_count(size_t count) { indices_.resize(count); }
    size_t vertex_count() const { return vertices_.size(); }
    void set_vertex_count(size_t count) {
        vertices_.resize(count);
        for (auto attr: attrs_)
            attr.data.resize(attr.stride * count);
    }
    size_t texcoord_count() const { return texcoords_.size(); }
    void set_texcoord_count(size_t count) { texcoords_.resize(count); }

    void add_attribute(AttributeType type) {
        attrs_.emplace_back(type, vertex_count());
    }
    size_t attribute_count() const { return attrs_.size(); }
    const uint8_t* attribute(int i) const { return attrs_[i].data.data(); }
    uint8_t* attribute(int i) { return attrs_[i].data.data(); }
    size_t attribute_stride(int i) const { return attrs_[i].stride; }

    Tri triangle(int i) const {
        int i0 = indices_[i * 3 + 0];
        int i1 = indices_[i * 3 + 1];
        int i2 = indices_[i * 3 + 2];
        return Tri(float3(vertices_[i0].x, vertices_[i0].y, vertices_[i0].z),
                   float3(vertices_[i1].x, vertices_[i1].y, vertices_[i1].z),
                   float3(vertices_[i2].x, vertices_[i2].y, vertices_[i2].z));
    }
    
    float2 calc_texcoords(int tri_id, float u, float v, float w) {
        int i0 = indices_[tri_id * 3 + 0];
        int i1 = indices_[tri_id * 3 + 1];
        int i2 = indices_[tri_id * 3 + 2];
        
        float2 uv0 = texcoords_[i0];
        float2 uv1 = texcoords_[i1];
        float2 uv2 = texcoords_[i2];
        
        return uv0 * w + uv1 * u + uv2 * v;
    }

    size_t triangle_count() const {
        return indices_.size() / 3;
    }

private:
    static int stride_bytes(AttributeType type) {
        switch (type) {
            case ATTR_FLOAT:    return 4;
            case ATTR_FLOAT2:   return 4 * 2;
            case ATTR_FLOAT3:   return 4 * 4;
            case ATTR_FLOAT4:   return 4 * 4;
            case ATTR_INT:      return 4;
            case ATTR_INT2:     return 4 * 2;
            case ATTR_INT3:     return 4 * 4;
            case ATTR_INT4:     return 4 * 4;
        }
    }

    struct Attribute {
        size_t stride;
        AttributeType type;
        std::vector<uint8_t> data;

        Attribute() {}
        Attribute(AttributeType type, size_t count)
            : stride(stride_bytes(type)), type(type) {
            data.resize(stride * count);
        }
    };

    std::vector<uint32_t> indices_;
    std::vector<float4> vertices_;
    std::vector<Attribute> attrs_;
    
    std::vector<float2> texcoords_;
};

} // namespace imba

#endif // IMBA_MESH_H
