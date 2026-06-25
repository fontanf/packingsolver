#include "packingsolver/irregular/optimize.hpp"

#include "packingsolver/irregular/algorithm_formatter.hpp"
#include "packingsolver/irregular/instance_builder.hpp"
#include "irregular/trivial.hpp"
#include "irregular/tree_search.hpp"
#include "irregular/milp_raster.hpp"
#include "irregular/local_search.hpp"
#include "irregular/sequential_feasibility.hpp"
#include "algorithms/dichotomic_search.hpp"
#include "algorithms/sequential_value_correction.hpp"
#include "algorithms/column_generation.hpp"
#include "algorithms/thread_pool.hpp"
#include "packingsolver/onedimensional/instance_builder.hpp"
#include "packingsolver/onedimensional/optimize.hpp"

using namespace packingsolver;
using namespace packingsolver::irregular;

namespace
{

void optimize_onedimensional_bound(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter)
{
    onedimensional::InstanceBuilder onedim_instance_builder;
    onedim_instance_builder.set_objective(instance.objective());
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        onedim_instance_builder.add_bin_type(
                std::ceil(bin_type.area_scaled),
                bin_type.cost,
                bin_type.copies,
                bin_type.copies_min);
    }
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        Length length = std::floor(item_type.area_scaled);
        if (length == 0)
            continue;
        onedim_instance_builder.add_item_type(
                length,
                item_type.profit,
                item_type.copies);
    }
    onedimensional::Instance onedim_instance = onedim_instance_builder.build();

    // Solve the instance.
    onedimensional::OptimizeParameters onedim_parameters;
    onedim_parameters.verbosity_level = 0;
    onedim_parameters.timer = parameters.timer;
    onedim_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    onedim_parameters.optimization_mode = OptimizationMode::NotAnytime;
    onedim_parameters.linear_programming_solver_name = parameters.linear_programming_solver_name;
    auto onedim_output = optimize(onedim_instance, onedim_parameters);

    std::stringstream ss("1D");
    switch (instance.objective()) {
    case Objective::BinPacking: {
        algorithm_formatter.update_bin_packing_bound(
                onedim_output.bin_packing_bound);
        break;
    } case Objective::Knapsack: {
        algorithm_formatter.update_knapsack_bound(
                onedim_output.knapsack_bound);
        break;
    } case Objective::VariableSizedBinPacking: {
        algorithm_formatter.update_variable_sized_bin_packing_bound(
                onedim_output.variable_sized_bin_packing_bound);
        break;
    } default: {
        std::stringstream ss;
        ss << FUNC_SIGNATURE << ": "
            << "objective \""
            << instance.objective() << "\" not supported.";
        throw std::logic_error(ss.str());
    }
    }
}

void optimize_trivial_single_item(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter)
{
    TrivialSingleItemParameters trivial_single_item_parameters;
    trivial_single_item_parameters.verbosity_level = 0;
    trivial_single_item_parameters.timer = parameters.timer;
    trivial_single_item_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    trivial_single_item_parameters.new_solution_callback = [&algorithm_formatter](
            const packingsolver::Output<Instance, Solution>& output) {
        algorithm_formatter.update_solution(
                output.solution_pool.best(),
                "Trivial");
    };
    trivial_single_item(instance, trivial_single_item_parameters);
}

void optimize_tree_search(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter)
{
    TreeSearchParameters ts_parameters;
    ts_parameters.verbosity_level = 0;
    ts_parameters.timer = parameters.timer;
    ts_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    ts_parameters.optimization_mode = parameters.optimization_mode;
    ts_parameters.guides = parameters.tree_search_guides;
    ts_parameters.not_anytime_tree_search_queue_size = parameters.not_anytime_tree_search_queue_size;
    ts_parameters.initial_maximum_approximation_ratio = parameters.initial_maximum_approximation_ratio;
    ts_parameters.not_anytime_maximum_approximation_ratio = parameters.not_anytime_maximum_approximation_ratio;
    ts_parameters.maximum_approximation_ratio_factor = parameters.maximum_approximation_ratio_factor;
    ts_parameters.json_search_tree_path = parameters.json_search_tree_path;
    ts_parameters.new_solution_callback = [&algorithm_formatter](
            const packingsolver::Output<Instance, Solution>& ts_output)
    {
        algorithm_formatter.update_solution(ts_output.solution_pool.best(), "TS");
    };
    tree_search(instance, ts_parameters);
}

void optimize_local_search(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter)
{
    LocalSearchParameters ls_parameters;
    ls_parameters.verbosity_level = 0;
    ls_parameters.timer = parameters.timer;
    ls_parameters.new_solution_callback = [&algorithm_formatter](
            const packingsolver::Output<Instance, Solution>& ps_output)
    {
        std::stringstream ss;
        ss << "LS";
        algorithm_formatter.update_solution(ps_output.solution_pool.best(), ss.str());
    };
    local_search(instance, ls_parameters);
}

void optimize_sequential_single_knapsack(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter,
        Counter queue_size_max = -1)
{
    double maximum_approximation_ratio = parameters.initial_maximum_approximation_ratio;
    for (Counter queue_size = 1;;) {

        if (parameters.optimization_mode != OptimizationMode::Anytime) {
            queue_size = parameters.not_anytime_sequential_single_knapsack_subproblem_tree_search_queue_size;
            maximum_approximation_ratio = parameters.not_anytime_maximum_approximation_ratio;
        }

        if (queue_size_max != -1
                && queue_size > queue_size_max) {
            break;
        }

        SequentialValueCorrectionFunction<Instance, Solution> kp_solve
            = [&algorithm_formatter, &parameters, queue_size, maximum_approximation_ratio](const Instance& kp_instance)
            {
                OptimizeParameters kp_parameters;
                kp_parameters.verbosity_level = 0;
                kp_parameters.timer = parameters.timer;
                kp_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
                kp_parameters.optimization_mode
                    = (parameters.optimization_mode == OptimizationMode::NotAnytimeSequential)?
                    OptimizationMode::NotAnytimeSequential:
                    OptimizationMode::NotAnytimeDeterministic;
                kp_parameters.not_anytime_maximum_approximation_ratio = maximum_approximation_ratio;
                kp_parameters.not_anytime_tree_search_queue_size = queue_size;
                kp_parameters.linear_programming_solver_name = parameters.linear_programming_solver_name;
                auto kp_output = optimize(kp_instance, kp_parameters);
                return kp_output.solution_pool;
            };
        SequentialValueCorrectionParameters<Instance, Solution> svc_parameters;
        svc_parameters.verbosity_level = 0;
        svc_parameters.timer = parameters.timer;
        svc_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
        svc_parameters.maximum_number_of_iterations = 1;
        svc_parameters.new_solution_callback = [
            &algorithm_formatter, &queue_size](
                    const packingsolver::Output<Instance, Solution>& ps_output)
            {
                const SequentialValueCorrectionOutput<Instance, Solution>& pssvc_output
                    = static_cast<const SequentialValueCorrectionOutput<Instance, Solution>&>(ps_output);
                std::stringstream ss;
                ss << "SSK q " << queue_size;
                algorithm_formatter.update_solution(pssvc_output.solution_pool.best(), ss.str());
            };
        sequential_value_correction<Instance, InstanceBuilder, Solution, AlgorithmFormatter>(instance, kp_solve, svc_parameters);

        // Check end.
        if (algorithm_formatter.end_boolean())
            break;
        if (parameters.timer.needs_to_end())
            break;

        if (parameters.optimization_mode != OptimizationMode::Anytime)
            break;

        // Update beam size.
        queue_size = std::max(
                queue_size + 1,
                (NodeId)(queue_size * 2));
        // Update maximum approximation ratio.
        maximum_approximation_ratio *= parameters.maximum_approximation_ratio_factor;
    }
}

void optimize_sequential_value_correction(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter)
{
    if (parameters.optimization_mode == OptimizationMode::Anytime) {
        optimize_sequential_single_knapsack(
                instance,
                parameters,
                algorithm_formatter,
                parameters.sequential_value_correction_subproblem_tree_search_queue_size - 1);
    }

    SequentialValueCorrectionFunction<Instance, Solution> kp_solve
        = [&algorithm_formatter, &parameters](const Instance& kp_instance)
        {
            OptimizeParameters kp_parameters;
            kp_parameters.verbosity_level = 0;
            kp_parameters.timer = parameters.timer;
            kp_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
            kp_parameters.optimization_mode
                = (parameters.optimization_mode == OptimizationMode::NotAnytimeSequential)?
                OptimizationMode::NotAnytimeSequential:
                OptimizationMode::NotAnytimeDeterministic;
            kp_parameters.not_anytime_maximum_approximation_ratio = parameters.not_anytime_maximum_approximation_ratio;
            kp_parameters.not_anytime_tree_search_queue_size
                = parameters.sequential_value_correction_subproblem_tree_search_queue_size;
            kp_parameters.linear_programming_solver_name = parameters.linear_programming_solver_name;
            auto kp_output = optimize(kp_instance, kp_parameters);
            return kp_output.solution_pool;
        };
    SequentialValueCorrectionParameters<Instance, Solution> svc_parameters;
    svc_parameters.verbosity_level = 0;
    svc_parameters.timer = parameters.timer;
    svc_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    if (parameters.optimization_mode != OptimizationMode::Anytime)
        svc_parameters.maximum_number_of_iterations = parameters.not_anytime_sequential_value_correction_number_of_iterations;
    svc_parameters.new_solution_callback = [&algorithm_formatter](
            const packingsolver::Output<Instance, Solution>& ps_output)
    {
        const SequentialValueCorrectionOutput<Instance, Solution>& pssvc_output
            = static_cast<const SequentialValueCorrectionOutput<Instance, Solution>&>(ps_output);
        std::stringstream ss;
        ss << "SVC it " << pssvc_output.number_of_iterations;
        algorithm_formatter.update_solution(pssvc_output.solution_pool.best(), ss.str());
    };
    sequential_value_correction<Instance, InstanceBuilder, Solution, AlgorithmFormatter>(instance, kp_solve, svc_parameters);
}

void optimize_dichotomic_search(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter)
{
    double waste_percentage_upper_bound = std::numeric_limits<double>::infinity();
    double maximum_approximation_ratio = parameters.initial_maximum_approximation_ratio;
    for (Counter queue_size = 1;;) {

        if (parameters.optimization_mode != OptimizationMode::Anytime) {
            queue_size = parameters.not_anytime_dichotomic_search_subproblem_tree_search_queue_size;
            maximum_approximation_ratio = parameters.not_anytime_maximum_approximation_ratio;
        }

        DichotomicSearchFunction<Instance, Solution> bpp_solve
            = [&algorithm_formatter, &parameters, queue_size, maximum_approximation_ratio](const Instance& bpp_instance)
            {
                OptimizeParameters bpp_parameters;
                bpp_parameters.verbosity_level = 0;
                bpp_parameters.timer = parameters.timer;
                bpp_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
                bpp_parameters.optimization_mode
                    = (parameters.optimization_mode == OptimizationMode::NotAnytimeSequential)?
                    OptimizationMode::NotAnytimeSequential:
                    OptimizationMode::NotAnytimeDeterministic;
                bpp_parameters.not_anytime_maximum_approximation_ratio = maximum_approximation_ratio;
                bpp_parameters.use_tree_search = 1;
                bpp_parameters.not_anytime_tree_search_queue_size = queue_size;
                bpp_parameters.linear_programming_solver_name = parameters.linear_programming_solver_name;
                auto bpp_output = optimize(bpp_instance, bpp_parameters);
                return bpp_output.solution_pool;
            };
        DichotomicSearchParameters<Instance, Solution> ds_parameters;
        ds_parameters.verbosity_level = 0;
        ds_parameters.timer = parameters.timer;
        ds_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
        ds_parameters.initial_waste_percentage_upper_bound = waste_percentage_upper_bound;
        ds_parameters.new_solution_callback = [
            &algorithm_formatter, &queue_size](
                    const packingsolver::Output<Instance, Solution>& ps_output)
            {
                const DichotomicSearchOutput<Instance, Solution>& psds_output
                    = static_cast<const DichotomicSearchOutput<Instance, Solution>&>(ps_output);
                std::stringstream ss;
                ss << "DS q " << queue_size
                    << " w " << psds_output.waste_percentage;
                algorithm_formatter.update_solution(psds_output.solution_pool.best(), ss.str());
            };
        auto ds_output = dichotomic_search<Instance, InstanceBuilder, Solution, AlgorithmFormatter>(instance, bpp_solve, ds_parameters);

        // Check end.
        if (algorithm_formatter.end_boolean())
            break;
        if (parameters.timer.needs_to_end())
            break;

        if (parameters.optimization_mode != OptimizationMode::Anytime)
            break;

        // Update beam size.
        queue_size = std::max(
                queue_size + 1,
                (NodeId)(queue_size * 2));
        // Update maximum approximation ratio.
        maximum_approximation_ratio *= parameters.maximum_approximation_ratio_factor;

        waste_percentage_upper_bound = ds_output.waste_percentage_upper_bound;
    }
}

void optimize_column_generation(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter)
{
    ColumnGenerationPricingFunction<Instance, InstanceBuilder, Solution> pricing_function
        = [&algorithm_formatter, &parameters](const Instance& kp_instance)
        {
            OptimizeParameters kp_parameters;
            kp_parameters.verbosity_level = 0;
            kp_parameters.timer = parameters.timer;
            kp_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
            kp_parameters.optimization_mode
                = (parameters.optimization_mode == OptimizationMode::NotAnytimeSequential)?
                OptimizationMode::NotAnytimeSequential:
                OptimizationMode::NotAnytimeDeterministic;
            kp_parameters.not_anytime_maximum_approximation_ratio = parameters.not_anytime_maximum_approximation_ratio;
            kp_parameters.not_anytime_tree_search_queue_size
                = parameters.column_generation_subproblem_tree_search_queue_size;
            kp_parameters.linear_programming_solver_name = parameters.linear_programming_solver_name;
            return optimize(kp_instance, kp_parameters);
        };

    columngenerationsolver::Model cgs_model = get_model<Instance, InstanceBuilder, Solution>(instance, pricing_function);
    columngenerationsolver::LimitedDiscrepancySearchParameters cgslds_parameters;
    cgslds_parameters.verbosity_level = 0;
    cgslds_parameters.timer = parameters.timer;
    cgslds_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    cgslds_parameters.internal_diving = 1;
    cgslds_parameters.dummy_column_objective_coefficient = 2 * (double)instance.largest_item_copies();
    if (parameters.optimization_mode != OptimizationMode::Anytime)
        cgslds_parameters.automatic_stop = true;
    cgslds_parameters.new_solution_callback = [&instance, &algorithm_formatter](
            const columngenerationsolver::Output& cgs_output)
    {
        const columngenerationsolver::LimitedDiscrepancySearchOutput& cgslds_output
            = static_cast<const columngenerationsolver::LimitedDiscrepancySearchOutput&>(cgs_output);
        if (cgslds_output.solution.feasible()) {
            Solution solution(instance);
            for (const auto& pair: cgslds_output.solution.columns()) {
                const Column& column = *(pair.first);
                BinPos value = std::round(pair.second);
                if (value < 0.5)
                    continue;
                solution.append(
                        *std::static_pointer_cast<Solution>(column.extra),
                        0,
                        value);
            }
            std::stringstream ss;
            ss << "CG n " << cgslds_output.number_of_nodes;
            algorithm_formatter.update_solution(solution, ss.str());
        }
    };
    cgslds_parameters.column_generation_parameters.solver_name
        = parameters.linear_programming_solver_name;
    columngenerationsolver::limited_discrepancy_search(cgs_model, cgslds_parameters);
}

void optimize_sequential_feasibility(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter)
{
    SequentialFeasibilityParameters sf_parameters;
    sf_parameters.verbosity_level = 0;
    sf_parameters.timer = parameters.timer;
    sf_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    sf_parameters.optimization_mode = parameters.optimization_mode;
    sf_parameters.not_anytime_maximum_approximation_ratio
        = parameters.not_anytime_maximum_approximation_ratio;
    sf_parameters.not_anytime_tree_search_queue_size
        = parameters.not_anytime_tree_search_queue_size;
    sf_parameters.linear_programming_solver_name = parameters.linear_programming_solver_name;
    sf_parameters.use_tree_search = parameters.sequential_feasibility_use_tree_search;
    sf_parameters.use_local_search = parameters.sequential_feasibility_use_local_search;
    sf_parameters.use_milp_raster = parameters.sequential_feasibility_use_milp_raster;
    sf_parameters.new_solution_callback = [&algorithm_formatter](
            const packingsolver::Output<Instance, Solution>& output) {
        algorithm_formatter.update_solution(
                output.solution_pool.best(),
                "Sequential feasibility");
    };
    sequential_feasibility(instance, sf_parameters);
}

void optimize_milp_raster(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter)
{
    MilpRasterParameters milp_raster_parameters;
    milp_raster_parameters.verbosity_level = 0;
    milp_raster_parameters.timer = parameters.timer;
    milp_raster_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    milp_raster_parameters.new_solution_callback = [&algorithm_formatter](
            const packingsolver::Output<Instance, Solution>& output) {
        algorithm_formatter.update_solution(
                output.solution_pool.best(),
                "MILP raster");
    };
    milp_raster(instance, milp_raster_parameters);
}

}

packingsolver::irregular::Output packingsolver::irregular::optimize(
        const Instance& instance,
        const OptimizeParameters& parameters)
{
    Output output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    // Select algorithms to run.
    ItemPos mean_number_of_items_in_bins
        = largest_bin_space(instance) / mean_item_space(instance);
    bool use_tree_search = parameters.use_tree_search;
    bool use_local_search = parameters.use_local_search;
    bool use_sequential_single_knapsack = parameters.use_sequential_single_knapsack;
    bool use_sequential_value_correction = parameters.use_sequential_value_correction;
    bool use_dichotomic_search = parameters.use_dichotomic_search;
    bool use_column_generation = parameters.use_column_generation;
    bool use_sequential_feasibility = parameters.use_sequential_feasibility;
    bool use_milp_raster = parameters.use_milp_raster;
    if (instance.number_of_bins() <= 1) {
        // Disable algorithms which are not available for this objective.
        use_sequential_single_knapsack = false;
        use_sequential_value_correction = false;
        use_dichotomic_search = false;
        use_column_generation = false;
        if (instance.objective() == Objective::OpenDimensionXY)
            use_tree_search = false;
        if (instance.objective() == Objective::Knapsack)
            use_sequential_feasibility = false;
        if (instance.objective() != Objective::Knapsack
                && instance.objective() != Objective::Feasibility) {
            use_milp_raster = false;
        }
        if (instance.objective() != Objective::Feasibility) {
            use_local_search = false;
        }
        // Automatic selection.
        if (!use_tree_search
                && !use_local_search
                && !use_milp_raster
                && !use_sequential_feasibility) {
            if (instance.objective() == Objective::OpenDimensionXY) {
                use_sequential_feasibility = true;
            } else {
                use_tree_search = true;
            }
        }
    } else if (instance.objective() == Objective::Knapsack) {
        // Disable algorithms which are not available for this objective.
        use_local_search = false;
        use_dichotomic_search = false;
        use_sequential_feasibility = false;
        // Automatic selection.
        if (!use_tree_search
                && !use_milp_raster
                && !use_sequential_single_knapsack
                && !use_sequential_value_correction
                && !use_column_generation) {
            if (mean_item_type_copies(instance)
                    > parameters.many_item_type_copies_factor
                    * mean_number_of_items_in_bins) {
                if (mean_number_of_items_in_bins
                        > parameters.many_items_in_bins_threshold) {
                    use_sequential_single_knapsack = true;
                } else {
                    use_sequential_value_correction = true;
                    use_column_generation = true;
                }
            } else {
                use_tree_search = true;
                use_column_generation = true;
            }
        }
    } else if (instance.objective() == Objective::Feasibility) {
        // Disable algorithms which are not available for this objective.
        use_dichotomic_search = false;
        use_sequential_feasibility = false;
        // Automatic selection.
        if (!use_tree_search
                && !use_milp_raster
                && !use_local_search
                && !use_column_generation) {
            use_tree_search = true;
            //if (mean_item_type_copies(instance)
            //        > parameters.many_item_type_copies_factor
            //        * mean_number_of_items_in_bins) {
            //    if (mean_number_of_items_in_bins
            //            > parameters.many_items_in_bins_threshold) {
            //        use_sequential_single_knapsack = true;
            //    } else {
            //        use_sequential_value_correction = true;
            //        use_column_generation = true;
            //    }
            //} else {
            //    use_tree_search = true;
            //    if (mean_number_of_items_in_bins
            //            > parameters.many_items_in_bins_threshold) {
            //        use_sequential_single_knapsack = true;
            //    } else {
            //        use_sequential_value_correction = true;
            //        use_column_generation = true;
            //    }
            //}
        }
    } else if (instance.objective() == Objective::BinPacking
            || instance.objective() == Objective::BinPackingWithLeftovers) {
        // Disable algorithms which are not available for this objective.
        if (instance.number_of_bin_types() > 1)
            use_column_generation = false;
        use_dichotomic_search = false;
        use_local_search = false;
        use_milp_raster = false;
        // Automatic selection.
        if (!use_tree_search
                && !use_milp_raster
                && !use_sequential_feasibility
                && !use_sequential_single_knapsack
                && !use_sequential_value_correction
                && !use_column_generation) {
            if (mean_item_type_copies(instance)
                    > parameters.many_item_type_copies_factor
                    * mean_number_of_items_in_bins) {
                if (mean_number_of_items_in_bins
                        > parameters.many_items_in_bins_threshold) {
                    use_sequential_single_knapsack = true;
                } else {
                    use_sequential_value_correction = true;
                    if (instance.number_of_bin_types() == 1)
                        use_column_generation = true;
                }
            } else {
                use_tree_search = true;
                if (mean_number_of_items_in_bins
                        > parameters.many_items_in_bins_threshold) {
                    use_sequential_single_knapsack = true;
                } else {
                    use_sequential_value_correction = true;
                    if (instance.number_of_bin_types() == 1)
                        use_column_generation = true;
                }
            }
        }
    } else if (instance.objective() == Objective::VariableSizedBinPacking) {
        // Disable algorithms which are not available for this objective.
        use_milp_raster = false;
        use_local_search = false;
        if (instance.number_of_bin_types() == 1) {
            if (use_dichotomic_search) {
                use_dichotomic_search = false;
                use_tree_search = true;
            }
        }
        use_sequential_feasibility = false;
        // Automatic selection.
        if (!use_tree_search
                && !use_local_search
                && !use_sequential_single_knapsack
                && !use_sequential_value_correction
                && !use_dichotomic_search
                && !use_column_generation) {
            if (mean_item_type_copies(instance)
                    > parameters.many_item_type_copies_factor
                    * mean_number_of_items_in_bins) {
                if (mean_number_of_items_in_bins
                        > parameters.many_items_in_bins_threshold) {
                    use_sequential_single_knapsack = true;
                } else {
                    use_sequential_value_correction = true;
                    use_column_generation = true;
                }
            } else {
                if (mean_number_of_items_in_bins
                        > parameters.many_items_in_bins_threshold) {
                    use_sequential_single_knapsack = true;
                    if (instance.number_of_bin_types() > 1) {
                        use_dichotomic_search = true;
                    } else {
                        use_tree_search = true;
                    }
                } else {
                    use_sequential_value_correction = true;
                    use_column_generation = true;
                }
            }
        }
    }

    if (instance.objective() == Objective::Knapsack)
        algorithm_formatter.update_knapsack_bound(instance.item_profit());

    if (instance.objective() == Objective::BinPacking
            || instance.objective() == Objective::Knapsack
            || instance.objective() == Objective::VariableSizedBinPacking) {
        optimize_onedimensional_bound(
                instance,
                parameters,
                algorithm_formatter);
    }

    if (instance.number_of_items() == 1
            && instance.number_of_bins() == 1
            && (instance.objective() == Objective::Knapsack
                || instance.objective() == Objective::BinPacking
                || instance.objective() == Objective::Feasibility)) {
        optimize_trivial_single_item(
                instance,
                parameters,
                algorithm_formatter);
    }

    if (algorithm_formatter.end_boolean()) {
        algorithm_formatter.end();
        return output;
    }
    if (parameters.timer.needs_to_end()) {
        algorithm_formatter.end();
        return output;
    }

    // Run selected algorithms.
    std::vector<std::function<void()>> tasks;
    std::forward_list<std::exception_ptr> exception_ptr_list;
    // Tree search.
    if (use_tree_search) {
        exception_ptr_list.push_front(std::exception_ptr());
        std::exception_ptr& exception_ptr = exception_ptr_list.front();
        tasks.push_back([&exception_ptr, &instance, &parameters, &algorithm_formatter]() {
            wrapper<decltype(&optimize_tree_search), optimize_tree_search>(
                    exception_ptr,
                    instance,
                    parameters,
                    algorithm_formatter);
        });
    }
    // MILP raster.
    if (use_milp_raster) {
        exception_ptr_list.push_front(std::exception_ptr());
        std::exception_ptr& exception_ptr = exception_ptr_list.front();
        tasks.push_back([&exception_ptr, &instance, &parameters, &algorithm_formatter]() {
            wrapper<decltype(&optimize_milp_raster), optimize_milp_raster>(
                    exception_ptr,
                    instance,
                    parameters,
                    algorithm_formatter);
        });
    }
    // Local search.
    if (use_local_search) {
        exception_ptr_list.push_front(std::exception_ptr());
        std::exception_ptr& exception_ptr = exception_ptr_list.front();
        tasks.push_back([&exception_ptr, &instance, &parameters, &algorithm_formatter]() {
            wrapper<decltype(&optimize_local_search), optimize_local_search>(
                    exception_ptr,
                    instance,
                    parameters,
                    algorithm_formatter);
        });
    }
    // Sequential single knapsack.
    if (use_sequential_single_knapsack) {
        exception_ptr_list.push_front(std::exception_ptr());
        std::exception_ptr& exception_ptr = exception_ptr_list.front();
        tasks.push_back([&exception_ptr, &instance, &parameters, &algorithm_formatter]() {
            wrapper<decltype(&optimize_sequential_single_knapsack), optimize_sequential_single_knapsack>(
                    exception_ptr,
                    instance,
                    parameters,
                    algorithm_formatter,
                    -1);
        });
    }
    // Sequential value correction.
    if (use_sequential_value_correction) {
        exception_ptr_list.push_front(std::exception_ptr());
        std::exception_ptr& exception_ptr = exception_ptr_list.front();
        tasks.push_back([&exception_ptr, &instance, &parameters, &algorithm_formatter]() {
            wrapper<decltype(&optimize_sequential_value_correction), optimize_sequential_value_correction>(
                    exception_ptr,
                    instance,
                    parameters,
                    algorithm_formatter);
        });
    }
    // Dichotomic search.
    if (use_dichotomic_search) {
        exception_ptr_list.push_front(std::exception_ptr());
        std::exception_ptr& exception_ptr = exception_ptr_list.front();
        tasks.push_back([&exception_ptr, &instance, &parameters, &algorithm_formatter]() {
            wrapper<decltype(&optimize_dichotomic_search), optimize_dichotomic_search>(
                    exception_ptr,
                    instance,
                    parameters,
                    algorithm_formatter);
        });
    }
    // Column generation.
    if (use_column_generation) {
        exception_ptr_list.push_front(std::exception_ptr());
        std::exception_ptr& exception_ptr = exception_ptr_list.front();
        tasks.push_back([&exception_ptr, &instance, &parameters, &algorithm_formatter]() {
            wrapper<decltype(&optimize_column_generation), optimize_column_generation>(
                    exception_ptr,
                    instance,
                    parameters,
                    algorithm_formatter);
        });
    }
    // Sequential feasibility.
    if (use_sequential_feasibility) {
        exception_ptr_list.push_front(std::exception_ptr());
        std::exception_ptr& exception_ptr = exception_ptr_list.front();
        tasks.push_back([&exception_ptr, &instance, &parameters, &algorithm_formatter]() {
            wrapper<decltype(&optimize_sequential_feasibility), optimize_sequential_feasibility>(
                    exception_ptr,
                    instance,
                    parameters,
                    algorithm_formatter);
        });
    }
    run(tasks, algorithm_formatter, parameters);
    for (std::exception_ptr exception_ptr: exception_ptr_list)
        if (exception_ptr)
            std::rethrow_exception(exception_ptr);

    const Solution& solution_best = output.solution_pool.best();
    if (instance.objective() == Objective::BinPackingWithLeftovers
            && parameters.optimization_mode != OptimizationMode::Anytime
            && parameters.tree_search_guides != std::vector<GuideId>({2, 3})
            && solution_best.number_of_bins() > 0
            && solution_best.bin(solution_best.number_of_different_bins() - 1).copies == 1) {

        InstanceBuilder last_bin_instance_builder;
        last_bin_instance_builder.set_objective(Objective::BinPackingWithLeftovers);
        last_bin_instance_builder.set_parameters(instance.parameters());

        // Add bin types.
        const SolutionBin& last_bin = solution_best.bin(solution_best.number_of_different_bins() - 1);
        last_bin_instance_builder.add_bin_type(instance, last_bin.bin_type_id, 1);

        // Add item types.
        std::vector<ItemPos> last_bin_item_copies(instance.number_of_item_types(), 0);
        for (const SolutionItem& solution_item: last_bin.items)
            last_bin_item_copies[solution_item.item_type_id]++;

        std::vector<ItemTypeId> last_bin_to_orig;
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = instance.item_type(item_type_id);
            if (last_bin_item_copies[item_type_id] > 0) {
                last_bin_instance_builder.add_item_type(
                        instance,
                        item_type_id,
                        item_type.profit,
                        last_bin_item_copies[item_type_id]);
                last_bin_to_orig.push_back(item_type_id);
            }
        }

        // Build instance.
        Instance last_bin_instance = last_bin_instance_builder.build();

        // Solve instance.
        OptimizeParameters last_bin_parameters;
        last_bin_parameters.verbosity_level = 0;
        last_bin_parameters.timer = parameters.timer;
        last_bin_parameters.optimization_mode = parameters.optimization_mode;
        last_bin_parameters.not_anytime_maximum_approximation_ratio = parameters.not_anytime_maximum_approximation_ratio;
        last_bin_parameters.not_anytime_tree_search_queue_size = parameters.not_anytime_tree_search_queue_size;
        last_bin_parameters.tree_search_guides = {2, 3};
        last_bin_parameters.linear_programming_solver_name = parameters.linear_programming_solver_name;
        auto last_bin_output = optimize(last_bin_instance, last_bin_parameters);

        if (last_bin_output.solution_pool.best().full()) {

            // Retrieve solution.
            Solution solution(instance);
            // Add first bins from current best solution.
            for (BinPos bin_pos = 0;
                    bin_pos < solution_best.number_of_different_bins() - 2;
                    ++bin_pos) {
                const SolutionBin& solution_bin = solution_best.bin(bin_pos);
                solution.append(solution_best, bin_pos, solution_bin.copies);
            }
            // Add last optimized bin.
            solution.append(
                    last_bin_output.solution_pool.best(),
                    0,
                    last_bin.copies,
                    {last_bin.bin_type_id},
                    last_bin_to_orig);

            // Update best solution.
            std::stringstream ss;
            ss << "post-process";
            algorithm_formatter.update_solution(solution, ss.str());

        }
    }

    algorithm_formatter.end();
    return output;
}
