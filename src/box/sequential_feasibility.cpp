#include "box/sequential_feasibility.hpp"

#include "packingsolver/box/algorithm_formatter.hpp"
#include "packingsolver/box/instance_builder.hpp"

#include <sstream>

using namespace packingsolver;
using namespace packingsolver::box;

SequentialFeasibilityOutput packingsolver::box::sequential_feasibility(
        const Instance& instance,
        const SequentialFeasibilitySolver& solver,
        const SequentialFeasibilityParameters& parameters)
{
    SequentialFeasibilityOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    Volume total_item_volume = instance.item_volume();

    BinPos current_number_of_bins = 0;
    Length current_x = 0;
    if (instance.objective() == Objective::BinPacking) {
        Volume total_bin_volume = 0;
        for (BinTypeId bin_type_id_iter = 0;
                bin_type_id_iter < instance.number_of_bin_types();
                ++bin_type_id_iter) {
            const BinType& bin_type_iter = instance.bin_type(bin_type_id_iter);
            Volume bin_volume = bin_type_iter.box.volume();
            Volume remaining = 2 * total_item_volume - total_bin_volume;
            BinPos copies_needed = (BinPos)((remaining + bin_volume - 1) / bin_volume);
            BinPos copies_used = std::min(copies_needed, bin_type_iter.copies);
            total_bin_volume += copies_used * bin_volume;
            current_number_of_bins += copies_used;
            if (total_bin_volume >= 2 * total_item_volume)
                break;
        }
    } else {  // BinPackingWithLeftovers
        Volume total_bin_volume = 0;
        for (BinTypeId bin_type_id_iter = 0;
                bin_type_id_iter < instance.number_of_bin_types();
                ++bin_type_id_iter) {
            const BinType& bin_type_iter = instance.bin_type(bin_type_id_iter);
            Volume bin_volume = bin_type_iter.box.volume();
            Volume remaining = 2 * total_item_volume - total_bin_volume;
            BinPos copies_needed = (BinPos)((remaining + bin_volume - 1) / bin_volume);
            BinPos copies_used = std::min(copies_needed, bin_type_iter.copies);
            total_bin_volume += copies_used * bin_volume;
            current_number_of_bins += copies_used;
            if (total_bin_volume >= 2 * total_item_volume)
                break;
        }
        BinTypeId last_bin_type_id = instance.bin_type_id(current_number_of_bins - 1);
        const BinType& last_bin_type = instance.bin_type(last_bin_type_id);
        current_x = last_bin_type.box.x;
    }

    for (Counter it = 0;; ++it) {
        if (algorithm_formatter.end_boolean())
            break;
        if (parameters.timer.needs_to_end())
            break;

        // Build the Feasibility sub-instance.
        InstanceBuilder sub_instance_builder;
        sub_instance_builder.set_objective(Objective::Feasibility);
        sub_instance_builder.set_parameters(instance.parameters());
        std::vector<BinTypeId> sub_to_orig_bin_type_ids;
        if (instance.objective() == Objective::BinPacking) {
            // Add bin types from the original instance up to current_number_of_bins.
            BinPos remaining_bins = current_number_of_bins;
            for (BinTypeId bin_type_id_iter = 0;
                    bin_type_id_iter < instance.number_of_bin_types() && remaining_bins > 0;
                    ++bin_type_id_iter) {
                const BinType& bin_type_iter = instance.bin_type(bin_type_id_iter);
                BinPos copies = std::min(bin_type_iter.copies, remaining_bins);
                BinTypeId sub_bin_type_id = sub_instance_builder.add_bin_type(
                        bin_type_iter.box.x,
                        bin_type_iter.box.y,
                        bin_type_iter.box.z);
                sub_instance_builder.set_bin_type_cost(sub_bin_type_id, bin_type_iter.cost);
                sub_instance_builder.set_bin_type_copies(sub_bin_type_id, copies);
                sub_to_orig_bin_type_ids.push_back(bin_type_id_iter);
                remaining_bins -= copies;
            }
        } else {  // BinPackingWithLeftovers
            // Like BinPacking, but every bin type is restricted to current_x
            // along the x-axis (the last bin's still-open dimension).
            BinPos remaining_bins = current_number_of_bins;
            for (BinTypeId bin_type_id_iter = 0;
                    bin_type_id_iter < instance.number_of_bin_types() && remaining_bins > 0;
                    ++bin_type_id_iter) {
                const BinType& bin_type_iter = instance.bin_type(bin_type_id_iter);
                BinPos copies = std::min(bin_type_iter.copies, remaining_bins);
                BinTypeId sub_bin_type_id = sub_instance_builder.add_bin_type(
                        current_x,
                        bin_type_iter.box.y,
                        bin_type_iter.box.z);
                sub_instance_builder.set_bin_type_cost(sub_bin_type_id, bin_type_iter.cost);
                sub_instance_builder.set_bin_type_copies(sub_bin_type_id, copies);
                sub_to_orig_bin_type_ids.push_back(bin_type_id_iter);
                remaining_bins -= copies;
            }
        }
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            sub_instance_builder.add_item_type(instance, item_type_id);
        }
        Instance sub_instance = sub_instance_builder.build();

        // Solve the sub-instance.
        SolutionPool<Instance, Solution> sub_solution_pool = solver(sub_instance);

        // If no feasible solution found, stop.
        if (!sub_solution_pool.best().feasible())
            break;

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
        } else {  // BinPackingWithLeftovers
            current_x = std::min(
                    (Length)(0.99 * solution.x_max()),
                    solution.x_max() - 1);
            if (solution.number_of_bins() < current_number_of_bins) {
                current_number_of_bins = solution.number_of_bins();
                BinTypeId bin_type_id_new = instance.bin_type_id(current_number_of_bins - 1);
                const BinType& bin_type_new = instance.bin_type(bin_type_id_new);
                current_x = bin_type_new.box.x;
            } else if (current_x <= 0) {
                current_number_of_bins--;
                if (current_number_of_bins == 0)
                    break;
                BinTypeId bin_type_id_new = instance.bin_type_id(current_number_of_bins - 1);
                const BinType& bin_type_new = instance.bin_type(bin_type_id_new);
                current_x = bin_type_new.box.x;
            }
        }
    }

    algorithm_formatter.end();
    return output;
}
