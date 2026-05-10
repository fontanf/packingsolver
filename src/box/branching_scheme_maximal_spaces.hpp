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
};

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
         * Used in child_tmp() to retrieve the anchor corner without re-searching.
         * Not included in operator== since two insertions placing the same block
         * at the same position are equivalent regardless of the originating space.
         */
        ItemPos space_id = -1;

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
        NodeId id = -1;

        std::shared_ptr<Node> parent = nullptr;

        /** Block placed at this node (index into blocks_). */
        ItemPos block_id = -1;

        /** Absolute position of the block placed at this node. */
        Point bl_corner = {0, 0, 0};

        /** Maximal empty spaces remaining after all placements up to this node. */
        std::vector<EmptySpace> empty_spaces;

        /** For each item type, how many copies have been placed so far. */
        std::vector<ItemPos> item_number_of_copies;

        /** True if this node opened a new bin. */
        bool new_bin = false;

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
            const Parameters& parameters);

    const Instance& instance() const { return instance_; }
    const Parameters& parameters() const { return parameters_; }

    /*
     * Branching scheme methods (treesearchsolver interface).
     */

    const std::vector<Insertion>& insertions(
            const std::shared_ptr<Node>& parent) const;

    Node child_tmp(
            const std::shared_ptr<Node>& parent,
            const Insertion& insertion) const;

    std::shared_ptr<Node> child(
            const std::shared_ptr<Node>& parent,
            const Insertion& insertion) const
    {
        return std::make_shared<Node>(child_tmp(parent, insertion));
    }

    std::vector<std::shared_ptr<Node>> children(
            const std::shared_ptr<Node>& parent) const
    {
        insertions(parent);
        std::vector<std::shared_ptr<Node>> nodes(insertions_.size());
        for (Counter i = 0; i < (Counter)insertions_.size(); ++i)
            nodes[i] = std::make_shared<Node>(child_tmp(parent, insertions_[i]));
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
        double volume_load_1 = (double)node_1->item_volume / node_1->block_volume;
        double volume_load_2 = (double)node_2->item_volume / node_2->block_volume;
        if ((volume_load_1 >= 0.98) != (volume_load_2 >= 0.98))
            return volume_load_1 > volume_load_2;
        if (node_1->contact_area != node_2->contact_area)
            return node_1->contact_area > node_2->contact_area;
        return true;
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

    void compute_reachable_lengths();

    mutable NodeId node_id_ = 0;
    mutable std::vector<Insertion> insertions_;

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

inline bool BranchingSchemeMaximalSpaces::operator()(
        const std::shared_ptr<Node>& node_1,
        const std::shared_ptr<Node>& node_2) const
{
    double volume_load_1 = (double)node_1->item_volume / node_1->block_volume;
    double volume_load_2 = (double)node_2->item_volume / node_2->block_volume;
    double waste_percentage_1 = (double)(node_1->block_volume - node_1->item_volume) / node_1->block_volume;
    double waste_percentage_2 = (double)(node_2->block_volume - node_2->item_volume) / node_2->block_volume;
    double mean_item_volume_1 = (double)(node_1->item_volume) / node_1->number_of_items;
    double mean_item_volume_2 = (double)(node_2->item_volume) / node_2->number_of_items;

    if ((volume_load_1 >= 0.98) != (volume_load_2 >= 0.98))
        return volume_load_1 > volume_load_2;

    if (node_1->contact_area != node_2->contact_area)
        return node_1->contact_area > node_2->contact_area;

    if (mean_item_volume_1 != mean_item_volume_2)
        return mean_item_volume_1 > mean_item_volume_2;

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

    return node_1->id < node_2->id;
}

} // namespace box
} // namespace packingsolver
