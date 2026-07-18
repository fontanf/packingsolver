#include "packingsolver/box/optimize.hpp"

#include "packingsolver/box/algorithm_formatter.hpp"
#include "packingsolver/box/instance_builder.hpp"
#include "box/tree_search.hpp"
#include "box/tree_search_maximal_spaces.hpp"
#include "box/dual_feasible_functions.hpp"
#include "algorithms/dichotomic_search.hpp"
#include "algorithms/sequential_value_correction.hpp"
#include "algorithms/column_generation.hpp"
#include "algorithms/thread_pool.hpp"

using namespace packingsolver;
using namespace packingsolver::box;

namespace
{

void optimize_trivial_bound(
        const Instance& instance,
        AlgorithmFormatter& algorithm_formatter)
{
    if (instance.objective() == Objective::Knapsack) {
        algorithm_formatter.update_knapsack_bound(instance.item_profit());
        return;
    }

    if (instance.objective() == Objective::BinPacking) {
        // Volume-based bound: fill bin types in the order they are
        // provided (as bins are used for this objective) until enough
        // volume is available to fit all the items. Cheap (linear in the
        // number of bin/item types), so useful when there are too many
        // (small) items for the more expensive dual feasible functions
        // bound to run.
        Volume remaining_item_volume = instance.item_volume();
        BinPos bound = 0;
        for (BinTypeId bin_type_id = 0;
                bin_type_id < instance.number_of_bin_types();
                ++bin_type_id) {
            if (remaining_item_volume <= 0)
                break;
            const BinType& bin_type = instance.bin_type(bin_type_id);
            if (bin_type.volume() <= 0)
                continue;
            BinPos bins_needed = (BinPos)((remaining_item_volume + bin_type.volume() - 1) / bin_type.volume());
            BinPos bins_used = std::min(bins_needed, bin_type.copies);
            bound += bins_used;
            remaining_item_volume -= bins_used * bin_type.volume();
        }
        algorithm_formatter.update_bin_packing_bound(bound);
        return;
    }

    if (instance.objective() != Objective::OpenDimensionX
            && instance.objective() != Objective::OpenDimensionY
            && instance.objective() != Objective::OpenDimensionZ) {
        return;
    }

    // Area-based bound: the open dimension cannot be smaller than what is
    // required to fit the total volume of the items in the fixed
    // cross-section of the bin.
    // Item-based bound: the open dimension cannot be smaller than the
    // smallest extent an item can have in that direction, over its allowed
    // rotations, for the item requiring the most space in that direction.
    const auto& bin_type = instance.bin_type(0);
    Volume cross_section = 0;
    if (instance.objective() == Objective::OpenDimensionX) {
        cross_section = bin_type.box.y * bin_type.box.z;
    } else if (instance.objective() == Objective::OpenDimensionY) {
        cross_section = bin_type.box.x * bin_type.box.z;
    } else {
        cross_section = bin_type.box.x * bin_type.box.y;
    }
    Length bound = (cross_section > 0)?
        (Length)((instance.item_volume() + cross_section - 1) / cross_section):
        0;

    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        Length item_min_extent = item_type.box.max();
        for (Rotation rotation: item_type.rotations) {
            Box rotated_box = item_type.box.rotate(rotation);
            Length extent
                = (instance.objective() == Objective::OpenDimensionX)? rotated_box.x:
                (instance.objective() == Objective::OpenDimensionY)? rotated_box.y:
                rotated_box.z;
            item_min_extent = std::min(item_min_extent, extent);
        }
        bound = std::max(bound, item_min_extent);
    }

    if (instance.objective() == Objective::OpenDimensionX) {
        algorithm_formatter.update_open_dimension_x_bound(bound);
    } else if (instance.objective() == Objective::OpenDimensionY) {
        algorithm_formatter.update_open_dimension_y_bound(bound);
    } else {
        algorithm_formatter.update_open_dimension_z_bound(bound);
    }
}

void optimize_dual_feasible_functions(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter)
{
    DualFeasibleFunctionsParameters dff_parameters;
    dff_parameters.verbosity_level = 0;
    dff_parameters.timer = parameters.timer;
    dff_parameters.new_solution_callback
        = [&algorithm_formatter](
                const box::Output& dff_output)
        {
            algorithm_formatter.update_bin_packing_bound(
                    dff_output.bin_packing_bound);
        };
    dual_feasible_functions(instance, dff_parameters);
}

void optimize_tree_search(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter,
        box::Output* local_output)
{
    TreeSearchParameters ts_parameters;
    ts_parameters.verbosity_level = 0;
    ts_parameters.timer = parameters.timer;
    ts_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    ts_parameters.optimization_mode = parameters.optimization_mode;
    ts_parameters.guides = parameters.tree_search_guides;
    ts_parameters.not_anytime_tree_search_queue_size = parameters.not_anytime_tree_search_queue_size;
    ts_parameters.json_search_tree_path = parameters.json_search_tree_path;
    ts_parameters.new_solution_callback = [&algorithm_formatter, local_output](
            const box::Output& ts_output)
    {
        if (local_output != nullptr) {
            local_output->solution_pool.add(ts_output.solution_pool.best(), "TS " + ts_output.solution_pool.best_label());
        } else {
            algorithm_formatter.update_solution(ts_output.solution_pool.best(), "TS " + ts_output.solution_pool.best_label());
        }
        algorithm_formatter.update_bounds(ts_output);
    };
    tree_search(instance, ts_parameters);
}

void optimize_tree_search_maximal_spaces(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter,
        box::Output* local_output)
{
    TreeSearchMaximalSpacesParameters ts_ms_parameters;
    ts_ms_parameters.verbosity_level = 0;
    ts_ms_parameters.timer = parameters.timer;
    ts_ms_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    ts_ms_parameters.optimization_mode = parameters.optimization_mode;
    ts_ms_parameters.not_anytime_tree_search_queue_size = parameters.not_anytime_tree_search_maximal_spaces_queue_size;
    ts_ms_parameters.new_solution_callback = [&algorithm_formatter, local_output](
            const box::Output& ts_output)
    {
        if (local_output != nullptr) {
            local_output->solution_pool.add(ts_output.solution_pool.best(), "TSMS " + ts_output.solution_pool.best_label());
        } else {
            algorithm_formatter.update_solution(ts_output.solution_pool.best(), "TSMS " + ts_output.solution_pool.best_label());
        }
    };
    tree_search_maximal_spaces(instance, ts_ms_parameters);
}

void optimize_sequential_single_knapsack(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter,
        box::Output* local_output)
{
    for (Counter queue_size = 1;;) {
        NodeId queue_size_ms = queue_size;
        if (parameters.optimization_mode != OptimizationMode::Anytime) {
            queue_size = parameters.not_anytime_sequential_single_knapsack_subproblem_tree_search_queue_size;
            queue_size_ms = parameters.not_anytime_sequential_single_knapsack_subproblem_tree_search_maximal_spaces_queue_size;
        }

        SequentialValueCorrectionFunction<Instance, Solution> kp_solve
            = [&algorithm_formatter, &parameters, &queue_size, &queue_size_ms](const Instance& kp_instance)
            {
                OptimizeParameters kp_parameters;
                kp_parameters.verbosity_level = 0;
                kp_parameters.timer = parameters.timer;
                kp_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
                kp_parameters.optimization_mode
                    = (parameters.optimization_mode == OptimizationMode::NotAnytimeSequential)?
                    OptimizationMode::NotAnytimeSequential:
                    OptimizationMode::NotAnytimeDeterministic;
                kp_parameters.not_anytime_tree_search_queue_size = queue_size;
                kp_parameters.not_anytime_tree_search_maximal_spaces_queue_size = queue_size_ms;
                kp_parameters.linear_programming_solver_name = parameters.linear_programming_solver_name;
                auto kp_output = optimize(kp_instance, kp_parameters);
                return kp_output.solution_pool;
            };
        SequentialValueCorrectionParameters<Instance, Solution, box::Output> svc_parameters;
        svc_parameters.verbosity_level = 0;
        svc_parameters.timer = parameters.timer;
        svc_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
        svc_parameters.maximum_number_of_iterations = 1;
        svc_parameters.new_solution_callback = [
            &algorithm_formatter, local_output, &queue_size](
                    const box::Output& ps_output)
            {
                const SequentialValueCorrectionOutput<Instance, Solution, box::Output>& pssvc_output
                    = static_cast<const SequentialValueCorrectionOutput<Instance, Solution, box::Output>&>(ps_output);
                std::stringstream ss;
                ss << "SSK q " << queue_size;
                if (local_output != nullptr) {
                    local_output->solution_pool.add(pssvc_output.solution_pool.best(), ss.str());
                } else {
                    algorithm_formatter.update_solution(pssvc_output.solution_pool.best(), ss.str());
                }
            };
        sequential_value_correction<Instance, InstanceBuilder, Solution, AlgorithmFormatter, box::Output>(instance, kp_solve, svc_parameters);

        // Check end.
        if (algorithm_formatter.end_boolean())
            break;
        if (parameters.timer.needs_to_end())
            break;

        if (parameters.optimization_mode != OptimizationMode::Anytime)
            break;

        queue_size = std::max(
                queue_size + 1,
                (NodeId)(queue_size * 2));
    }
}

void optimize_sequential_value_correction(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter,
        box::Output* local_output)
{
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
            kp_parameters.not_anytime_tree_search_queue_size
                = parameters.sequential_value_correction_subproblem_tree_search_queue_size;
            kp_parameters.not_anytime_tree_search_maximal_spaces_queue_size
                = parameters.sequential_value_correction_subproblem_tree_search_maximal_spaces_queue_size;
            kp_parameters.linear_programming_solver_name = parameters.linear_programming_solver_name;
            auto kp_output = optimize(kp_instance, kp_parameters);
            return kp_output.solution_pool;
        };
    SequentialValueCorrectionParameters<Instance, Solution, box::Output> svc_parameters;
    svc_parameters.verbosity_level = 0;
    svc_parameters.timer = parameters.timer;
    svc_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    if (parameters.optimization_mode != OptimizationMode::Anytime)
        svc_parameters.maximum_number_of_iterations = parameters.not_anytime_sequential_value_correction_number_of_iterations;
    svc_parameters.new_solution_callback = [&algorithm_formatter, local_output](
            const box::Output& ps_output)
    {
        const SequentialValueCorrectionOutput<Instance, Solution, box::Output>& pssvc_output
            = static_cast<const SequentialValueCorrectionOutput<Instance, Solution, box::Output>&>(ps_output);
        std::stringstream ss;
        ss << "SVC it " << pssvc_output.number_of_iterations;
        if (local_output != nullptr) {
            local_output->solution_pool.add(pssvc_output.solution_pool.best(), ss.str());
        } else {
            algorithm_formatter.update_solution(pssvc_output.solution_pool.best(), ss.str());
        }
    };
    sequential_value_correction<Instance, InstanceBuilder, Solution, AlgorithmFormatter, box::Output>(instance, kp_solve, svc_parameters);
}

void optimize_dichotomic_search(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter,
        box::Output* local_output)
{
    double waste_percentage_upper_bound = std::numeric_limits<double>::infinity();
    for (Counter queue_size = 1;;) {

        if (parameters.optimization_mode != OptimizationMode::Anytime)
            queue_size = parameters.not_anytime_dichotomic_search_subproblem_tree_search_queue_size;

        DichotomicSearchFunction<Instance, Solution> bpp_solve
            = [&algorithm_formatter, &parameters, &queue_size](const Instance& bpp_instance)
            {
                OptimizeParameters bpp_parameters;
                bpp_parameters.verbosity_level = 0;
                bpp_parameters.timer = parameters.timer;
                bpp_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
                bpp_parameters.optimization_mode
                    = (parameters.optimization_mode == OptimizationMode::NotAnytimeSequential)?
                    OptimizationMode::NotAnytimeSequential:
                    OptimizationMode::NotAnytimeDeterministic;
                bpp_parameters.use_tree_search = 1;
                bpp_parameters.not_anytime_tree_search_queue_size = queue_size;
                bpp_parameters.linear_programming_solver_name = parameters.linear_programming_solver_name;
                auto bpp_output = optimize(bpp_instance, bpp_parameters);
                return bpp_output.solution_pool;
            };
        DichotomicSearchParameters<Instance, Solution, box::Output> ds_parameters;
        ds_parameters.verbosity_level = 0;
        ds_parameters.timer = parameters.timer;
        ds_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
        ds_parameters.initial_waste_percentage_upper_bound = waste_percentage_upper_bound;
        ds_parameters.new_solution_callback = [
            &algorithm_formatter, local_output, &queue_size](
                    const box::Output& ps_output)
            {
                const DichotomicSearchOutput<Instance, Solution, box::Output>& psds_output
                    = static_cast<const DichotomicSearchOutput<Instance, Solution, box::Output>&>(ps_output);
                std::stringstream ss;
                ss << "DS q " << queue_size
                    << " w " << psds_output.waste_percentage;
                if (local_output != nullptr) {
                    local_output->solution_pool.add(psds_output.solution_pool.best(), ss.str());
                } else {
                    algorithm_formatter.update_solution(psds_output.solution_pool.best(), ss.str());
                }
            };
        auto ds_output = dichotomic_search<Instance, InstanceBuilder, Solution, AlgorithmFormatter, box::Output>(instance, bpp_solve, ds_parameters);

        // Check end.
        if (algorithm_formatter.end_boolean())
            break;
        if (parameters.timer.needs_to_end())
            break;

        if (parameters.optimization_mode != OptimizationMode::Anytime)
            break;

        queue_size = std::max(
                queue_size + 1,
                (NodeId)(queue_size * 2));
        waste_percentage_upper_bound = ds_output.waste_percentage_upper_bound;
    }
}

void optimize_column_generation(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter,
        box::Output* local_output)
{
    ColumnGenerationPricingFunction<Instance, InstanceBuilder, Solution, box::Output> pricing_function
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
            kp_parameters.not_anytime_tree_search_queue_size
                = parameters.column_generation_subproblem_tree_search_queue_size;
            kp_parameters.not_anytime_tree_search_maximal_spaces_queue_size
                = parameters.column_generation_subproblem_tree_search_maximal_spaces_queue_size;
            kp_parameters.linear_programming_solver_name = parameters.linear_programming_solver_name;
            return optimize(kp_instance, kp_parameters);
        };

    ColumnGenerationParameters<Instance, Solution, box::Output> cg_parameters;
    cg_parameters.verbosity_level = 0;
    cg_parameters.timer = parameters.timer;
    cg_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    cg_parameters.optimization_mode = parameters.optimization_mode;
    cg_parameters.linear_programming_solver_name = parameters.linear_programming_solver_name;
    cg_parameters.new_solution_callback = [&algorithm_formatter, local_output](
            const box::Output& ps_output)
    {
        if (local_output != nullptr) {
            local_output->solution_pool.add(ps_output.solution_pool.best(), "CG " + ps_output.solution_pool.best_label());
        } else {
            algorithm_formatter.update_solution(ps_output.solution_pool.best(), "CG " + ps_output.solution_pool.best_label());
        }
        algorithm_formatter.update_bounds(ps_output);
    };
    column_generation<Instance, InstanceBuilder, Solution, AlgorithmFormatter, box::Output>(instance, pricing_function, cg_parameters);
}

}

packingsolver::box::Output packingsolver::box::optimize(
        const Instance& instance,
        const OptimizeParameters& parameters)
{
    box::Output output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    // Select algorithms to run.
    ItemPos mean_number_of_items_in_bins
        = largest_bin_space(instance) / mean_item_space(instance);
    bool use_tree_search = parameters.use_tree_search;
    bool use_tree_search_maximal_spaces = parameters.use_tree_search_maximal_spaces;
    bool use_sequential_single_knapsack = parameters.use_sequential_single_knapsack;
    bool use_sequential_value_correction = parameters.use_sequential_value_correction;
    bool use_dichotomic_search = parameters.use_dichotomic_search;
    bool use_column_generation = parameters.use_column_generation;
    if (instance.number_of_bins() <= 1) {
        use_sequential_single_knapsack = false;
        use_sequential_value_correction = false;
        use_dichotomic_search = false;
        use_column_generation = false;
        if (instance.objective() != Objective::Knapsack
                && instance.objective() != Objective::Feasibility)
            use_tree_search_maximal_spaces = false;
        // Automatic selection.
        if (!use_tree_search
                && !use_tree_search_maximal_spaces) {
            if ((instance.objective() == Objective::Knapsack
                        || instance.objective() == Objective::Feasibility)
                    && mean_number_of_items_in_bins > parameters.many_items_in_bins_threshold_2) {
                use_tree_search_maximal_spaces = true;
            } else {
                use_tree_search = true;
            }
        }
    } else if (instance.objective() == Objective::Knapsack) {
        // Disable algorithms which are not available for this objective.
        use_tree_search_maximal_spaces = false;
        use_dichotomic_search = false;
        // Automatic selection.
        if (!use_tree_search
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
                if (mean_number_of_items_in_bins
                        > parameters.many_items_in_bins_threshold_2) {
                    use_sequential_single_knapsack = true;
                } else {
                    use_tree_search = true;
                    use_column_generation = true;
                }
            }
        }
    } else if (instance.objective() == Objective::BinPacking
            || instance.objective() == Objective::BinPackingWithLeftovers) {
        // Disable algorithms which are not available for this objective.
        use_tree_search_maximal_spaces = false;
        if (instance.number_of_bin_types() > 1)
            use_column_generation = false;
        use_dichotomic_search = false;
        // Automatic selection.
        if (!use_tree_search
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
                if (mean_number_of_items_in_bins
                        > parameters.many_items_in_bins_threshold_2) {
                    use_sequential_single_knapsack = true;
                } else if (mean_number_of_items_in_bins
                        > parameters.many_items_in_bins_threshold) {
                    use_tree_search = true;
                } else {
                    use_tree_search = true;
                    use_sequential_value_correction = true;
                    if (instance.number_of_bin_types() == 1)
                        use_column_generation = true;
                }
            }
        }
    } else if (instance.objective() == Objective::VariableSizedBinPacking) {
        // Disable algorithms which are not available for this objective.
        use_tree_search_maximal_spaces = false;
        if (instance.number_of_bin_types() == 1) {
            if (use_dichotomic_search) {
                use_dichotomic_search = false;
                use_tree_search = true;
            }
        } else {
            use_tree_search = false;
        }
        // Automatic selection.
        if (!use_tree_search
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
                        > parameters.many_items_in_bins_threshold_2) {
                    use_sequential_single_knapsack = true;
                    if (instance.number_of_bin_types() > 1)
                        use_dichotomic_search = true;
                } else if (mean_number_of_items_in_bins
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

    optimize_trivial_bound(instance, algorithm_formatter);

    if (instance.objective() == Objective::BinPacking) {
        // The 3-axis threshold sweep is cubic in the number of item types
        // (against quadratic for the 2D 'rectangle' case), so this is
        // gated more conservatively.
        if (instance.number_of_bin_types() == 1
                && instance.number_of_items() <= 50) {
            optimize_dual_feasible_functions(
                    instance,
                    parameters,
                    algorithm_formatter);
        }
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
    // In 'NotAnytimeDeterministic' mode, algorithms still run in parallel, but
    // each writes its solutions to its own 'local_output' instead of the
    // shared 'algorithm_formatter', so that they can be replayed into it in a
    // fixed, deterministic order once every algorithm has terminated
    // ('run(tasks, ...)' does not guarantee a deterministic finish order).
    // 'local_outputs' owns these; a 'unique_ptr' is used so that it growing
    // does not invalidate the raw pointers captured by the tasks below.
    bool deterministic = (parameters.optimization_mode == OptimizationMode::NotAnytimeDeterministic);
    std::vector<std::unique_ptr<box::Output>> local_outputs;
    std::vector<std::function<void()>> tasks;
    std::forward_list<std::exception_ptr> exception_ptr_list;
    // Tree search.
    if (use_tree_search) {
        exception_ptr_list.push_front(std::exception_ptr());
        std::exception_ptr& exception_ptr = exception_ptr_list.front();
        std::unique_ptr<box::Output> local_output;
        if (deterministic)
            local_output = std::make_unique<box::Output>(instance);
        tasks.push_back([&exception_ptr, &instance, &parameters, &algorithm_formatter, local_output = local_output.get()]() {
            wrapper<decltype(&optimize_tree_search), optimize_tree_search>(
                    exception_ptr,
                    instance,
                    parameters,
                    algorithm_formatter,
                    local_output);
        });
        local_outputs.push_back(std::move(local_output));
    }
    // Tree search with maximal spaces.
    if (use_tree_search_maximal_spaces) {
        exception_ptr_list.push_front(std::exception_ptr());
        std::exception_ptr& exception_ptr = exception_ptr_list.front();
        std::unique_ptr<box::Output> local_output;
        if (deterministic)
            local_output = std::make_unique<box::Output>(instance);
        tasks.push_back([&exception_ptr, &instance, &parameters, &algorithm_formatter, local_output = local_output.get()]() {
            wrapper<decltype(&optimize_tree_search_maximal_spaces), optimize_tree_search_maximal_spaces>(
                    exception_ptr,
                    instance,
                    parameters,
                    algorithm_formatter,
                    local_output);
        });
        local_outputs.push_back(std::move(local_output));
    }
    // Sequential single knapsack.
    if (use_sequential_single_knapsack) {
        exception_ptr_list.push_front(std::exception_ptr());
        std::exception_ptr& exception_ptr = exception_ptr_list.front();
        std::unique_ptr<box::Output> local_output;
        if (deterministic)
            local_output = std::make_unique<box::Output>(instance);
        tasks.push_back([&exception_ptr, &instance, &parameters, &algorithm_formatter, local_output = local_output.get()]() {
            wrapper<decltype(&optimize_sequential_single_knapsack), optimize_sequential_single_knapsack>(
                    exception_ptr,
                    instance,
                    parameters,
                    algorithm_formatter,
                    local_output);
        });
        local_outputs.push_back(std::move(local_output));
    }
    // Sequential value correction.
    if (use_sequential_value_correction) {
        exception_ptr_list.push_front(std::exception_ptr());
        std::exception_ptr& exception_ptr = exception_ptr_list.front();
        std::unique_ptr<box::Output> local_output;
        if (deterministic)
            local_output = std::make_unique<box::Output>(instance);
        tasks.push_back([&exception_ptr, &instance, &parameters, &algorithm_formatter, local_output = local_output.get()]() {
            wrapper<decltype(&optimize_sequential_value_correction), optimize_sequential_value_correction>(
                    exception_ptr,
                    instance,
                    parameters,
                    algorithm_formatter,
                    local_output);
        });
        local_outputs.push_back(std::move(local_output));
    }
    // Dichotomic search.
    if (use_dichotomic_search) {
        exception_ptr_list.push_front(std::exception_ptr());
        std::exception_ptr& exception_ptr = exception_ptr_list.front();
        std::unique_ptr<box::Output> local_output;
        if (deterministic)
            local_output = std::make_unique<box::Output>(instance);
        tasks.push_back([&exception_ptr, &instance, &parameters, &algorithm_formatter, local_output = local_output.get()]() {
            wrapper<decltype(&optimize_dichotomic_search), optimize_dichotomic_search>(
                    exception_ptr,
                    instance,
                    parameters,
                    algorithm_formatter,
                    local_output);
        });
        local_outputs.push_back(std::move(local_output));
    }
    // Column generation.
    if (use_column_generation) {
        exception_ptr_list.push_front(std::exception_ptr());
        std::exception_ptr& exception_ptr = exception_ptr_list.front();
        std::unique_ptr<box::Output> local_output;
        if (deterministic)
            local_output = std::make_unique<box::Output>(instance);
        tasks.push_back([&exception_ptr, &instance, &parameters, &algorithm_formatter, local_output = local_output.get()]() {
            wrapper<decltype(&optimize_column_generation), optimize_column_generation>(
                    exception_ptr,
                    instance,
                    parameters,
                    algorithm_formatter,
                    local_output);
        });
        local_outputs.push_back(std::move(local_output));
    }
    run(tasks, algorithm_formatter, parameters);
    for (std::exception_ptr exception_ptr: exception_ptr_list)
        if (exception_ptr)
            std::rethrow_exception(exception_ptr);

    // Replay the solutions found by each algorithm in a fixed, deterministic
    // order (registration order), instead of the (non-deterministic) order in
    // which the algorithms actually finished.
    if (deterministic) {
        for (const auto& local_output: local_outputs) {
            algorithm_formatter.update_solution(
                    local_output->solution_pool.best(),
                    local_output->solution_pool.best_label());
        }
    }

    algorithm_formatter.end();
    return output;
}
