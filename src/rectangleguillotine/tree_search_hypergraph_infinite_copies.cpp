#include "rectangleguillotine/tree_search_hypergraph_infinite_copies.hpp"

#include "packingsolver/rectangleguillotine/algorithm_formatter.hpp"
#include "packingsolver/rectangleguillotine/post_process.hpp"
#include "rectangleguillotine/solution_builder.hpp"

#include <algorithm>
#include <limits>
#include <tuple>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Dynamic programming ////////////////////////////
////////////////////////////////////////////////////////////////////////////////

namespace
{

/*
 * The normal positions along a dimension of the bin: the lengths of the
 * combinations of item dimensions (with a cut thickness between two
 * consecutive items), up to the length of the bin. Any guillotine pattern
 * can be transformed into a pattern with the same items whose cuts are at
 * normal positions (pushing its items towards the origin), so the dynamic
 * program only needs the sub-rectangles of normal dimensions (Herz 1972;
 * Christofides and Whitlock 1977).
 */
struct NormalPositions
{
    /** The normal positions, increasing; 'positions[0]' is 0. */
    std::vector<Length> positions;

    /**
     * For each length 'x' between 0 and the length of the bin, the index of
     * the largest normal position not larger than 'x'.
     */
    std::vector<Counter> floor;
};

NormalPositions compute_normal_positions(
        const std::vector<Length>& dimensions,
        Length length,
        Length cut_thickness)
{
    // A combination of 'n' items, followed by their 'n' cuts, has a length
    // which is a sum of 'dimension + cut_thickness'; its normal position is
    // this length minus the last cut.
    std::vector<bool> reached(length + cut_thickness + 1, false);
    reached[0] = true;
    for (Length x = 0; x <= length + cut_thickness; ++x) {
        if (!reached[x])
            continue;
        for (Length dimension: dimensions) {
            Length next = x + dimension + cut_thickness;
            if (next <= length + cut_thickness)
                reached[next] = true;
        }
    }
    NormalPositions normal_positions;
    normal_positions.positions.push_back(0);
    for (Length x = cut_thickness + 1; x <= length + cut_thickness; ++x)
        if (reached[x])
            normal_positions.positions.push_back(x - cut_thickness);
    normal_positions.floor.resize(length + 1);
    Counter index = 0;
    for (Length x = 0; x <= length; ++x) {
        while (index + 1 < (Counter)normal_positions.positions.size()
                && normal_positions.positions[index + 1] <= x) {
            ++index;
        }
        normal_positions.floor[x] = index;
    }
    return normal_positions;
}

/*
 * The raster points along a dimension of the bin (Scheithauer and Terno,
 * 1996): the lengths of what remains of the bin after cutting a normal
 * length, rounded down to a normal position:
 *   R = { <L - q - t> : q normal position } (and 0),
 * where <x> is the largest normal position not larger than x.
 *
 * They are the only lengths of the sub-rectangles that the dynamic program
 * needs: a sub-rectangle of a pattern can be extended until the content of
 * its neighbor (pushed to the other side), and its length is then a raster
 * point. If 'x' is a raster point and 'c' a normal position, <x - c - t> is
 * also a raster point, so the recurrence only uses raster points.
 */
NormalPositions compute_raster_points(
        const NormalPositions& normal_positions,
        Length length,
        Length cut_thickness)
{
    std::vector<bool> is_raster_point(length + 1, false);
    is_raster_point[0] = true;
    is_raster_point[normal_positions.positions[normal_positions.floor[length]]] = true;
    for (Length position: normal_positions.positions) {
        if (position == 0)
            continue;
        Length rest = length - position - cut_thickness;
        if (rest < 0)
            break;
        is_raster_point[normal_positions.positions[normal_positions.floor[rest]]] = true;
    }
    NormalPositions raster_points;
    for (Length x = 0; x <= length; ++x)
        if (is_raster_point[x])
            raster_points.positions.push_back(x);
    raster_points.floor.resize(length + 1);
    Counter index = 0;
    for (Length x = 0; x <= length; ++x) {
        while (index + 1 < (Counter)raster_points.positions.size()
                && raster_points.positions[index + 1] <= x) {
            ++index;
        }
        raster_points.floor[x] = index;
    }
    return raster_points;
}

/*
 * The table of the dynamic program: the maximum profit of a guillotine
 * packing of items with infinite copies into each sub-rectangle of the trimmed
 * area of the bin whose dimensions are raster points.
 */
struct DynamicProgram
{
    NormalPositions x;
    NormalPositions y;

    /** values[ix + x.positions.size() * iy]. */
    std::vector<Profit> values;

    Profit& at(Counter ix, Counter iy)
    {
        return values[ix + x.positions.size() * iy];
    }

    /**
     * The value of a sub-rectangle: the value of the largest sub-rectangle
     * whose dimensions are raster points it contains. Only valid for the
     * sub-rectangles of the patterns, whose dimensions are a raster point
     * plus some waste (see 'compute_raster_points').
     */
    Profit value(Length width, Length height) const
    {
        return values[x.floor[width] + x.positions.size() * y.floor[height]];
    }
};

/*
 * The raster points along x and along y of the dynamic program with the item
 * types of 'item_types_used'.
 */
std::pair<NormalPositions, NormalPositions> compute_positions(
        const Instance& instance,
        const std::vector<bool>& item_types_used)
{
    const BinType& bin_type = instance.bin_type(0);
    Length eff_width  = bin_type.rect.w - bin_type.left_trim - bin_type.right_trim;
    Length eff_height = bin_type.rect.h - bin_type.bottom_trim - bin_type.top_trim;
    Length cut_thickness = instance.parameters().cut_thickness;

    // The dimensions of the item types along each dimension of the bin.
    std::vector<Length> dimensions_x;
    std::vector<Length> dimensions_y;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        if (!item_types_used[item_type_id])
            continue;
        const ItemType& item_type = instance.item_type(item_type_id);
        dimensions_x.push_back(item_type.rect.w);
        dimensions_y.push_back(item_type.rect.h);
        if (!item_type.oriented) {
            dimensions_x.push_back(item_type.rect.h);
            dimensions_y.push_back(item_type.rect.w);
        }
    }

    return {
        compute_raster_points(
                compute_normal_positions(dimensions_x, eff_width, cut_thickness),
                eff_width,
                cut_thickness),
        compute_raster_points(
                compute_normal_positions(dimensions_y, eff_height, cut_thickness),
                eff_height,
                cut_thickness)};
}

/*
 * Recurrence (unbounded variant — item copy limits are ignored), for the
 * sub-rectangles whose dimensions w x h are raster points:
 *   dp[w][h] = max(
 *       dp[w'][h], dp[w][h'],   // w', h': the previous raster points
 *       max_{item j: wj <= w && hj <= h, or rotated} profit[j],
 *       max_{raster x, 2x+t <= w} dp[x][h] + dp[floor(w-x-t)][h],   // split along x
 *       max_{raster y, 2y+t <= h} dp[w][y] + dp[w][floor(h-y-t)]    // split along y
 *   )
 * where t is the cut thickness and floor(x) is the largest raster point not
 * larger than x. Only the item types of 'item_types_used' are considered.
 */
DynamicProgram compute_dp(
        const Instance& instance,
        const std::vector<bool>& item_types_used,
        const optimizationtools::Timer& timer,
        bool& interrupted)
{
    Length cut_thickness = instance.parameters().cut_thickness;

    DynamicProgram dp;
    std::tie(dp.x, dp.y) = compute_positions(instance, item_types_used);
    Counter nx = dp.x.positions.size();
    Counter ny = dp.y.positions.size();
    dp.values.assign(nx * ny, 0.0);

    // The breakpoints of the rows and of the columns of the table: the
    // indices 'ic' such that 'dp(ic, iy) > dp(ic - 1, iy)' (resp.
    // 'dp(ix, ic) > dp(ix, ic - 1)'). A cut only needs to be tried at a
    // breakpoint of its smaller part: if the value of the smaller part is
    // also the value of a smaller sub-rectangle, cutting at the dimension of
    // this sub-rectangle gives the same smaller part and a larger other part
    // (Russo et al., 2014).
    std::vector<std::vector<Counter>> breakpoints_x(ny);
    std::vector<std::vector<Counter>> breakpoints_y(nx);

    // The splits along y read the table by columns: a column-major copy of
    // the table, for contiguous memory accesses.
    // 'values_transposed[iy + ny * ix]' = 'dp.at(ix, iy)'.
    std::vector<Profit> values_transposed(nx * ny, 0.0);

    interrupted = false;
    for (Counter iy = 1; iy < ny; ++iy) {
        if (timer.needs_to_end()) {
            interrupted = true;
            return dp;
        }
        Length height = dp.y.positions[iy];
        for (Counter ix = 1; ix < nx; ++ix) {
            Length width = dp.x.positions[ix];
            Profit& dp_current = dp.at(ix, iy);

            // A sub-rectangle contains the smaller sub-rectangles of normal
            // dimensions, the rest being waste.
            dp_current = std::max(dp.at(ix - 1, iy), dp.at(ix, iy - 1));

            // Try placing each item type (with or without rotation). The
            // dimensions of the sub-rectangle are raster points, which may
            // not be the dimensions of the item: the rest is waste.
            for (ItemTypeId item_type_id = 0;
                    item_type_id < instance.number_of_item_types();
                    ++item_type_id) {
                if (!item_types_used[item_type_id])
                    continue;
                const ItemType& item_type = instance.item_type(item_type_id);
                if (item_type.rect.w <= width && item_type.rect.h <= height)
                    dp_current = std::max(dp_current, item_type.profit);
                if (!item_type.oriented
                        && item_type.rect.h <= width
                        && item_type.rect.w <= height)
                    dp_current = std::max(dp_current, item_type.profit);
            }

            // Try the splits along x at the breakpoints of the row. The
            // smaller part first: the other part is the larger one, rounded
            // down.
            for (Counter ic: breakpoints_x[iy]) {
                Length cut = dp.x.positions[ic];
                if (2 * cut + cut_thickness > width)
                    break;
                dp_current = std::max(dp_current,
                        dp.at(ic, iy) + dp.at(dp.x.floor[width - cut - cut_thickness], iy));
            }

            // Try the splits along y at the breakpoints of the column.
            const Profit* column = &values_transposed[ny * ix];
            for (Counter ic: breakpoints_y[ix]) {
                Length cut = dp.y.positions[ic];
                if (2 * cut + cut_thickness > height)
                    break;
                dp_current = std::max(dp_current,
                        column[ic] + column[dp.y.floor[height - cut - cut_thickness]]);
            }
            values_transposed[iy + ny * ix] = dp_current;

            if (dp_current > dp.at(ix - 1, iy))
                breakpoints_x[iy].push_back(ix);
            if (dp_current > column[iy - 1])
                breakpoints_y[ix].push_back(iy);
        }
    }

    return dp;
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Solution reconstruction ////////////////////////
////////////////////////////////////////////////////////////////////////////////

/*
 * Trace back through the DP table to build the solution tree.
 *
 * 'width' and 'height' are the real dimensions of the current sub-rectangle,
 * whose value is the value of the largest sub-rectangle whose dimensions are
 * raster points it contains. Its cuts are searched at all the raster points
 * (not only the first half, as in the dynamic program): when the
 * sub-rectangle is larger, the part with the items may be the larger one.
 *
 * x_offset / y_offset are the absolute coordinates of the bottom-left corner
 * of the current sub-rectangle (already accounting for hard trims).
 * 'pass_through': the node is a pass-through node (no cut found in the other
 * direction at the previous depth).
 */
void reconstruct_rec(
        SolutionBuilder& builder,
        const Instance& instance,
        const std::vector<bool>& item_types_used,
        const DynamicProgram& dp,
        Length cut_thickness,
        CutOrientation first_cut_orientation,
        Length width,
        Length height,
        Depth depth,
        Length x_offset,
        Length y_offset,
        bool pass_through = false)
{
    Profit dp_current = dp.value(width, height);
    if (dp_current == 0.0)
        return;

    // Determine expected cut direction at this depth.
    bool cut_is_vertical = (first_cut_orientation == CutOrientation::Vertical)
        == (depth % 2 == 1);

    // Try item placement, in the bottom left corner of the sub-rectangle, the
    // rest being waste.
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        if (!item_types_used[item_type_id])
            continue;
        const ItemType& item_type = instance.item_type(item_type_id);
        if (item_type.profit != dp_current)
            continue;
        Length item_width = -1;
        Length item_height = -1;
        if (item_type.rect.w <= width && item_type.rect.h <= height) {
            item_width = item_type.rect.w;
            item_height = item_type.rect.h;
        } else if (!item_type.oriented
                && item_type.rect.h <= width
                && item_type.rect.w <= height) {
            item_width = item_type.rect.h;
            item_height = item_type.rect.w;
        } else {
            continue;
        }
        if (item_width == width && item_height == height) {
            builder.set_last_node_item(item_type_id);
            return;
        }
        // A node with the dimension of the item along the cut direction of
        // this depth, then a node with its other dimension.
        if (cut_is_vertical) {
            builder.add_node(depth, x_offset + item_width);
            if (item_height < height)
                builder.add_node(depth + 1, y_offset + item_height);
        } else {
            builder.add_node(depth, y_offset + item_height);
            if (item_width < width)
                builder.add_node(depth + 1, x_offset + item_width);
        }
        builder.set_last_node_item(item_type_id);
        return;
    }

    if (cut_is_vertical) {
        for (Length cut: dp.x.positions) {
            if (cut == 0)
                continue;
            if (cut >= width)
                break;
            if (cut + cut_thickness >= width) {
                // The rest of the sub-rectangle is too small for a cut: it
                // is waste.
                if (dp.value(cut, height) == dp_current) {
                    builder.add_node(depth, x_offset + cut);
                    reconstruct_rec(builder, instance, item_types_used, dp,
                            cut_thickness, first_cut_orientation,
                            cut, height, depth + 1, x_offset, y_offset);
                    return;
                }
                break;
            }
            Profit left  = dp.value(cut, height);
            Profit right = dp.value(width - cut - cut_thickness, height);
            if (left + right == dp_current) {
                builder.add_node(depth, x_offset + cut);
                reconstruct_rec(builder, instance, item_types_used, dp,
                        cut_thickness, first_cut_orientation,
                        cut, height, depth + 1, x_offset, y_offset);
                builder.add_node(depth, x_offset + width);
                reconstruct_rec(builder, instance, item_types_used, dp,
                        cut_thickness, first_cut_orientation,
                        width - cut - cut_thickness, height,
                        depth + 1, x_offset + cut + cut_thickness, y_offset);
                return;
            }
        }
        if (pass_through)
            return;
        // No x-split at this depth — insert pass-through node and try y-split.
        builder.add_node(depth, x_offset + width);
        reconstruct_rec(builder, instance, item_types_used, dp,
                cut_thickness, first_cut_orientation,
                width, height, depth + 1, x_offset, y_offset, true);
    } else {
        for (Length cut: dp.y.positions) {
            if (cut == 0)
                continue;
            if (cut >= height)
                break;
            if (cut + cut_thickness >= height) {
                // The rest of the sub-rectangle is too small for a cut: it
                // is waste.
                if (dp.value(width, cut) == dp_current) {
                    builder.add_node(depth, y_offset + cut);
                    reconstruct_rec(builder, instance, item_types_used, dp,
                            cut_thickness, first_cut_orientation,
                            width, cut, depth + 1, x_offset, y_offset);
                    return;
                }
                break;
            }
            Profit bottom = dp.value(width, cut);
            Profit top    = dp.value(width, height - cut - cut_thickness);
            if (bottom + top == dp_current) {
                builder.add_node(depth, y_offset + cut);
                reconstruct_rec(builder, instance, item_types_used, dp,
                        cut_thickness, first_cut_orientation,
                        width, cut, depth + 1, x_offset, y_offset);
                builder.add_node(depth, y_offset + height);
                reconstruct_rec(builder, instance, item_types_used, dp,
                        cut_thickness, first_cut_orientation,
                        width, height - cut - cut_thickness,
                        depth + 1, x_offset, y_offset + cut + cut_thickness);
                return;
            }
        }
        if (pass_through)
            return;
        // No y-split at this depth — insert pass-through node and try x-split.
        builder.add_node(depth, y_offset + height);
        reconstruct_rec(builder, instance, item_types_used, dp,
                cut_thickness, first_cut_orientation,
                width, height, depth + 1, x_offset, y_offset, true);
    }
}

Solution reconstruct_solution(
        const Instance& instance,
        const std::vector<bool>& item_types_used,
        const DynamicProgram& dp)
{
    const BinType& bin_type = instance.bin_type(0);
    Length eff_width  = bin_type.rect.w - bin_type.left_trim - bin_type.right_trim;
    Length eff_height = bin_type.rect.h - bin_type.bottom_trim - bin_type.top_trim;

    // Hard trims physically shift the usable area; soft trims do not.
    Length x_offset = (bin_type.left_trim_type  == TrimType::Hard)? bin_type.left_trim:  0;
    Length y_offset = (bin_type.bottom_trim_type == TrimType::Hard)? bin_type.bottom_trim: 0;

    CutOrientation first_cut_orientation
        = instance.parameters().first_stage_orientation;
    if (first_cut_orientation == CutOrientation::Any)
        first_cut_orientation = CutOrientation::Vertical;

    Length cut_thickness = instance.parameters().cut_thickness;

    SolutionBuilder builder(instance);
    builder.add_bin(0, 1, first_cut_orientation);

    if (dp.value(eff_width, eff_height) > 0.0) {
        builder.add_node(1, x_offset + eff_width);
        reconstruct_rec(builder, instance, item_types_used, dp,
                cut_thickness, first_cut_orientation,
                eff_width, eff_height, 2, x_offset, y_offset);
    }

    return builder.build();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Main function ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

const TreeSearchHypergraphInfiniteCopiesOutput
packingsolver::rectangleguillotine::tree_search_hypergraph_infinite_copies(
        const Instance& instance,
        const TreeSearchHypergraphInfiniteCopiesParameters& parameters)
{
    TreeSearchHypergraphInfiniteCopiesOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    const BinType& bin_type = instance.bin_type(0);
    Area eff_area = (Area)(bin_type.rect.w - bin_type.left_trim - bin_type.right_trim)
        * (bin_type.rect.h - bin_type.bottom_trim - bin_type.top_trim);

    // The bound: all the item types, ignoring their copies. It is also valid
    // when the other constraints of the instance (defects, stacks, numbers of
    // stages, distances between cuts...) aren't satisfied by the patterns of
    // the dynamic program: they only restrict the solutions further.
    bool interrupted = false;

    // First, the dynamic programs with only the item types of largest profit
    // per unit area (see 'TreeSearchHypergraphInfiniteCopiesParameters::
    // maximum_array_size').
    std::vector<ItemTypeId> sorted_item_types;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        if (instance.item_type(item_type_id).profit > 0)
            sorted_item_types.push_back(item_type_id);
    }
    std::sort(
            sorted_item_types.begin(),
            sorted_item_types.end(),
            [&instance](ItemTypeId item_type_id_1, ItemTypeId item_type_id_2)
            {
                const ItemType& item_type_1 = instance.item_type(item_type_id_1);
                const ItemType& item_type_2 = instance.item_type(item_type_id_2);
                return item_type_1.profit * item_type_2.area()
                    > item_type_2.profit * item_type_1.area();
            });
    // The largest number of these item types whose array fits in a maximum
    // size. 'array_sizes[k]': the size of the array with the 'k + 1' first
    // item types, computed as needed.
    std::vector<Counter> array_sizes;
    auto number_of_item_types_fitting = [&instance, &sorted_item_types, &array_sizes](
            Counter maximum_array_size)
    {
        ItemTypeId number_of_item_types = 0;
        while (number_of_item_types < (ItemTypeId)sorted_item_types.size()) {
            if (number_of_item_types == (ItemTypeId)array_sizes.size()) {
                std::vector<bool> item_types_used(instance.number_of_item_types(), false);
                for (ItemTypeId pos = 0; pos <= number_of_item_types; ++pos)
                    item_types_used[sorted_item_types[pos]] = true;
                auto positions = compute_positions(instance, item_types_used);
                array_sizes.push_back(
                        (Counter)positions.first.positions.size()
                        * (Counter)positions.second.positions.size());
            }
            if (array_sizes[number_of_item_types] > maximum_array_size)
                break;
            number_of_item_types++;
        }
        return number_of_item_types;
    };
    // Iterative scheme: maximum sizes 2^14, 2^16, 2^18..., up to the maximum
    // size of the parameters, then all the item types if they fit. With a
    // knapsack goal, the first dynamic programs may already reach it.
    ItemTypeId number_of_sorted_item_types = sorted_item_types.size();
    std::vector<ItemTypeId> numbers_of_item_types;
    bool all = true;
    // With a maximum size, only two dynamic programs: a quarter of the
    // maximum size, then the maximum size.
    Counter first_maximum_array_size = (parameters.maximum_array_size >= 0)?
        parameters.maximum_array_size / 4:
        (1 << 14);
    for (Counter maximum_array_size = first_maximum_array_size;;
            maximum_array_size *= 4) {
        bool last = false;
        if (parameters.maximum_array_size >= 0
                && maximum_array_size >= parameters.maximum_array_size) {
            maximum_array_size = parameters.maximum_array_size;
            last = true;
        }
        ItemTypeId number_of_item_types = number_of_item_types_fitting(maximum_array_size);
        if (number_of_item_types == number_of_sorted_item_types)
            break;
        if (number_of_item_types > 0
                && (numbers_of_item_types.empty()
                    || number_of_item_types > numbers_of_item_types.back())) {
            numbers_of_item_types.push_back(number_of_item_types);
        }
        if (last) {
            all = false;
            break;
        }
    }
    for (ItemTypeId number_of_item_types: numbers_of_item_types) {
        std::vector<bool> item_types_used(instance.number_of_item_types(), false);
        for (ItemTypeId pos = 0; pos < number_of_item_types; ++pos)
            item_types_used[sorted_item_types[pos]] = true;
        DynamicProgram dp = compute_dp(instance, item_types_used, parameters.timer, interrupted);
        if (interrupted) {
            algorithm_formatter.end();
            return output;
        }
        Solution solution = reconstruct_solution(instance, item_types_used, dp);
        solution = minimize_number_of_stages(solution);
        // The copies of the item types may not be satisfied.
        if (solution.feasible())
            algorithm_formatter.update_solution(solution, "DP");
        if (algorithm_formatter.end_boolean()) {
            algorithm_formatter.end();
            return output;
        }
    }
    if (!all) {
        algorithm_formatter.end();
        return output;
    }

    std::vector<bool> all_item_types(instance.number_of_item_types(), true);
    DynamicProgram dp = compute_dp(instance, all_item_types, parameters.timer, interrupted);
    if (interrupted) {
        algorithm_formatter.end();
        return output;
    }
    Length eff_width = bin_type.rect.w - bin_type.left_trim - bin_type.right_trim;
    Length eff_height = bin_type.rect.h - bin_type.bottom_trim - bin_type.top_trim;
    if (instance.objective() == Objective::Knapsack)
        algorithm_formatter.update_knapsack_bound(dp.value(eff_width, eff_height));

    // The solution of the bound. If it satisfies the copies of the item
    // types (and the other constraints of the instance: defects, stacks,
    // numbers of stages...), it is optimal.
    Solution solution = reconstruct_solution(instance, all_item_types, dp);
    solution = minimize_number_of_stages(solution);
    if (solution.feasible()) {
        algorithm_formatter.update_solution(solution, "DP");
        algorithm_formatter.end();
        return output;
    }

    // Otherwise, a feasible solution: only the item types whose copies can't
    // run out in a bin (more copies than the number of items of this type
    // which fit in it), so that the copies of the solution are always
    // satisfied.
    std::vector<bool> infinite_item_types(instance.number_of_item_types(), false);
    bool all_infinite = true;
    bool none_infinite = true;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        infinite_item_types[item_type_id] = (item_type.copies >= eff_area / item_type.area());
        if (infinite_item_types[item_type_id])
            none_infinite = false;
        else
            all_infinite = false;
    }
    // If all the item types are "infinite", the solution of the bound
    // satisfies the copies: it is infeasible because of another constraint,
    // and so would be the solution of the same dynamic program.
    if (none_infinite || all_infinite) {
        algorithm_formatter.end();
        return output;
    }
    DynamicProgram dp_infinite = compute_dp(instance, infinite_item_types, parameters.timer, interrupted);
    if (interrupted) {
        algorithm_formatter.end();
        return output;
    }
    solution = reconstruct_solution(instance, infinite_item_types, dp_infinite);
    solution = minimize_number_of_stages(solution);
    if (solution.feasible())
        algorithm_formatter.update_solution(solution, "DP");

    algorithm_formatter.end();
    return output;
}
