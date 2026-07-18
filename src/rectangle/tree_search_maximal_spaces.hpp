#pragma once

#include "rectangle/block.hpp"
#include "packingsolver/rectangle/optimize.hpp"

#include "optimizationtools/utils/utils.hpp"

#include <sstream>

namespace packingsolver
{
namespace rectangle
{

/**
 * A maximal empty space: the largest axis-aligned rectangle that can be placed
 * at a given position without overlapping any placed block or the bin walls.
 */
struct EmptySpace
{
    Point bl_corner = {0, 0};
    Rectangle rect = {0, 0};

    inline Length xs() const { return this->bl_corner.x; }
    inline Length ys() const { return this->bl_corner.y; }
    inline Length xe() const { return this->bl_corner.x + this->rect.x; }
    inline Length ye() const { return this->bl_corner.y + this->rect.y; }

    /** Returns true iff this space fully contains other. */
    inline bool contains(const EmptySpace& other) const
    {
        return this->xs() <= other.xs()
            && this->ys() <= other.ys()
            && this->xe() >= other.xe()
            && this->ye() >= other.ye();
    }

    inline bool operator==(const EmptySpace& other) const
    {
        return this->bl_corner.x == other.bl_corner.x
            && this->bl_corner.y == other.bl_corner.y
            && this->rect.x == other.rect.x
            && this->rect.y == other.rect.y;
    }

    inline bool operator<(const EmptySpace& other) const
    {
        Length this_dist = this->bl_corner.x + this->bl_corner.y;
        Length other_dist = other.bl_corner.x + other.bl_corner.y;
        if (this_dist != other_dist) return this_dist < other_dist;
        Area this_area = this->rect.area();
        Area other_area = other.rect.area();
        if (this_area != other_area) return this_area > other_area;
        if (this->bl_corner.x != other.bl_corner.x) return this->bl_corner.x < other.bl_corner.x;
        if (this->bl_corner.y != other.bl_corner.y) return this->bl_corner.y < other.bl_corner.y;
        if (this->rect.x != other.rect.x) return this->rect.x < other.rect.x;
        return this->rect.y < other.rect.y;
    }
};

struct AnchorInfo {
    Length distance = 0;
    bool dir_x = false;
    bool dir_y = false;
    Area space_area = 0;
};

inline std::ostream& operator<<(std::ostream& os, const EmptySpace& space)
{
    os << "bl (" << space.bl_corner.x << ", " << space.bl_corner.y << ")"
       << " size (" << space.rect.x << ", " << space.rect.y << ")";
    return os;
}

/**
 * Branching scheme for the single container loading problem using a maximal
 * empty-space representation, adapted to 2D rectangle packing.
 *
 * This is the 2D analog of box::BranchingSchemeMaximalSpaces (K1–K5 from
 * Lucas Guesser Targino da Silva, "A Tree Search-Based Heuristic for the 3D
 * SCLP", UNICAMP 2025), with the Z dimension removed.
 *
 * At each node the algorithm:
 *   1. Selects the empty space with minimum anchor distance to the nearest
 *      bin corner (K3).
 *   2. Generates one insertion per block that fits in that space with
 *      sufficient remaining copies, sorted by decreasing fitness (K4).
 *   3. Places the chosen block at the anchor corner and updates the maximal
 *      empty-space collection via 4-cut (K5).
 */
class BranchingSchemeMaximalSpaces
{

public:

    /*
     * Sub-structures.
     */

    struct Insertion
    {
        /** Index into the blocks array passed to the constructor. */
        ItemPos block_id = -1;

        /** Absolute position of the block's bottom-left corner. */
        Point bl_corner = {0, 0};

        /**
         * Index of the space in parent->empty_spaces from which this insertion
         * was generated.
         */
        ItemPos space_id = -1;

        /** Fitness score for this insertion. */
        double guide = 0.0;

        bool operator==(const Insertion& other) const
        {
            return this->bl_corner.x == other.bl_corner.x
                && this->bl_corner.y == other.bl_corner.y
                && this->block_id == other.block_id;
        }

        bool operator!=(const Insertion& other) const { return !(*this == other); }
    };

    struct Node
    {
        struct PlacedBlock {
            ItemPos block_id = -1;
            Point bl_corner = {0, 0};
        };

        NodeId id = -1;

        /** All blocks placed so far, in order of placement. */
        std::vector<PlacedBlock> placed_blocks;

        /** Maximal empty spaces remaining after all placements up to this node. */
        std::vector<EmptySpace> empty_spaces;

        /** For each item type, how many copies have been placed so far. */
        std::vector<ItemPos> item_number_of_copies;

        Area item_area = 0;
        Area block_area = 0;
        Weight weight = 0;
        ItemPos number_of_items = 0;
        ItemPos number_of_blocks = 0;
        Profit profit = 0;

        /**
         * IDs of blocks that can still be placed: those whose item quantities
         * are still available.
         */
        std::vector<ItemPos> valid_block_ids;

        /**
         * Profit reached by running the greedy (best_insertion loop) from
         * this node to completion.
         */
        Profit greedy_value = 0;

        std::vector<Insertion> cached_insertions;

        NodeId next_child_pos = 0;
    };

    /**
     * Fitness-function parameters (K4).
     *
     * The fitness of a block placed in a space is:
     *   V * C^alpha * L^beta * N^(-gamma)
     * where V = item profit, C = relative contact perimeter, L = area-loss
     * factor, N = number of items in the block.
     */
    struct Parameters
    {
        double alpha = 3.7015;
        double beta = 2.6631;
        double gamma = 0.3968;
        double delta = 0.0139;
        double alpha_2 = 2.0934;
        double beta_2 = 1.4609;
        double gamma_2 = 0.4732;
        double delta_2 = 0.0366;
        double configuration_switch_threshold = 0.6740;
    };

    BranchingSchemeMaximalSpaces(
            const Instance& instance,
            const std::vector<std::vector<Block>>& blocks,
            const MaxReachableLengths& max_reachable_lengths,
            const Parameters& parameters);

    const Instance& instance() const { return instance_; }
    const Parameters& parameters() const { return parameters_; }

    /*
     * Branching scheme methods (treesearchsolver interface).
     */

    const std::vector<Insertion>& insertions(
            const std::shared_ptr<Node>& parent) const;

    Insertion best_insertion(
            Node& parent) const;

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
        ss << node->item_area;
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

    /** Bin rectangle (effective, clipped to max reachable lengths). */
    Rectangle bin_rect_;

    /** max_reachable_x_[r] = largest length ≤ r achievable by stacking items along x. */
    mutable std::vector<Length> max_reachable_x_;

    /** max_reachable_y_[r] = largest length ≤ r achievable by stacking items along y. */
    mutable std::vector<Length> max_reachable_y_;

    mutable NodeId node_id_ = 0;

    mutable std::vector<Insertion> insertions_;

    /** Scratch buffers reused across calls to avoid repeated heap allocations. */
    mutable std::vector<uint8_t> scratch_reachable_x_;
    mutable std::vector<uint8_t> scratch_reachable_y_;
    mutable std::vector<uint8_t> scratch_temp_reachable_x_;
    mutable std::vector<uint8_t> scratch_temp_reachable_y_;

    void update_node_max_reachable(const Node& node) const;

    struct BestSpaceResult {
        ItemPos space_idx = -1;
        AnchorInfo anchor = {};
    };

    BestSpaceResult find_best_space(
            const Node& parent,
            BinTypeId bin_type_id) const;

    /**
     * Per-edge neighbour information for a selected empty space.
     *
     * Precomputed once per space selection for contact-perimeter scoring.
     * In 2D each neighbor has only one transverse axis (lo1/hi1).
     */
    struct SpaceContactInfo
    {
        bool xl_wall = false;
        bool yl_wall = false;
        bool xh_wall = false;
        bool yh_wall = false;
        Length space_xs = 0;
        Length space_ys = 0;
        Length space_bx = 0;
        Length space_by = 0;
        Length tol_x = 0;
        Length tol_y = 0;

        struct Neighbor {
            Length lo1 = 0;
            Length hi1 = 0;
            Length orthogonal_pos = 0;
        };

        /** xe_pb == space.bl_corner.x; transverse axis: y. */
        std::vector<Neighbor> xl_neighbors;

        /** xl_pb == space.bl_corner.x + space.rect.x; transverse axis: y. */
        std::vector<Neighbor> xh_neighbors;

        /** ye_pb == space.bl_corner.y; transverse axis: x. */
        std::vector<Neighbor> yl_neighbors;

        /** yl_pb == space.bl_corner.y + space.rect.y; transverse axis: x. */
        std::vector<Neighbor> yh_neighbors;
    };

    SpaceContactInfo compute_space_contact_info(
            const std::vector<Node::PlacedBlock>& placed_blocks,
            BinTypeId bin_type_id,
            const EmptySpace& space,
            double delta) const;

    /**
     * Pseudo-contact perimeter C of block placed at bl_corner: total edge
     * length touching bin walls and neighbouring blocks (within delta
     * tolerance), divided by the block's perimeter.  Result is in [0, 1].
     */
    double compute_relative_contact_area(
            const SpaceContactInfo& info,
            const Block& block,
            Point bl_corner,
            double delta) const;

    /**
     * Fraction of space_area that is usable after placing block.
     */
    double compute_area_loss_factor(
            const SpaceContactInfo& info,
            const Block& block) const;

    double compute_insertion_guide(
            const Node& parent,
            const Insertion& insertion,
            const SpaceContactInfo& info) const;

    /** True iff the block occupies any interior point of the space. */
    static bool overlaps(
            const EmptySpace& space,
            Point bl_corner,
            const Rectangle& block_rect);

    static void add_empty_space(
            std::vector<EmptySpace>& spaces,
            const EmptySpace& new_space);

    /**
     * Remove all spaces overlapping the placed block, generate their
     * sub-spaces via the 4-cut scheme, and add the maximal ones back.
     */
    static void cut_spaces(
            std::vector<EmptySpace>& spaces,
            Point bl_corner,
            const Rectangle& block_rect);

    Profit compute_guide_greedy(const Node& node) const;

    double active_delta(const Node& node) const;
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

} // namespace rectangle
} // namespace packingsolver

#include "packingsolver/rectangle/solution.hpp"

namespace packingsolver
{
namespace rectangle
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
