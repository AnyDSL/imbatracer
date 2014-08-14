#ifndef SCENELOAD_H
#define SCENELOAD_H

#include "interface.h"
#include <vector>
#include <tuple>

namespace rt {

class Scene;
struct BuildState;

const unsigned NoIdx = 0xFFFFFFFF;

/** Collect all the data for a Tri - this is not the form in which it is sent to Impala! */
struct Tri {
    Tri(unsigned p1, unsigned p2, unsigned p3,
        unsigned n1 = NoIdx, unsigned n2 = NoIdx, unsigned n3 = NoIdx,
        unsigned t1 = NoIdx, unsigned t2 = NoIdx, unsigned t3 = NoIdx,
        unsigned mat = NoIdx)
        : p1(p1), p2(p2), p3(p3), n1(n1), n2(n2), n3(n3), t1(t1), t2(t2), t3(t3), mat(mat) {}
    unsigned p1, p2, p3; // vertex indices
    unsigned n1, n2, n3; // normal indices
    unsigned t1, t2, t3; // texCoord indices
    unsigned mat;    // material index - this is global, not per-object
};

enum class BVHMode {
    Simple,
    SHAFast,
    SHASlow,
};

/** C++-side representation of an object. */
class Object {
public:
    Object() {}

    friend class Scene;
protected:
    std::vector<impala::Point> verts;
    std::vector<impala::Vec> normals;
    std::vector<impala::TexCoord> texCoords;
    std::vector<Tri> tris;

    // Build the BVH tree. Put primitives into triBuf (starting at offset triBufOff), and the flattened nodes into nodeBuf.
    size_t buildBVH(BuildState *state, BVHMode bvhMode = BVHMode::SHAFast);

private:

    /* all the stuff needed for BVH construction */
    // information needed for a node split - avoid passing dozens of parameters to the split function
    struct NodeSplitInformation
	{
		NodeSplitInformation(unsigned *left, unsigned *right)
			: left(left), right(right), leftBBox(impala::BBox::empty()), rightBBox(impala::BBox::empty()), bestAxis(-1)
		{}

		unsigned nLeft;
		unsigned *left, *right;
        impala::BBox leftBBox;
        impala::BBox rightBBox;
		int bestAxis;
        float bestCost;
	};
    struct SplitPlaneCandidate
    {
        unsigned nLeft;
        impala::BBox leftBBox;
        impala::BBox rightBBox;
    };

    // extra BVH construction stuff
    BVHMode bvhMode;
    std::vector<std::tuple<impala::BBox, impala::Point>> triData; // bounds and centroid of each tri
    SplitPlaneCandidate *splitPlaneCands;

    // accessors
    impala::Point triCentroid(unsigned tri) { return std::get<1>(triData[tri]); }
    impala::BBox triBound(unsigned tri) { return std::get<0>(triData[tri]); }


    // target buffers for building Impala data
    BuildState *state;

    /* Build a BVH node from the triangles (in tris). These are indices into the tris vector above!
     * Put primitives into triBuf (starting at offset triBufOff), and the flattened nodes into nodeBuf.
     * Return the index of the root (in nodeBuf).
     */
    size_t buildBVHNode(unsigned *splitTris, unsigned nTris, impala::BBox triBounds, unsigned depth, unsigned *leftTris, unsigned *rightTris);

    /* Do a split decision, and update splitInfo accordingly */
    void split(unsigned *splitTris, unsigned nTris, impala::BBox triBounds, unsigned depth, NodeSplitInformation *splitInfo);
    void splitSHAAxis(unsigned *splitTris, unsigned nTris, impala::BBox triBounds, NodeSplitInformation *splitInfo, unsigned axis);
};

/** C++-side representation of a scene. Also manages the dynamically allocated memory. */
class Scene {
public:
    Scene(impala::Scene *scene);
    virtual ~Scene(void);

    void build();
    void clear();

    void add(Object &&obj) { free(); objects.push_back(obj); }
    void add(const Object &obj) { free(); objects.push_back(obj); }
    size_t addMaterial(const impala::Material &mat) {
        materials.push_back(mat);
        return materials.size()-1;
    }
    size_t addTexture(const impala::Texture &tex) {
        textures.push_back(tex);
        return textures.size()-1;
    }

protected:
    impala::Scene *scene; //!< the Impala Scene. We *own* its dynamically allocated parts!
    std::vector<Object> objects;
    std::vector<impala::Material> materials;
    std::vector<impala::Texture> textures;

private:
    void free();
};

/** Test objects */
class Cube : public Object
{
public:
    Cube(float size = 1.0f);
};


}


#endif
