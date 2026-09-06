#pragma once

#include "rectangleguillotine/block.hpp"

#include "packingsolver/rectangleguillotine/optimize.hpp"

#include "optimizationtools/utils/utils.hpp"

#include <sstream>

namespace packingsolver
{
namespace rectangleguillotine
{

/**
 * An unused rectangular space (URS).
 *
 * Each URS corresponds to a leaf node in the guillotine cut tree at the given
 * `depth`. The `cut_orientation` records the first-cut orientation for this
 * URS and all its descendants. `Any` is used only at the root when both
 * orientations are explored; after the first placement it is always Vertical
 * or Horizontal.
 *
 * The forced cut direction when a block is placed:
 *   cut_is_vertical(depth, cut_orientation).
 *
 * Because the guillotine split of a URS always produces exactly two disjoint
 * sub-rectangles, no two URSs in the same node ever overlap.
 */
struct EmptySpace
{
    Coord bl_corner = {0, 0};
    Length width = 0;
    Length height = 0;
    Depth depth = 0;
    CutOrientation cut_orientation = CutOrientation::Any;

    inline Length xs() const { return bl_corner.x; }
    inline Length ys() const { return bl_corner.y; }
    inline Length xe() const { return bl_corner.x + width; }
    inline Length ye() const { return bl_corner.y + height; }
    inline Area area() const { return (Area)width * height; }
};

inline std::ostream& operator<<(std::ostream& os, const EmptySpace& space)
{
    os << "bl (" << space.bl_corner.x << ", " << space.bl_corner.y << ")"
       << " size (" << space.width << ", " << space.height << ")"
       << " depth " << space.depth;
    return os;
}

/**
 * Branching scheme for the 2D guillotine knapsack (unlimited stages) using
 * a maximal empty-space (URS) representation.
 *
 * At each node the algorithm:
 *   1. Selects the URS with minimum area (most constrained).
 *   2. Generates one insertion per (block, cut_orientation) pair that fits.
 *      When the URS has cut_orientation == Any, both Vertical and Horizontal
 *      are generated per block, giving two children per block at the root.
 *      Once an orientation is chosen, child URSs inherit it and subsequent
 *      insertions generate one child per block.
 *   3. Places the block at the BL corner of the selected URS and replaces the
 *      URS with two new (non-overlapping) URSs using the guillotine cut
 *      direction forced by the URS depth and cut_orientation.
 *
 * Placing block (w_b, h_b) in a URS at BL=(xs,ys) with width W, height H:
 *
 *   Vertical-first (cut_is_vertical(d, Vertical) == true):
 *     right URS: [xs+w_b+ct, xs+W] × [ys, ys+H]         depth d+1
 *     above URS: [xs, xs+w_b]       × [ys+h_b+ct, ys+H]  depth d+2
 *
 *   Horizontal-first (cut_is_vertical(d, Horizontal) == false):
 *     above URS: [xs, xs+W]    × [ys+h_b+ct, ys+H]      depth d+1
 *     right URS: [xs+w_b+ct, xs+W] × [ys, ys+h_b]       depth d+2
 */
class BranchingSchemeMaximalSpaces
{

public:

    /*
     * Sub-structures.
     */

    struct Insertion
    {
        /** Bin the space belongs to. */
        BinPos bin_pos = -1;

        ItemPos block_id = -1;
        ItemPos space_id = -1;
        CutOrientation cut_orientation = CutOrientation::Vertical;
        double guide = 0.0;

        bool operator==(const Insertion& other) const
        {
            return this->bin_pos == other.bin_pos
                && this->block_id == other.block_id
                && this->space_id == other.space_id
                && this->cut_orientation == other.cut_orientation;
        }

        bool operator!=(const Insertion& other) const { return !(*this == other); }
    };

    struct Node
    {
        struct PlacedBlock {
            /** Bin the block was placed in. */
            BinPos bin_pos = -1;

            ItemPos block_id = -1;
            Coord bl_corner = {0, 0};
            CutOrientation cut_orientation = CutOrientation::Vertical;
        };

        NodeId id = -1;

        /**
         * All blocks placed so far, in true global placement order (across
         * all bins): to_solution()'s cut-tree replay depends on this order
         * to correctly mirror apply_insertion()'s cross-bin item-copy
         * pruning step by step, so this stays a single flat, order-preserving
         * list (each entry tagged with its own bin_pos) rather than one list
         * per bin.
         */
        std::vector<PlacedBlock> placed_blocks;

        /** Maximal empty spaces (URSs) remaining after all placements up to this node, indexed by bin position. */
        std::vector<std::vector<EmptySpace>> empty_spaces;

        /**
         * For each item type, how many copies have been placed so far
         * (shared across all bins: item copies are a global resource).
         */
        std::vector<ItemPos> item_number_of_copies;

        Area item_area = 0;
        Profit profit = 0;
        ItemPos number_of_items = 0;
        ItemPos number_of_blocks = 0;

        /**
         * IDs of blocks that can still be placed in a given bin, indexed by
         * bin position: those whose item quantities are still available
         * (shared item-copy constraint).
         */
        std::vector<std::vector<ItemPos>> valid_block_ids;

        Profit greedy_value = 0;

        std::vector<Insertion> cached_insertions;
        NodeId next_child_pos = 0;
    };

    /**
     * `first_cut_orientation`:
     *   - Any:        generate two children per block at the root (default).
     *   - Vertical:   only explore Vertical first cuts.
     *   - Horizontal: only explore Horizontal first cuts.
     */
    struct Parameters
    {
        CutOrientation first_cut_orientation = CutOrientation::Any;
    };

    BranchingSchemeMaximalSpaces(
            const Instance& instance,
            const std::vector<std::vector<Block>>& blocks,
            const Parameters& parameters);

    const Instance& instance() const { return instance_; }
    const Parameters& parameters() const { return parameters_; }

    /*
     * Branching scheme methods (treesearchsolver interface).
     */

    const std::vector<Insertion>& insertions(
            const std::shared_ptr<Node>& parent) const;

    Insertion best_insertion(Node& node) const;

    void apply_insertion(
            Node& node,
            const Insertion& insertion) const;

    std::shared_ptr<Node> child(
            const std::shared_ptr<Node>& parent,
            const Insertion& insertion) const
    {
        auto node = std::make_shared<Node>(*parent);
        apply_insertion(*node, insertion);
        node->greedy_value = compute_guide_greedy(*node);
        return node;
    }

    std::shared_ptr<Node> next_child(const std::shared_ptr<Node>& parent) const;

    inline bool infertile(const std::shared_ptr<Node>& node) const
    {
        return node->next_child_pos > 0
            && node->next_child_pos >= (NodeId)node->cached_insertions.size();
    }

    std::vector<std::shared_ptr<Node>> children(
            const std::shared_ptr<Node>& parent,
            NodeId number_of_children = -1) const
    {
        insertions(parent);
        if (number_of_children < 0
                || number_of_children > (NodeId)insertions_.size())
            number_of_children = (NodeId)insertions_.size();
        std::vector<std::shared_ptr<Node>> nodes(number_of_children);
        for (NodeId i = 0; i < number_of_children; ++i) {
            nodes[i] = std::make_shared<Node>(*parent);
            apply_insertion(*nodes[i], insertions_[i]);
            nodes[i]->greedy_value = compute_guide_greedy(*nodes[i]);
        }
        return nodes;
    }

    const std::shared_ptr<Node> root() const;

    inline bool leaf(const std::shared_ptr<Node>& node) const
    {
        return node->number_of_items == instance().number_of_items();
    }

    bool better(
            const std::shared_ptr<Node>& node_1,
            const std::shared_ptr<Node>& node_2) const;

    bool bound(
            const std::shared_ptr<Node>& node_1,
            const std::shared_ptr<Node>& node_2) const;

    bool equals(
            const std::shared_ptr<Node>&,
            const std::shared_ptr<Node>&) const
    {
        return false;
    }

    /*
     * Dominances.
     */

    inline bool comparable(const std::shared_ptr<Node>&) const { return true; }

    struct NodeHasher
    {
        std::hash<ItemPos> hasher;
        const BranchingSchemeMaximalSpaces& branching_scheme;

        NodeHasher(const BranchingSchemeMaximalSpaces& branching_scheme):
            branching_scheme(branching_scheme) {}

        inline bool operator()(
                const std::shared_ptr<Node>& node_1,
                const std::shared_ptr<Node>& node_2) const
        {
            return node_1->item_number_of_copies == node_2->item_number_of_copies;
        }

        inline std::size_t operator()(
                const std::shared_ptr<Node>& node) const
        {
            std::size_t hash = 0;
            for (ItemPos copies: node->item_number_of_copies)
                optimizationtools::hash_combine(hash, hasher(copies));
            return hash;
        }
    };

    inline NodeHasher node_hasher() const { return NodeHasher(*this); }

    inline bool dominates(
            const std::shared_ptr<Node>& node_1,
            const std::shared_ptr<Node>& node_2) const
    {
        return node_1->greedy_value >= node_2->greedy_value;
    }

    /*
     * Outputs.
     */

    std::string display(const std::shared_ptr<Node>& node) const
    {
        std::stringstream ss;
        ss << node->profit;
        return ss.str();
    }

    Solution to_solution(const std::shared_ptr<Node>& node) const;

    inline bool operator()(
            const std::shared_ptr<Node>& node_1,
            const std::shared_ptr<Node>& node_2) const;

private:

    const Instance& instance_;
    const std::vector<std::vector<Block>>& blocks_;
    Parameters parameters_;
    Length cut_thickness_;

    /** Effective bin dimensions, indexed by bin position. */
    std::vector<Rectangle> bin_rects_;

    mutable NodeId node_id_ = 0;
    mutable std::vector<Insertion> insertions_;

    /** True iff the forced first cut for a URS at (depth, cut_orientation) is vertical. */
    bool cut_is_vertical(Depth depth, CutOrientation cut_orientation) const;

    struct BestSpaceResult {
        BinPos bin_pos = -1;
        ItemPos space_idx = -1;
    };

    /** Bin position and index of the URS with smallest area across all bins. */
    BestSpaceResult find_best_space(const Node& node) const;

    /**
     * Remove from node.valid_block_ids[bin_pos] every block that no longer
     * has sufficient item copies available after this node's placements
     * (item-copy usage is shared/global across bins, so a placement in any
     * bin can invalidate blocks in every bin's own list, not just the one it
     * was placed in).
     */
    void prune_valid_block_ids(
            Node& node,
            BinPos bin_pos) const;

    /**
     * Remove empty spaces (URSs) of bin 'bin_pos' that fit no currently
     * valid block: otherwise find_best_space could pick one of these and
     * find no insertion there, wrongly making the node infertile even
     * though other, larger spaces remain usable.
     */
    void remove_unusable_spaces(
            Node& node,
            BinPos bin_pos) const;

    Profit compute_guide_greedy(const Node& node) const;
};

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Inlined methods ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

inline bool BranchingSchemeMaximalSpaces::operator()(
        const std::shared_ptr<Node>& node_1,
        const std::shared_ptr<Node>& node_2) const
{
    if (node_1->number_of_blocks == 0)
        return true;
    if (node_2->number_of_blocks == 0)
        return false;
    if (node_1->greedy_value != node_2->greedy_value)
        return node_1->greedy_value > node_2->greedy_value;
    if (node_1->profit != node_2->profit)
        return node_1->profit > node_2->profit;
    return node_1->id > node_2->id;
}

} // namespace rectangleguillotine
} // namespace packingsolver

namespace packingsolver
{
namespace rectangleguillotine
{

struct TreeSearchMaximalSpacesOutput: Output
{
    TreeSearchMaximalSpacesOutput(const Instance& instance):
        Output(instance) { }
};

struct TreeSearchMaximalSpacesParameters: packingsolver::Parameters<Instance, Solution, Output>
{
    OptimizationMode optimization_mode = OptimizationMode::Anytime;

    NodeId not_anytime_tree_search_queue_size = 1;
};

const TreeSearchMaximalSpacesOutput tree_search_maximal_spaces(
        const Instance& instance,
        const TreeSearchMaximalSpacesParameters& parameters = {});

}
}
