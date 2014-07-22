#include <thorin_ext_runtime.h>
#include <iostream>
#include <core/util.h>
#include <core/assert.h>
#include <algorithm>
#include "scene.h"

namespace rt {

const unsigned depthLimit = 16; // this is also set in impala!
const unsigned maxPrimsPerLeaf = 4;

template<typename T>
static T* thorin_new(unsigned n)
{
    return (T*)thorin_malloc(n*sizeof(T));
}

/** Object */
size_t Object::buildBVH(unsigned *triBuf, unsigned triBufOff, std::vector<impala::BVHNode> *nodeBuf)
{
    auto nodeBufOrigSize = nodeBuf->size();
    this->nodeBuf = nodeBuf;
    this->triBuf = triBuf;
    this->triBufOff = triBufOff;
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
    assert(this->triBufOff == triBufOff+tris.size(), "Wrong number of triangles added?");

    // free space
    freeContainer(triData);
    delete[] leftPrims;
    delete[] rightPrims;
    delete[] primsToSplit;

    // done!
    std::cout << "BVH construction finished: " << (nodeBuf->size()-nodeBufOrigSize) << " nodes, "
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
    size_t insertIdx = nodeBuf->size();

    // did we get as leaf?
    if (splitInfo.bestAxis < 0) {
        // create a leaf
        node.nPrim = nTris;
        node.sndChildFirstPrim = triBufOff;
        for (unsigned i = 0; i < nTris; ++i) {
            triBuf[triBufOff*3 + 0] = tris[splitTris[i]].p1;
            triBuf[triBufOff*3 + 1] = tris[splitTris[i]].p2;
            triBuf[triBufOff*3 + 2] = tris[splitTris[i]].p3;
            ++triBufOff;
        }
        nodeBuf->push_back(node);
        return insertIdx;
    }

    // Create an inner node
	// node.secondChild is not yet known, will be fixed up below
    node.nPrim = 0;
	nodeBuf->push_back(node);

	// Process child nodes recursively, using space further down in our arrays (after the space in leftPrims, rightPrims that we needed)
	size_t firstChild  = buildBVHNode(leftTris , splitInfo.nLeft      , depth+1, leftTris+nTris, rightTris+nTris);
	size_t secondChild = buildBVHNode(rightTris, nTris-splitInfo.nLeft, depth+1, leftTris+nTris, rightTris+nTris);

	// Make sure the children's order in the vector is as expected
	UNUSED(firstChild);
	assert(firstChild == insertIdx+1, "Left child doesn't come right after me");

	// Now that the second child's index in the vector is known, fix it and be done
	(*nodeBuf)[insertIdx].sndChildFirstPrim = secondChild;
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
    // allocate their buffers
    scene->verts = thorin_new<impala::Point>(totalVerts);
    scene->triVerts = thorin_new<unsigned>(totalTris*3);
    scene->objs = thorin_new<impala::Object>(objects.size());

    // now for each object, build the BVH tree and copy stuff
    unsigned curVert = 0, curTri = 0, curObj = 0;
    std::vector<impala::BVHNode> nodeBuf;
    nodeBuf.reserve(totalTris/2); // just a wild guess
    for (auto& obj : objects) {
        // copy vertex data
        std::copy(obj.verts.begin(), obj.verts.end(), scene->verts+curVert);
        curVert += obj.verts.size();
        // build BVH. This will copy the triangles.
        auto rootIdx = obj.buildBVH(scene->triVerts, curTri, &nodeBuf);
        curTri += obj.tris.size();
        // set object stuff
        scene->objs[curObj].bvhRoot = rootIdx;
        curObj += 1;
    }
    scene->nObjs = curObj;

    // finally, copy the BVH nodes
    scene->bvhNodes = thorin_new<impala::BVHNode>(nodeBuf.size());
    std::copy(nodeBuf.begin(), nodeBuf.end(), scene->bvhNodes);
}

/** CubeScene */
Cube::Cube()
{
    verts.push_back(impala::Point(-1, -1, -1));
    verts.push_back(impala::Point( 1, -1, -1));
    verts.push_back(impala::Point(-1,  1, -1));
    verts.push_back(impala::Point(-1, -1,  1));
    verts.push_back(impala::Point(-1,  1,  1));
    verts.push_back(impala::Point( 1, -1,  1));
    verts.push_back(impala::Point( 1,  1, -1));
    verts.push_back(impala::Point( 1,  1,  1));

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

