#ifndef SCENELOAD_H
#define SCENELOAD_H

#include "interface.h"
#include <vector>
#include <tuple>

namespace rt {

class Scene;

/** Collect all the data for a Tri - this is not the form in which it is sent to Impala! */
struct Tri {
    Tri(unsigned p1, unsigned p2, unsigned p3) : p1(p1), p2(p2), p3(p3) {}
    unsigned p1, p2, p3;
};

/** C++-side representation of an object. */
class Object {
public:
    Object() {}

    friend class Scene;
protected:
    std::vector<impala::Point> verts;
    std::vector<impala::Vec> normals; // FIXME these are currently ignored
    std::vector<impala::TexCoord> texcoords; // FIXME these are currently ignored
    std::vector<Tri> tris;

    // Build the BVH tree. Put primitives into triBuf (starting at offset triBufOff), and the flattened nodes into nodeBuf.
    size_t buildBVH(unsigned vertBufBase, unsigned *triBuf, unsigned triBufOff, std::vector<impala::BVHNode> *nodeBuf);

private:

    /* all the stuff needed for BVH construction */
    // information needed for a node split - avoid passing dozens of parameters to the split function
    struct NodeSplitInformation
	{
		NodeSplitInformation(unsigned *left, unsigned *right)
			: left(left), right(right), bestAxis(-1)
		{}

		unsigned nLeft;
		unsigned *left, *right;
		//float bestCost, bestPos;
		int bestAxis;
	};

    // extra info about tris: bounds, and centroid
    std::vector<std::tuple<impala::BBox, impala::Point>> triData;

    // target buffers
    std::vector<impala::BVHNode> *nodeBuf;
    unsigned *triBuf;
    unsigned triBufOff;
    unsigned vertBufBase;

    /* Build a BVH node from the triangles (in tris). These are indices into the tris vector above!
     * Put primitives into triBuf (starting at offset triBufOff), and the flattened nodes into nodeBuf.
     * Return the index of the root (in nodeBuf).
     */
    size_t buildBVHNode(unsigned *splitTris, unsigned nTris, unsigned depth, unsigned *leftTris, unsigned *rightTris);

    /* Do a split decision, and update splitInfo accordingly */
    void split(unsigned *splitTris, unsigned nTris, unsigned depth, const impala::BBox &centroidBounds, NodeSplitInformation *splitInfo);
};

/** C++-side representation of a scene. Also manages the dynamically allocated memory. */
class Scene {
public:
    Scene(impala::Scene *scene);
    virtual ~Scene(void);

    void add(Object &&obj) { free(); objects.push_back(obj); }
    void build();
    void clear();

protected:
    impala::Scene *scene; //!< the Impala Scene. We *own* its dynamically allocated parts!
    std::vector<Object> objects;

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
