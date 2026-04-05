#include "packingsolver/irregular/post_process.hpp"

#include "irregular/linear_programming.hpp"

using namespace packingsolver;
using namespace packingsolver::irregular;

/**
 * Build a solution in which each bin is independently shifted towards the
 * target corner (controlled by x_weight / y_weight).  If the shift keeps a
 * bin feasible (no items outside the bin, no item/border or item/defect
 * overlaps) the shifted bin is used; otherwise the original bin is kept.
 */
static Solution compute_shifted_solution(
        const Solution& solution,
        double x_weight,
        double y_weight)
{
    const Instance& instance = solution.instance();
    Solution result(instance);

    for (BinPos bin_pos = 0;
            bin_pos < solution.number_of_different_bins();
            ++bin_pos) {
        const SolutionBin& solution_bin = solution.bin(bin_pos);
        const BinType& bin_type = instance.bin_type(solution_bin.bin_type_id);

        // Compute the shift that pushes items against the target border.
        LengthDbl dx = 0.0;
        LengthDbl dy = 0.0;
        if (x_weight > 0) {
            dx = bin_type.aabb.x_min - solution_bin.x_min;
        } else if (x_weight < 0) {
            dx = bin_type.aabb.x_max - solution_bin.x_max;
        }
        if (y_weight > 0) {
            dy = bin_type.aabb.y_min - solution_bin.y_min;
        } else if (y_weight < 0) {
            dy = bin_type.aabb.y_max - solution_bin.y_max;
        }
        //std::cout << "dx " << dx << " dy " << dy << std::endl;

        if (!shape::strictly_greater(dx, 0.0) && !shape::strictly_greater(dy, 0.0)) {
            //std::cout << "skip" << std::endl;
            result.append(solution, bin_pos, solution_bin.copies);
            continue;
        }

        // Build the candidate shifted bin.
        Solution shifted_bin(instance);
        BinPos tmp_pos = shifted_bin.add_bin(
                solution_bin.bin_type_id,
                solution_bin.copies);
        for (const SolutionItem& item: solution_bin.items) {
            Point bl_corner = item.bl_corner;
            bl_corner.x += dx;
            bl_corner.y += dy;
            shifted_bin.add_item(
                    tmp_pos,
                    item.item_type_id,
                    bl_corner,
                    item.angle,
                    item.mirror);
        }

        // Check feasibility: items must not leave the bin or overlap defects/borders.
        const Solution::OverlappingItems overlapping
                = shifted_bin.compute_overlapping_items(0);
        bool feasible = overlapping.item_border_pairs.empty()
                && overlapping.item_defect_pairs.empty()
                && overlapping.items_outside_bin.empty();

        if (feasible) {
            result.append(shifted_bin, 0, solution_bin.copies);
        } else {
            result.append(solution, bin_pos, solution_bin.copies);
        }
    }

    return result;
}

AnchorOutput packingsolver::irregular::anchor(
        const Solution& solution,
        double x_weight,
        double y_weight,
        const AnchorParameters& parameters)
{
    AnchorOutput output(solution.instance());

    // Before running the LP anchor, try to shift each bin rigidly to the
    // target corner; use the result as a warm start.
    Solution shifted_solution = compute_shifted_solution(solution, x_weight, y_weight);

    LinearProgrammingAnchorParameters linear_programming_anchor_parameters;
    linear_programming_anchor_parameters.verbosity_level = 0;
    LinearProgrammingAnchorOutput linear_programming_anchor_output = linear_programming_anchor(
            shifted_solution,
            x_weight,
            y_weight,
            linear_programming_anchor_parameters);
    output.solution_pool.add(linear_programming_anchor_output.solution);

    return output;
}
