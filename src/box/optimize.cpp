#include "packingsolver/box/optimize.hpp"

#include "packingsolver/box/algorithm_formatter.hpp"
#include "packingsolver/box/instance_builder.hpp"
#include "packingsolver/box/post_process.hpp"
#include "box/tree_search.hpp"
#include "box/tree_search_maximal_spaces.hpp"
#include "box/dual_feasible_functions.hpp"
#include "box/trivial.hpp"
#include "packingsolver/onedimensional/instance_builder.hpp"
#include "packingsolver/onedimensional/optimize.hpp"
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
    TrivialBoundsParameters trivial_bounds_parameters;
    trivial_bounds_parameters.verbosity_level = 0;
    TrivialBoundsOutput trivial_bounds_output = trivial_bounds(instance, trivial_bounds_parameters);
    algorithm_formatter.update_bounds(trivial_bounds_output);
}

void optimize_onedimensional_bound(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter)
{
    // Relax the instance to 1D, keeping only bin/item volumes: any solution
    // of the original instance is also a solution of this relaxation (with
    // the same cost), so the bound the onedimensional solver finds for it
    // (which runs the same dichotomic search) is a valid lower bound here.
    onedimensional::InstanceBuilder onedim_instance_builder;
    onedim_instance_builder.set_objective(instance.objective());
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        BinTypeId onedim_bin_type_id = onedim_instance_builder.add_bin_type(bin_type.volume());
        onedim_instance_builder.set_bin_type_cost(onedim_bin_type_id, bin_type.cost);
        onedim_instance_builder.set_bin_type_copies(onedim_bin_type_id, bin_type.copies);
        onedim_instance_builder.set_bin_type_copies_min(onedim_bin_type_id, bin_type.copies_min);
    }
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        if (item_type.volume() <= 0)
            continue;
        ItemTypeId onedim_item_type_id = onedim_instance_builder.add_item_type(item_type.volume());
        onedim_instance_builder.set_item_type_profit(onedim_item_type_id, item_type.profit);
        onedim_instance_builder.set_item_type_copies(onedim_item_type_id, item_type.copies);
    }
    onedimensional::Instance onedim_instance = onedim_instance_builder.build();

    onedimensional::OptimizeParameters onedim_parameters;
    onedim_parameters.verbosity_level = 0;
    onedim_parameters.timer = parameters.timer;
    onedim_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    onedim_parameters.optimization_mode
        = (parameters.optimization_mode == OptimizationMode::NotAnytimeSequential)?
        OptimizationMode::NotAnytimeSequential:
        OptimizationMode::NotAnytime;
    onedim_parameters.linear_programming_solver_name = parameters.linear_programming_solver_name;
    auto onedim_output = optimize(onedim_instance, onedim_parameters);

    // The one-dimensional instance is a relaxation: if it is infeasible, so
    // is the instance.
    if (onedim_output.is_proven_infeasible)
        algorithm_formatter.update_is_proven_infeasible();

    switch (instance.objective()) {
    case Objective::BinPacking: {
        algorithm_formatter.update_bin_packing_bound(
                onedim_output.bin_packing_bound);
        break;
    } case Objective::Knapsack: {
        // The 1D relaxation this bound comes from doesn't carry over
        // resources at all. A 'penalize' resource with a negative penalty
        // *increases* the reported profit when triggered (see 'Resource'),
        // so add back the worst case - every such resource triggering at
        // once - to keep the bound valid.
        algorithm_formatter.update_knapsack_bound(
                onedim_output.knapsack_bound + negative_penalty_sum(instance));
        break;
    } case Objective::VariableSizedBinPacking: {
        algorithm_formatter.update_variable_sized_bin_packing_bound(
                onedim_output.variable_sized_bin_packing_bound);
        break;
    } case Objective::BinPackingWithLeftovers: case Objective::Feasibility: {
        // No bound for these objectives; only the infeasibility above.
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
            algorithm_formatter.update_bounds(dff_output);
        };
    dual_feasible_functions(instance, dff_parameters);
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
    ts_parameters.directions = parameters.tree_search_directions;
    ts_parameters.not_anytime_tree_search_queue_size = parameters.not_anytime_tree_search_queue_size;
    ts_parameters.json_search_tree_path = parameters.json_search_tree_path;
    ts_parameters.new_solution_callback = [&algorithm_formatter](
            const box::Output& ts_output)
    {
        algorithm_formatter.update_solution(ts_output.solution_pool.best(), "TS " + ts_output.solution_pool.best_label());
        algorithm_formatter.update_bounds(ts_output);
    };
    tree_search(instance, ts_parameters);
}

void optimize_tree_search_maximal_spaces(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter)
{
    TreeSearchMaximalSpacesParameters ts_ms_parameters;
    ts_ms_parameters.verbosity_level = 0;
    ts_ms_parameters.timer = parameters.timer;
    ts_ms_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    ts_ms_parameters.optimization_mode = parameters.optimization_mode;
    ts_ms_parameters.not_anytime_tree_search_queue_size = parameters.not_anytime_tree_search_maximal_spaces_queue_size;
    ts_ms_parameters.new_solution_callback = [&algorithm_formatter](
            const box::Output& ts_output)
    {
        algorithm_formatter.update_solution(ts_output.solution_pool.best(), "TSMS " + ts_output.solution_pool.best_label());
    };
    tree_search_maximal_spaces(instance, ts_ms_parameters);
}

void optimize_sequential_single_knapsack(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter)
{
    for (Counter iteration = 0, queue_size = 1;; ++iteration) {
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
        // No later iteration to adjust profits away from this initial
        // value - see 'SequentialValueCorrectionParameters::
        // initial_profit_exponent''s own doc comment.
        svc_parameters.initial_profit_exponent = 1.0;
        svc_parameters.new_solution_callback = [
            &algorithm_formatter, iteration](
                    const box::Output& ps_output)
            {
                const SequentialValueCorrectionOutput<Instance, Solution, box::Output>& pssvc_output
                    = static_cast<const SequentialValueCorrectionOutput<Instance, Solution, box::Output>&>(ps_output);
                std::stringstream ss;
                ss << "SSK it " << iteration;
                algorithm_formatter.update_solution(pssvc_output.solution_pool.best(), ss.str());
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
        AlgorithmFormatter& algorithm_formatter)
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
    svc_parameters.initial_profit_exponent = 1.1;
    if (parameters.optimization_mode != OptimizationMode::Anytime)
        svc_parameters.maximum_number_of_iterations = parameters.not_anytime_sequential_value_correction_number_of_iterations;
    svc_parameters.new_solution_callback = [&algorithm_formatter](
            const box::Output& ps_output)
    {
        const SequentialValueCorrectionOutput<Instance, Solution, box::Output>& pssvc_output
            = static_cast<const SequentialValueCorrectionOutput<Instance, Solution, box::Output>&>(ps_output);
        std::stringstream ss;
        ss << "SVC it " << pssvc_output.number_of_iterations;
        algorithm_formatter.update_solution(pssvc_output.solution_pool.best(), ss.str());
    };
    sequential_value_correction<Instance, InstanceBuilder, Solution, AlgorithmFormatter, box::Output>(instance, kp_solve, svc_parameters);
}

void optimize_dichotomic_search(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter)
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
                bpp_parameters.use_tree_search = true;
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
            &algorithm_formatter, &queue_size](
                    const box::Output& ps_output)
            {
                const DichotomicSearchOutput<Instance, Solution, box::Output>& psds_output
                    = static_cast<const DichotomicSearchOutput<Instance, Solution, box::Output>&>(ps_output);
                std::stringstream ss;
                ss << "DS q " << queue_size
                    << " w " << psds_output.waste_percentage;
                algorithm_formatter.update_solution(psds_output.solution_pool.best(), ss.str());
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
        AlgorithmFormatter& algorithm_formatter)
{
    ColumnGenerationPricingFunction<Instance, InstanceBuilder, Solution, box::Output> pricing_function
        = [&algorithm_formatter, &parameters](const Instance& kp_instance, PricingType)
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
    cg_parameters.new_solution_callback = [&algorithm_formatter](
            const box::Output& ps_output)
    {
        algorithm_formatter.update_solution(ps_output.solution_pool.best(), "CG " + ps_output.solution_pool.best_label());
        algorithm_formatter.update_bounds(ps_output);
    };
    column_generation<Instance, InstanceBuilder, Solution, AlgorithmFormatter, box::Output>(instance, pricing_function, cg_parameters);
}

/**
 * Prove the instance infeasible if an item type doesn't fit in any bin type:
 * with every objective but 'Knapsack', every item must be packed.
 *
 * Only uses 'Instance::fits_some_bin', which is a cheap necessary condition.
 */
void optimize_item_types_fit(
        const Instance& instance,
        AlgorithmFormatter& algorithm_formatter)
{
    if (instance.objective() == Objective::Knapsack)
        return;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        if (instance.item_type(item_type_id).copies == 0)
            continue;
        if (!instance.fits_some_bin(item_type_id)) {
            algorithm_formatter.update_is_proven_infeasible();
            return;
        }
    }
}

/**
 * Post-processes for BinPackingWithLeftovers in a not-anytime mode.
 *
 * Last bin post-process: re-optimize just the last bin of 'solution_best'
 * with the tree search (see 'reoptimize_bin'), in case it packs it into less
 * leftover space, whichever algorithm found the solution. It is only applied
 * if the last bin contains fewer items than
 * 'post_process_last_bin_maximum_number_of_items': the tree search may take
 * much longer than the algorithm which found the solution.
 *
 * It is applied to the best solution of each algorithm, not only to the
 * best solution overall: the re-optimized last bin of a solution which isn't
 * the best one may make it better than the best one.
 *
 * Other bins post-process: the leftover values of the other bins only come
 * after the ones of the last bin in the objective, so the algorithms hardly
 * try to keep the free space of the other bins in a single region.
 * Re-optimize each other bin of the best solution, with the tree search with
 * maximal spaces (see 'reoptimize_non_last_bins'). It is only
 * applied if 'leftover_all_bins' is enabled, since it can only improve the
 * leftover values of the other bins.
 */
bool post_process_applies(
        const Instance& instance,
        const OptimizeParameters& parameters)
{
    return instance.objective() == Objective::BinPackingWithLeftovers
        && parameters.optimization_mode != OptimizationMode::Anytime
        && parameters.post_process;
}

void optimize_last_bin_post_process(
        const Instance& instance,
        const OptimizeParameters& parameters,
        const Solution& solution_best,
        AlgorithmFormatter& algorithm_formatter)
{
    if (post_process_applies(instance, parameters)
            && solution_best.number_of_bins() > 0
            && solution_best.bin(solution_best.number_of_different_bins() - 1).copies == 1
            && (ItemPos)solution_best.bin(solution_best.number_of_different_bins() - 1).items.size()
            < parameters.post_process_last_bin_maximum_number_of_items) {
        OptimizeParameters last_bin_parameters;
        last_bin_parameters.timer = parameters.timer;
        last_bin_parameters.optimization_mode = parameters.optimization_mode;
        last_bin_parameters.not_anytime_tree_search_queue_size = parameters.not_anytime_tree_search_queue_size;
        last_bin_parameters.linear_programming_solver_name = parameters.linear_programming_solver_name;
        last_bin_parameters.use_tree_search = true;
        Solution solution = reoptimize_bin(
                solution_best,
                solution_best.number_of_different_bins() - 1,
                last_bin_parameters);
        algorithm_formatter.update_solution(solution, "post-process");
    }
}

void optimize_other_bins_post_process(
        const Instance& instance,
        const OptimizeParameters& parameters,
        const Solution& solution_best,
        AlgorithmFormatter& algorithm_formatter)
{
    if (!post_process_applies(instance, parameters)
            || !instance.parameters().leftover_all_bins) {
        return;
    }
    OptimizeParameters post_process_parameters = parameters;
    post_process_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    // A bin may contain many items: the tree search would take much longer
    // than the algorithm which found it.
    post_process_parameters.use_tree_search = false;
    post_process_parameters.use_tree_search_maximal_spaces = true;
    Solution solution = reoptimize_non_last_bins(
            solution_best,
            post_process_parameters);
    algorithm_formatter.update_solution(solution, "post-process");
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

    optimize_item_types_fit(instance, algorithm_formatter);
    if (output.is_proven_infeasible) {
        algorithm_formatter.end();
        return output;
    }

    // Instance reduction (see 'Reduction'): applied once, upfront, wrapping
    // the whole dispatch logic below uniformly for every algorithm.
    // 'reduced_parameters.reduction_parameters.reduce' is set to 'false'
    // below to avoid re-running the reduction recursively on the
    // already-reduced instance.
    if (parameters.reduction_parameters.reduce) {
        ReductionParameters reduction_parameters = parameters.reduction_parameters;
        reduction_parameters.timer = parameters.timer;
        Reduction reduction(instance, reduction_parameters);

        if (reduction.proven_infeasible()) {
            // The dedicated bins (see 'Reduction::reduce_full_bin_items'/
            // 'reduce_perfect_pairs') take every copy of the bin type while
            // items remain: 'reduction.instance()' is not meaningful.
            algorithm_formatter.update_is_proven_infeasible();
            algorithm_formatter.end();
            return output;
        }

        // Forwards a solution/bound found for the reduced instance to the
        // original 'algorithm_formatter', in original-instance
        // coordinates. Removing/merging items via this reduction never
        // changes the bounds (cost, feasibility, ...), except the dedicated
        // bins (see 'Reduction::number_of_dedicated_bins', 'BinPacking'
        // only), absent from the reduced instance: they are added back onto
        // the bin packing bound.
        auto report_reduced_output = [&reduction, &algorithm_formatter](
                const box::Output& reduced_output)
            {
                algorithm_formatter.update_solution(
                        reduction.unreduce_solution(reduced_output.solution_pool.best()),
                        reduced_output.solution_pool.best_label());
                box::Output translated_output(reduced_output);
                translated_output.bin_packing_bound += reduction.number_of_dedicated_bins();
                algorithm_formatter.update_bounds(translated_output);
            };

        OptimizeParameters reduced_parameters = parameters;
        reduced_parameters.verbosity_level = 0;
        reduced_parameters.reduction_parameters.reduce = false;
        // Forward every solution/bound the recursive solve finds to the
        // original 'algorithm_formatter' as soon as it is found, rather
        // than only once at the very end.
        reduced_parameters.new_solution_callback = report_reduced_output;
        Output reduced_output = optimize(reduction.instance(), reduced_parameters);
        // Also report the final result explicitly: harmless to call again
        // since 'update_solution'/'update_bounds' are themselves no-ops
        // for anything that doesn't improve on what is already recorded.
        report_reduced_output(reduced_output);

        algorithm_formatter.end();
        return output;
    }

    optimize_trivial_bound(instance, algorithm_formatter);

    if (instance.objective() == Objective::BinPacking
            || instance.objective() == Objective::BinPackingWithLeftovers
            || instance.objective() == Objective::Knapsack
            || instance.objective() == Objective::VariableSizedBinPacking
            || instance.objective() == Objective::Feasibility) {
        optimize_onedimensional_bound(
                instance,
                parameters,
                algorithm_formatter);
    }

    if (instance.objective() == Objective::BinPacking
            || instance.objective() == Objective::Feasibility
            || instance.objective() == Objective::Knapsack) {
        // The 3-axis threshold sweep is cubic in the number of item types
        // (against quadratic for the 2D 'rectangle' case), so this is
        // gated more conservatively.
        if (instance.number_of_bin_types() == 1
                && (parameters.use_dual_feasible_functions
                    || instance.number_of_items() <= 50)) {
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

    // Select algorithms to run.
    ItemPos mean_number_of_items_in_bins
        = largest_bin_space(instance) / mean_item_space(instance);
    double mean_item_x = 0;
    double mean_item_y = 0;
    double mean_item_z = 0;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        mean_item_x += (double)item_type.copies * item_type.box.x;
        mean_item_y += (double)item_type.copies * item_type.box.y;
        mean_item_z += (double)item_type.copies * item_type.box.z;
    }
    mean_item_x /= instance.number_of_items();
    mean_item_y /= instance.number_of_items();
    mean_item_z /= instance.number_of_items();
    double mean_bin_x = 0;
    double mean_bin_y = 0;
    double mean_bin_z = 0;
    for (BinPos bin_pos = 0; bin_pos < instance.number_of_bins(); ++bin_pos) {
        const BinType& bin_type = instance.bin_type(instance.bin_type_id(bin_pos));
        mean_bin_x += bin_type.box.x;
        mean_bin_y += bin_type.box.y;
        mean_bin_z += bin_type.box.z;
    }
    mean_bin_x /= instance.number_of_bins();
    mean_bin_y /= instance.number_of_bins();
    mean_bin_z /= instance.number_of_bins();
    double mean_number_of_items_in_x_section
        = (mean_bin_y * mean_bin_z) / (mean_item_y * mean_item_z);
    double mean_number_of_items_in_y_section
        = (mean_bin_x * mean_bin_z) / (mean_item_x * mean_item_z);
    double mean_number_of_items_in_z_section
        = (mean_bin_x * mean_bin_y) / (mean_item_x * mean_item_y);
    // Directions 'tree_search' should run with, when the automatic
    // selection below picks it over 'tree_search_maximal_spaces' by
    // restricting to whichever of 'X'/'Y'/'Z' has few enough items per
    // section (see 'many_items_in_section_threshold''s own doc comment).
    // Empty means "no restriction" (let 'tree_search' decide on its own).
    std::vector<Direction> tree_search_directions_override;
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
        // 'tree_search_maximal_spaces' supports all the objectives with a
        // single bin but 'VariableSizedBinPacking' (through the sequential
        // feasibility algorithm for the objectives other than 'Knapsack' and
        // 'Feasibility').
        if (instance.objective() == Objective::VariableSizedBinPacking)
            use_tree_search_maximal_spaces = false;
        // Automatic selection.
        if (!use_tree_search
                && !use_tree_search_maximal_spaces) {
            if (instance.objective() == Objective::Knapsack
                    || instance.objective() == Objective::Feasibility) {
                // Restrict 'tree_search' to whichever direction(s) keep few
                // enough items per section - its skyline branching factor
                // stays manageable there - falling back to
                // 'tree_search_maximal_spaces' only if no direction does.
                std::vector<Direction> restricted_directions;
                if (mean_number_of_items_in_x_section <= parameters.many_items_in_section_threshold)
                    restricted_directions.push_back(Direction::X);
                if (mean_number_of_items_in_y_section <= parameters.many_items_in_section_threshold)
                    restricted_directions.push_back(Direction::Y);
                if (mean_number_of_items_in_z_section <= parameters.many_items_in_section_threshold)
                    restricted_directions.push_back(Direction::Z);
                if (!restricted_directions.empty()) {
                    use_tree_search = true;
                    tree_search_directions_override = restricted_directions;
                } else {
                    use_tree_search_maximal_spaces = true;
                }
            } else {
                use_tree_search = true;
            }
        }
    } else if (instance.objective() == Objective::Feasibility) {
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
        // 'column_generation' doesn't build item-type rows for
        // 'BinPackingWithLeftovers' at all (see 'get_model' in
        // 'algorithms/column_generation.hpp'); for 'BinPacking' with more
        // than one bin type, it instead falls back to its own sequential
        // feasibility scheme (see 'ColumnGenerationParameters::
        // use_sequential_feasibility'), so no restriction is needed there.
        if (instance.objective() == Objective::BinPackingWithLeftovers
                && instance.number_of_bin_types() > 1) {
            use_column_generation = false;
        }
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
                    if (instance.objective() == Objective::BinPacking)
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
                    if (instance.objective() == Objective::BinPacking)
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

    // Run selected algorithms.
    // In 'NotAnytimeDeterministic' mode, algorithms still run in parallel, but
    // each reports to its own algorithm formatter, writing into its own
    // output, instead of the shared 'algorithm_formatter', so that their
    // results can be replayed into it in a fixed, deterministic order once
    // every algorithm has terminated ('run(tasks, ...)' does not guarantee a
    // deterministic finish order). Each algorithm can still stop as soon as
    // its own solution is proven optimal against the bounds already known,
    // which only depends on its own progress.
    // These local formatters share 'local_parameters': silenced (no
    // messages, no JSON output, no new solution callback), and whose timer
    // also stops on the shared formatter's end boolean (e.g. set by the
    // memory monitor), since the algorithms only add the end boolean of the
    // formatter they report to. A 'unique_ptr' is used since an
    // 'AlgorithmFormatter' is neither copyable nor movable.
    bool deterministic = (parameters.optimization_mode == OptimizationMode::NotAnytimeDeterministic);
    OptimizeParameters local_parameters = parameters;
    local_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    local_parameters.verbosity_level = 0;
    local_parameters.messages_to_stdout = false;
    local_parameters.messages_path = "";
    local_parameters.messages_streams.clear();
    local_parameters.write_json_output = false;
    local_parameters.new_solution_callback = [](const Output&) { };
    // The last bin post-process (see 'optimize_last_bin_post_process') is
    // applied to the best solution of each algorithm, so in that case too,
    // each algorithm reports to its own algorithm formatter. Outside of
    // 'NotAnytimeDeterministic' mode, these local formatters forward every
    // solution and bound to the shared 'algorithm_formatter' as soon as they
    // get it, instead of only at the end.
    bool local = deterministic || post_process_applies(instance, parameters);
    if (local && !deterministic) {
        local_parameters.new_solution_callback = [&algorithm_formatter](const Output& local_output)
        {
            algorithm_formatter.update_solution(
                    local_output.solution_pool.best(),
                    local_output.solution_pool.best_label());
            algorithm_formatter.update_bounds(local_output);
        };
    }
    std::vector<std::unique_ptr<Output>> local_outputs;
    std::vector<std::unique_ptr<AlgorithmFormatter>> local_algorithm_formatters;
    // Return the parameters and the algorithm formatter a new algorithm task
    // should use.
    auto task_context = [&instance, &parameters, &algorithm_formatter, &output,
         local, &local_parameters, &local_outputs, &local_algorithm_formatters]()
        -> std::pair<const OptimizeParameters*, AlgorithmFormatter*>
    {
        if (!local)
            return {&parameters, &algorithm_formatter};
        local_outputs.push_back(std::make_unique<Output>(instance));
        local_outputs.back()->update_bounds(output);
        local_algorithm_formatters.push_back(std::make_unique<AlgorithmFormatter>(
                    instance,
                    local_parameters,
                    *local_outputs.back()));
        return {&local_parameters, local_algorithm_formatters.back().get()};
    };
    std::vector<std::function<void()>> tasks;
    std::forward_list<std::exception_ptr> exception_ptr_list;
    // Tree search.
    if (use_tree_search) {
        exception_ptr_list.push_front(std::exception_ptr());
        std::exception_ptr& exception_ptr = exception_ptr_list.front();
        auto context = task_context();
        // 'tree_search_directions_override' (set by the automatic selection
        // above) takes priority over any direction restriction the caller
        // already set in 'parameters' - the automatic selection only ever
        // runs when the caller left both 'use_tree_search' and
        // 'use_tree_search_maximal_spaces' unset, so there is no explicit
        // choice here to override.
        OptimizeParameters tree_search_parameters = *context.first;
        if (!tree_search_directions_override.empty())
            tree_search_parameters.tree_search_directions = tree_search_directions_override;
        AlgorithmFormatter* tree_search_algorithm_formatter = context.second;
        tasks.push_back([&exception_ptr, &instance, tree_search_parameters, tree_search_algorithm_formatter]() {
            wrapper<decltype(&optimize_tree_search), optimize_tree_search>(
                    exception_ptr,
                    instance,
                    tree_search_parameters,
                    *tree_search_algorithm_formatter);
        });
    }
    // Tree search with maximal spaces.
    if (use_tree_search_maximal_spaces) {
        exception_ptr_list.push_front(std::exception_ptr());
        std::exception_ptr& exception_ptr = exception_ptr_list.front();
        auto context = task_context();
        tasks.push_back([&exception_ptr, &instance, context]() {
            wrapper<decltype(&optimize_tree_search_maximal_spaces), optimize_tree_search_maximal_spaces>(
                    exception_ptr,
                    instance,
                    *context.first,
                    *context.second);
        });
    }
    // Sequential single knapsack.
    if (use_sequential_single_knapsack) {
        exception_ptr_list.push_front(std::exception_ptr());
        std::exception_ptr& exception_ptr = exception_ptr_list.front();
        auto context = task_context();
        tasks.push_back([&exception_ptr, &instance, context]() {
            wrapper<decltype(&optimize_sequential_single_knapsack), optimize_sequential_single_knapsack>(
                    exception_ptr,
                    instance,
                    *context.first,
                    *context.second);
        });
    }
    // Sequential value correction.
    if (use_sequential_value_correction) {
        exception_ptr_list.push_front(std::exception_ptr());
        std::exception_ptr& exception_ptr = exception_ptr_list.front();
        auto context = task_context();
        tasks.push_back([&exception_ptr, &instance, context]() {
            wrapper<decltype(&optimize_sequential_value_correction), optimize_sequential_value_correction>(
                    exception_ptr,
                    instance,
                    *context.first,
                    *context.second);
        });
    }
    // Dichotomic search.
    if (use_dichotomic_search) {
        exception_ptr_list.push_front(std::exception_ptr());
        std::exception_ptr& exception_ptr = exception_ptr_list.front();
        auto context = task_context();
        tasks.push_back([&exception_ptr, &instance, context]() {
            wrapper<decltype(&optimize_dichotomic_search), optimize_dichotomic_search>(
                    exception_ptr,
                    instance,
                    *context.first,
                    *context.second);
        });
    }
    // Column generation.
    if (use_column_generation) {
        exception_ptr_list.push_front(std::exception_ptr());
        std::exception_ptr& exception_ptr = exception_ptr_list.front();
        auto context = task_context();
        tasks.push_back([&exception_ptr, &instance, context]() {
            wrapper<decltype(&optimize_column_generation), optimize_column_generation>(
                    exception_ptr,
                    instance,
                    *context.first,
                    *context.second);
        });
    }
    run(tasks, algorithm_formatter, parameters);
    for (std::exception_ptr exception_ptr: exception_ptr_list)
        if (exception_ptr)
            std::rethrow_exception(exception_ptr);

    // Replay the solutions and bounds found by each algorithm in a fixed,
    // deterministic order (registration order), instead of the
    // (non-deterministic) order in which the algorithms actually finished.
    if (deterministic) {
        for (const auto& local_output: local_outputs) {
            algorithm_formatter.update_solution(
                    local_output->solution_pool.best(),
                    local_output->solution_pool.best_label());
            algorithm_formatter.update_bounds(*local_output);
        }
    }

    // Last bin post-process, on the best solution of each algorithm, and on
    // the best solution overall if it doesn't come from one of them (e.g.
    // found before running them). The best solution is copied since the
    // post-process may replace it.
    if (post_process_applies(instance, parameters)) {
        Solution solution_best = output.solution_pool.best();
        bool solution_best_found_by_an_algorithm = false;
        for (const auto& local_output: local_outputs) {
            const Solution& local_solution = local_output->solution_pool.best();
            if (!(local_solution < solution_best))
                solution_best_found_by_an_algorithm = true;
            optimize_last_bin_post_process(
                    instance,
                    parameters,
                    local_solution,
                    algorithm_formatter);
        }
        if (!solution_best_found_by_an_algorithm) {
            optimize_last_bin_post_process(
                    instance,
                    parameters,
                    solution_best,
                    algorithm_formatter);
        }
    }

    // Other bins post-process, on the best solution overall.
    optimize_other_bins_post_process(
            instance,
            parameters,
            output.solution_pool.best(),
            algorithm_formatter);

    algorithm_formatter.end();
    return output;
}
