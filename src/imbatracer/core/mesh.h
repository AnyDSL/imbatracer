#ifndef IMBA_MESH_H
#define IMBA_MESH_H

#include <cstdint>
#include <vector>

#include "float4.h"
#include "float4x4.h"
#include "float3x4.h"
#include "tri.h"

namespace imba {

/// Triangle mesh represented as a list of indices, a list of vertices
/// and a collection of attributes.
class Mesh {
public:
    /// A mesh instance that refers to a particular mesh within an array of meshes.
    struct Instance {
        int      id;
        float3x4 mat;
        float3x4 inv_mat;

        Instance() {}
        Instance(int i, const float4x4& m) : id(i), mat(m), inv_mat(invert(m)) {}
    };

    enum class AttributeType {
        FLOAT,
        FLOAT2,
        FLOAT3,
        FLOAT4,
        INT,
        INT2,
        INT3,
        INT4
    };

    enum class AttributeBinding {
        PER_VERTEX = 0,
        PER_FACE = 1
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
    void set_index_count(size_t count) {
        indices_.resize(count);
        for (auto& attr: attrs_) {
            if (attr.binding == AttributeBinding::PER_FACE)
                attr.data.resize(attr.stride * count / 4);
        }
    }
    size_t vertex_count() const { return vertices_.size(); }
    void set_vertex_count(size_t count) {
        vertices_.resize(count);
        for (auto& attr: attrs_) {
            if (attr.binding == AttributeBinding::PER_VERTEX)
                attr.data.resize(attr.stride * count);
        }
    }

    void add_attribute(AttributeType type, AttributeBinding binding = AttributeBinding::PER_VERTEX) {
        attrs_.emplace_back(type, binding, vertex_count());
    }
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
    AttributeType attribute_type(int i) const { return attrs_[i].type; }
    AttributeBinding attribute_binding(int i) const { return attrs_[i].binding; }

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

    void compute_normals(int normal_attr);
    void compute_bounding_box();

    BBox bounding_box() const { return bbox_; }

private:
    static int stride_bytes(AttributeType type) {
        switch (type) {
            case AttributeType::FLOAT:    return 4;
            case AttributeType::FLOAT2:   return 4 * 2;
            case AttributeType::FLOAT3:   return 4 * 4;
            case AttributeType::FLOAT4:   return 4 * 4;
            case AttributeType::INT:      return 4;
            case AttributeType::INT2:     return 4 * 2;
            case AttributeType::INT3:     return 4 * 4;
            case AttributeType::INT4:     return 4 * 4;
        }
    }

    struct Attribute {
        size_t stride;
        AttributeType type;
        AttributeBinding binding;
        std::vector<uint8_t> data;

        Attribute() {}
        Attribute(AttributeType type, AttributeBinding binding, size_t count)
            : stride(stride_bytes(type))
            , type(type)
            , binding(binding)
            , data(stride * count)
        {}
    };

    std::vector<uint32_t> indices_;
    std::vector<float4> vertices_;
    std::vector<Attribute> attrs_;
    BBox bbox_;
};

} // namespace imba

#endif // IMBA_MESH_H
