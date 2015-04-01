#ifndef IMBA_TRIANGLE_MESH_HPP
#define IMBA_TRIANGLE_MESH_HPP

#include <vector>
#include <algorithm>
#include <cassert>
#include "../common/memory.hpp"
#include "../common/vector.hpp"

namespace imba {

/// A triangle mesh object. Holds positions (per vertex), normals (per vertex),
/// texture coordinates (per vertex), material indices (per triangle), triangle indices.
/// A triangle mesh may not have normals, texcoords, or materials.
class TriangleMesh {
public:
    typedef Vec3 Vertex;
    typedef Vec3 Normal;
    typedef Vec2 Texcoord;

    struct Triangle {
        Triangle() {}
        Triangle(int a, int b, int c) {
            indices_[0] = a;
            indices_[1] = b;
            indices_[2] = c;
        }

        int operator[] (int i) const { return indices_[i]; }
        int& operator[] (int i) { return indices_[i]; }

        int indices_[3];
    };

    TriangleMesh() {}

    TriangleMesh(TriangleMesh&& other) {
        vertices_ = std::move(other.vertices_);
        normals_ = std::move(other.normals_);
        texcoords_ = std::move(other.texcoords_);
        materials_ = std::move(other.materials_);
        triangles_ = std::move(other.triangles_);
    }

    TriangleMesh(const Vertex* verts, int vcount,
                 const Triangle* tris, int tcount)
        : vertices_(verts, verts + vcount),
          triangles_(tris, tris + tcount)
    {}

    TriangleMesh(const Vertex* verts, const Normal* norms, int vcount,
                 const Triangle* tris, int tcount)
        : vertices_(verts, verts + vcount),
          normals_(norms, norms + vcount),
          triangles_(tris, tris + tcount)
    {}

    TriangleMesh(const Vertex* verts, const Normal* norms,
                 const Texcoord* texs, int vcount,
                 const Triangle* tris, int tcount)
        : vertices_(verts, verts + vcount),
          normals_(norms, norms + vcount),
          texcoords_(texs, texs + vcount),
          triangles_(tris, tris + tcount)
    {}

    TriangleMesh(const Vertex* verts, const Normal* norms,
                 const Texcoord* texs, int vcount,
                 const Triangle* tris, const int* mats,
                 int tcount)
        : vertices_(verts, verts + vcount),
          normals_(norms, norms + vcount),
          texcoords_(texs, texs + vcount),
          materials_(mats, mats + tcount),
          triangles_(tris, tris + tcount)
    {}

    void add_vertex(const Vertex& v) { vertices_.push_back(v); }
    void add_normal(const Normal& n) { normals_.push_back(n); }
    void add_texcoord(const Texcoord& t) { texcoords_.push_back(t); }
    void add_material(int m) { materials_.push_back(m); }
    void add_triangle(const Triangle& tri) { triangles_.push_back(tri); }

    void set_vertices(const Vertex* verts, int count) { vertices_.assign(verts, verts + count); }
    void set_normals(const Normal* norms, int count) { normals_.assign(norms, norms + count); }
    void set_texcoords(const Texcoord* texs, int count) { texcoords_.assign(texs, texs + count); }
    void set_materials(const int* mats, int count) { materials_.assign(mats, mats + count); }
    void set_triangles(const Triangle* tris, int count) { triangles_.assign(tris, tris + count); }

    const Vertex* vertices() const { return vertices_.data(); }
    const Normal* normals() const { return normals_.data(); }
    const Texcoord* texcoords() const { return texcoords_.data(); }
    const int* materials() const { return materials_.data(); }
    const Triangle* triangles() const { return triangles_.data(); }

    Vertex* vertices() { return vertices_.data(); }
    Normal* normals() { return normals_.data(); }
    Texcoord* texcoords() { return texcoords_.data(); }
    int* materials() { return materials_.data(); }
    Triangle* triangles() { return triangles_.data(); }

    int vertex_count() const { return vertices_.size(); }
    int normal_count() const { return normals_.size(); }
    int texcoord_count() const { return texcoords_.size(); }
    int material_count() const { return materials_.size(); }
    int triangle_count() const { return triangles_.size(); }

    void set_vertex_count(int count) { vertices_.resize(count); }
    void set_normal_count(int count) { normals_.resize(count); }
    void set_texcoord_count(int count) { texcoords_.resize(count); }
    void set_material_count(int count) { materials_.resize(count); }
    void set_triangle_count(int count) { triangles_.resize(count); }

    bool has_normals() const { return normals_.size() > 0; }
    bool has_texcoords() const { return texcoords_.size() > 0; }
    bool has_materials() const { return materials_.size() > 0; }

    void append(const TriangleMesh& other) {
        assert(normals_.size()   == 0 || normals_.size()   == vertices_.size());
        assert(texcoords_.size() == 0 || texcoords_.size() == vertices_.size());
        assert(materials_.size() == triangles_.size());

        size_t old_verts = vertices_.size();

        vertices_.insert(vertices_.end(), other.vertices_.begin(), other.vertices_.end());
        normals_.insert(normals_.end(), other.normals_.begin(), other.normals_.end());
        texcoords_.insert(texcoords_.end(), other.texcoords_.begin(), other.texcoords_.end());
        materials_.insert(materials_.end(), other.materials_.begin(), other.materials_.end());

        // Transform indices so that they refer to the new normals and indices
        std::transform(other.triangles_.begin(), other.triangles_.end(),
                       std::back_inserter(triangles_), [old_verts] (const Triangle& tri) {
            return Triangle(tri[0] + old_verts, tri[1] + old_verts, tri[2] + old_verts);
        });
    }

    /// Recomputes the mesh normals. Smoothly interpolates normals if "smooth" is set to true.
    void compute_normals(bool smooth) {
        if (smooth) {
            normals_.resize(vertices_.size());
            std::fill(normals_.begin(), normals_.end(), imba::Vec3(0.0f));

            for (auto& t : triangles_) {
                const imba::Vec3& v0 = vertices_[t[0]];
                const imba::Vec3& v1 = vertices_[t[1]];
                const imba::Vec3& v2 = vertices_[t[2]];
                const imba::Vec3& e0 = v1 - v0;
                const imba::Vec3& e1 = v2 - v0;
                const imba::Vec3& n = imba::cross(e0, e1);
                normals_[t[0]] += n;
                normals_[t[1]] += n;
                normals_[t[2]] += n;
            }

            for (auto& n : normals_)
                n = imba::normalize(n);
        } else {
            ThorinVector<Vertex> new_verts;
            ThorinVector<Texcoord> new_tex;
            ThorinVector<Normal> new_norm;

            for (auto& t : triangles_) {
                const imba::Vec3& v0 = vertices_[t[0]];
                const imba::Vec3& v1 = vertices_[t[1]];
                const imba::Vec3& v2 = vertices_[t[2]];

                new_verts.push_back(v0);
                new_verts.push_back(v1);
                new_verts.push_back(v2);

                if (has_texcoords()) {
                    new_tex.push_back(texcoords_[t[0]]);
                    new_tex.push_back(texcoords_[t[1]]);
                    new_tex.push_back(texcoords_[t[2]]);
                }

                const imba::Vec3& e0 = v1 - v0;
                const imba::Vec3& e1 = v2 - v0;
                const imba::Vec3& n = imba::normalize(imba::cross(e0, e1));
                new_norm.push_back(n);
                new_norm.push_back(n);
                new_norm.push_back(n);

                t[0] = new_verts.size() - 3;
                t[1] = new_verts.size() - 2;
                t[2] = new_verts.size() - 1;
            }

            std::swap(vertices_, new_verts);
            std::swap(texcoords_, new_tex);
            std::swap(normals_, new_norm);
        }
    }

private:
    ThorinVector<Vertex>   vertices_;
    ThorinVector<Normal>   normals_;
    ThorinVector<Texcoord> texcoords_;
    ThorinVector<int>      materials_;
    ThorinVector<Triangle> triangles_;
};

} // namespace imba

#endif // IMBA_TRIANGLE_MESH_HPP

