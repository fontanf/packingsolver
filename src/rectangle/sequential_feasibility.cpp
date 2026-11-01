#include "rectangle/sequential_feasibility.hpp"

#include "packingsolver/rectangle/algorithm_formatter.hpp"
#include "packingsolver/rectangle/instance_builder.hpp"

#include <sstream>

using namespace packingsolver;
using namespace packingsolver::rectangle;

namespace
{

/**
 * Return the length of a bin along the shrunk direction, for objective
 * BinPackingWithLeftovers.
 */
Length leftover_length(
        const Instance& instance,
        const Rectangle& rect)
{
    return (instance.parameters().leftover_mode == LeftoverMode::Y)? rect.y: rect.x;
}

/**
 * Return the length used by the last bin of a solution along the shrunk
 * direction, for objective BinPackingWithLeftovers.
 */
Length leftover_length(
        const Instance& instance,
        const Solution& solution)
{
    return (instance.parameters().leftover_mode == LeftoverMode::Y)?
        solution.y_max():
        solution.x_max();
}

/**
 * Return the number of bins of the first bin types whose area is at least
 * twice the item area.
 */
BinPos initial_number_of_bins(
        const Instance& instance)
{
    Area total_item_area = instance.item_area();
    Area total_bin_area = 0;
    BinPos number_of_bins = 0;
    for (BinPos bin_pos = 0; bin_pos < instance.number_of_bins(); ++bin_pos) {
        if (total_bin_area >= 2 * total_item_area)
            break;
        const BinType& bin_type = instance.bin_type(instance.bin_type_id(bin_pos));
        total_bin_area += bin_type.area();
        number_of_bins++;
    }
    return (std::max)(number_of_bins, (BinPos)1);
}

}

SequentialFeasibilityOutput packingsolver::rectangle::sequential_feasibility(
        const Instance& instance,
        const SequentialFeasibilitySolver& solver,
        const SequentialFeasibilityParameters& parameters)
{
    SequentialFeasibilityOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    const BinType& bin_type = instance.bin_type(instance.bin_type_id(0));
    Area total_item_area = instance.item_area();

    BinPos current_number_of_bins = 0;
    // Length of the last bin along the shrunk direction.
    Length current_length = 0;
    if (instance.objective() == Objective::BinPacking) {
        current_number_of_bins = initial_number_of_bins(instance);
    } else if (instance.objective() == Objective::BinPackingWithLeftovers) {
        current_number_of_bins = initial_number_of_bins(instance);
        const BinType& last_bin_type = instance.bin_type(
                instance.bin_type_id(current_number_of_bins - 1));
        current_length = leftover_length(instance, last_bin_type.rect);
    } else if (instance.objective() == Objective::OpenDimensionX) {
        current_length = std::min(
                (2 * total_item_area + bin_type.rect.y - 1) / bin_type.rect.y,
                bin_type.rect.x);
    } else {  // OpenDimensionY
        current_length = std::min(
                (2 * total_item_area + bin_type.rect.x - 1) / bin_type.rect.x,
                bin_type.rect.y);
    }

    for (Counter it = 0;; ++it) {
        if (algorithm_formatter.end_boolean())
            break;
        if (parameters.timer.needs_to_end())
            break;

        // Build the Feasibility sub-instance. Its bins are the first
        // 'current_number_of_bins' bins of the instance; for objective
        // BinPackingWithLeftovers, the last one is shrunk and is a bin type of
        // its own.
        InstanceBuilder sub_instance_builder;
        sub_instance_builder.set_objective(Objective::Feasibility);
        sub_instance_builder.set_parameters(instance.parameters());
        sub_instance_builder.set_original_instance(instance);
        std::vector<BinTypeId> sub_to_orig_bin_type_ids;
        if (instance.objective() == Objective::BinPacking
                || instance.objective() == Objective::BinPackingWithLeftovers) {
            bool restrict_last_bin = (instance.objective() == Objective::BinPackingWithLeftovers);
            BinPos remaining_bins = current_number_of_bins;
            for (BinTypeId bin_type_id = 0;
                    bin_type_id < instance.number_of_bin_types() && remaining_bins > 0;
                    ++bin_type_id) {
                const BinType& bin_type_cur = instance.bin_type(bin_type_id);
                BinPos copies = std::min(bin_type_cur.copies, remaining_bins);
                remaining_bins -= copies;
                BinPos copies_full = (restrict_last_bin && remaining_bins == 0)?
                    copies - 1:
                    copies;
                if (copies_full > 0) {
                    BinTypeId sub_bin_type_id = sub_instance_builder.add_bin_type_from_original_instance(bin_type_id);
                    sub_instance_builder.set_bin_type_copies(sub_bin_type_id, copies_full);
                    sub_instance_builder.set_bin_type_copies_min(sub_bin_type_id, 0);
                    sub_to_orig_bin_type_ids.push_back(bin_type_id);
                }
                if (copies_full == copies)
                    continue;

                // Last bin.
                BinTypeId sub_bin_type_id = sub_instance_builder.add_bin_type_from_original_instance(bin_type_id);
                sub_instance_builder.set_bin_type_copies(sub_bin_type_id, 1);
                sub_instance_builder.set_bin_type_copies_min(sub_bin_type_id, 0);
                if (instance.parameters().leftover_mode == LeftoverMode::Y) {
                    sub_instance_builder.set_bin_type_rect(
                            sub_bin_type_id,
                            bin_type_cur.rect.x,
                            current_length);
                } else {
                    sub_instance_builder.set_bin_type_rect(
                            sub_bin_type_id,
                            current_length,
                            bin_type_cur.rect.y);
                }
                sub_to_orig_bin_type_ids.push_back(bin_type_id);
            }
        } else {
            BinTypeId sub_bin_type_id = sub_instance_builder.add_bin_type_from_original_instance(instance.bin_type_id(0));
            sub_instance_builder.set_bin_type_copies(sub_bin_type_id, 1);
            sub_instance_builder.set_bin_type_copies_min(sub_bin_type_id, 0);
            if (instance.objective() == Objective::OpenDimensionX) {
                sub_instance_builder.set_bin_type_rect(
                        sub_bin_type_id,
                        current_length,
                        bin_type.rect.y);
            } else {  // OpenDimensionY
                sub_instance_builder.set_bin_type_rect(
                        sub_bin_type_id,
                        bin_type.rect.x,
                        current_length);
            }
            sub_to_orig_bin_type_ids.push_back(instance.bin_type_id(0));
        }
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            sub_instance_builder.add_item_type_from_original_instance(item_type_id);
        }
        Instance sub_instance = sub_instance_builder.build();

        // Solve the sub-instance.
        SolutionPool<Instance, Solution> sub_solution_pool = solver(sub_instance);

        // If no feasible solution found, stop.
        if (!sub_solution_pool.best().feasible()
                || !sub_solution_pool.best().full()) {
            break;
        }

        // Transfer the sub-solution to the main instance.
        Solution solution(instance);
        solution.append_bins(
                sub_solution_pool.best(),
                sub_to_orig_bin_type_ids,
                {});
        std::stringstream ss;
        ss << "SF it " << it << " " << sub_solution_pool.best_label();
        algorithm_formatter.update_solution(solution, ss.str());

        // Update for the next iteration.
        if (instance.objective() == Objective::BinPacking) {
            current_number_of_bins = solution.number_of_bins() - 1;
            if (current_number_of_bins == 0)
                break;
        } else if (instance.objective() == Objective::BinPackingWithLeftovers) {
            Length length = leftover_length(instance, solution);
            current_length = std::min((Length)(0.99 * length), length - 1);
            if (solution.number_of_bins() < current_number_of_bins) {
                current_number_of_bins = solution.number_of_bins();
                const BinType& bin_type_new = instance.bin_type(
                        instance.bin_type_id(current_number_of_bins - 1));
                current_length = leftover_length(instance, bin_type_new.rect);
            } else if (current_length <= 0) {
                current_number_of_bins--;
                if (current_number_of_bins == 0)
                    break;
                const BinType& bin_type_new = instance.bin_type(
                        instance.bin_type_id(current_number_of_bins - 1));
                current_length = leftover_length(instance, bin_type_new.rect);
            }
        } else if (instance.objective() == Objective::OpenDimensionX) {
            current_length = std::min(
                    (Length)(0.99 * solution.x_max()),
                    solution.x_max() - 1);
            if (current_length <= 0)
                break;
        } else {  // OpenDimensionY
            current_length = std::min(
                    (Length)(0.99 * solution.y_max()),
                    solution.y_max() - 1);
            if (current_length <= 0)
                break;
        }
    }

    algorithm_formatter.end();
    return output;
}
