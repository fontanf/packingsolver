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
 * `depth`. The `cut_orientation` records the orientation of the first cut of
 * the bin, which gives the orientation of the cuts at each depth of the cut
 * tree. `Any` is used only at the root when both orientations are explored;
 * after the first placement it is always Vertical or Horizontal.
 *
 * The orientation of the cuts of the children of the URS:
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
 *   2. Generates two insertions per block that fits, one per orientation of
 *      the first cut in the URS. When the URS has cut_orientation == Any (at
 *      the root), the two orientations are given by the cut_orientation of
 *      the bin; child URSs inherit it. Otherwise, the first cut either has
 *      the orientation of the depth of the URS, or the opposite one
 *      ('flip_first_cut'): the URS is then first wrapped in a node covering
 *      it, one depth deeper, whose children are cut with that orientation.
 *      Otherwise, a URS whose orientation is forced by its depth couldn't
 *      receive some packings, for example a strip of thin items stacked on
 *      top of each other with columns of items above them.
 *   3. Places the block at the BL corner of the selected URS and replaces the
 *      URS with two new (non-overlapping) URSs, using the orientation of the
 *      first cut of the insertion.
 *
 * Placing block (w_b, h_b) in a URS at BL=(xs,ys) with width W, height H,
 * at depth d (d + 1 if the insertion flips the first cut):
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
        ItemPos block_id = -1;
        ItemPos space_id = -1;
        CutOrientation cut_orientation = CutOrientation::Vertical;

        /**
         * True iff the first cut in the URS has the orientation opposite to
         * the one of its depth; see 'BranchingSchemeMaximalSpaces'.
         */
        bool flip_first_cut = false;

        double guide = 0.0;

        bool operator==(const Insertion& other) const
        {
            return this->block_id == other.block_id
                && this->space_id == other.space_id
                && this->cut_orientation == other.cut_orientation
                && this->flip_first_cut == other.flip_first_cut;
        }

        bool operator!=(const Insertion& other) const { return !(*this == other); }
    };

    struct Node
    {
        struct PlacedBlock {
            ItemPos block_id = -1;
            Coord bl_corner = {0, 0};
            CutOrientation cut_orientation = CutOrientation::Vertical;
            bool flip_first_cut = false;
        };

        NodeId id = -1;

        std::vector<PlacedBlock> placed_blocks;
        std::vector<EmptySpace> empty_spaces;
        std::vector<ItemPos> item_number_of_copies;

        /**
         * Consumption of each resource of the bin type by the blocks placed
         * so far: the sum of their 'Block::resource_consumption'. Empty iff
         * the bin type has no resources.
         */
        std::vector<double> resource_consumption;

        Area item_area = 0;
        Profit profit = 0;
        ItemPos number_of_items = 0;
        ItemPos number_of_blocks = 0;

        std::vector<ItemPos> valid_block_ids;

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

        /**
         * If true, the guide of an insertion is the fitness of the tree
         * searches with maximal spaces of the other problem types (Guesser et
         * al., 2026), without the contact factor:
         *
         *   guide = V * L^loss_factor_exponent * N^(-number_of_items_exponent)
         *
         * where V is the area of the items of the block weighted by their
         * profit density to the power 'profit_density_exponent', L is the
         * fraction of the area of the URS which can still be used by items
         * after the insertion (see 'insertion_loss_factor'), and N is the
         * number of items of the block.
         *
         * Otherwise, the guide is the fill rate of the block weighted by its
         * profit density to the power 'profit_density_exponent'. It has no
         * factor for the waste that the insertion leaves in the URS: with an
         * exponent greater than 1, the blocks of high density with a lot of
         * waste come before the full blocks of lower density.
         */
        bool use_loss_factor = true;

        /**
         * Exponent of the profit density of a block in the guide of its
         * insertions. It has no effect when the profits are proportional to
         * the areas. With 1 and without the loss factor, the guide is the
         * profit per unit of area of the block.
         */
        double profit_density_exponent = 2.0;

        /** Exponent of the loss factor in the guide. */
        double loss_factor_exponent = 2.6631;

        /** Exponent of the number of items of the block in the guide. */
        double number_of_items_exponent = 0.3968;
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
        // The insertions of the parent are not the ones of the child.
        node->cached_insertions.clear();
        node->next_child_pos = 0;
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
        // 'insertions_' is only filled if the insertions of the parent were
        // not cached yet.
        const std::vector<Insertion>& parent_insertions = insertions(parent);
        if (number_of_children < 0
                || number_of_children > (NodeId)parent_insertions.size())
            number_of_children = (NodeId)parent_insertions.size();
        std::vector<std::shared_ptr<Node>> nodes(number_of_children);
        for (NodeId i = 0; i < number_of_children; ++i) {
            nodes[i] = std::make_shared<Node>(*parent);
            // The insertions of the parent are not the ones of the child.
            nodes[i]->cached_insertions.clear();
            nodes[i]->next_child_pos = 0;
            apply_insertion(*nodes[i], parent_insertions[i]);
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

    /** Bottom-left corner of the area available for the items (after the trims). */
    Length x_start_;
    Length y_start_;

    /** Dimensions of the area available for the items (from bin type 0). */
    Rectangle bin_rect_;

    /**
     * For each length 'length' between 0 and the width (height) of the area
     * available for the items, the largest width (height) at most 'length'
     * of a row (column) of items separated by cuts.
     */
    std::vector<Length> max_reachable_widths_;
    std::vector<Length> max_reachable_heights_;

    mutable NodeId node_id_ = 0;
    mutable std::vector<Insertion> insertions_;

    /** True iff the bin type has a resource which is not penalized. */
    bool has_hard_resources_ = false;

    /**
     * For each item type, true iff its consumption of a resource of the bin
     * type depends on the number of its copies already in the bin.
     */
    std::vector<bool> copy_dependent_resource_consumptions_;

    /** Buffer of 'insertion_resource_consumption'. */
    mutable std::vector<double> insertion_resource_consumption_;

    /**
     * Consumption of each resource of the bin type by the items of 'block'
     * if it is inserted in 'node': 'Block::resource_consumption', shifted
     * for the item types whose consumption depends on the number of their
     * copies already in 'node'.
     *
     * The returned reference may be invalidated by the next call.
     */
    const std::vector<double>& insertion_resource_consumption(
            const Node& node,
            const Block& block) const;

    /** True iff the forced first cut for a URS at (depth, cut_orientation) is vertical. */
    bool cut_is_vertical(Depth depth, CutOrientation cut_orientation) const;

    /**
     * Return true iff the first cut in a URS at depth 'depth' may have the
     * orientation opposite to the one of its depth: the cut tree is then one
     * depth deeper, which must not exceed the number of stages.
     */
    bool first_cut_can_be_flipped(Depth depth) const;

    /**
     * Return true iff placing 'block' in 'node' doesn't exceed the capacity
     * of a resource which is not penalized.
     */
    bool block_resource_feasible(
            const Node& node,
            const Block& block) const;

    /**
     * Profit of placing 'block' in 'node': the profit of its items minus the
     * penalties of the penalized resources whose capacity it exceeds first.
     */
    Profit insertion_profit(
            const Node& node,
            const Block& block) const;

    /**
     * Largest area of a URS of dimensions 'width' x 'height' which can be
     * covered by the rows and columns of items of 'max_reachable_widths_'
     * and 'max_reachable_heights_'.
     */
    Area usable_area(
            Length width,
            Length height) const;

    /**
     * Fraction of the area of 'space' which can still be used by items after
     * the insertion of 'block' with a vertical first cut iff
     * 'vertical_first': the area of the items of the block plus the usable
     * areas of the two URSs of the insertion, divided by the area of the
     * space.
     */
    double insertion_loss_factor(
            const EmptySpace& space,
            const Block& block,
            bool vertical_first) const;

    /**
     * Guide of the insertion of 'block' in 'space' of 'node' with a vertical
     * first cut iff 'vertical_first'. Without 'Parameters::use_loss_factor',
     * its profit (see 'insertion_profit') per unit of area of the block. The
     * insertions of a node are generated, and the greedy chooses its
     * insertions, in decreasing order of guide.
     */
    double insertion_guide(
            const Node& node,
            const EmptySpace& space,
            const Block& block,
            bool vertical_first) const;

    /** Index of the URS with smallest area in node.empty_spaces. */
    ItemPos find_best_space(const Node& node) const;

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
