#ifndef IMBA_MESH_H
#define IMBA_MESH_H

#include <cstdint>

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

    template <typename T>
    class AttributeProxy {
        friend class Mesh;
    public:
        const T& operator [] (int i) const {
            return *reinterpret_cast<const T*>(data_ + stride_ * i);
        }

        T& operator [] (int i) {
            return *reinterpret_cast<T*>(data_ + stride_ * i);
        }

    private:
        AttributeProxy(uint8_t* data, int stride)
            : data_(data), stride_(stride)
        {}

        uint8_t* data_;
        size_t stride_;
    };

    const uint32_t* indices() const { return indices_.data(); }
    const float4* vertices() const { return vertices_.data(); }
    uint32_t* indices() { return indices_.data(); }
    float4* vertices() { return vertices_.data(); }

    size_t index_count() const { return indices_.size(); }
    void set_index_count(size_t count) { indices_.resize(count); }
    size_t vertex_count() const { return vertices_.size(); }
    void set_vertex_count(size_t count) {
        vertices_.resize(count);
        for (auto& attr: attrs_)
            attr.data.resize(attr.stride * count);
    }

    void add_attribute(AttributeType type) { attrs_.emplace_back(type, vertex_count()); }
    size_t attribute_count() const { return attrs_.size(); }
    template <typename T>
    const AttributeProxy<T> attribute(int i) const {
        return AttributeProxy<T>(const_cast<uint8_t*>(attrs_[i].data.data()), attrs_[i].stride);
    }
    template <typename T>
    AttributeProxy<T> attribute(int i) {
        return AttributeProxy<T>(attrs_[i].data.data(), attrs_[i].stride);
    }
    size_t attribute_stride(int i) const { return attrs_[i].stride; }

    Tri triangle(int i) const {
        int i0 = indices_[i * 4 + 0];
        int i1 = indices_[i * 4 + 1];
        int i2 = indices_[i * 4 + 2];
        return Tri(float3(vertices_[i0].x, vertices_[i0].y, vertices_[i0].z),
                   float3(vertices_[i1].x, vertices_[i1].y, vertices_[i1].z),
                   float3(vertices_[i2].x, vertices_[i2].y, vertices_[i2].z));
    }

    size_t triangle_count() const {
        return indices_.size() / 4;
    }

    void compute_normals(bool smooth, int normal_attr);

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
            : stride(stride_bytes(type))
            , type(type)
            , data(stride * count)
        {}
    };
    
    std::vector<uint32_t> indices_;
    std::vector<float4> vertices_;
    std::vector<Attribute> attrs_;
};

} // namespace imba

#endif // IMBA_MESH_H
