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
            std::vector<SolutionItem> extra_items;
        };

        NodeId id = -1;

        /** Block placed at this node (index into blocks_). */
        ItemPos block_id = -1;

        /** Absolute position of the block placed at this node. */
        Point bl_corner = {0, 0, 0};

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
         * For each block placed so far, the volume of the block's bounding box
         * that lies inside its anchor octant minus the volume that lies outside.
         * The bin is split at its midpoint along each axis into 8 octants; a
         * block's anchor octant is the one belonging to the bin corner the block
         * was snapped towards.  Accumulated over the path from root to this node.
         */
        Volume corner_alignment = 0;

        /**
         * Total face-contact area accumulated over all blocks placed so far.
         * Counts each shared face once: block-to-block contacts (both blocks in
         * the same bin) and block-to-bin-wall contacts.
         */
        Volume contact_area = 0;

        /**
         * Accumulated estimate of provably-wasted volume across all block
         * placements so far (Zhu and Lim, 2012).  For each placed block b in
         * empty space e, the contribution is v_e - (l_b + u_x)(w_b + u_y)(h_b + u_z),
         * where u_d = max_reachable_d[space.d - block.d] is the maximum length
         * achievable by remaining items in direction d.  The usable box
         * (l_b + u_x) x (w_b + u_y) x (h_b + u_z) is the largest region of e
         * that could conceivably be filled; everything outside it is permanently
         * unreachable regardless of future placements.  Lower is better.
         */
        Volume volume_loss_estimate = 0;

        /**
         * Accumulated volume of empty spaces that were found to be permanently
         * unfillable and removed.  For each removed space, the contribution is
         * max(0, vol(space) - sum of intersection volumes with surviving spaces):
         * the portion of the space's volume not covered by any fillable space,
         * which can never be reached regardless of future placements.
         * Lower is better.
         */
        Volume unable_volume = 0;

        /**
         * Item volume reached by running the greedy (best_insertion loop)
         * from this node to completion.  Set by child() and children();
         * not set by apply_insertion().
         */
        Volume volume_guide = 0;

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

    /** Fitness-function exponents (K4). */
    struct Parameters
    {
        double alpha = 1.0;
        double beta = 1.0;
        double gamma = 1.0;
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
        node->volume_guide = compute_guide_greedy(*node);
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
            nodes[i]->volume_guide = compute_guide_greedy(*nodes[i]);
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
        if (node_1->empty_spaces == node_2->empty_spaces)
            return true;
        return false;
        //double volume_load_1 = (double)node_1->item_volume / node_1->block_volume;
        //double volume_load_2 = (double)node_2->item_volume / node_2->block_volume;
        //if ((volume_load_1 >= 0.98) != (volume_load_2 >= 0.98))
        //    return volume_load_1 > volume_load_2;
        //if (node_1->contact_area != node_2->contact_area)
        //    return node_1->contact_area > node_2->contact_area;
        //return true;
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

    const Instance& instance_;
    const std::vector<std::vector<Block>>& blocks_;
    Parameters parameters_;

    /** max_reachable_x_[r] = largest length ≤ r achievable by stacking items along x. */
    std::vector<Length> max_reachable_x_;
    std::vector<Length> max_reachable_y_;
    std::vector<Length> max_reachable_z_;

    /**
     * For each bin type, the indices of Pareto-minimal blocks: those not
     * dominated by any other block in all three box dimensions.  A space has
     * no fitting block by size iff none of these blocks fits inside it.
     */
    std::vector<std::vector<ItemPos>> pareto_front_block_ids_;

    void compute_pareto_fronts();

    /**
     * For each space ID in space_ids, compute the waste contribution
     * (vol(space) - coverage by surviving spaces, floored at 0) and add it to
     * node.unable_volume, then erase all those spaces from node.empty_spaces.
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

    void remove_spaces_and_update_waste(
            Node& node,
            const std::vector<ItemPos>& space_ids) const;

    /**
     * For the given direction (X, Y or Z), check whether the far face of the
     * placed block directly faces the bin wall (no packed block in the gap
     * region), then attempt to pack all remaining fitting items in that gap
     * using a grid heuristic.  On success, node.extra_items, item counts,
     * volumes and profit are updated, and the effective far extent in that
     * direction (eff_xe / eff_ye / eff_ze) is advanced to the bin wall.
     */
    void try_extend_block(
            Node& node,
            BinTypeId bin_type_id,
            const BinType& bin_type,
            const Insertion& insertion,
            std::vector<SolutionItem>& extra_items,
            Length eff_xs,
            Length eff_ys,
            Length eff_zs,
            Length& eff_xe,
            Length& eff_ye,
            Length& eff_ze,
            Direction direction) const;

    /**
     * Recompute node_max_reachable_x/y/z_ for the given node using each item
     * type's remaining copies (item_type.copies - node.item_number_of_copies).
     */
    void update_node_max_reachable(const Node& node) const;

    /**
     * Contact area of a block placed at bl_corner with the bin walls and all
     * previously placed blocks in the same bin (found by walking placed_blocks
     * backward until the new_bin marker).  Does not include the block's own
     * internal contact area (block.contact_area).
     * Pass an empty placed_blocks when the block opens a new bin.
     */
    Volume compute_new_contact_area(
            const std::vector<Node::PlacedBlock>& placed_blocks,
            BinTypeId bin_type_id,
            Point bl_corner,
            const Block& block) const;

    /**
     * Estimate contact_area * mean_item_volume * volume_load for the child
     * that would result from applying insertion to parent.
     */
    double compute_insertion_guide(
            const Node& parent,
            const Insertion& insertion) const;

    /**
     * Copy node, run best_insertion + apply_insertion until no insertion is
     * possible, and return the item_volume of the resulting greedy solution.
     */
    Volume compute_guide_greedy(const Node& node) const;

    inline double volume_load(const Node& node) const;

    /**
     * Return true iff node's volume load (item_volume / block_volume) meets
     * a density threshold that decreases linearly as the bin fills up:
     *   threshold(f) = 0.99 - 0.04 * f,  f = block_volume / bin_volume
     * At the start (f = 0) the threshold is 0.99; at the end (f = 1) it
     * relaxes to 0.95, accepting the small inevitable waste in a full bin.
     */
    inline bool volume_load_ok(const Node& node) const;

    mutable NodeId node_id_ = 0;
    mutable std::vector<Insertion> insertions_;

    /** Stack item used by remove_spaces_and_update_waste(). */
    struct RemoveWasteStackItem {
        EmptySpace remaining;
        ItemPos next_surviving_idx;
    };

    /** Scratch buffers reused across calls to avoid repeated heap allocations. */
    mutable std::vector<bool> scratch_is_removed_;
    mutable std::vector<ItemPos> scratch_surviving_ids_;
    mutable std::vector<ItemPos> scratch_sorted_ids_;
    mutable std::vector<RemoveWasteStackItem> scratch_remove_stack_;
    mutable std::vector<ItemPos> scratch_unfillable_space_ids_;

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
};

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Inlined methods ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

inline double BranchingSchemeMaximalSpaces::volume_load(const Node& node) const
{
    Volume used_volume = node.block_volume + node.unable_volume;
    if (used_volume == 0)
        return 0;
    return (double)node.item_volume / used_volume;
}

inline bool BranchingSchemeMaximalSpaces::volume_load_ok(const Node& node) const
{
    Volume used_volume = node.block_volume + node.unable_volume;
    if (used_volume == 0)
        return true;
    double volume_load = (double)node.item_volume / used_volume;
    double f = std::min(1.0, (double)used_volume / instance_.bin_volume());
    double threshold = 0.99 - 0.04 * f;
    return volume_load >= threshold;
}

inline bool BranchingSchemeMaximalSpaces::operator()(
        const std::shared_ptr<Node>& node_1,
        const std::shared_ptr<Node>& node_2) const
{
    if (node_1->volume_guide != node_2->volume_guide)
        return node_1->volume_guide > node_2->volume_guide;

    //double mean_item_volume_1 = (double)(node_1->item_volume) / node_1->number_of_items;
    //double mean_item_volume_2 = (double)(node_2->item_volume) / node_2->number_of_items;

    //double alpha = 4.0;
    //double beta = 1.0;
    //double gamma = 5.0;
    //double guide_1 = node_1->item_volume
    //    * std::pow(node_1->contact_area, alpha)
    //    * std::pow(volume_load(*node_1), beta)
    //    * std::pow(mean_item_volume_1, gamma);
    //double guide_2 = node_2->item_volume
    //    * std::pow(node_2->contact_area, alpha)
    //    * std::pow(volume_load(*node_2), beta)
    //    * std::pow(mean_item_volume_2, gamma);
    //if (guide_1 != guide_2)
    //    return guide_1 < guide_2;

    //if (volume_load(*node_1) != volume_load(*node_2))
    //    return volume_load(*node_1) > volume_load(*node_2);

    //bool ok_1 = volume_load_ok(*node_1);
    //bool ok_2 = volume_load_ok(*node_2);
    //if (ok_1 != ok_2)
    //    return ok_1;

    //double guide_1 = node_1->contact_area * mean_item_volume_1 * std::pow(volume_load(*node_1), 1);
    //double guide_2 = node_2->contact_area * mean_item_volume_2 * std::pow(volume_load(*node_2), 1);
    //if (guide_1 != guide_2)
    //    return guide_1 > guide_2;

    //if (node_1->contact_area != node_2->contact_area)
    //    return node_1->contact_area > node_2->contact_area;

    //if (mean_item_volume_1 != mean_item_volume_2)
    //    return mean_item_volume_1 > mean_item_volume_2;

    //if (node_1->corner_alignment != node_2->corner_alignment)
    //    return node_1->corner_alignment > node_2->corner_alignment;

    //if ((volume_load_1 >= 0.98) != (volume_load_2 >= 0.98))
    //    return volume_load_1 > volume_load_2;

    //if (node_1->item_volume != node_2->item_volume)
    //    return node_1->item_volume > node_2->item_volume;

    //if (volume_load_1 != volume_load_2)
    //    return volume_load_1 > volume_load_2;

    //if (node_1->empty_spaces.size() != node_2->empty_spaces.size())
    //    return node_1->empty_spaces.size() < node_2->empty_spaces.size();

    //double guide_1 = node_1->item_volume;
    //double guide_2 = node_2->item_volume;
    ////double guide_1 = waste_percentage_1 / mean_item_volume_1;
    ////double guide_2 = waste_percentage_2 / mean_item_volume_2;
    //if (guide_1 != guide_2)
    //    return guide_1 > guide_2;

    return node_1->id > node_2->id;
}

} // namespace box
} // namespace packingsolver
