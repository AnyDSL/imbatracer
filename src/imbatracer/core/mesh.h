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

    const uint32_t* indices() const { return tris_.data(); }
    const float4* vertices() const { return verts_.data(); }
    uint32_t* indices() { return tris_.data(); }
    float4* vertices() { return verts_.data(); }

    size_t index_count() const { return tris_.size(); }
    void set_index_count(size_t count) { tris_.resize(count); }
    size_t vertex_count() const { return verts_.size(); }
    void set_vertex_count(size_t count) {
        verts_.resize(count);
        for (auto attr: attrs_)
            attr.data.resize(attr.stride * count);
    }

    void add_attribute(AttributeType type) {
        attrs_.emplace_back(type, vertex_count());
    }
    size_t attribute_count() const { return attrs_.size(); }
    const uint8_t* attribute(int i) const { return attrs_[i].data.data(); }
    uint8_t* attribute(int i) { return attrs_[i].data.data(); }
    size_t attribute_stride(int i) const { return attrs_[i].stride; }

    Tri triangle(int i) const {
        int i0 = tris_[i * 3 + 0];
        int i1 = tris_[i * 3 + 1];
        int i2 = tris_[i * 3 + 2];
        return Tri(float3(verts_[i0].x, verts_[i0].y, verts_[i0].z),
                   float3(verts_[i1].x, verts_[i1].y, verts_[i1].z),
                   float3(verts_[i2].x, verts_[i2].y, verts_[i2].z));
    }

    size_t triangle_count() const {
        return tris_.size() / 3;    
    }

private:
    static int stride_bytes(AttributeType type) {
        switch (type) {
            case ATTR_FLOAT:    return 4;
            case ATTR_FLOAT2:   return 4 * 4;
            case ATTR_FLOAT3:   return 4 * 4;
            case ATTR_FLOAT4:   return 4 * 4;
            case ATTR_INT:      return 4;
            case ATTR_INT2:     return 4 * 4;
            case ATTR_INT3:     return 4 * 4;
            case ATTR_INT4:     return 4 * 4;
        }
    }

    struct Attribute {
        size_t stride;
        AttributeType type;
        ThorinVector<uint8_t> data;

        Attribute() {}
        Attribute(AttributeType type, size_t count)
            : stride(stride_bytes(type)), type(type) {
            data.resize(stride * count);
        }
    };

    ThorinVector<uint32_t> tris_;
    ThorinVector<float4> verts_;
    std::vector<Attribute> attrs_;
};

} // namespace imba

#endif // IMBA_MESH_H
