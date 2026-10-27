#include "packingsolver/irregular/post_process.hpp"

#include "packingsolver/irregular/instance_builder.hpp"

#include "irregular/linear_programming/anchor.hpp"
#include "irregular/solution_builder.hpp"

using namespace packingsolver;
using namespace packingsolver::irregular;

GroupIdenticalBinsOutput packingsolver::irregular::group_identical_bins(
        const Solution& solution)
{
    GroupIdenticalBinsOutput output(solution.instance());
    Solution grouped_solution = packingsolver::group_identical_bins(solution);
    output.solution_pool.add(grouped_solution);
    return output;
}

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
            dx = bin_type.aabb_scaled.x_min - solution_bin.x_min;
        } else if (x_weight < 0) {
            dx = bin_type.aabb_scaled.x_max - solution_bin.x_max;
        }
        if (y_weight > 0) {
            dy = bin_type.aabb_scaled.y_min - solution_bin.y_min;
        } else if (y_weight < 0) {
            dy = bin_type.aabb_scaled.y_max - solution_bin.y_max;
        }
        //std::cout << "dx " << dx << " dy " << dy << std::endl;

        if (!shape::strictly_greater(dx, 0.0) && !shape::strictly_greater(dy, 0.0)) {
            //std::cout << "skip" << std::endl;
            result.append_bin(solution, bin_pos, solution_bin.copies);
            continue;
        }

        // Build the candidate shifted bin.
        SolutionBuilder shifted_bin_builder(instance);
        BinPos tmp_pos = shifted_bin_builder.add_bin(
                solution_bin.bin_type_id,
                solution_bin.copies);
        for (const SolutionItem& item: solution_bin.items) {
            Point bl_corner = item.bl_corner;
            bl_corner.x += dx;
            bl_corner.y += dy;
            shifted_bin_builder.add_item(
                    tmp_pos,
                    item.item_type_id,
                    bl_corner,
                    item.angle,
                    item.mirror);
        }
        Solution shifted_bin = shifted_bin_builder.build();

        // Check feasibility: items must not leave the bin or overlap defects/borders.
        const Solution::OverlappingItems overlapping
                = shifted_bin.compute_overlapping_items(0);
        bool feasible = overlapping.item_border_pairs.empty()
                && overlapping.item_defect_pairs.empty()
                && overlapping.items_outside_bin.empty();

        if (feasible) {
            result.append_bin(shifted_bin, 0, solution_bin.copies);
        } else {
            result.append_bin(solution, bin_pos, solution_bin.copies);
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

Solution packingsolver::irregular::reoptimize_bin(
        const Solution& solution,
        BinPos bin_pos,
        const OptimizeParameters& parameters)
{
    const Instance& instance = solution.instance();

    InstanceBuilder bin_instance_builder;
    bin_instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    bin_instance_builder.set_parameters(instance.parameters());

    // Add bin types.
    const SolutionBin& bin = solution.bin(bin_pos);
    BinTypeId bin_type_id = bin_instance_builder.add_bin_type(instance, bin.bin_type_id);
    bin_instance_builder.set_bin_type_copies(bin_type_id, 1);
    bin_instance_builder.set_bin_type_copies_min(bin_type_id, 0);

    // Add item types.
    std::vector<ItemPos> bin_item_copies(instance.number_of_item_types(), 0);
    for (const SolutionItem& solution_item: bin.items)
        bin_item_copies[solution_item.item_type_id]++;

    std::vector<ItemTypeId> bin_to_orig;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        if (bin_item_copies[item_type_id] > 0) {
            ItemTypeId sub_item_type_id = bin_instance_builder.add_item_type(instance, item_type_id);
            bin_instance_builder.set_item_type_copies(sub_item_type_id, bin_item_copies[item_type_id]);
            bin_to_orig.push_back(item_type_id);
        }
    }

    // Build instance.
    Instance bin_instance = bin_instance_builder.build();

    // Solve instance.
    OptimizeParameters bin_parameters = parameters;
    bin_parameters.messages_to_stdout = false;
    bin_parameters.messages_path = "";
    bin_parameters.messages_streams.clear();
    bin_parameters.write_json_output = false;
    bin_parameters.new_solution_callback = [](const packingsolver::irregular::Output&) { };
    bin_parameters.verbosity_level = 0;
    // The sub-instance has a single bin: its post-process would re-optimize
    // this same bin again.
    bin_parameters.post_process = false;
    auto bin_output = optimize(bin_instance, bin_parameters);
    const Solution& bin_solution = bin_output.solution_pool.best();
    if (!bin_solution.feasible() || !bin_solution.full())
        return solution;

    // Retrieve solution: the bins of 'solution', the bin at 'bin_pos' being
    // replaced by the re-optimized one.
    Solution new_solution(instance);
    for (BinPos pos = 0;
            pos < solution.number_of_different_bins();
            ++pos) {
        if (pos == bin_pos) {
            new_solution.append_bin(
                    bin_solution,
                    0,
                    bin.copies,
                    {bin.bin_type_id},
                    bin_to_orig);
        } else {
            new_solution.append_bin(solution, pos, solution.bin(pos).copies);
        }
    }
    return new_solution;
}

Solution packingsolver::irregular::reoptimize_non_last_bins(
        const Solution& solution,
        const OptimizeParameters& parameters)
{
    Solution new_solution = solution;
    for (BinPos bin_pos = 0;
            bin_pos < solution.number_of_different_bins() - 1;
            ++bin_pos) {
        if (parameters.timer.needs_to_end())
            break;
        Solution bin_solution = reoptimize_bin(
                new_solution,
                bin_pos,
                parameters);
        // Only the bin at 'bin_pos' differs: the comparison is on its leftover
        // value, then on its staircase leftover value.
        if (new_solution < bin_solution)
            new_solution = bin_solution;
    }
    return new_solution;
}
