#pragma once

#include "packingsolver/irregular/optimize.hpp"
#include "irregular/periodic_packing.hpp"

#include "optimizationtools/utils/utils.hpp"

#include <sstream>

namespace packingsolver
{
namespace irregular
{

/**
 * A periodic block: an nx × ny tiling of a PeriodicItemPacking unit cell.
 *
 * Items are positioned relative to the block's bottom-left corner (0, 0)
 * in scaled coordinates. bx and by are the AABB dimensions of the full tiling.
 *
 * Unlike PeriodicItemPacking, this depends on bin dimensions and on the item
 * type's available copies (both of which can differ across sub-instances
 * built from the same source item type), so it is never cached on ItemType
 * and is always recomputed by compute_periodic_blocks_for_item_type /
 * compute_periodic_blocks below.
 */
struct PeriodicBlock
{
    /** Index into the packings array the block was built from. */
    int packing_id = -1;

    /** Number of unit-cell repetitions along vector_1 and vector_2. */
    int nx = 1;
    int ny = 1;

    /** Bounding-box dimensions in scaled coordinates. */
    LengthDbl bx = 0.0;
    LengthDbl by = 0.0;

    /** All items in the tiling with positions relative to block BL corner. */
    std::vector<SolutionItem> items;

    /**
     * Number of copies of each item type used in this block,
     * as a sorted sparse list of (item_type_id, count) pairs.
     */
    std::vector<std::pair<ItemTypeId, ItemPos>> item_copies;

    /** Sum of areas of all items in the block (scaled). */
    AreaDbl item_area = 0.0;

    /** Sum of profits of all items in the block. */
    Profit item_profit = 0;

    /** Total number of items in the block. */
    int number_of_items = 0;

    double fill_rate() const { return item_area / (bx * by); }
};

/**
 * Compute periodic blocks (tilings of periodic-packing unit cells, plus a
 * plain AABB-grid fallback for rotations with no periodic packing) for a
 * single item type, bounded both by the item type's available copies and by
 * the given instance's bin dimensions.
 *
 * Unlike compute_periodic_packings_for_item_type, this does depend on bin
 * dimensions (block growth stops once a tiling no longer fits the bin) and
 * on the item type's available copies, both of which can differ across
 * sub-instances built from the same source item type; for that reason,
 * unlike periodic_packings, results are never cached on ItemType and are
 * always recomputed by the caller.
 */
std::vector<PeriodicBlock> compute_periodic_blocks_for_item_type(
        const Instance& instance,
        ItemTypeId item_type_id,
        const std::vector<PeriodicItemPacking>& packings,
        const std::vector<ItemTypeRotation>& rotations);

std::vector<PeriodicBlock> compute_periodic_blocks(
        const Instance& instance,
        const std::vector<PeriodicItemPacking>& packings,
        const std::vector<std::vector<ItemTypeRotation>>& item_type_rotations);

/**
 * A maximal empty space: the largest axis-aligned rectangle that can be placed
 * at a given position without overlapping any placed block or the bin walls.
 * All lengths are in scaled coordinates.
 */
struct EmptySpace
{
    Point bl_corner = {0.0, 0.0};
    LengthDbl bx = 0.0;
    LengthDbl by = 0.0;

    inline LengthDbl xs() const { return bl_corner.x; }
    inline LengthDbl ys() const { return bl_corner.y; }
    inline LengthDbl xe() const { return bl_corner.x + bx; }
    inline LengthDbl ye() const { return bl_corner.y + by; }

    /** Returns true iff this space fully contains other. */
    inline bool contains(const EmptySpace& other) const
    {
        return !strictly_greater(xs(), other.xs())
            && !strictly_greater(ys(), other.ys())
            && !strictly_lesser(xe(), other.xe())
            && !strictly_lesser(ye(), other.ye());
    }

    inline AreaDbl area() const { return bx * by; }
};

////////////////////////////////////////////////////////////////////////////////

struct AnchorInfo
{
    LengthDbl distance = 0.0;
    bool dir_x = false;
    bool dir_y = false;
    AreaDbl space_area = 0.0;
};

/**
 * Branching scheme for the irregular knapsack using a maximal empty-space
 * representation with periodic packings as blocks.
 *
 * Mirrors rectangle::BranchingSchemeMaximalSpaces with LengthDbl arithmetic
 * and PeriodicBlock instead of Block.  The area-loss factor is omitted
 * (treated as 1.0) because computing max-reachable lengths for continuous
 * coordinates is non-trivial.
 */
class BranchingSchemePeriodicPacking
{

public:

    struct Insertion
    {
        /** Index into the blocks array. */
        ItemPos block_id = -1;

        /** Absolute position of the block BL corner (scaled coordinates). */
        Point bl_corner = {0.0, 0.0};

        /** Index of the empty space in parent->empty_spaces. */
        ItemPos space_id = -1;

        /** Fitness score. */
        double guide = 0.0;

        bool operator==(const Insertion& other) const
        {
            return shape::equal(bl_corner.x, other.bl_corner.x)
                && shape::equal(bl_corner.y, other.bl_corner.y)
                && block_id == other.block_id;
        }
        bool operator!=(const Insertion& other) const { return !(*this == other); }
    };

    struct Node
    {
        struct PlacedBlock {
            ItemPos block_id = -1;
            Point bl_corner = {0.0, 0.0};
        };

        NodeId id = -1;

        std::vector<PlacedBlock> placed_blocks;

        std::vector<EmptySpace> empty_spaces;

        std::vector<ItemPos> item_number_of_copies;

        AreaDbl item_area = 0.0;
        ItemPos number_of_items = 0;
        ItemPos number_of_blocks = 0;
        Profit profit = 0;

        std::vector<ItemPos> valid_block_ids;

        Profit greedy_value = 0;

        std::vector<Insertion> cached_insertions;

        NodeId next_child_pos = 0;
    };

    /**
     * Fitness-function parameters.
     *
     * guide = V * C^alpha * F^beta * N^(-gamma)
     * where V = item profit, C = relative contact area, F = the block's own
     * fill rate (item area / block area), and N = number of items.
     *
     * F is unrelated to the "area-loss factor" of the sibling
     * rectangle/box maximal-spaces branching schemes (which measures how
     * much of the *remaining* empty space stays usable after this
     * insertion, via a reachability grid): here it is simply how much of
     * the block's own bounding box its items actually cover, penalizing
     * blocks with a lot of internal waste (e.g. blocks sized to nearly
     * match the bin, chosen mostly for wall contact, over a denser block
     * with the same item count).
     */
    struct Parameters
    {
        double alpha = 3.7015;
        double beta = 3.0;
        double gamma = 0.3968;
        double delta = 0.0139;
        double alpha_2 = 2.0934;
        double beta_2 = 3.0;
        double gamma_2 = 0.4732;
        double delta_2 = 0.0366;
        double configuration_switch_threshold = 0.6740;
    };

    /**
     * Assembles periodic packings (reading from the ItemType::periodic_packings
     * cache when available, computing on the fly otherwise) and builds the
     * blocks internally.
     */
    BranchingSchemePeriodicPacking(
            const Instance& instance,
            const Parameters& parameters);

    const Instance& instance() const { return instance_; }
    const Parameters& parameters() const { return parameters_; }

    /** Returns true iff no block could be built from the given packings. */
    bool empty() const { return blocks_.empty(); }

    /*
     * Branching scheme interface.
     */

    const std::vector<Insertion>& insertions(
            const std::shared_ptr<Node>& parent) const;

    Insertion best_insertion(Node& parent) const;

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
        if (number_of_children < 0 || number_of_children > (NodeId)insertions_.size())
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
        const BranchingSchemePeriodicPacking& branching_scheme;

        NodeHasher(const BranchingSchemePeriodicPacking& bs): branching_scheme(bs) {}

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
        ss << node->item_area;
        return ss.str();
    }

    Solution to_solution(const std::shared_ptr<Node>& node) const;

    inline bool operator()(
            const std::shared_ptr<Node>& node_1,
            const std::shared_ptr<Node>& node_2) const;

private:

    const Instance& instance_;
    std::vector<PeriodicBlock> blocks_;
    Parameters parameters_;

    /** Bin bounding-box dimensions (scaled). */
    LengthDbl bin_bx_ = 0.0;
    LengthDbl bin_by_ = 0.0;

    mutable NodeId node_id_ = 0;
    mutable std::vector<Insertion> insertions_;

    struct BestSpaceResult {
        ItemPos space_idx = -1;
        AnchorInfo anchor = {};
    };

    BestSpaceResult find_best_space(const Node& parent) const;

    struct SpaceContactInfo
    {
        bool xl_wall = false;
        bool yl_wall = false;
        bool xh_wall = false;
        bool yh_wall = false;
        LengthDbl space_xs = 0.0;
        LengthDbl space_ys = 0.0;
        LengthDbl space_bx = 0.0;
        LengthDbl space_by = 0.0;
        LengthDbl tol_x = 0.0;
        LengthDbl tol_y = 0.0;

        struct Neighbor {
            LengthDbl lo1 = 0.0;
            LengthDbl hi1 = 0.0;
            LengthDbl orthogonal_pos = 0.0;
        };

        std::vector<Neighbor> xl_neighbors;
        std::vector<Neighbor> xh_neighbors;
        std::vector<Neighbor> yl_neighbors;
        std::vector<Neighbor> yh_neighbors;
    };

    SpaceContactInfo compute_space_contact_info(
            const std::vector<Node::PlacedBlock>& placed_blocks,
            const EmptySpace& space,
            double delta) const;

    double compute_relative_contact_area(
            const SpaceContactInfo& info,
            const PeriodicBlock& block,
            Point bl_corner,
            double delta) const;

    double compute_insertion_guide(
            const Node& parent,
            const Insertion& insertion,
            const SpaceContactInfo& info) const;

    static bool overlaps(
            const EmptySpace& space,
            Point bl_corner,
            LengthDbl bx,
            LengthDbl by);

    static void add_empty_space(
            std::vector<EmptySpace>& spaces,
            const EmptySpace& new_space);

    static void cut_spaces(
            std::vector<EmptySpace>& spaces,
            Point bl_corner,
            LengthDbl bx,
            LengthDbl by);

    Profit compute_guide_greedy(const Node& node) const;

    double active_delta(const Node& node) const;
};

////////////////////////////////////////////////////////////////////////////////

inline bool BranchingSchemePeriodicPacking::operator()(
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

} // namespace irregular
} // namespace packingsolver

namespace packingsolver
{
namespace irregular
{

struct TreeSearchPeriodicPackingOutput: Output
{
    TreeSearchPeriodicPackingOutput(const Instance& instance):
        Output(instance) { }
};

struct TreeSearchPeriodicPackingParameters: packingsolver::Parameters<Instance, Solution, Output>
{
    OptimizationMode optimization_mode = OptimizationMode::Anytime;

    NodeId not_anytime_tree_search_queue_size = 1;
};

/**
 * Computes everything from scratch: periodic packings (item shapes/rotations
 * only, independent of bin size; read from the ItemType::periodic_packings
 * cache when available), then blocks (bin-size-dependent), then runs the
 * search.
 */
const TreeSearchPeriodicPackingOutput tree_search_periodic_packing(
        const Instance& instance,
        const TreeSearchPeriodicPackingParameters& parameters = {});

} // namespace irregular
} // namespace packingsolver
