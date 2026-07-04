#include "rectangleguillotine/sequential_feasibility.hpp"

#include "packingsolver/rectangleguillotine/algorithm_formatter.hpp"
#include "packingsolver/rectangleguillotine/instance_builder.hpp"

#include <sstream>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

SequentialFeasibilityOutput packingsolver::rectangleguillotine::sequential_feasibility(
        const Instance& instance,
        const SequentialFeasibilitySolver& solver,
        const SequentialFeasibilityParameters& parameters)
{
    SequentialFeasibilityOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    BinTypeId bin_type_id = instance.bin_type_id(0);
    const BinType& bin_type = instance.bin_type(bin_type_id);

    Area total_item_area = instance.item_area();

    BinPos current_number_of_bins = 0;
    Length current_x = 0;
    Length current_y = 0;
    if (instance.objective() == Objective::BinPacking) {
        Area total_bin_area = 0;
        for (BinTypeId bin_type_id_iter = 0;
                bin_type_id_iter < instance.number_of_bin_types();
                ++bin_type_id_iter) {
            const BinType& bin_type_iter = instance.bin_type(bin_type_id_iter);
            Area bin_area = bin_type_iter.rect.w * bin_type_iter.rect.h;
            Area remaining = 2 * total_item_area - total_bin_area;
            BinPos copies_needed = (BinPos)((remaining + bin_area - 1) / bin_area);
            BinPos copies_used = std::min(copies_needed, bin_type_iter.copies);
            total_bin_area += copies_used * bin_area;
            current_number_of_bins += copies_used;
            if (total_bin_area >= 2 * total_item_area)
                break;
        }
    } else if (instance.objective() == Objective::BinPackingWithLeftovers) {
        Area total_bin_area = 0;
        for (BinTypeId bin_type_id_iter = 0;
                bin_type_id_iter < instance.number_of_bin_types();
                ++bin_type_id_iter) {
            const BinType& bin_type_iter = instance.bin_type(bin_type_id_iter);
            Area bin_area = bin_type_iter.rect.w * bin_type_iter.rect.h;
            Area remaining = 2 * total_item_area - total_bin_area;
            BinPos copies_needed = (BinPos)((remaining + bin_area - 1) / bin_area);
            BinPos copies_used = std::min(copies_needed, bin_type_iter.copies);
            total_bin_area += copies_used * bin_area;
            current_number_of_bins += copies_used;
            if (total_bin_area >= 2 * total_item_area)
                break;
        }
        BinTypeId last_bin_type_id = instance.bin_type_id(current_number_of_bins - 1);
        const BinType& last_bin_type = instance.bin_type(last_bin_type_id);
        current_x = last_bin_type.rect.w;
    } else if (instance.objective() == Objective::OpenDimensionX) {
        current_x = std::min(
                (2 * total_item_area + bin_type.rect.h - 1) / bin_type.rect.h,
                bin_type.rect.w);
    } else {  // OpenDimensionY
        current_y = std::min(
                (2 * total_item_area + bin_type.rect.w - 1) / bin_type.rect.w,
                bin_type.rect.h);
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
            BinPos remaining_bins = current_number_of_bins;
            for (BinTypeId bin_type_id_iter = 0;
                    bin_type_id_iter < instance.number_of_bin_types() && remaining_bins > 0;
                    ++bin_type_id_iter) {
                const BinType& bin_type_iter = instance.bin_type(bin_type_id_iter);
                BinPos copies = std::min(bin_type_iter.copies, remaining_bins);
                sub_instance_builder.add_bin_type(
                        bin_type_iter.rect.w,
                        bin_type_iter.rect.h,
                        bin_type_iter.cost,
                        copies);
                sub_to_orig_bin_type_ids.push_back(bin_type_id_iter);
                remaining_bins -= copies;
            }
        } else if (instance.objective() == Objective::BinPackingWithLeftovers) {
            BinPos remaining_bins = current_number_of_bins;
            for (BinTypeId bin_type_id_iter = 0;
                    bin_type_id_iter < instance.number_of_bin_types() && remaining_bins > 0;
                    ++bin_type_id_iter) {
                const BinType& bin_type_iter = instance.bin_type(bin_type_id_iter);
                BinPos copies = std::min(bin_type_iter.copies, remaining_bins);
                sub_instance_builder.add_bin_type(
                        current_x,
                        bin_type_iter.rect.h,
                        bin_type_iter.cost,
                        copies);
                sub_to_orig_bin_type_ids.push_back(bin_type_id_iter);
                remaining_bins -= copies;
            }
        } else if (instance.objective() == Objective::OpenDimensionX) {
            sub_instance_builder.add_bin_type(
                    current_x,
                    bin_type.rect.h,
                    bin_type.cost);
        } else {  // OpenDimensionY
            sub_instance_builder.add_bin_type(
                    bin_type.rect.w,
                    current_y,
                    bin_type.cost);
        }
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = instance.item_type(item_type_id);
            sub_instance_builder.add_item_type(
                    instance,
                    item_type_id,
                    item_type.profit,
                    item_type.copies);
        }
        Instance sub_instance = sub_instance_builder.build();

        // Solve the sub-instance.
        SolutionPool<Instance, Solution> sub_solution_pool = solver(sub_instance);

        // If no feasible solution found, stop.
        if (!sub_solution_pool.best().full())
            break;

        // Transfer the sub-solution to the main instance.
        Solution solution(instance);
        if (instance.objective() == Objective::BinPacking
                || instance.objective() == Objective::BinPackingWithLeftovers) {
            solution.append(
                    sub_solution_pool.best(),
                    sub_to_orig_bin_type_ids,
                    {});
        } else {
            solution.append(
                    sub_solution_pool.best(),
                    0,  // bin_pos
                    1,  // copies
                    {0});  // bin_type_ids
        }
        // Check feasibility.
        if (!solution.item_copies_feasible()) {
            throw std::logic_error(
                    FUNC_SIGNATURE + ": solution doesn't satisfy item copies.");
        }

        std::stringstream ss;
        ss << "SF it " << it << " " << sub_solution_pool.best_label();
        algorithm_formatter.update_solution(solution, ss.str());

        // Update for the next iteration.
        if (instance.objective() == Objective::BinPacking) {
            current_number_of_bins = sub_solution_pool.best().number_of_bins() - 1;
            if (current_number_of_bins == 0)
                break;
        } else if (instance.objective() == Objective::BinPackingWithLeftovers) {
            current_x = std::min(
                    (Length)(0.99 * sub_solution_pool.best().width()),
                    sub_solution_pool.best().width() - 1);
            if (sub_solution_pool.best().number_of_bins() < current_number_of_bins) {
                current_number_of_bins = sub_solution_pool.best().number_of_bins();
                BinTypeId bin_type_id_new = instance.bin_type_id(current_number_of_bins - 1);
                const BinType& bin_type_new = instance.bin_type(bin_type_id_new);
                current_x = bin_type_new.rect.w;
            } else if (current_x <= 0) {
                current_number_of_bins--;
                if (current_number_of_bins == 0)
                    break;
                BinTypeId bin_type_id_new = instance.bin_type_id(current_number_of_bins - 1);
                const BinType& bin_type_new = instance.bin_type(bin_type_id_new);
                current_x = bin_type_new.rect.w;
            }
        } else if (instance.objective() == Objective::OpenDimensionX) {
            current_x = std::min(
                    (Length)(0.99 * sub_solution_pool.best().width()),
                    sub_solution_pool.best().width() - 1);
            if (current_x <= 0)
                break;
        } else {  // OpenDimensionY
            current_y = std::min(
                    (Length)(0.99 * sub_solution_pool.best().height()),
                    sub_solution_pool.best().height() - 1);
            if (current_y <= 0)
                break;
        }
    }

    algorithm_formatter.end();
    return output;
}
