#include <thorin_ext_runtime.h>
#include <iostream>
#include <core/util.h>
#include <core/assert.h>
#include <algorithm>
#include "scene.h"

namespace rt {

const unsigned depthLimit = 16; // this is also set in impala!
const unsigned maxPrimsPerLeaf = 4;

/** The state of transforming the C++-side representation to the Impala one */
struct BuildState {
private:
    // holder of buffers
    impala::Scene *scene;
#ifndef NDEBUG
    unsigned totalVerts, totalTris, totalObjects;
#endif
public:
    // number of already used vertices, triangles, ...
    unsigned nVerts, nTris, nObjs;
    // growing buffer of BVH nodes
    std::vector<impala::BVHNode> bvhNodes;

    BuildState(impala::Scene *scene, unsigned totalVerts, unsigned totalTris, unsigned totalObjects)
        : scene(scene), nVerts(0), nTris(0), nObjs(0)
    {
        scene->verts = thorin_new<impala::Point>(totalVerts);
        scene->triVerts = thorin_new<unsigned>(3*totalTris);
        scene->objs = thorin_new<impala::Object>(totalObjects);
        scene->nObjs = totalObjects;

        bvhNodes.reserve(totalTris/2); // just a wild guess

#ifndef NDEBUG
        this->totalVerts = totalVerts;
        this->totalTris = totalTris;
        this->totalObjects = totalObjects;
#endif
    }

    /** Add a Tri to the tri lists. This assumes that the vertices (etc.) have NOT been added yet! */
    void addTri(Tri t)
    {
        scene->triVerts[nTris*3 + 0] = t.p1 + nVerts;
        scene->triVerts[nTris*3 + 1] = t.p2 + nVerts;
        scene->triVerts[nTris*3 + 2] = t.p3 + nVerts;
        assert(t.n1 == NoIdx && t.n2 == NoIdx && t.n3 == NoIdx, "Normals not yet supported");
        assert(t.t1 == NoIdx && t.t2 == NoIdx && t.t3 == NoIdx, "TexCoords not yet supported");
        assert(t.surface == NoIdx, "Materials not yet supported");
        ++nTris;
    }
    /** Add vertices (etc.) to the corresponding lists */
    void addVerts(const std::vector<impala::Point> &verts, const std::vector<impala::Vec> &/*normals*/, const std::vector<impala::TexCoord> /*texcoords*/)
    {
        std::copy(verts.begin(), verts.end(), scene->verts+nVerts);
        nVerts += verts.size();
    }
    /** Add an objects */
    void addObj(unsigned rootIdx)
    {
        scene->objs[nObjs].bvhRoot = rootIdx;
        nObjs += 1;
    }
    /** Dump BVH nodes to Impala */
    void copyNodes()
    {
        assert(nVerts == totalVerts && nTris == totalTris && nObjs == totalObjects, "Wrong number of things added");
        scene->bvhNodes = thorin_new<impala::BVHNode>(bvhNodes.size());
        std::copy(bvhNodes.begin(), bvhNodes.end(), scene->bvhNodes);
    }
};

/** Object */
size_t Object::buildBVH(BuildState *state)
{
    this->state = state;
    auto nTrisOld = state->nTris;
    auto nNodesOld = state->bvhNodes.size();
    // compute bounds of all triangles
    triData.clear();
    triData.reserve(tris.size());
    for (auto &tri: tris) {
        auto bounds = impala::BBox(verts[tri.p1]).extend(verts[tri.p2]).extend(verts[tri.p3]);
        triData.push_back(std::make_tuple(bounds, bounds.centroid()));
    }

    // get space for the index lists we create
    unsigned *leftPrims = new unsigned[tris.size()*depthLimit];
    unsigned *rightPrims = new unsigned[tris.size()*depthLimit];
    unsigned *primsToSplit = new unsigned[tris.size()];
    for (unsigned i = 0; i < tris.size(); ++i) {
        primsToSplit[i] = i;
    }

    // build the tree
    auto rootIdx = buildBVHNode(primsToSplit, tris.size(), 0, leftPrims, rightPrims);
    assert(state->nTris == nTrisOld+tris.size(), "Wrong number of triangles added?");

    // free space
    freeContainer(triData);
    delete[] leftPrims;
    delete[] rightPrims;
    delete[] primsToSplit;

    // done!
    std::cout << "BVH construction finished: " << (state->bvhNodes.size()-nNodesOld) << " nodes, "
              << tris.size() << " primitives in tree." << std::endl;
    return rootIdx;
}

size_t Object::buildBVHNode(unsigned *splitTris, unsigned nTris, unsigned depth, unsigned *leftTris, unsigned *rightTris)
{
    // compute bounds of this node
    impala::BBox centroidBounds = impala::BBox::empty(), triBounds = impala::BBox::empty();
    for (unsigned i = 0; i < nTris; ++i) {
        centroidBounds.extend(std::get<1>(triData[splitTris[i]]));
        triBounds.extend(std::get<0>(triData[splitTris[i]]));
    }
    impala::BVHNode node(triBounds);

    // split
    NodeSplitInformation splitInfo(leftTris, rightTris);
    split(splitTris, nTris, depth, centroidBounds, &splitInfo);
    node.axis = splitInfo.bestAxis;
    size_t insertIdx = state->bvhNodes.size();

    // did we get as leaf?
    if (splitInfo.bestAxis < 0) {
        // create a leaf
        node.nPrim = nTris;
        node.sndChildFirstPrim = state->nTris;
        for (unsigned i = 0; i < nTris; ++i) {
            // add references from tri buf to vert buf in appropriate position
            state->addTri(tris[splitTris[i]]);
        }
        state->bvhNodes.push_back(node);
        return insertIdx;
    }

    // Create an inner node
	// node.secondChild is not yet known, will be fixed up below
    node.nPrim = 0;
	state->bvhNodes.push_back(node);

	// Process child nodes recursively, using space further down in our arrays (after the space in leftPrims, rightPrims that we needed)
	size_t firstChild  = buildBVHNode(leftTris , splitInfo.nLeft      , depth+1, leftTris+nTris, rightTris+nTris);
	size_t secondChild = buildBVHNode(rightTris, nTris-splitInfo.nLeft, depth+1, leftTris+nTris, rightTris+nTris);

	// Make sure the children's order in the vector is as expected
	UNUSED(firstChild);
	assert(firstChild == insertIdx+1, "Left child doesn't come right after me");

	// Now that the second child's index in the vector is known, fix it and be done
	state->bvhNodes[insertIdx].sndChildFirstPrim = secondChild;
	return insertIdx;
}

void Object::split(unsigned *splitTris, unsigned nTris, unsigned depth, const impala::BBox &centroidBounds, Object::NodeSplitInformation *splitInfo)
{
    // termination criterion
    if(nTris <= maxPrimsPerLeaf || depth >= depthLimit) {
        //  don't do any split
        return;
    }

    // sort objects by centroid, along longest axis
    unsigned longestAxis = splitInfo->bestAxis = centroidBounds.longestAxis();
    std::sort(splitTris, splitTris+nTris,
              [this,longestAxis](unsigned tri1, unsigned tri2)
              { return std::get<1>(triData[tri1])[longestAxis] < std::get<1>(triData[tri2])[longestAxis]; });

    // put first half left, second half right
    splitInfo->nLeft = nTris/2;
    unsigned i = 0;
    for (; i < splitInfo->nLeft; ++i) {
        splitInfo->left[i] = splitTris[i];
    }
    for (; i < nTris; ++i) {
        splitInfo->right[i-splitInfo->nLeft] = splitTris[i];
    }
}




/** Scene */
Scene::Scene(impala::Scene *scene) : scene(scene)
{
    scene->verts = nullptr;
    scene->triVerts = nullptr;
    scene->bvhNodes = nullptr;
    scene->objs = nullptr;
    scene->nObjs = 0;
    scene->sceneMgr = this;
}

Scene::~Scene(void)
{
    free();
}

void Scene::free(void)
{
    thorin_free(scene->verts);
    scene->verts = nullptr;
    thorin_free(scene->triVerts);
    scene->triVerts = nullptr;
    thorin_free(scene->bvhNodes);
    scene->bvhNodes = nullptr;
    thorin_free(scene->objs);
    scene->objs = nullptr;
}

void Scene::clear()
{
    free();
    freeContainer(objects);
}

void Scene::build()
{
    free();

    // how many vertices and triangles are there overall?
    unsigned totalVerts = 0, totalTris = 0;
    for (auto& obj : objects) {
        totalVerts += obj.verts.size();
        totalTris += obj.tris.size();
    }
    // allocate appropiate build state
    BuildState state(scene, totalVerts, totalTris, objects.size());

    // now for each object, build the BVH tree
    for (auto& obj : objects) {
        // build BVH. This will copy the triangles and fix up the vertex IDs.
        auto rootIdx = obj.buildBVH(&state);
        // copy vertex data
        state.addVerts(obj.verts, obj.normals, obj.texCoords);
        // set object
        state.addObj(rootIdx);
    }

    // finally, copy the BVH nodes
    state.copyNodes();
}

/** CubeScene */
Cube::Cube(float size)
{
    float halfSize = size/2;
    verts.push_back(impala::Point(-halfSize, -halfSize, -halfSize));
    verts.push_back(impala::Point( halfSize, -halfSize, -halfSize));
    verts.push_back(impala::Point(-halfSize,  halfSize, -halfSize));
    verts.push_back(impala::Point(-halfSize, -halfSize,  halfSize));
    verts.push_back(impala::Point(-halfSize,  halfSize,  halfSize));
    verts.push_back(impala::Point( halfSize, -halfSize,  halfSize));
    verts.push_back(impala::Point( halfSize,  halfSize, -halfSize));
    verts.push_back(impala::Point( halfSize,  halfSize,  halfSize));

    tris.push_back(Tri(0, 1, 2));
    tris.push_back(Tri(6, 1, 2));

    tris.push_back(Tri(0, 1, 3));
    tris.push_back(Tri(5, 1, 3));

    tris.push_back(Tri(0, 2, 3));
    tris.push_back(Tri(4, 2, 3));

    tris.push_back(Tri(7, 6, 5));
    tris.push_back(Tri(1, 6, 5));

    tris.push_back(Tri(7, 6, 4));
    tris.push_back(Tri(2, 6, 4));

    tris.push_back(Tri(7, 5, 4));
    tris.push_back(Tri(3, 5, 4));
}

}

