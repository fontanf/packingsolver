#include "packingsolver/boxstacks/optimize.hpp"

#include "packingsolver/boxstacks/algorithm_formatter.hpp"
#include "packingsolver/boxstacks/instance_builder.hpp"
#include "boxstacks/tree_search.hpp"
#include "boxstacks/sequential_onedimensional_rectangle.hpp"
#include "packingsolver/box/instance_builder.hpp"
#include "packingsolver/box/optimize.hpp"

#include "algorithms/sequential_value_correction.hpp"

using namespace packingsolver;
using namespace packingsolver::boxstacks;

namespace
{

void optimize_trivial_bound(
        const Instance& instance,
        AlgorithmFormatter& algorithm_formatter)
{
    if (instance.objective() == Objective::Knapsack)
        algorithm_formatter.update_knapsack_bound(instance.item_profit());
}

void optimize_box_bound(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter)
{
    // Relax the instance to a plain 'box' instance: drop the stacking,
    // axle weight, stack density and unloading constraints, keep only the
    // dimensions, cost, weight, profit and rotations. Any boxstacks-feasible
    // solution is also feasible for this relaxation, so a bound computed on
    // it remains a valid bound for the original instance.
    box::InstanceBuilder box_instance_builder;
    box_instance_builder.set_objective(instance.objective());
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        BinTypeId box_bin_type_id = box_instance_builder.add_bin_type(
                bin_type.box.x,
                bin_type.box.y,
                bin_type.box.z);
        box_instance_builder.set_bin_type_cost(box_bin_type_id, bin_type.cost);
        box_instance_builder.set_bin_type_copies(box_bin_type_id, bin_type.copies);
        box_instance_builder.set_bin_type_copies_min(box_bin_type_id, bin_type.copies_min);
        box_instance_builder.set_bin_type_maximum_weight(box_bin_type_id, bin_type.maximum_weight);
    }
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        ItemTypeId box_item_type_id = box_instance_builder.add_item_type(
                item_type.box.x,
                item_type.box.y,
                item_type.box.z);
        box_instance_builder.set_item_type_profit(box_item_type_id, item_type.profit);
        box_instance_builder.set_item_type_copies(box_item_type_id, item_type.copies);
        box_instance_builder.set_item_type_weight(box_item_type_id, item_type.weight);
        for (Rotation rotation: item_type.rotations) {
            box_instance_builder.add_item_type_rotation(
                    box_item_type_id,
                    static_cast<box::Rotation>(rotation));
        }
    }
    box::Instance box_instance = box_instance_builder.build();

    box::OptimizeParameters box_parameters;
    box_parameters.verbosity_level = 0;
    box_parameters.timer = parameters.timer;
    box_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    box_parameters.optimization_mode = OptimizationMode::NotAnytime;
    box_parameters.linear_programming_solver_name = parameters.linear_programming_solver_name;
    box::Output box_output = box::optimize(box_instance, box_parameters);

    switch (instance.objective()) {
    case Objective::Knapsack:
        algorithm_formatter.update_knapsack_bound(box_output.knapsack_bound);
        break;
    case Objective::BinPacking:
        algorithm_formatter.update_bin_packing_bound(box_output.bin_packing_bound);
        break;
    case Objective::VariableSizedBinPacking:
        algorithm_formatter.update_variable_sized_bin_packing_bound(box_output.variable_sized_bin_packing_bound);
        break;
    default:
        break;
    }
}

}

packingsolver::boxstacks::Output packingsolver::boxstacks::optimize(
        const Instance& instance,
        const OptimizeParameters& parameters)
{
    Output output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();
    auto logger = parameters.get_logger();

    optimize_trivial_bound(instance, algorithm_formatter);
    if (instance.objective() == Objective::Knapsack
            || instance.objective() == Objective::BinPacking
            || instance.objective() == Objective::VariableSizedBinPacking) {
        optimize_box_bound(instance, parameters, algorithm_formatter);
    }

    if (algorithm_formatter.end_boolean()) {
        algorithm_formatter.end();
        return output;
    }
    if (parameters.timer.needs_to_end()) {
        algorithm_formatter.end();
        return output;
    }

    if (instance.number_of_bins() == 1) {

        auto sor_begin = std::chrono::steady_clock::now();

        SequentialOneDimensionalRectangleParameters sor_parameters;
        sor_parameters.verbosity_level = 0;
        sor_parameters.timer = parameters.timer;
        sor_parameters.logger = logger;
        sor_parameters.onedimensional_parameters.linear_programming_solver_name = parameters.linear_programming_solver_name;
        //sor_parameters.info.set_verbosity_level(2);
        sor_parameters.new_solution_callback = [
            &algorithm_formatter](
                    const boxstacks::Output& ps_output)
            {
                const SequentialOneDimensionalRectangleOutput& sor_output
                    = static_cast<const SequentialOneDimensionalRectangleOutput&>(ps_output);
                std::stringstream ss;
                ss << "SOR it " << sor_output.number_of_iterations;
                algorithm_formatter.update_solution(sor_output.solution_pool.best(), ss.str());
            };

        auto sor_output = sequential_onedimensional_rectangle(instance, sor_parameters);

        std::stringstream sor_ss;
        sor_ss << "SOR";
        algorithm_formatter.update_solution(sor_output.solution_pool.best(), sor_ss.str());
        //std::cout << "dec " << sor_output.solution_pool.best().item_weight() << std::endl;
        //std::cout << output.solution_pool.best().item_weight() << std::endl;
        output.sequential_onedimensional_rectangle_number_of_items = sor_output.maximum_number_of_items;
        output.sequential_onedimensional_rectangle_profit = sor_output.solution_pool.best().profit();
        //std::cout << output.sequential_onedimensional_rectangle_number_of_items << std::endl;

        auto sor_end = std::chrono::steady_clock::now();
        std::chrono::duration<double> sor_time_span
            = std::chrono::duration_cast<std::chrono::duration<double>>(sor_end - sor_begin);
        output.sequential_onedimensional_rectangle_time = sor_time_span.count();
        output.sequential_onedimensional_rectangle_onedimensional_time += sor_output.onedimensional_time;
        output.sequential_onedimensional_rectangle_rectangle_time += sor_output.rectangle_time;
        output.number_of_sequential_onedimensional_rectangle_calls++;
        if (!sor_output.solution_pool.best().full())
            output.sequential_onedimensional_rectangle_failed = sor_output.failed;

        // The boxstacks branching scheme is significantly more expensive than the
        // sequential_onedimensional_rectangle algorithm. Therefore, we only use it
        // if it looks promising enough.
        // We don't run the boxstacks branching scheme if:
        // - The best solution already contains all the items
        // - The solution of the sequential_onedimensional_rectangle algorithm
        //   never violated the axle weight constraints.
        // - The weight of the smallest unpacked item is greater than the remaining
        //   capacity.
        // - TODO The items need at least i bins to be all packed, and more than
        //   0.8 * 1 / i + 0.2 * 1 / (i - 1)
        //   fraction of the volume and weight have been packed.
        bool run_boxstacks_branching_scheme = true;
        if (output.solution_pool.best().number_of_items() == instance.number_of_items())
            run_boxstacks_branching_scheme = false;
        if (!sor_output.failed)
            run_boxstacks_branching_scheme = false;
        bool no_lighter_item = true;
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            if (output.solution_pool.best().item_copies(item_type_id)
                    == instance.item_type(item_type_id).copies)
                continue;
            if (output.solution_pool.best().item_weight()
                    + instance.item_type(item_type_id).weight
                    <= instance.bin_weight())
                no_lighter_item = false;
        }
        if (no_lighter_item)
            run_boxstacks_branching_scheme = false;
        //BinPos minimum_number_of_bins = std::max(
        //        std::ceil((double)instance.item_volume() / instance.bin_volume()),
        //        std::ceil((double)instance.item_weight() / instance.bin_weight()));
        //if (minimum_number_of_bins > 1) {
        //    double minimum_fraction = 0.8 / minimum_number_of_bins + 0.2 / (minimum_number_of_bins - 1);
        //    if (output.solution_pool.best().item_volume() >= minimum_fraction * instance.item_volume()
        //            && output.solution_pool.best().item_weight() >= minimum_fraction * instance.item_weight())
        //        run_boxstacks_branching_scheme = false;
        //}

        if (output.solution_pool.best().number_of_items() == instance.number_of_items()) {
            output.number_of_sequential_onedimensional_rectangle_perfect++;
        } else if (!run_boxstacks_branching_scheme) {
            output.number_of_sequential_onedimensional_rectangle_good++;
        }

        if (run_boxstacks_branching_scheme) {
            auto bs_begin = std::chrono::steady_clock::now();

            TreeSearchParameters ts_parameters;
            ts_parameters.verbosity_level = 0;
            ts_parameters.timer = parameters.timer;
            ts_parameters.optimization_mode = parameters.optimization_mode;
            ts_parameters.not_anytime_tree_search_queue_size = parameters.not_anytime_tree_search_queue_size;
            ts_parameters.guides = parameters.tree_search_guides;
            ts_parameters.maximum_number_of_selected_items = output.sequential_onedimensional_rectangle_number_of_items;
            ts_parameters.new_solution_callback = [&algorithm_formatter](
                    const boxstacks::Output& ts_output)
            {
                algorithm_formatter.update_solution(ts_output.solution_pool.best(), "TS " + ts_output.solution_pool.best_label());
            };
            tree_search(instance, ts_parameters);

            auto bs_end = std::chrono::steady_clock::now();
            std::chrono::duration<double> bs_time_span
                = std::chrono::duration_cast<std::chrono::duration<double>>(bs_end - bs_begin);
            output.tree_search_time = bs_time_span.count();
            output.number_of_tree_search_calls++;
            if (output.solution_pool.best().number_of_items() == instance.number_of_items()) {
                output.number_of_tree_search_perfect++;
            } else if (output.solution_pool.best().profit() > sor_output.solution_pool.best().profit()) {
                output.number_of_tree_search_better++;
            }
        }

        //for (ItemTypeId j = 0; j < instance.number_of_item_types(); ++j) {
        //    const ItemType& item_type = instance.item_type(j);
        //    ItemPos c = output.solution_pool.best().item_copies(j);
        //    if (c != item_type.copies)
        //        std::cout << "j " << j << std::endl;
        //}

    } else {

        SequentialValueCorrectionFunction<Instance, Solution> kp_solve
            = [&algorithm_formatter, &parameters, &output](const Instance& kp_instance)
            {
                OptimizeParameters kp_parameters;
                kp_parameters.verbosity_level = 0;
                kp_parameters.timer = parameters.timer;
                kp_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
                kp_parameters.optimization_mode
                    = (parameters.optimization_mode == OptimizationMode::NotAnytimeSequential)?
                    OptimizationMode::NotAnytimeSequential:
                    OptimizationMode::NotAnytimeDeterministic;
                kp_parameters.linear_programming_solver_name = parameters.linear_programming_solver_name;
                kp_parameters.not_anytime_tree_search_queue_size
                    = parameters.sequential_value_correction_subproblem_tree_search_queue_size;
                //kp_parameters.sequential_onedimensional_rectangle_parameters.rectangle_queue_size = parameters.sequential_value_correction_queue_size;
                auto kp_output = optimize(kp_instance, kp_parameters);

                // Update output.
                output.sequential_onedimensional_rectangle_time += kp_output.sequential_onedimensional_rectangle_time;
                output.sequential_onedimensional_rectangle_rectangle_time += kp_output.sequential_onedimensional_rectangle_rectangle_time;
                output.sequential_onedimensional_rectangle_onedimensional_time += kp_output.sequential_onedimensional_rectangle_onedimensional_time;
                output.tree_search_time += kp_output.tree_search_time;
                output.number_of_sequential_onedimensional_rectangle_calls += kp_output.number_of_sequential_onedimensional_rectangle_calls;
                output.number_of_sequential_onedimensional_rectangle_perfect += kp_output.number_of_sequential_onedimensional_rectangle_perfect;
                output.number_of_sequential_onedimensional_rectangle_good += kp_output.number_of_sequential_onedimensional_rectangle_good;
                output.number_of_tree_search_calls += kp_output.number_of_tree_search_calls;
                output.number_of_tree_search_perfect += kp_output.number_of_tree_search_perfect;
                output.number_of_tree_search_better += kp_output.number_of_tree_search_better;
                return kp_output.solution_pool;
            };
        SequentialValueCorrectionParameters<Instance, Solution, boxstacks::Output> svc_parameters;
        svc_parameters.verbosity_level = 0;
        svc_parameters.timer = parameters.timer;
        svc_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
        svc_parameters.new_solution_callback = [&algorithm_formatter](
                const boxstacks::Output& ps_output)
        {
            const SequentialValueCorrectionOutput<Instance, Solution, boxstacks::Output>& pssvc_output
                = static_cast<const SequentialValueCorrectionOutput<Instance, Solution, boxstacks::Output>&>(ps_output);
            std::stringstream ss;
            ss << "iteration " << pssvc_output.number_of_iterations;
            algorithm_formatter.update_solution(pssvc_output.solution_pool.best(), ss.str());
        };
        auto svc_output = sequential_value_correction<Instance, InstanceBuilder, Solution, AlgorithmFormatter, boxstacks::Output>(instance, kp_solve, svc_parameters);

    }

    algorithm_formatter.end();
    return output;
}
