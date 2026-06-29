#include "packingsolver/rectangleguillotine/post_process.hpp"

#include "rectangleguillotine/solution_builder.hpp"

#include <algorithm>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

GroupIdenticalBinsOutput packingsolver::rectangleguillotine::group_identical_bins(
        const Solution& solution)
{
    GroupIdenticalBinsOutput output(solution.instance());
    Solution grouped_solution = packingsolver::group_identical_bins(solution);
    output.solution_pool.add(grouped_solution);
    return output;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////// build_guillotine_solutions //////////////////////////
////////////////////////////////////////////////////////////////////////////////

namespace
{

/**
 * One group of items that cannot be separated by a cut in the current
 * direction.  cut_position is the right (for vertical) or top (for horizontal)
 * edge of the group — the value passed to SolutionBuilder::add_node.
 */
struct CutGroup
{
    Length cut_position;
    std::vector<ItemPlacement> items;
};

/**
 * Partition items into consecutive non-overlapping groups in the given
 * direction.
 *
 * For vertical groups (is_vertical = true): groups by x-connectivity.
 * Items are in the same group when their x-intervals overlap or are closer
 * than cut_thickness apart; otherwise a cut between them is possible.
 * cut_position of each group = max(item.r) of items in the group.
 *
 * For horizontal groups: same logic along y, cut_position = max(item.t).
 */
static std::vector<CutGroup> find_groups(
        const std::vector<ItemPlacement>& items,
        bool is_vertical,
        Length cut_thickness)
{
    std::vector<ItemPlacement> sorted_items = items;
    if (is_vertical) {
        std::sort(sorted_items.begin(), sorted_items.end(),
                [](const ItemPlacement& item_1, const ItemPlacement& item_2) {
                    return item_1.l < item_2.l;
                });
    } else {
        std::sort(sorted_items.begin(), sorted_items.end(),
                [](const ItemPlacement& item_1, const ItemPlacement& item_2) {
                    return item_1.b < item_2.b;
                });
    }

    std::vector<CutGroup> groups;
    CutGroup current_group;
    current_group.cut_position = -1;

    for (const ItemPlacement& item: sorted_items) {
        Length item_start = is_vertical? item.l: item.b;
        Length item_end = is_vertical? item.r: item.t;

        if (current_group.cut_position < 0) {
            current_group.items.push_back(item);
            current_group.cut_position = item_end;
        } else if (item_start >= current_group.cut_position + cut_thickness) {
            groups.push_back(std::move(current_group));
            current_group = CutGroup();
            current_group.items.push_back(item);
            current_group.cut_position = item_end;
        } else {
            current_group.items.push_back(item);
            if (item_end > current_group.cut_position)
                current_group.cut_position = item_end;
        }
    }

    if (!current_group.items.empty())
        groups.push_back(std::move(current_group));

    return groups;
}

/**
 * Recursively build the guillotine cut tree for a region [l, r] x [b, t]
 * that already has a node added to the builder (i.e. the last node in the
 * builder is the parent at depth - 1).
 *
 * Items are the subset of placements that belong to this region.
 * depth is the depth of the children to add (1 for first-level cuts, etc.).
 *
 * Returns false if the items cannot be arranged as guillotine cuts.
 */
static bool build_region(
        const Instance& instance,
        Length l, Length r, Length b, Length t,
        const std::vector<ItemPlacement>& items,
        Depth depth,
        CutOrientation first_cut_orientation,
        SolutionBuilder& builder)
{
    // Base case: no items — waste region, nothing to add.
    if (items.empty())
        return true;

    // Base case: single item exactly filling the region.
    // Only when depth > 1: at depth == 1 no cut node has been added yet, so
    // set_last_node_item would target the bin node rather than a cut node.
    // Falling through to find_groups lets the recursive call at depth 2 apply
    // the base case against the correct (depth-1) node.
    if (depth > 1 && items.size() == 1) {
        const ItemPlacement& item = items[0];
        if (item.l == l && item.r == r && item.b == b && item.t == t) {
            builder.set_last_node_item(item.item_type_id);
            return true;
        }
    }

    const Length cut_thickness = instance.parameters().cut_thickness;

    // Determine cut direction at this depth.
    bool is_vertical = (first_cut_orientation == CutOrientation::Vertical)?
        (depth % 2 == 1):
        (depth % 2 == 0);

    std::vector<CutGroup> groups = find_groups(items, is_vertical, cut_thickness);

    // prev_pos tracks the start of the current sub-region in the cut direction.
    Length prev_pos = is_vertical? l: b;

    for (const CutGroup& group: groups) {
        // group.items is sorted, so its first element starts at the
        // smallest coordinate in the group, i.e. the start of the group's
        // items in the cut direction.
        Length group_start = is_vertical?
            group.items.front().l: group.items.front().b;

        // If the group's items start strictly after prev_pos, the
        // sub-region begins with a waste area (e.g. the sub-plate starts
        // with a waste). Close that gap with an explicit waste node before
        // adding the group's own node, otherwise the recursive call below
        // would receive a sub-region that does not match its items, which
        // can recurse indefinitely.
        if (group_start - cut_thickness > prev_pos) {
            builder.add_node(depth, group_start - cut_thickness);
            prev_pos = group_start;
        }

        builder.add_node(depth, group.cut_position);

        Length sub_l = is_vertical? prev_pos: l;
        Length sub_r = is_vertical? group.cut_position: r;
        Length sub_b = is_vertical? b: prev_pos;
        Length sub_t = is_vertical? t: group.cut_position;

        if (!build_region(instance, sub_l, sub_r, sub_b, sub_t,
                          group.items, depth + 1, first_cut_orientation, builder))
            return false;

        prev_pos = group.cut_position + cut_thickness;
    }

    return true;
}

/**
 * Try to build a single Solution for the given bin and item placements using
 * the specified first cut orientation.
 *
 * Returns false (and leaves result unchanged) if building fails.
 */
static bool try_build(
        const Instance& instance,
        BinTypeId bin_type_id,
        const std::vector<ItemPlacement>& item_placements,
        CutOrientation first_cut_orientation,
        Solution& result)
{
    const BinType& bin_type = instance.bin_type(bin_type_id);

    // Determine the usable region (accounting for hard trims only; soft trims
    // are not physically cut so they are left for build() to handle as waste).
    Length l = (bin_type.left_trim_type == TrimType::Hard)? bin_type.left_trim: 0;
    Length r = bin_type.rect.w
        - ((bin_type.right_trim_type == TrimType::Hard)? bin_type.right_trim: 0);
    Length b = (bin_type.bottom_trim_type == TrimType::Hard)? bin_type.bottom_trim: 0;
    Length t = bin_type.rect.h
        - ((bin_type.top_trim_type == TrimType::Hard)? bin_type.top_trim: 0);

    SolutionBuilder builder(instance);
    builder.add_bin(bin_type_id, 1, first_cut_orientation);

    if (!item_placements.empty()) {
        if (!build_region(instance, l, r, b, t, item_placements,
                          1, first_cut_orientation, builder))
            return false;
    }

    result = builder.build();
    return true;
}

}  // namespace

Solution packingsolver::rectangleguillotine::build_guillotine_solution(
        const Instance& instance,
        BinTypeId bin_type_id,
        const std::vector<ItemPlacement>& item_placements)
{
    std::vector<Solution> solutions = build_guillotine_solutions(
            instance, bin_type_id, item_placements);

    if (solutions.empty())
        return Solution(instance);

    if (solutions.size() == 1)
        return solutions[0];

    // Both orientations produced a valid solution; keep the one with fewer stages.
    Counter stages_0 = solutions[0].number_of_stages();
    Counter stages_1 = solutions[1].number_of_stages();
    return (stages_0 <= stages_1)? solutions[0]: solutions[1];
}

Solution packingsolver::rectangleguillotine::minimize_number_of_stages(
        const Solution& solution)
{
    const Instance& instance = solution.instance();
    Solution result(instance);

    for (BinPos bin_pos = 0;
            bin_pos < solution.number_of_different_bins();
            ++bin_pos) {
        const SolutionBin& bin = solution.bin(bin_pos);

        // Extract item placements from this bin.
        std::vector<ItemPlacement> item_placements;
        for (const SolutionNode& node: bin.nodes) {
            if (node.f != -1 && node.item_type_id >= 0) {
                ItemPlacement placement;
                placement.item_type_id = node.item_type_id;
                placement.l = node.l;
                placement.r = node.r;
                placement.b = node.b;
                placement.t = node.t;
                item_placements.push_back(placement);
            }
        }

        Solution bin_solution = build_guillotine_solution(
                instance, bin.bin_type_id, item_placements);

        if (bin_solution.number_of_different_bins() > 0) {
            result.append(bin_solution, 0, bin.copies);
        } else {
            result.append(solution, bin_pos, bin.copies);
        }
    }

    return result;
}

std::vector<Solution> packingsolver::rectangleguillotine::build_guillotine_solutions(
        const Instance& instance,
        BinTypeId bin_type_id,
        const std::vector<ItemPlacement>& item_placements)
{
    std::vector<Solution> solutions;
    CutOrientation first_stage_orientation = instance.parameters().first_stage_orientation;

    auto try_orientation = [&](CutOrientation orientation) {
        Solution solution(instance);
        try {
            if (try_build(instance, bin_type_id, item_placements,
                          orientation, solution))
                solutions.push_back(std::move(solution));
        } catch (const std::exception&) {
            // Building failed (e.g. item dimensions don't match any node).
        }
    };

    if (first_stage_orientation == CutOrientation::Vertical) {
        try_orientation(CutOrientation::Vertical);
    } else if (first_stage_orientation == CutOrientation::Horizontal) {
        try_orientation(CutOrientation::Horizontal);
    } else {
        try_orientation(CutOrientation::Vertical);
        try_orientation(CutOrientation::Horizontal);
    }

    return solutions;
}

namespace
{

/**
 * Lengths of the item content of a node, computed bottom-up from leaf items.
 *
 * length_1 is the dimension in the direction of the cut that produced this
 * node (the varying dimension among siblings).  length_2 is the perpendicular
 * dimension.
 *
 * For a leaf item, both dimensions equal the item's geometric extent.
 * For a waste/residual leaf, both are zero.
 * For an internal cut node, the dimension along which its children are
 * arranged equals the sum of the children's extents in that direction; the
 * perpendicular dimension equals the maximum.
 */
struct NodeLengths
{
    Length length_1 = 0;
    Length length_2 = 0;
};

/**
 * Compute length_1 and length_2 for every node in a bin, bottom-up.
 *
 * Traverses nodes from last to first (children before parents in DFS
 * pre-order) and accumulates item-content extents into each parent:
 *   - in the cut direction: sum
 *   - perpendicular: max
 *
 * Returns a vector indexed by node_id within the bin.
 */
std::vector<NodeLengths> compute_node_lengths(
        const Solution& solution,
        BinPos bin_pos)
{
    const SolutionBin& bin = solution.bin(bin_pos);
    const std::vector<SolutionNode>& nodes = bin.nodes;
    std::vector<NodeLengths> result(nodes.size());
    CutOrientation first_cut_orientation = bin.first_cut_orientation;

    for (int node_id = (int)nodes.size() - 1; node_id >= 0; --node_id) {
        const SolutionNode& node = nodes[node_id];

        bool is_vertical = (first_cut_orientation == CutOrientation::Vertical)?
            (node.d % 2 == 1):
            (node.d % 2 == 0);

        if (node.item_type_id >= 0) {
            if (is_vertical) {
                result[node_id].length_1 = node.r - node.l;
                result[node_id].length_2 = node.t - node.b;
            } else {
                result[node_id].length_1 = node.t - node.b;
                result[node_id].length_2 = node.r - node.l;
            }
        } else if (!node.children.empty()) {
            for (NodeId child_id: node.children) {
                result[node_id].length_1 = std::max(
                        result[node_id].length_1,
                        result[child_id].length_2);
                result[node_id].length_2 += result[child_id].length_1;
            }
        }
    }

    return result;
}

/**
 * Sort the children of each node by item-content lengths (descending).
 *
 * Only reorders the children lists — no coordinate updates.  Waste and
 * residual children (item_type_id == -1 or -3) are dropped; build() will
 * re-add trailing waste automatically.
 */
static void sort_node_subtrees(
        NodeId node_id,
        const std::vector<SolutionNode>& nodes,
        std::vector<std::vector<NodeId>>& children,
        const std::vector<NodeLengths>& node_lengths,
        bool mode)
{
    std::vector<NodeId>& kids = children[node_id];

    std::vector<NodeId> content_kids;
    for (NodeId kid: kids) {
        ItemTypeId item_type_id = nodes[kid].item_type_id;
        if (item_type_id != -1 && item_type_id != -3)
            content_kids.push_back(kid);
    }

    if ((int)content_kids.size() >= 2) {
        std::stable_sort(content_kids.begin(), content_kids.end(),
                [&](NodeId a, NodeId b) {
                    const NodeLengths& la = node_lengths[a];
                    const NodeLengths& lb = node_lengths[b];
                    if (mode) {
                        if (la.length_1 != lb.length_1)
                            return la.length_1 > lb.length_1;
                        return la.length_2 > lb.length_2;
                    } else {
                        if (la.length_2 != lb.length_2)
                            return la.length_2 > lb.length_2;
                        return la.length_1 > lb.length_1;
                    }
                });
    }

    kids = content_kids;

    for (NodeId kid: kids)
        sort_node_subtrees(kid, nodes, children, node_lengths, mode);
}

/**
 * Replay sorted children into SolutionBuilder, computing absolute cut
 * positions from the original node spans without touching node coordinates.
 *
 * cut_start   — absolute start of the current group in the cut direction.
 * perp_start  — absolute start in the perpendicular direction (inherited by
 *               all siblings in this group from their common parent).
 *
 * For each sibling, cut_pos = cut_start + span (using the original node
 * width or height).  Children of that sibling are replayed with
 *   new cut_start  = perp_start  (the current group's perp start)
 *   new perp_start = current cut position before this sibling was placed.
 */
static void replay_group(
        const std::vector<NodeId>& siblings,
        Depth depth,
        Length cut_start,
        Length perp_start,
        const std::vector<SolutionNode>& nodes,
        const std::vector<std::vector<NodeId>>& children,
        CutOrientation first_cut_orientation,
        Length cut_thickness,
        SolutionBuilder& solution_builder)
{
    bool is_vertical_cut = (first_cut_orientation == CutOrientation::Vertical)?
        (depth % 2 == 1):
        (depth % 2 == 0);

    Length current = cut_start;
    for (NodeId kid: siblings) {
        const SolutionNode& node = nodes[kid];
        Length span = is_vertical_cut?
            (node.r - node.l):
            (node.t - node.b);
        Length cut_pos = current + span;
        solution_builder.add_node(depth, cut_pos);
        if (node.item_type_id >= 0)
            solution_builder.set_last_node_item(node.item_type_id);
        // Children are at depth+1 in the perpendicular direction.
        // Their cut_start = this group's perp_start.
        // Their perp_start = current (where this sibling began in cut dir).
        replay_group(
                children[kid],
                depth + 1,
                perp_start,
                current,
                nodes,
                children,
                first_cut_orientation,
                cut_thickness,
                solution_builder);
        current = cut_pos + cut_thickness;
    }
}

}

Solution packingsolver::rectangleguillotine::sort_subplates(
        const Solution& solution,
        int mode)
{
    const Instance& instance = solution.instance();
    const Length cut_thickness = instance.parameters().cut_thickness;
    Solution result(instance);

    for (BinPos bin_pos = 0;
            bin_pos < solution.number_of_different_bins();
            ++bin_pos) {
        const SolutionBin& bin = solution.bin(bin_pos);

        // Use original nodes read-only; reconstruct children lists from f.
        const std::vector<SolutionNode>& nodes = bin.nodes;
        std::vector<std::vector<NodeId>> children(nodes.size());
        for (NodeId node_id = 0;
                node_id < (NodeId)nodes.size();
                ++node_id) {
            if (nodes[node_id].f >= 0)
                children[nodes[node_id].f].push_back(node_id);
        }

        // Compute item-content lengths and sort children lists (no coord updates).
        std::vector<NodeLengths> node_lengths = compute_node_lengths(solution, bin_pos);
        sort_node_subtrees(0, nodes, children, node_lengths, mode);

        // Find effective root: if trims exist it is the inner d=0 node (f>=0),
        // otherwise it is node 0.  Its l/b give the initial cut/perp starts.
        NodeId effective_root_id = 0;
        for (NodeId node_id = 0;
                node_id < (NodeId)nodes.size();
                ++node_id) {
            if (nodes[node_id].d == 0 && nodes[node_id].f >= 0) {
                effective_root_id = node_id;
                break;
            }
        }
        const SolutionNode& eff_root = nodes[effective_root_id];
        bool first_is_vertical = (bin.first_cut_orientation == CutOrientation::Vertical);
        Length cut_start  = first_is_vertical? eff_root.l: eff_root.b;
        Length perp_start = first_is_vertical? eff_root.b: eff_root.l;

        // Replay sorted children into a new solution, computing absolute
        // cut positions from the original node spans on the fly.
        SolutionBuilder solution_builder(instance);
        solution_builder.add_bin(bin.bin_type_id, 1, bin.first_cut_orientation);
        replay_group(
                children[effective_root_id],
                1,
                cut_start, perp_start,
                nodes,
                children,
                bin.first_cut_orientation,
                cut_thickness,
                solution_builder);
        result.append(solution_builder.build(), 0, bin.copies);
    }

    return result;
}
