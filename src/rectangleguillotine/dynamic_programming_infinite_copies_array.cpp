#include "rectangleguillotine/dynamic_programming_infinite_copies_array.hpp"

#include "packingsolver/rectangleguillotine/algorithm_formatter.hpp"
#include "packingsolver/rectangleguillotine/post_process.hpp"
#include "rectangleguillotine/solution_builder.hpp"

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Dynamic programming ////////////////////////////
////////////////////////////////////////////////////////////////////////////////

namespace
{

/*
 * For each box (width x height) within the trimmed area, dp_values[width +
 * (eff_width + 1) * height] stores the maximum profit achievable by a
 * guillotine packing of items with infinite copies into that rectangle, where
 * eff_width = bin_width - left_trim - right_trim and
 * eff_height = bin_height - bottom_trim - top_trim.
 *
 * Items are only placed in boxes whose dimensions exactly match (with or
 * without rotation), so that solution reconstruction can call set_last_node_item
 * directly without further cuts.
 *
 * Recurrence (unbounded variant — item copy limits are ignored):
 *   dp[w][h] = max(
 *       max_{item j: w==wj && h==hj, or rotated} profit[j],
 *       max_{1 <= x, 2x+t <= w} dp[x][h] + dp[w-x-t][h],   // split along x
 *       max_{1 <= y, 2y+t <= h} dp[w][y] + dp[w][h-y-t]    // split along y
 *   )
 * where t is the cut thickness.
 */
std::vector<Profit> compute_dp(const Instance& instance)
{
    const BinType& bin_type = instance.bin_type(0);
    Length eff_width  = bin_type.rect.w - bin_type.left_trim - bin_type.right_trim;
    Length eff_height = bin_type.rect.h - bin_type.bottom_trim - bin_type.top_trim;
    Length cut_thickness = instance.parameters().cut_thickness;

    std::vector<Profit> dp_values((eff_width + 1) * (eff_height + 1), 0.0);

    auto dp = [&](Length width, Length height) -> Profit& {
        return dp_values[width + (eff_width + 1) * height];
    };

    for (Length width = 1; width <= eff_width; ++width) {
        for (Length height = 1; height <= eff_height; ++height) {
            Profit& dp_current = dp(width, height);

            // Try placing each item type (exact fit, with or without rotation).
            for (ItemTypeId item_type_id = 0;
                    item_type_id < instance.number_of_item_types();
                    ++item_type_id) {
                const ItemType& item_type = instance.item_type(item_type_id);
                if (item_type.rect.w == width && item_type.rect.h == height)
                    dp_current = std::max(dp_current, item_type.profit);
                if (!item_type.oriented
                        && item_type.rect.h == width
                        && item_type.rect.w == height)
                    dp_current = std::max(dp_current, item_type.profit);
            }

            // Try all splits along x.
            for (Length cut = 1; 2 * cut + cut_thickness <= width; ++cut)
                dp_current = std::max(dp_current,
                        dp(cut, height) + dp(width - cut - cut_thickness, height));

            // Try all splits along y.
            for (Length cut = 1; 2 * cut + cut_thickness <= height; ++cut)
                dp_current = std::max(dp_current,
                        dp(width, cut) + dp(width, height - cut - cut_thickness));
        }
    }

    return dp_values;
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Solution reconstruction ////////////////////////
////////////////////////////////////////////////////////////////////////////////

/*
 * Trace back through the DP table to build the solution tree.
 *
 * dp_width is the stride of dp_values (= eff_width).
 * x_offset / y_offset are the absolute coordinates of the bottom-left corner
 * of the current sub-rectangle (already accounting for hard trims).
 */
void reconstruct_rec(
        SolutionBuilder& builder,
        const Instance& instance,
        const std::vector<Profit>& dp_values,
        Length dp_width,
        Length cut_thickness,
        CutOrientation first_cut_orientation,
        Length width,
        Length height,
        Depth depth,
        Length x_offset,
        Length y_offset)
{
    Profit dp_current = dp_values[width + (dp_width + 1) * height];
    if (dp_current == 0.0)
        return;

    // Try exact item placement.
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        bool fits_normal = (item_type.rect.w == width && item_type.rect.h == height);
        bool fits_rotated = (!item_type.oriented
                && item_type.rect.h == width
                && item_type.rect.w == height);
        if ((fits_normal || fits_rotated) && item_type.profit == dp_current) {
            builder.set_last_node_item(item_type_id);
            return;
        }
    }

    // Determine expected cut direction at this depth.
    bool cut_is_vertical = (first_cut_orientation == CutOrientation::Vertical)
        == (depth % 2 == 1);

    if (cut_is_vertical) {
        for (Length cut = 1; 2 * cut + cut_thickness <= width; ++cut) {
            Profit left  = dp_values[cut + (dp_width + 1) * height];
            Profit right = dp_values[(width - cut - cut_thickness) + (dp_width + 1) * height];
            if (left + right == dp_current) {
                builder.add_node(depth, x_offset + cut);
                reconstruct_rec(builder, instance, dp_values, dp_width,
                        cut_thickness, first_cut_orientation,
                        cut, height, depth + 1, x_offset, y_offset);
                builder.add_node(depth, x_offset + width);
                reconstruct_rec(builder, instance, dp_values, dp_width,
                        cut_thickness, first_cut_orientation,
                        width - cut - cut_thickness, height,
                        depth + 1, x_offset + cut + cut_thickness, y_offset);
                return;
            }
        }
        // No x-split at this depth — insert pass-through node and try y-split.
        builder.add_node(depth, x_offset + width);
        reconstruct_rec(builder, instance, dp_values, dp_width,
                cut_thickness, first_cut_orientation,
                width, height, depth + 1, x_offset, y_offset);
    } else {
        for (Length cut = 1; 2 * cut + cut_thickness <= height; ++cut) {
            Profit bottom = dp_values[width + (dp_width + 1) * cut];
            Profit top    = dp_values[width + (dp_width + 1) * (height - cut - cut_thickness)];
            if (bottom + top == dp_current) {
                builder.add_node(depth, y_offset + cut);
                reconstruct_rec(builder, instance, dp_values, dp_width,
                        cut_thickness, first_cut_orientation,
                        width, cut, depth + 1, x_offset, y_offset);
                builder.add_node(depth, y_offset + height);
                reconstruct_rec(builder, instance, dp_values, dp_width,
                        cut_thickness, first_cut_orientation,
                        width, height - cut - cut_thickness,
                        depth + 1, x_offset, y_offset + cut + cut_thickness);
                return;
            }
        }
        // No y-split at this depth — insert pass-through node and try x-split.
        builder.add_node(depth, y_offset + height);
        reconstruct_rec(builder, instance, dp_values, dp_width,
                cut_thickness, first_cut_orientation,
                width, height, depth + 1, x_offset, y_offset);
    }
}

Solution reconstruct_solution(
        const Instance& instance,
        const std::vector<Profit>& dp_values)
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

    if (dp_values[eff_width + (eff_width + 1) * eff_height] > 0.0) {
        builder.add_node(1, x_offset + eff_width);
        reconstruct_rec(builder, instance, dp_values, eff_width,
                cut_thickness, first_cut_orientation,
                eff_width, eff_height, 2, x_offset, y_offset);
    }

    return builder.build();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Main function ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

const DynamicProgrammingInfiniteCopiesArrayOutput
packingsolver::rectangleguillotine::dynamic_programming_infinite_copies_array(
        const Instance& instance,
        const DynamicProgrammingInfiniteCopiesArrayParameters& parameters)
{
    DynamicProgrammingInfiniteCopiesArrayOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    output.dp_values = compute_dp(instance);

    Solution solution = reconstruct_solution(instance, output.dp_values);
    solution = minimize_number_of_stages(solution);
    algorithm_formatter.update_solution(solution, "DP");

    algorithm_formatter.end();
    return output;
}
