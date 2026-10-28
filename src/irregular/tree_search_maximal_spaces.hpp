#pragma once

#include "packingsolver/irregular/optimize.hpp"

#include "irregular/trapezoid_slider.hpp"
#include "irregular/empty_spaces.hpp"
#include "irregular/shape_simplification.hpp"

#include "optimizationtools/utils/utils.hpp"
#include "optimizationtools/utils/timer.hpp"

#include <sstream>

namespace packingsolver
{
namespace irregular
{

/**
 * Branching scheme for the irregular knapsack using a maximal empty-space
 * representation, with the actual geometry of the items.
 *
 * It lies between 'BranchingScheme' (tree search) and
 * 'BranchingSchemePeriodicPacking':
 * - like the latter, a node stores the maximal empty spaces of the bin, an
 *   insertion packs a block of items (a single item or a periodic block) in
 *   the empty space closest to the bottom-left corner of the bin, and a node
 *   is evaluated by completing it greedily;
 * - like the former, the items are described by their trapezoids, and a
 *   block is inserted by resting one of its supported parts on a supporting
 *   part of the bin or of a block already packed and by sliding it to the
 *   right until it doesn't overlap the items already packed.
 *
 * The empty spaces are only used to select where to insert a block: they
 * are computed from rectangles covering the items, while the overlaps are
 * checked on the trapezoids. A block may thus stick out of the empty space
 * it is inserted in, for example to fit in a concavity of an item already
 * packed.
 *
 * Everything is computed in the frame of the direction of the branching
 * scheme, in which the bottom-left corner is the anchor corner.
 */
class BranchingSchemeMaximalSpaces
{

public:

    /*
     * Sub-structures
     */

    /** Item of a block. */
    struct BlockItem
    {
        /** Trapezoid set of the item. */
        TrapezoidSetId trapezoid_set_id = -1;

        /** x-coordinate of the item, relative to the block. */
        LengthDbl x = 0.0;

        /** y-coordinate of the item, relative to the block. */
        LengthDbl y = 0.0;
    };

    /** Items packed together, in the frame of the direction. */
    struct Block
    {
        /** Items of the block. */
        std::vector<BlockItem> items;

        /**
         * Trapezoids of the items of the block, relative to the block.
         *
         * Its attributes 'item_type_id', 'angle' and 'mirror' are
         * meaningless. Its supporting and supported parts refer to the ones
         * of the block.
         */
        TrapezoidSet trapezoid_set;

        /**
         * Rectangles covering the inflated shapes of the items of the block,
         * relative to the block.
         */
        std::vector<AxisAlignedBoundingBox> rectangles;

        /**
         * Largest rectangle of 'rectangles', used to discard the empty
         * spaces too small for the block.
         */
        AxisAlignedBoundingBox core_rectangle;

        /**
         * Supporting parts of the block which are not covered by another
         * item of the block, relative to the block.
         */
        std::vector<Support> supporting_parts;

        /**
         * Supported parts with which the block rests on a supporting part,
         * relative to the block.
         */
        std::vector<Support> supported_parts;

        /**
         * Number of copies of each item type used in this block, as a sorted
         * sparse list of (item_type_id, count) pairs.
         */
        std::vector<std::pair<ItemTypeId, ItemPos>> item_copies;

        /** Number of items. */
        ItemPos number_of_items = 0;

        /** Area of the items. */
        AreaDbl item_area = 0.0;

        /** Profit of the items. */
        Profit profit = 0;
    };

    struct Insertion
    {
        /** Inserted block. */
        ItemPos block_id = -1;

        /** x-coordinate of the block. */
        LengthDbl x = 0.0;

        /** y-coordinate of the block. */
        LengthDbl y = 0.0;

        /** Score of the insertion, the larger the better. */
        double guide = 0.0;
    };

    /**
     * Node structure of the branching scheme.
     */
    struct Node
    {
        /** Block packed in a node. */
        struct PlacedBlock
        {
            /** Block. */
            ItemPos block_id = -1;

            /** x-coordinate of the block. */
            LengthDbl x = 0.0;

            /** y-coordinate of the block. */
            LengthDbl y = 0.0;
        };

        /** Id of the node. */
        NodeId id = -1;

        /** Packed blocks. */
        std::vector<PlacedBlock> placed_blocks;

        /**
         * Trapezoids that a new block must not overlap: the obstacles of the
         * bin and the inflated trapezoids of the packed items.
         */
        std::vector<GeneralizedTrapezoid> trapezoids;

        /** Maximal empty spaces. */
        std::vector<EmptySpace> empty_spaces;

        /** For each item type, number of copies in the node. */
        std::vector<ItemPos> item_number_of_copies;

        /** Blocks which don't use more copies than available. */
        std::vector<ItemPos> valid_block_ids;

        /** Area of the packed items. */
        AreaDbl item_area = 0.0;

        /** Number of packed items. */
        ItemPos number_of_items = 0;

        /** Number of packed blocks. */
        ItemPos number_of_blocks = 0;

        /** Profit of the packed items. */
        Profit profit = 0;

        /** Profit of the greedy completion of the node. */
        Profit greedy_value = 0;

        /** Insertions of the node, computed when its first child is. */
        std::vector<Insertion> cached_insertions;

        /** Position of the next child in 'cached_insertions'. */
        NodeId next_child_pos = 0;
    };

    struct Parameters
    {
        /** Direction. */
        Direction direction = Direction::LeftToRightThenBottomToTop;

        /** Maximum approximation ratio of the shape simplification. */
        double maximum_approximation_ratio = 0.05;

        /**
         * Maximum waste of the rectangles covering an item, used to compute
         * the empty spaces, as a ratio of the area of the item.
         */
        double rectangle_approximation_ratio = 0.1;

        /**
         * Maximum number of rectangles covering an item: the maximum waste
         * is increased until it is reached.
         */
        ElementPos maximum_number_of_rectangles = 8;

        /** Use the periodic blocks, and not only the single items. */
        bool use_periodic_blocks = true;

        /**
         * Exponent of the density of an insertion in its score:
         *
         * score = profit * density^beta * number_of_items^(-gamma)
         *
         * where the density is the area of the items of the block divided by
         * the area of the rectangle from the corner of the empty space to the
         * top-right corner of the block.
         */
        double beta = 2.0;

        /** Exponent of the number of items of an insertion in its score. */
        double gamma = 0.4;

        /**
         * Timer, checked during the greedy completions, which may be long.
         */
        optimizationtools::Timer timer;
    };

    /** Constructor. */
    BranchingSchemeMaximalSpaces(
            const Instance& instance,
            const Parameters& parameters);

    /** Get instance. */
    inline const Instance& instance() const { return instance_; }

    /** Get parameters. */
    inline const Parameters& parameters() const { return parameters_; }

    /** Get the blocks. */
    inline const std::vector<Block>& blocks() const { return blocks_; }

    /*
     * Branching scheme methods
     */

    const std::shared_ptr<Node> root() const;

    std::shared_ptr<Node> next_child(const std::shared_ptr<Node>& parent) const;

    inline bool infertile(const std::shared_ptr<Node>& node) const
    {
        return node->next_child_pos > 0
            && node->next_child_pos >= (NodeId)node->cached_insertions.size();
    }

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

    inline bool operator()(
            const std::shared_ptr<Node>& node_1,
            const std::shared_ptr<Node>& node_2) const;

    /*
     * Dominances
     */

    inline bool comparable(const std::shared_ptr<Node>&) const { return true; }

    struct NodeHasher
    {
        std::hash<ItemPos> hasher;

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

    inline NodeHasher node_hasher() const { return NodeHasher(); }

    /**
     * Among the nodes with the same items, only keep the one with the best
     * greedy completion.
     */
    inline bool dominates(
            const std::shared_ptr<Node>& node_1,
            const std::shared_ptr<Node>& node_2) const
    {
        return node_1->greedy_value >= node_2->greedy_value;
    }

    /*
     * Outputs
     */

    std::string display(const std::shared_ptr<Node>& node) const
    {
        std::stringstream ss;
        ss << node->greedy_value;
        return ss.str();
    }

    /** Complete the node greedily and convert it into a solution. */
    Solution to_solution(const std::shared_ptr<Node>& node) const;

private:

    /*
     * Private attributes
     */

    /** Instance. */
    const Instance& instance_;

    /** Parameters. */
    Parameters parameters_;

    /** Simplified instance. */
    SimplifiedInstance simplified_instance_;

    /** Trapezoid bin type of the (single) bin type, in the direction frame. */
    TrapezoidBinType bin_type_;

    /**
     * Trapezoid sets of the items: the ones of 'bin_type_', then the ones of
     * the orientations used by the periodic blocks only.
     */
    std::vector<TrapezoidSet> trapezoid_sets_;

    /** Supporting parts of the trapezoid sets. */
    std::vector<Support> supporting_parts_;

    /** Supported parts of the trapezoid sets. */
    std::vector<Support> supported_parts_;

    /** Rectangles covering the inflated shape of each trapezoid set. */
    std::vector<std::vector<AxisAlignedBoundingBox>> trapezoid_sets_rectangles_;

    /** Blocks. */
    std::vector<Block> blocks_;

    /**
     * Rectangles covering the obstacles of the bin, cut out of the empty
     * spaces of the root node.
     */
    std::vector<AxisAlignedBoundingBox> obstacles_rectangles_;

    /** Usable part of the bin, in the direction frame. */
    AxisAlignedBoundingBox bin_aabb_;

    mutable NodeId node_id_ = 0;

    /*
     * Private methods
     */

    /**
     * Get the trapezoid set of an item type in an orientation, building it
     * if needed.
     */
    TrapezoidSetId trapezoid_set_id(
            ItemTypeId item_type_id,
            Angle angle,
            bool mirror);

    /** Compute the rectangles covering the inflated shape of a trapezoid set. */
    std::vector<AxisAlignedBoundingBox> compute_rectangles(
            const TrapezoidSet& trapezoid_set) const;

    /** Build a block from its items. */
    Block build_block(const std::vector<BlockItem>& items) const;

    /** Build the blocks. */
    void compute_blocks();

    /** Select the empty space in which to insert the next block. */
    ItemPos select_empty_space(const Node& node) const;

    /** Return 'true' iff the block may be inserted in the empty space. */
    bool may_fit(
            const Block& block,
            const EmptySpace& space) const;

    /**
     * Compute the insertions of the node in its selected empty space; the
     * empty spaces in which no block can be inserted are removed from the
     * node.
     *
     * If 'only_best' is 'true', only the insertion with the best score is
     * returned.
     */
    std::vector<Insertion> insertions(
            Node& node,
            bool only_best) const;

    /** Apply an insertion to a node. */
    void apply_insertion(
            Node& node,
            const Insertion& insertion) const;

    /** Remove the empty spaces too small for all the valid blocks. */
    void remove_unusable_spaces(Node& node) const;

    /** Complete a node greedily. */
    void complete_greedily(Node& node) const;

};

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

}
}

namespace packingsolver
{
namespace irregular
{

struct TreeSearchMaximalSpacesOutput: Output
{
    TreeSearchMaximalSpacesOutput(const Instance& instance):
        Output(instance) { }
};

struct TreeSearchMaximalSpacesParameters: packingsolver::Parameters<Instance, Solution, Output>
{
    OptimizationMode optimization_mode = OptimizationMode::Anytime;

    /** Maximum approximation ratio of the shape simplification. */
    double maximum_approximation_ratio = 0.05;

    NodeId not_anytime_tree_search_queue_size = 1;
};

/**
 * Tree search with maximal empty spaces on the actual geometry of the items.
 *
 * It only handles instances with a single bin. The knapsack and feasibility
 * objectives are solved directly; the bin packing and open dimension ones
 * through the sequential feasibility algorithm.
 */
const TreeSearchMaximalSpacesOutput tree_search_maximal_spaces(
        const Instance& instance,
        const TreeSearchMaximalSpacesParameters& parameters = {});

}
}
