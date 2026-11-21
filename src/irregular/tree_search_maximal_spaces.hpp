#pragma once

#include "packingsolver/irregular/optimize.hpp"

#include "irregular/trapezoid_slider.hpp"
#include "irregular/empty_spaces.hpp"
#include "irregular/shape_simplification.hpp"

#include "algorithms/item_copies_hash.hpp"

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
 *   the empty space closest to a corner of the bin, and a node is evaluated
 *   by completing it greedily;
 * - like the former, the items are described by their trapezoids, and a
 *   block is inserted by resting one of its supported parts on a supporting
 *   part of the bin or of a block already packed and by sliding it towards
 *   the side of the corner until it doesn't overlap the items already
 *   packed.
 *
 * The corners are the ones of the direction frame: corner 0 is its
 * bottom-left corner; in corner 1 (resp. 2), the x-axis (resp. y-axis) is
 * mirrored; in corner 3, both are. An insertion at a corner is computed in
 * the frame of the corner, in which it is the bottom-left corner: the blocks
 * are built in each of these frames, and the trapezoids of the node, stored
 * in the frame of corner 0, are mirrored into it.
 *
 * When minimizing the extent of the items (see 'minimize_extent'), the
 * selected empty space is the one closest to the left side of the bin, then
 * to one of its left corners, so that the packed items progress along the
 * x-axis of the direction frame, and the nodes are compared on the extent of
 * their greedy completions.
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

    /** Corner of the bin (see the description of the class). */
    using Corner = int;

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
         * Bounding box of the supporting parts, which are computed on the
         * inflated shapes of the items, relative to the block.
         */
        AxisAlignedBoundingBox supporting_parts_aabb;

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

        /**
         * Consumption of each resource of the bin type by the items of the
         * block alone in the bin (see 'compute_resource_consumption'). The
         * blocks which exceed the capacity of a resource which is not
         * penalized are not generated. Empty iff the bin type has no
         * resources.
         */
        std::vector<double> resource_consumption;
    };

    struct Insertion
    {
        /** Inserted block. */
        ItemPos block_id = -1;

        /** Corner of the insertion. */
        Corner corner = 0;

        /** x-coordinate of the block, in the frame of the corner. */
        LengthDbl x = 0.0;

        /** y-coordinate of the block, in the frame of the corner. */
        LengthDbl y = 0.0;

        /** Score of the insertion, the larger the better. */
        double guide = 0.0;
    };

    /**
     * Insertions of a node in its selected empty space, computed lazily, by
     * decreasing upper bound of the score of the blocks (see
     * 'InsertionGenerator::Candidate::score_bound'): the insertion of rank k
     * is known once the bound of the remaining blocks is below its score.
     */
    struct InsertionGenerator
    {
        /** Block which may be inserted in the empty space. */
        struct Candidate
        {
            /** Block. */
            ItemPos block_id = -1;

            /**
             * Position of the block in the valid blocks of the node, used to
             * break the ties between insertions of equal scores.
             */
            ItemPos valid_block_pos = -1;

            /**
             * Upper bound of the score of the insertions of the block: its
             * score with the density replaced by the area of its items
             * divided by the area of its bounding box.
             */
            double score_bound = 0.0;
        };

        /** Insertion found, with the position of its block. */
        struct FoundInsertion
        {
            Insertion insertion;

            ItemPos valid_block_pos = -1;
        };

        /** 'true' iff an empty space is selected. */
        bool space_selected = false;

        /** Position of the selected empty space in the node. */
        ItemPos space_pos = -1;

        /** Corner at which the blocks are inserted in the empty space. */
        Corner corner = 0;

        /** Selected empty space, in the frame of the corner. */
        EmptySpace space;

        /** Supporting parts touching the bottom side of the empty space. */
        std::vector<Shape> supporting_shapes;

        /**
         * For each supporting part, the trapezoids a block resting on it may
         * overlap.
         */
        std::vector<std::vector<GeneralizedTrapezoid>> supporting_shapes_obstacles;

        /** Left limit of the walls of the slides. */
        LengthDbl x_wall_min = 0.0;

        /** Candidate blocks, by decreasing score bound. */
        std::vector<Candidate> candidates;

        /** Position of the next candidate block to slide. */
        ItemPos next_candidate_pos = 0;

        /**
         * Insertions of the candidates slid so far, at most one per block,
         * by decreasing score, then by increasing position of their block in
         * the valid blocks.
         */
        std::vector<FoundInsertion> insertions;
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

            /**
             * x-coordinate of the block, in the frame of corner 0: its items
             * are the ones of the block in this frame, shifted.
             */
            LengthDbl x = 0.0;

            /** y-coordinate of the block, in the frame of corner 0. */
            LengthDbl y = 0.0;
        };

        /** Id of the node. */
        NodeId id = -1;

        /** Packed blocks. */
        std::vector<PlacedBlock> placed_blocks;

        /**
         * Trapezoids that a new block must not overlap: the obstacles of the
         * bin and the inflated trapezoids of the packed items, in the frame
         * of corner 0.
         */
        std::vector<GeneralizedTrapezoid> trapezoids;

        /** Maximal empty spaces, in the frame of corner 0. */
        std::vector<EmptySpace> empty_spaces;

        /** For each item type, number of copies in the node. */
        std::vector<ItemPos> item_number_of_copies;

        /**
         * Hash of 'item_number_of_copies', maintained incrementally (see
         * 'ItemCopiesHashKeys').
         */
        std::size_t item_number_of_copies_hash = 0;

        /**
         * Consumption of each resource of the bin type by the fixed items and
         * the packed blocks: the sum of their consumptions. Empty iff the bin
         * type has no resources.
         */
        std::vector<double> resource_consumption;

        /**
         * Blocks which don't use more copies than available and don't exceed
         * the capacity of a resource which is not penalized.
         */
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

        /** Bounding box of the packed items, in the direction frame. */
        AxisAlignedBoundingBox items_aabb;

        /** Number of items of the greedy completion of the node. */
        ItemPos greedy_number_of_items = 0;

        /**
         * Extent of the greedy completion of the node (see 'extent'); only
         * used when minimizing the extent.
         */
        double greedy_extent = 0.0;

        /**
         * Insertions of the node, created when its first child is, and reset
         * once they are all generated.
         */
        std::shared_ptr<InsertionGenerator> insertion_generator;

        /** Rank of the insertion of the next child. */
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
         * Insert the blocks at several corners of the bin, and not only at
         * corner 0: at the 4 corners for the knapsack and feasibility
         * objectives, at the 2 left ones when minimizing the extent along
         * the x-axis of the direction frame (open dimension objectives, and
         * bin packing with leftovers objective with a side leftover mode).
         */
        bool use_multiple_corners = true;

        /**
         * Exponent of the density of an insertion in its score:
         *
         * score = value * density^beta * number_of_items^(-gamma)
         *
         * where the value is the one of 'insertion_value' and the density is
         * the area of the items of the block divided by the area of the
         * rectangle from the corner of the empty space to the top-right
         * corner of the block.
         */
        double beta = 2.0;

        /** Exponent of the number of items of an insertion in its score. */
        double gamma = 0.4;

        /**
         * Exponent of the profit density of a block in the value of its
         * insertions: the area of its items is weighted by its profit per
         * unit of area to this power. It has no effect when the profits are
         * proportional to the areas; 1 weights the area by the density
         * once, which amounts to the profit of the block.
         */
        double profit_density_exponent = 2.0;

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

    /** Get the blocks, in the frame of corner 0. */
    inline const std::vector<Block>& blocks() const { return corners_[0].blocks; }

    /** Get the corners at which the blocks may be inserted. */
    inline const std::vector<Corner>& corner_ids() const { return corner_ids_; }

    /**
     * Return 'true' iff the objective is to pack all the items while
     * minimizing their extent (see 'extent'): open dimension X or Y, or bin
     * packing with leftovers in a single bin.
     */
    inline bool minimize_extent() const { return minimize_extent_; }

    /**
     * Extent of the items of a node, to minimize:
     * - their x_max for the open dimension X objective;
     * - their y_max for the open dimension Y objective;
     * - minus the leftover value of the bin for the bin packing with
     *   leftovers objective.
     *
     * It doesn't decrease when items are added.
     */
    double extent(const Node& node) const;

    /*
     * Branching scheme methods
     */

    const std::shared_ptr<Node> root() const;

    std::shared_ptr<Node> next_child(const std::shared_ptr<Node>& parent) const;

    inline bool infertile(const std::shared_ptr<Node>& node) const
    {
        return node->next_child_pos > 0
            && node->insertion_generator == nullptr;
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

        inline bool operator()(
                const std::shared_ptr<Node>& node_1,
                const std::shared_ptr<Node>& node_2) const
        {
            return node_1->item_number_of_copies == node_2->item_number_of_copies;
        }

        inline std::size_t operator()(
                const std::shared_ptr<Node>& node) const
        {
            return node->item_number_of_copies_hash;
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
        if (minimize_extent_)
            return !greedy_better(*node_2, *node_1);
        return node_1->greedy_value >= node_2->greedy_value;
    }

    /*
     * Outputs
     */

    std::string display(const std::shared_ptr<Node>& node) const
    {
        std::stringstream ss;
        if (minimize_extent_) {
            ss << node->greedy_number_of_items << " " << node->greedy_extent;
        } else {
            ss << node->greedy_value;
        }
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

    /** Keys of the hashes of the numbers of copies of the item types. */
    ItemCopiesHashKeys item_copies_hash_keys_;

    /** Simplified instance. */
    SimplifiedInstance simplified_instance_;

    /** Geometry of the bin and of the blocks in the frame of a corner. */
    struct CornerData
    {
        /** Direction of the frame of the corner. */
        Direction direction = Direction::LeftToRightThenBottomToTop;

        /** Trapezoid bin type of the (single) bin type. */
        TrapezoidBinType bin_type;

        /**
         * Trapezoid sets of the items: the ones of 'bin_type', then the ones
         * of the orientations used by the periodic blocks only. They are
         * the same in all the corners.
         */
        std::vector<TrapezoidSet> trapezoid_sets;

        /** Supporting parts of the trapezoid sets. */
        std::vector<Support> supporting_parts;

        /** Supported parts of the trapezoid sets. */
        std::vector<Support> supported_parts;

        /**
         * Rectangles covering the inflated shape of each trapezoid set; only
         * computed for corner 0.
         */
        std::vector<std::vector<AxisAlignedBoundingBox>> trapezoid_sets_rectangles;

        /**
         * Blocks: the same in all the corners, the ones of a corner being the
         * ones of corner 0 mirrored.
         */
        std::vector<Block> blocks;

        /** Usable part of the bin. */
        AxisAlignedBoundingBox bin_aabb;
    };

    /** Data of each corner, only built for the corners of 'corner_ids_'. */
    std::vector<CornerData> corners_;

    /** Corners at which the blocks may be inserted. */
    std::vector<Corner> corner_ids_;

    /**
     * For each item type, true iff its consumption of a resource of the bin
     * type depends on the number of its copies already in the bin.
     */
    std::vector<bool> copy_dependent_resource_consumptions_;

    /** Buffer of 'insertion_resource_consumption'. */
    mutable std::vector<double> insertion_resource_consumption_;

    /**
     * Rectangles covering the obstacles of the bin, cut out of the empty
     * spaces of the root node.
     */
    std::vector<AxisAlignedBoundingBox> obstacles_rectangles_;

    /** See 'minimize_extent'. */
    bool minimize_extent_ = false;

    /** Bounding box of the fixed items, in the direction frame. */
    AxisAlignedBoundingBox fixed_items_aabb_;

    mutable NodeId node_id_ = 0;

    /*
     * Private methods
     */

    /**
     * Get the trapezoid set of an item type in an orientation, building it
     * if needed.
     */
    TrapezoidSetId trapezoid_set_id(
            Corner corner,
            ItemTypeId item_type_id,
            Angle angle,
            bool mirror);

    /**
     * Compute the rectangles covering the inflated shape of a trapezoid set
     * of corner 0.
     */
    std::vector<AxisAlignedBoundingBox> compute_rectangles(
            const TrapezoidSet& trapezoid_set) const;

    /** Build a block from its items, in the frame of a corner. */
    Block build_block(
            Corner corner,
            const std::vector<BlockItem>& items) const;

    /** Build the blocks. */
    void compute_blocks();

    /**
     * Select the empty space in which to insert the next block, and the
     * corner at which to insert it.
     */
    ItemPos select_empty_space(
            const Node& node,
            Corner& corner) const;

    /** Return 'true' iff the block may be inserted in the empty space. */
    bool may_fit(
            const Block& block,
            const EmptySpace& space) const;

    /**
     * Compute the insertion of rank 'rank' (0 for the best one) of the node
     * in its selected empty space; the empty spaces in which no block can be
     * inserted are removed from the node.
     *
     * Return 'false' if the node has at most 'rank' insertions.
     */
    bool insertion(
            Node& node,
            InsertionGenerator& generator,
            NodeId rank,
            Insertion& insertion) const;

    /**
     * Select the empty space of the node and compute its candidate blocks
     * and its supporting parts.
     */
    void select_empty_space(
            const Node& node,
            InsertionGenerator& generator) const;

    /**
     * Compute the best insertion of a block in the selected empty space;
     * 'block_id' is -1 in the returned insertion if the block can't be
     * inserted there.
     */
    Insertion best_insertion(
            const Node& node,
            const InsertionGenerator& generator,
            ItemPos block_id) const;

    /**
     * Return true iff a block may be packed in a node: it doesn't use more
     * copies than available and doesn't exceed the capacity of a resource
     * which is not penalized.
     */
    bool block_valid(
            const Node& node,
            const Block& block) const;

    /**
     * Consumption of each resource of the bin type by the items of 'block'
     * if it is packed in 'node': 'Block::resource_consumption', shifted for
     * the item types whose consumption depends on the number of their copies
     * already in 'node' (see 'shifted_resource_consumption').
     *
     * The returned reference may be invalidated by the next call.
     */
    const std::vector<double>& insertion_resource_consumption(
            const Node& node,
            const Block& block) const;

    /**
     * Profit of packing a block in a node: the profit of its items minus the
     * penalties of the penalized resources whose capacity it exceeds first.
     */
    Profit insertion_profit(
            const Node& node,
            const Block& block) const;

    /**
     * Value of packing a block in a node in the score of its insertions.
     *
     * Generalization of the volume of the items of the block of the fitness
     * function of Guesser et al. (2026) to profits different from the areas:
     * the area of the items weighted by their profit density. When the
     * profits are proportional to the areas, all the blocks have the same
     * density, and the order of the insertions is the one of the paper.
     */
    double insertion_value(
            const Node& node,
            const Block& block) const;

    /** Apply an insertion to a node. */
    void apply_insertion(
            Node& node,
            const Insertion& insertion) const;

    /** Remove the empty spaces too small for all the valid blocks. */
    void remove_unusable_spaces(Node& node) const;

    /** Complete a node greedily. */
    void complete_greedily(Node& node) const;

    /**
     * Return 'true' iff the greedy completion of 'node_1' is strictly better
     * than the one of 'node_2' when minimizing the extent: it packs all the
     * items and the one of 'node_2' doesn't, or both pack all the items and
     * its extent is smaller, or none pack all the items and its profit is
     * larger.
     */
    bool greedy_better(
            const Node& node_1,
            const Node& node_2) const;

};

inline bool BranchingSchemeMaximalSpaces::operator()(
        const std::shared_ptr<Node>& node_1,
        const std::shared_ptr<Node>& node_2) const
{
    if (node_1->number_of_blocks == 0)
        return true;
    if (node_2->number_of_blocks == 0)
        return false;
    if (minimize_extent_) {
        if (greedy_better(*node_1, *node_2))
            return true;
        if (greedy_better(*node_2, *node_1))
            return false;
    }
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

    /**
     * Solve the open dimension X, open dimension Y and bin packing with
     * leftovers objectives through the sequential feasibility algorithm,
     * instead of directly. The bin packing and open dimension XY objectives
     * are always solved through it.
     */
    bool use_sequential_feasibility = true;

    /**
     * Insert the blocks at several corners of the bin (see
     * 'BranchingSchemeMaximalSpaces::Parameters::use_multiple_corners').
     */
    bool use_multiple_corners = true;
};

/**
 * Tree search with maximal empty spaces on the actual geometry of the items.
 *
 * It only handles instances with a single bin. The knapsack, feasibility,
 * open dimension X, open dimension Y and bin packing with leftovers
 * objectives are solved directly; the bin packing and open dimension XY ones
 * through the sequential feasibility algorithm, as well as the other ones if
 * 'use_sequential_feasibility' is set.
 */
const TreeSearchMaximalSpacesOutput tree_search_maximal_spaces(
        const Instance& instance,
        const TreeSearchMaximalSpacesParameters& parameters = {});

}
}
