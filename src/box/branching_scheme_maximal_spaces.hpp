#pragma once

#include "box/block.hpp"

#include "optimizationtools/utils/utils.hpp"

#include <sstream>

namespace packingsolver
{
namespace box
{

/**
 * A maximal empty space: the largest axis-aligned box that can be placed at a
 * given position without overlapping any placed block or the bin walls.
 */
struct EmptySpace
{
    Point bl_corner = {0, 0, 0};
    Box box = {0, 0, 0};

    inline Length xs() const { return this->bl_corner.x; }
    inline Length ys() const { return this->bl_corner.y; }
    inline Length zs() const { return this->bl_corner.z; }
    inline Length xe() const { return this->bl_corner.x + this->box.x; }
    inline Length ye() const { return this->bl_corner.y + this->box.y; }
    inline Length ze() const { return this->bl_corner.z + this->box.z; }

    /** Returns true iff this space fully contains other. */
    inline bool contains(const EmptySpace& other) const
    {
        return this->xs() <= other.xs()
            && this->ys() <= other.ys()
            && this->zs() <= other.zs()
            && this->xe() >= other.xe()
            && this->ye() >= other.ye()
            && this->ze() >= other.ze();
    }

    inline bool operator==(const EmptySpace& other) const
    {
        return this->bl_corner == other.bl_corner && this->box == other.box;
    }

    inline bool operator<(const EmptySpace& other) const
    {
        Length this_dist = this->bl_corner.x + this->bl_corner.y + this->bl_corner.z;
        Length other_dist = other.bl_corner.x + other.bl_corner.y + other.bl_corner.z;
        if (this_dist != other_dist) return this_dist < other_dist;
        Volume this_vol = this->box.volume();
        Volume other_vol = other.box.volume();
        if (this_vol != other_vol) return this_vol > other_vol;
        if (this->bl_corner.x != other.bl_corner.x) return this->bl_corner.x < other.bl_corner.x;
        if (this->bl_corner.y != other.bl_corner.y) return this->bl_corner.y < other.bl_corner.y;
        if (this->bl_corner.z != other.bl_corner.z) return this->bl_corner.z < other.bl_corner.z;
        if (this->box.x != other.box.x) return this->box.x < other.box.x;
        if (this->box.y != other.box.y) return this->box.y < other.box.y;
        return this->box.z < other.box.z;
    }
};

struct AnchorInfo {
    Length distance = 0;
    bool dir_x = false;
    bool dir_y = false;
    bool dir_z = false;
    Volume space_volume = 0;
};

inline std::ostream& operator<<(std::ostream& os, const EmptySpace& space)
{
    os << "bl (" << space.bl_corner.x << ", " << space.bl_corner.y << ", " << space.bl_corner.z << ")"
       << " size (" << space.box.x << ", " << space.box.y << ", " << space.box.z << ")";
    return os;
}

/**
 * Branching scheme for the single container loading problem using a maximal
 * empty-space representation (K1–K5 from Lucas Guesser Targino da Silva,
 * "A Tree Search-Based Heuristic for the 3D SCLP", UNICAMP 2025).
 *
 * At each node the algorithm:
 *   1. Selects the empty space with minimum anchor distance to the nearest
 *      container corner (K3).
 *   2. Generates one insertion per block that fits in that space with
 *      sufficient remaining copies, sorted by decreasing fitness (K4).
 *   3. Places the chosen block at the anchor corner and updates the maximal
 *      empty-space collection (K5).
 *
 * Used with treesearchsolver::iterative_beam_search_2.
 */
class BranchingSchemeMaximalSpaces
{

public:

    /*
     * Sub-structures.
     */

    struct Insertion
    {
        /** True if this insertion opens a new bin before placing the block. */
        bool new_bin = false;

        /** Index into the blocks array passed to the constructor. */
        ItemPos block_id = -1;

        /** Absolute position of the block's bottom-left-back corner. */
        Point bl_corner = {0, 0, 0};

        /**
         * Index of the space in parent->empty_spaces from which this insertion
         * was generated.  -1 for new_bin insertions (the space is the full bin).
         * Used in apply_insertion() to retrieve the anchor corner without re-searching.
         * Not included in operator== since two insertions placing the same block
         * at the same position are equivalent regardless of the originating space.
         */
        ItemPos space_id = -1;

        /** contact_area * mean_item_volume * volume_load estimated for the child node. */
        double guide = 0.0;

        bool operator==(const Insertion& other) const
        {
            return this->new_bin == other.new_bin
                && this->bl_corner == other.bl_corner
                && this->block_id == other.block_id;
        }

        bool operator!=(const Insertion& other) const { return !(*this == other); }
    };

    struct Node
    {
        /**
         * One entry per block placed so far (in order of placement).
         * Holds block identity, position, and the extra items packed into the
         * gap next to it.  Used instead of a parent pointer to avoid
         * linked-list chain walks.
         */
        struct PlacedBlock {
            ItemPos block_id = -1;
            Point bl_corner = {0, 0, 0};
        };

        NodeId id = -1;

        /** True if this node opened a new bin. */
        bool new_bin = false;

        /**
         * All blocks placed so far, indexed by bin position.
         * placed_blocks[bin_idx] contains all PlacedBlocks in that bin, in
         * order of placement.
         */
        std::vector<std::vector<PlacedBlock>> placed_blocks;

        /** Maximal empty spaces remaining after all placements up to this node. */
        std::vector<EmptySpace> empty_spaces;

        /** For each item type, how many copies have been placed so far. */
        std::vector<ItemPos> item_number_of_copies;

        Volume item_volume = 0;
        Volume block_volume = 0;
        ItemPos number_of_items = 0;
        ItemPos number_of_blocks = 0;
        BinPos number_of_bins = 0;
        Profit profit = 0;

        /**
         * Accumulated volume of empty spaces that were found to be permanently
         * unfillable and removed.  For each removed space, the contribution is
         * max(0, vol(space) - sum of intersection volumes with surviving spaces):
         * the portion of the space's volume not covered by any fillable space,
         * which can never be reached regardless of future placements.
         * Lower is better.
         */
        Volume waste = 0;

        /**
         * IDs of blocks in blocks_[current bin type] that can still be placed:
         * those whose item quantities are still available and that fit within
         * at least one remaining empty space.  Empty before the first bin is
         * opened.  Maintained by apply_insertion() so that insertions() /
         * best_insertion() / find_best_space() can iterate a pre-filtered list
         * instead of rescanning all blocks from scratch on every call.
         */
        std::vector<ItemPos> valid_block_ids;

        /**
         * Item volume reached by running the greedy (best_insertion loop)
         * from this node to completion.  Set by child() and children();
         * not set by apply_insertion().
         */
        Volume greedy_value = 0;

        /**
         * Sorted insertion list populated on the first call to next_child()
         * for this node.  Stored here so that interleaved next_child() calls
         * on other nodes do not disturb this node's expansion.
         */
        std::vector<Insertion> cached_insertions;

        /**
         * Index of the next child to generate via next_child().
         * Incremented each time next_child() is called on this node.
         */
        NodeId next_child_pos = 0;
    };

    /**
     * Fitness-function parameters (K4).
     *
     * The fitness of a block placed in a space is:
     *   V * C^alpha * L^beta * N^(-gamma)
     * where V = item volume, C = relative contact area, L = volume-loss
     * factor, N = number of items in the block.
     *
     * Two sets of parameters are supported.  The first set is used when the
     * bin fill rate (block_volume / bin_volume) is below
     * configuration_switch_threshold; the second set is used above it.
     * Paper best values (da Silva 2025, Section 5.5.2):
     *   first:   α1 = 3.7015, β1 = 2.6631, γ1 = 0.3968, δ1 = 0.0139
     *   second:  α2 = 2.0934, β2 = 1.4609, γ2 = 0.4732, δ2 = 0.0366
     *   switch threshold φ = 0.6740
     */
    struct Parameters
    {
        /** Exponent of the relative contact area C.  Paper best: 3.7015. */
        double alpha = 3.7015;

        /** Exponent of the volume-loss factor L.  Paper best: 2.6631. */
        double beta = 2.6631;

        /** Exponent of the item-count penalty N^(-gamma).  Paper best: 0.3968. */
        double gamma = 0.3968;

        /** Relative contact tolerance: pseudo-contact is detected when the
         *  gap between a placed block and a space face is ≤ floor(delta *
         *  space_dim); the scored block must also satisfy gap ≤ floor(delta *
         *  block_dim).  Paper best: 0.0139. */
        double delta = 0.0139;

        /** Second-configuration alpha, active above the switch threshold.  Paper best: 2.0934. */
        double alpha_2 = 2.0934;

        /** Second-configuration beta.  Paper best: 1.4609. */
        double beta_2 = 1.4609;

        /** Second-configuration gamma.  Paper best: 0.4732. */
        double gamma_2 = 0.4732;

        /** Second-configuration delta.  Paper best: 0.0366. */
        double delta_2 = 0.0366;

        /**
         * Fill-rate threshold φ: when block_volume / bin_volume ≥ φ the
         * second configuration (alpha_2, beta_2, gamma_2, delta_2) is used
         * instead of the first.  Paper best: 0.6740.
         */
        double configuration_switch_threshold = 0.6740;
    };

    /**
     * Constructor.
     *
     * blocks must outlive this object (stored by reference).
     * blocks[bin_type_id] is the list of blocks that fit in bin type bin_type_id.
     */
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
        //std::cout << "children " << parent->id
        //    << " " << parent->number_of_blocks << std::endl;
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
        if (node_1->number_of_bins < node_2->number_of_bins)
            return true;
        if (node_1->number_of_bins > node_2->number_of_bins)
            return false;
        return node_1->greedy_value >= node_2->greedy_value;
    }

    /*
     * Outputs.
     */

    std::string display(const std::shared_ptr<Node>& node) const
    {
        std::stringstream ss;
        ss << node->item_volume;
        return ss.str();
    }

    Solution to_solution(const std::shared_ptr<Node>& node) const;

    inline bool operator()(
            const std::shared_ptr<Node>& node_1,
            const std::shared_ptr<Node>& node_2) const;

private:

    /** Stack item used by remove_spaces_and_update_waste(). */
    struct RemoveWasteStackItem
    {
        EmptySpace remaining;
        ItemPos next_surviving_idx;
    };


    const Instance& instance_;

    const std::vector<std::vector<Block>>& blocks_;

    Parameters parameters_;

    /** max_reachable_x_[r] = largest length ≤ r achievable by stacking items along x. */
    mutable std::vector<Length> max_reachable_x_;

    /** max_reachable_y_[r] = largest length ≤ r achievable by stacking items along y. */
    mutable std::vector<Length> max_reachable_y_;

    /** max_reachable_z_[r] = largest length ≤ r achievable by stacking items along z. */
    mutable std::vector<Length> max_reachable_z_;

    mutable NodeId node_id_ = 0;

    mutable std::vector<Insertion> insertions_;

    /** Scratch buffers reused across calls to avoid repeated heap allocations. */
    mutable std::vector<bool> scratch_is_removed_;
    mutable std::vector<ItemPos> scratch_surviving_ids_;
    mutable std::vector<ItemPos> scratch_sorted_ids_;
    mutable std::vector<RemoveWasteStackItem> scratch_remove_stack_;
    mutable std::vector<ItemPos> scratch_unfillable_space_ids_;

    /** Scratch buffers for update_node_max_reachable() 0-1 knapsack DP. */
    mutable std::vector<uint8_t> scratch_reachable_x_;
    mutable std::vector<uint8_t> scratch_reachable_y_;
    mutable std::vector<uint8_t> scratch_reachable_z_;
    mutable std::vector<uint8_t> scratch_temp_reachable_x_;
    mutable std::vector<uint8_t> scratch_temp_reachable_y_;
    mutable std::vector<uint8_t> scratch_temp_reachable_z_;


    /**
     * Recompute node_max_reachable_x/y/z_ for the given node using each item
     * type's remaining copies (item_type.copies - node.item_number_of_copies).
     */
    void update_node_max_reachable(const Node& node) const;

    /**
     * For each space ID in space_ids, compute the waste contribution
     * (vol(space) - coverage by surviving spaces, floored at 0) and add it to
     * node.waste, then erase all those spaces from node.empty_spaces.
     * Coverage is measured only against spaces not in space_ids, since other
     * removed spaces are also permanently inaccessible.
     */
    struct BestSpaceResult {
        ItemPos space_idx = -1;
        AnchorInfo anchor = {};
    };

    /**
     * Remove spaces with no fitting feasible block (Pass 1), then return the
     * index of the space with the smallest anchor distance (Pass 2).
     * Returns space_idx == -1 if no space remains.
     */
    BestSpaceResult find_best_space(
            const Node& parent,
            BinTypeId bin_type_id) const;

    /**
     * Per-face neighbour information for a selected empty space.
     *
     * Precomputed once per space selection so that contact-area scoring can
     * skip the full placed_blocks list for every block candidate.
     *
     * NOTE: valid only when all anchor directions are false (bl_corner ==
     * space.bl_corner for every block), which is the current invariant.
     */
    struct SpaceContactInfo
    {
        bool xl_wall = false;
        bool yl_wall = false;
        bool zl_wall = false;
        bool xh_wall = false;
        bool yh_wall = false;
        bool zh_wall = false;
        Length space_xs = 0;
        Length space_ys = 0;
        Length space_zs = 0;
        Length space_bx = 0;
        Length space_by = 0;
        Length space_bz = 0;
        /** floor(delta * space_bx/y/z): first-filter tolerance for neighbor collection. */
        Length tol_x = 0;
        Length tol_y = 0;
        Length tol_z = 0;

        /**
         * Transverse overlap range for one adjacent placed block, stored
         * relative to space.bl_corner and clamped to [0, space.box.<axis>].
         * lo1/hi1 span the first transverse axis; lo2/hi2 the second.
         * orthogonal_pos: the placed block's face position on the orthogonal axis.
         */
        struct Neighbor {
            Length lo1 = 0;
            Length hi1 = 0;
            Length lo2 = 0;
            Length hi2 = 0;
            Length orthogonal_pos = 0;
        };

        /** xe_pb == space.bl_corner.x; transverse axes: y, z. */
        std::vector<Neighbor> xl_neighbors;

        /**
         * xl_pb == space.bl_corner.x + space.box.x; transverse axes: y, z.
         *
         * Only contributes when block.box.x == space_bx.
         *  */
        std::vector<Neighbor> xh_neighbors;

        /** ye_pb == space.bl_corner.y; transverse axes: x, z. */
        std::vector<Neighbor> yl_neighbors;

        /**
         * yl_pb == space.bl_corner.y + space.box.y; transverse axes: x, z.
         *
         * Only contributes when block.box.y == space_by.
         */
        std::vector<Neighbor> yh_neighbors;

        /** ze_pb == space.bl_corner.z; transverse axes: x, y. */
        std::vector<Neighbor> zl_neighbors;

        /**
         * zl_pb == space.bl_corner.z + space.box.z; transverse axes: x, y.
         *
         * Only contributes when block.box.z == space_bz.
         */
        std::vector<Neighbor> zh_neighbors;
    };

    /** Build SpaceContactInfo for space from the placed blocks of the current bin. */
    SpaceContactInfo compute_space_contact_info(
            const std::vector<Node::PlacedBlock>& placed_blocks,
            BinTypeId bin_type_id,
            const EmptySpace& space,
            double delta) const;

    /**
     * Pseudo-contact area C of block placed at bl_corner: total face area
     * touching bin walls and neighbouring blocks (within delta tolerance),
     * divided by the block's surface area.  Result is in [0, 1].
     *
     * Uses two filters:
     *   1. (precomputed) placed block's face within floor(delta*space_dim) of space face
     *   2. (per block) block's face within floor(delta*block_dim) of neighbour's face
     */
    double compute_relative_contact_area(
            const SpaceContactInfo& info,
            const Block& block,
            Point bl_corner,
            double delta) const;

    /**
     * Fraction of space_volume that is usable after placing block: the block
     * itself plus the maximum reachable gap in each axis.  Equals 1 when the
     * block fills the space exactly and approaches 0 when it leaves a large
     * unreachable remainder.  Uses the global max_reachable tables (all item
     * copies), which is optimistic but avoids per-node recomputation.
     */
    double compute_volume_loss_factor(
            const SpaceContactInfo& info,
            const Block& block) const;

    /**
     * Fitness score for placing block in the selected space:
     *   V * C^alpha * L^beta * N^(-gamma)
     * where V = block item volume, C = relative contact area, L = volume loss
     * factor, N = number of items in the block.  Exponents are taken from
     * Parameters.  Matches the reference formula from Araya et al. / da Silva 2025.
     */
    double compute_insertion_guide(
            const Node& parent,
            const Insertion& insertion,
            const SpaceContactInfo& info) const;

    /** True iff the block occupies any interior point of the space. */
    static bool overlaps(
            const EmptySpace& space,
            Point bl_corner,
            const Box& block_box);

    /**
     * Add new_space to spaces if it is not already contained in an existing
     * space.  Zero-volume spaces are silently ignored.
     */
    static void add_empty_space(
            std::vector<EmptySpace>& spaces,
            const EmptySpace& new_space);

    void remove_spaces_and_update_waste(
            Node& node,
            const std::vector<ItemPos>& space_ids) const;

    /**
     * Remove all spaces overlapping the placed block, generate their
     * sub-spaces via the 6-cut scheme, and add the maximal ones back.
     *
     * Processes spaces one at a time so that not-yet-cut spaces participate
     * in the containment filter for newly generated sub-spaces.
     */
    static void cut_spaces(
            std::vector<EmptySpace>& spaces,
            Point bl_corner,
            const Box& block_box);

    /**
     * Copy node, run best_insertion + apply_insertion until no insertion is
     * possible, and return the item_volume of the resulting greedy solution.
     */
    Volume compute_guide_greedy(const Node& node) const;

    /**
     * Return the delta (pseudo-contact tolerance) appropriate for node:
     * delta_2 if the bin fill rate has crossed configuration_switch_threshold,
     * delta otherwise.
     */
    double active_delta(const Node& node) const;
};

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Inlined methods ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

inline bool BranchingSchemeMaximalSpaces::operator()(
        const std::shared_ptr<Node>& node_1,
        const std::shared_ptr<Node>& node_2) const
{
    if (node_1->greedy_value != node_2->greedy_value)
        return node_1->greedy_value > node_2->greedy_value;
    return node_1->id > node_2->id;
}

} // namespace box
} // namespace packingsolver
