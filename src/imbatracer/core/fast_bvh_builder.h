#ifndef IMBA_FAST_BVH_BUILDER
#define IMBA_FAST_BVH_BUILDER

#include <cstdint>
#include <algorithm>
#include <cassert>
#include <iostream>
#include <chrono>

#include "common.h"
#include "mem_pool.h"
#include "bvh_helper.h"
#include "float4.h"
#include "stack.h"
#include "mesh.h"
#include "bbox.h"
#include "tri.h"

namespace imba {

/// A fast binning BVH builder, which produces medium-quality BVHs.
/// Inspired from "On fast Construction of SAH-based Bounding Volume Hierarchies", I. Wald, 2007
/// http://www.sci.utah.edu/~wald/Publications/2007/ParallelBVHBuild/fastbuild.pdf
template <int N, typename CostFn>
class FastBvhBuilder {
public:
    template <typename NodeWriter, typename LeafWriter>
    void build(const Mesh& mesh, NodeWriter write_node, LeafWriter write_leaf, int leaf_threshold) {
        const int tri_count = mesh.triangle_count();
        BBox* bboxes = mem_pool_.alloc<BBox>(tri_count);
        float3* centers = mem_pool_.alloc<float3>(tri_count);

        for (int i = 0; i < tri_count; i++) {
            const Tri& tri = mesh.triangle(i);
            tri.compute_bbox(bboxes[i]);
            centers[i] = (1.0f / 3.0f) * (tri.v0 + tri.v1 + tri.v2);
        }

        build(bboxes, centers, tri_count, write_node, write_leaf, leaf_threshold);
    }

    template <typename NodeWriter, typename LeafWriter>
    void build(const BBox* bboxes, const float3* centers, int obj_count, NodeWriter write_node, LeafWriter write_leaf, int leaf_threshold) {
        assert(leaf_threshold >= 1);

#ifdef STATISTICS
        auto time_start = std::chrono::high_resolution_clock::now();
#endif

        BBox global_bb = BBox::empty();
        for (int i = 0; i < obj_count; i++) global_bb.extend(bboxes[i]);
        int* refs = mem_pool_.alloc<int>(obj_count);
        for (int i = 0; i < obj_count; i++) refs[i] = i;

        Stack<Node> stack;
        stack.push(0, obj_count, global_bb);

        while (!stack.is_empty()) {
            MultiNode<Node, N> multi_node(stack.pop());

            // Iterate over the available split candidates in the multi-node
            while (!multi_node.full() && multi_node.node_available()) {
                const int node_id = multi_node.next_node();
                Node node = multi_node.nodes[node_id];

                multi_node.nodes[node_id].tested = true;

                const int begin = node.begin;
                const int end = node.end;
                const BBox& parent_bb = node.bbox;
                assert(end - begin != 0);

                // Test longest axes first
                float3 extents = parent_bb.max - parent_bb.min;
                int axes[3] = {0, 1, 2};
                if (extents[axes[0]] < extents[axes[1]]) std::swap(axes[0], axes[1]);
                if (extents[axes[1]] < extents[axes[2]]) std::swap(axes[1], axes[2]);
                if (extents[axes[0]] < extents[axes[1]]) std::swap(axes[0], axes[1]);
                for (int j = 0; j < 3; j++) {
                    const int axis = axes[j];

                    // Compute the min/max center position
                    float center_min = parent_bb.max[axis];
                    float center_max = parent_bb.min[axis];
                    for (int i = begin; i < end; i++) {
                        const float c = centers[refs[i]][axis];
                        center_min = std::min(center_min, c);
                        center_max = std::max(center_max, c);
                    }

                    // Put the triangles into the bins
                    Bin bins[num_bins];
                    bin_triangles(axis, bins, refs, bboxes, centers, center_min, center_max, begin, end);

                    // Find the best split position
                    const float parent_area = parent_bb.half_area();
                    int best_split = find_best_split(bins, CostFn::leaf_cost(end - begin, parent_area) - CostFn::traversal_cost(parent_area));
                    if (best_split >= 0 && best_split < num_bins - 1) {
                        // The node was succesfully split
                        const int begin_right = apply_split(axis, best_split, refs, centers, center_min, center_max, begin, end);
                        const int end_right = end;
                        const int begin_left = begin;
                        const int end_left = begin_right;

                        BBox left_bb  = BBox::empty();
                        BBox right_bb = BBox::empty();
                        if (num_bins < end - begin) {
                            // Compute the bounding box using the bins
                            for (int i = 0; i < best_split; i++) left_bb.extend(bins[i].bbox);
                            for (int i = best_split; i < num_bins; i++) right_bb.extend(bins[i].bbox);
                        } else {
                            // Compute the bounding box using the objects
                            for (int i = begin_left; i < end_left; i++) left_bb.extend(bboxes[refs[i]]);
                            for (int i = begin_right; i < end_right; i++) right_bb.extend(bboxes[refs[i]]);
                        }

                        // Exit once the first candidate is found
                        multi_node.split_node(node_id,
                                              Node(begin_left, end_left, left_bb),
                                              Node(begin_right, end_right, right_bb));
                        break;
                    }
                }
            }

            assert(multi_node.count > 0);
            // Process the smallest nodes first
            multi_node.sort_nodes();

            // The multi-node is ready to be stored
            if (multi_node.is_leaf()) {
                // Store a leaf if it could not be split
                const Node& node = multi_node.nodes[0];
                assert(node.tested);
                make_leaf(node, refs, write_leaf);
            } else {
                // Store a multi-node
                make_node(multi_node, write_node);
                assert(N > 2 || multi_node.count == 2);

                if (stack.size() + multi_node.count < stack.capacity()) {
                    for (int i = multi_node.count - 1; i >= 0; i--) {
                        stack.push(multi_node.nodes[i]);
                    }
                } else {
                    // Insufficient space on the stack, we have to stop recursion here
                    for (int i = 0; i < multi_node.count; i++) {
                        make_leaf(multi_node.nodes[i], refs, write_leaf);
                    }
                }
            }
        }

#ifdef STATISTICS
        auto time_end = std::chrono::high_resolution_clock::now();
        total_time_ += std::chrono::duration_cast<std::chrono::milliseconds>(time_end - time_start).count();
#endif

        mem_pool_.cleanup();
    }

#ifdef STATISTICS
    void print_stats() const {
        std::cout << "BVH built in " << total_time_ << "ms ("
                  << total_nodes_ << " nodes, "
                  << total_leaves_ << " leaves)"
                  << std::endl;
    }
#endif

private:
    static constexpr int num_bins = 32;

    struct Bin {
        int count;
        BBox bbox;
    };

    struct Node {
        BBox bbox;
        int begin, end;
        float cost;
        bool tested;

        Node() {}
        Node(int begin, int end, const BBox& bbox)
            : begin(begin), end(end), bbox(bbox)
            , cost(CostFn::leaf_cost(end - begin, bbox.half_area()))
            , tested(false)
        {}
        int size() const { return end - begin; }
    };

    template <typename NodeWriter>
    void make_node(const MultiNode<Node, N>& multi_node, NodeWriter write_node) {
        write_node(multi_node.bbox, multi_node.count, [&] (int i) {
            return multi_node.nodes[i].bbox;
        });
#ifdef STATISTICS
        total_nodes_++;
#endif
    }

    template <typename LeafWriter>
    void make_leaf(const Node& node, const int* refs, LeafWriter write_leaf) {
        write_leaf(node.bbox, node.end - node.begin, [&] (int i) {
            return refs[node.begin + i];
        });
#ifdef STATISTICS
        total_leaves_++;
#endif
    }

    int compute_bin_id(float c, float min, float inv) {
        return std::min(num_bins - 1, std::max(0, (int)(num_bins * (c - min) * inv)));
    }

    void bin_triangles(int axis, Bin* bins, const int* refs, const BBox* bboxes, const float3* centers, float min, float max, int begin, int end) {
        for (int i = 0; i < num_bins; i++) {
            bins[i].count = 0;
            bins[i].bbox = BBox::empty();
        }

        const float inv = 1.0f / (max - min);
        for (int i = begin; i < end; i++) {
            const int ref = refs[i];
            const int bin_id = compute_bin_id(centers[ref][axis], min, inv);
            bins[bin_id].count++;
            bins[bin_id].bbox.extend(bboxes[ref]);
        }
    }

    int find_best_split(const Bin* bins, float max_cost) {
        float left_cost[num_bins];
        int left_count = 0;
        BBox left_bb = BBox::empty();

        // Sweep from the left
        for (int i = 0; i < num_bins - 1; i++) {
            left_bb.extend(bins[i].bbox);
            left_count += bins[i].count;
            left_cost[i] = CostFn::leaf_cost(left_count, left_bb.half_area());
        }

        int right_count = 0;
        BBox right_bb = BBox::empty();
        float best_cost = max_cost;
        int best_split = -1;

        // Sweep from the right
        for (int i = num_bins - 1; i > 0; i--) {
            right_bb.extend(bins[i].bbox);
            right_count += bins[i].count;
            const float cost = CostFn::leaf_cost(right_count, right_bb.half_area()) + left_cost[i - 1];
            if (cost < best_cost) {
                best_split = i;
                best_cost = cost;
            }
        }

        return best_split;
    }

    int apply_split(int axis, int split, int* refs, const float3* centers, float center_min, float center_max, int begin, int end) {
        const float inv = 1.0f / (center_max - center_min);
        return std::partition(refs + begin, refs + end, [&] (const int ref) {
            return compute_bin_id(centers[ref][axis], center_min, inv) < split;
        }) - refs;
    }

#ifdef STATISTICS
    long total_time_ = 0;
    int total_nodes_ = 0;
    int total_leaves_ = 0;
#endif

    MemoryPool<> mem_pool_;
};

} // namespace imba

#endif // IMBA_FAST_BVH_BUILDER_H

