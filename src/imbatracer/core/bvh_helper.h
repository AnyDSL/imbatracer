#ifndef BVH_HELPER_H
#define BVH_HELPER_H

#include <cfloat>

#include "bbox.h"

namespace imba {

template <typename Node, int N>
struct MultiNode {
    Node nodes[N];
    BBox bbox;
    int count;

    MultiNode(const Node& node) {
        nodes[0] = node;
        bbox = node.bbox;
        count = 1;
    }

    bool full() const { return count == N; }
    bool is_leaf() const { return count == 1; }

    int next_node() const {
        assert(node_available());
        if (N == 2)
            return 0;
        else {
            float min_cost = FLT_MAX;
            int min_idx = 0;
            for (int i = 0; i < count; i++) {
                if (!nodes[i].tested && min_cost > nodes[i].cost) {
                    min_idx = i;
                    min_cost = nodes[i].cost;
                }
            }
            return min_idx;
        }
    }

    bool node_available() const {
        for (int i = 0; i < count; i++) {
            if (!nodes[i].tested) return true;
        }
        return false;
    }

    void split_node(int i, const Node& left, const Node& right) {
        assert(count < N);
        nodes[i] = left;
        nodes[count++] = right;
    }
};

} // namespace imba

#endif // BVH_HELPER_H

