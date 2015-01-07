#ifndef IMBA_TRIANGLE_MESH_HPP
#define IMBA_TRIANGLE_MESH_HPP

#include <vector>
#include <algorithm>
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

    void set_vertices(Vertex* verts, int count) { vertices_.assign(verts, verts + count); }
    void set_normals(Normal* norms, int count) { normals_.assign(norms, norms + count); }
    void set_texcoords(Texcoord* texs, int count) { texcoords_.assign(texs, texs + count); }
    void set_materials(int* mats, int count) { materials_.assign(mats, mats + count); }
    void set_triangles(Triangle* tris, int count) { triangles_.assign(tris, tris + count); }

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

private:
    ThorinVector<Vertex>   vertices_;
    ThorinVector<Normal>   normals_;
    ThorinVector<Texcoord> texcoords_;
    ThorinVector<int>      materials_;
    ThorinVector<Triangle> triangles_;
};

} // namespace imba

#endif // IMBA_TRIANGLE_MESH_HPP

