#include "packingsolver/boxstacks/optimize.hpp"

#include "packingsolver/boxstacks/algorithm_formatter.hpp"
#include "packingsolver/boxstacks/instance_builder.hpp"
#include "boxstacks/tree_search.hpp"
#include "boxstacks/sequential_onedimensional_rectangle.hpp"
#include "packingsolver/box/instance_builder.hpp"
#include "box/trivial.hpp"
#include "box/dual_feasible_functions.hpp"
#include "packingsolver/onedimensional/instance_builder.hpp"
#include "packingsolver/onedimensional/optimize.hpp"

#include "algorithms/sequential_value_correction.hpp"

using namespace packingsolver;
using namespace packingsolver::boxstacks;

namespace
{

void optimize_onedimensional_bound(
        const box::Instance& box_instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter)
{
    // Relax the 'box' relaxation further down to 1D, keeping only bin/item
    // volumes: any solution of 'box_instance' is also a solution of this
    // relaxation (with the same cost), so the bound the onedimensional
    // solver finds for it is a valid bound here too. Unlike the full
    // geometric 'box' relaxation, 1D bin packing is solved exactly and
    // cheaply (dichotomic search), so this stays fast regardless of how
    // hard the geometric relaxation would be to search.
    onedimensional::InstanceBuilder onedim_instance_builder;
    onedim_instance_builder.set_objective(box_instance.objective());
    for (BinTypeId bin_type_id = 0;
            bin_type_id < box_instance.number_of_bin_types();
            ++bin_type_id) {
        const box::BinType& bin_type = box_instance.bin_type(bin_type_id);
        BinTypeId onedim_bin_type_id = onedim_instance_builder.add_bin_type(bin_type.volume());
        onedim_instance_builder.set_bin_type_cost(onedim_bin_type_id, bin_type.cost);
        onedim_instance_builder.set_bin_type_copies(onedim_bin_type_id, bin_type.copies);
        onedim_instance_builder.set_bin_type_copies_min(onedim_bin_type_id, bin_type.copies_min);
    }
    for (ItemTypeId item_type_id = 0;
            item_type_id < box_instance.number_of_item_types();
            ++item_type_id) {
        const box::ItemType& item_type = box_instance.item_type(item_type_id);
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
    onedim_parameters.optimization_mode = OptimizationMode::NotAnytime;
    onedim_parameters.linear_programming_solver_name = parameters.linear_programming_solver_name;
    auto onedim_output = onedimensional::optimize(onedim_instance, onedim_parameters);

    switch (box_instance.objective()) {
    case Objective::Knapsack:
        algorithm_formatter.update_knapsack_bound(onedim_output.knapsack_bound);
        break;
    case Objective::BinPacking:
        algorithm_formatter.update_bin_packing_bound(onedim_output.bin_packing_bound);
        break;
    case Objective::VariableSizedBinPacking:
        algorithm_formatter.update_variable_sized_bin_packing_bound(onedim_output.variable_sized_bin_packing_bound);
        break;
    default:
        break;
    }
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
        // Items with a positive nesting height take up less vertical space
        // than their full height once stacked, so that reduced height is
        // used instead (matches how effective item heights are computed
        // throughout boxstacks, e.g. in tree_search.cpp).
        Length item_z = item_type.box.z - std::max(item_type.nesting_height, (Length)0);
        ItemTypeId box_item_type_id = box_instance_builder.add_item_type(
                item_type.box.x,
                item_type.box.y,
                item_z);
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

    // Trivial (closed-form, no search) bound on the 'box' relaxation.
    box::TrivialBoundsParameters trivial_bounds_parameters;
    trivial_bounds_parameters.verbosity_level = 0;
    box::TrivialBoundsOutput trivial_bounds_output = box::trivial_bounds(
            box_instance,
            trivial_bounds_parameters);
    switch (instance.objective()) {
    case Objective::Knapsack:
        algorithm_formatter.update_knapsack_bound(trivial_bounds_output.knapsack_bound);
        break;
    case Objective::BinPacking:
        algorithm_formatter.update_bin_packing_bound(trivial_bounds_output.bin_packing_bound);
        break;
    default:
        break;
    }

    // 1D relaxation bound: solved exactly, so tighter than the trivial bound
    // above whenever bin/item volumes don't fill up evenly, and still cheap.
    optimize_onedimensional_bound(box_instance, parameters, algorithm_formatter);

    // Dual feasible functions bound: a fast (polynomial-time), no-search
    // geometric bound, tighter than the volume-only bounds above since it
    // accounts for the shapes not tiling the bin perfectly. Only valid for a
    // single bin type, and only computed by default up to a certain instance
    // size since it is cubic in the number of item types.
    if ((instance.objective() == Objective::Knapsack
                || instance.objective() == Objective::BinPacking)
            && box_instance.number_of_bin_types() == 1
            && box_instance.number_of_items() <= 50) {
        box::DualFeasibleFunctionsParameters dff_parameters;
        dff_parameters.verbosity_level = 0;
        box::DualFeasibleFunctionsOutput dff_output = box::dual_feasible_functions(
                box_instance,
                dff_parameters);
        switch (instance.objective()) {
        case Objective::Knapsack:
            algorithm_formatter.update_knapsack_bound(dff_output.knapsack_bound);
            break;
        case Objective::BinPacking:
            algorithm_formatter.update_bin_packing_bound(dff_output.bin_packing_bound);
            break;
        default:
            break;
        }
    }
}

// Grows a 'growth_factor' from 1 in 'Anytime' mode (a single pass at the
// algorithms' own 'not_anytime_*' sizes otherwise), the same shape as every
// problem type's own 'optimize_sequential_single_knapsack'. At each level:
// run the sequential_onedimensional_rectangle algorithm ('SOR' - 1D +
// rectangle; its own internal retries over 'number_of_iterations' /
// 'number_of_stack_splits' to work around axle weight infeasibility always
// run to completion within the call, only the rectangle queue size is
// driven by this loop) and then, once that whole process has run its
// course without packing every item and only if it still looks promising
// enough, the boxstacks branching scheme ('tree_search', the 3D fallback)
// at a matching queue size. This way, a time limit or a manual stop still
// leaves tree_search a chance to run, instead of SOR's own queue growth
// (now bounded to one level at a time) silently consuming the whole budget
// by itself before tree_search is ever considered.
//
// The rectangle and tree search queue sizes both grow from their own
// 'anytime_*_initial_queue_size' by the same 'growth_factor', rather than
// from a shared literal '1': their non-anytime sizes differ (1024 vs 512 by
// default), so growing both from the same starting point would have one
// reach its own non-anytime size, and stop mattering, well before the
// other. Scaling the initial sizes by that same ratio (2:1 by default)
// keeps the fraction of "full effort" reached at any given level the same
// for both.
void optimize_sequential_onedimensional_rectangle(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter,
        boxstacks::Output& output)
{
    // The 'not_anytime_*' sizes are the largest queues ever run: the
    // non-anytime modes run a single pass at them, and in 'Anytime' mode the
    // growth below stops once both queues have reached them, the level at
    // those sizes being the last one. Without this bound, an instance that
    // does not pack fully and whose rectangle search does not exhaust its
    // tree at these sizes has no other exit signal, so the loop would keep
    // doubling the queues past them until the time limit, and forever
    // without one.
    NodeId rectangle_queue_size_maximum
        = parameters.not_anytime_sequential_onedimensional_rectangle_rectangle_tree_search_queue_size;
    NodeId tree_search_queue_size_maximum
        = parameters.not_anytime_tree_search_queue_size;
    for (Counter growth_factor = 1;;) {
        NodeId rectangle_queue_size = std::min(
                rectangle_queue_size_maximum,
                parameters.anytime_sequential_onedimensional_rectangle_rectangle_initial_queue_size
                * growth_factor);
        NodeId tree_search_queue_size = std::min(
                tree_search_queue_size_maximum,
                parameters.anytime_tree_search_initial_queue_size
                * growth_factor);
        if (parameters.optimization_mode != OptimizationMode::Anytime) {
            rectangle_queue_size = rectangle_queue_size_maximum;
            tree_search_queue_size = tree_search_queue_size_maximum;
        }
        bool last_level
            = (parameters.optimization_mode != OptimizationMode::Anytime)
            || (rectangle_queue_size >= rectangle_queue_size_maximum
                    && tree_search_queue_size >= tree_search_queue_size_maximum);

        auto sor_begin = std::chrono::steady_clock::now();

        SequentialOneDimensionalRectangleParameters sor_parameters;
        sor_parameters.verbosity_level = 0;
        sor_parameters.logger = parameters.get_logger();
        sor_parameters.timer = parameters.timer;
        sor_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
        sor_parameters.rectangle_queue_size = rectangle_queue_size;
        sor_parameters.onedimensional_parameters.linear_programming_solver_name = parameters.linear_programming_solver_name;
        sor_parameters.new_solution_callback = [
            &algorithm_formatter, &growth_factor](
                    const boxstacks::Output& ps_output)
            {
                const SequentialOneDimensionalRectangleOutput& sor_output
                    = static_cast<const SequentialOneDimensionalRectangleOutput&>(ps_output);
                std::stringstream ss;
                ss << "SOR it " << sor_output.number_of_iterations
                    << " g " << growth_factor;
                algorithm_formatter.update_solution(sor_output.solution_pool.best(), ss.str());
            };
        auto sor_output = sequential_onedimensional_rectangle(instance, sor_parameters);

        std::stringstream sor_ss;
        sor_ss << "SOR q " << rectangle_queue_size
            << " g " << growth_factor;
        algorithm_formatter.update_solution(sor_output.solution_pool.best(), sor_ss.str());

        auto sor_end = std::chrono::steady_clock::now();
        output.sequential_onedimensional_rectangle_number_of_items = std::max(
                output.sequential_onedimensional_rectangle_number_of_items,
                sor_output.maximum_number_of_items);
        output.sequential_onedimensional_rectangle_profit = sor_output.solution_pool.best().profit();
        output.sequential_onedimensional_rectangle_time += std::chrono::duration_cast<
            std::chrono::duration<double>>(sor_end - sor_begin).count();
        output.sequential_onedimensional_rectangle_onedimensional_time += sor_output.onedimensional_time;
        output.sequential_onedimensional_rectangle_rectangle_time += sor_output.rectangle_time;
        output.number_of_sequential_onedimensional_rectangle_calls++;
        if (!sor_output.solution_pool.best().full())
            output.sequential_onedimensional_rectangle_failed = sor_output.failed;

        bool run_boxstacks_branching_scheme = false;

        // The boxstacks branching scheme is significantly more expensive than
        // SOR above. Run it, once this level's whole 1D + rectangle process
        // has run its course, only if it looks promising enough:
        // - SOR's solution above violated the axle weight constraints (no
        //   reason to expect tree_search would do better otherwise).
        // - The weight of every unpacked item is greater than the remaining
        //   capacity (no way to pack more regardless of geometry).
        // A full pack already sets 'end_boolean()' on its own (every
        // 'update_solution()' call checks 'is_proven_optimal()'), so no
        // separate check for it is needed here.
        if (!algorithm_formatter.end_boolean()
                && !parameters.timer.needs_to_end()) {
            run_boxstacks_branching_scheme = sor_output.failed;
            bool no_lighter_item = true;
            for (ItemTypeId item_type_id = 0;
                    item_type_id < instance.number_of_item_types();
                    ++item_type_id) {
                const ItemType& item_type = instance.item_type(item_type_id);
                if (output.solution_pool.best().item_copies(item_type_id) == item_type.copies)
                    continue;
                if (output.solution_pool.best().item_weight() + item_type.weight <= instance.bin_weight())
                    no_lighter_item = false;
            }
            if (no_lighter_item)
                run_boxstacks_branching_scheme = false;

            if (run_boxstacks_branching_scheme) {
                auto bs_begin = std::chrono::steady_clock::now();

                TreeSearchParameters ts_parameters;
                ts_parameters.verbosity_level = 0;
                ts_parameters.timer = parameters.timer;
                ts_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
                ts_parameters.optimization_mode = OptimizationMode::NotAnytime;
                ts_parameters.not_anytime_tree_search_queue_size = tree_search_queue_size;
                ts_parameters.guides = parameters.tree_search_guides;
                ts_parameters.maximum_number_of_selected_items = output.sequential_onedimensional_rectangle_number_of_items;
                ts_parameters.new_solution_callback = [&algorithm_formatter, &growth_factor](
                        const boxstacks::Output& ts_output)
                    {
                        std::stringstream ss;
                        ss << "TS " << ts_output.solution_pool.best_label()
                            << " g " << growth_factor;
                        algorithm_formatter.update_solution(ts_output.solution_pool.best(), ss.str());
                        // Forward any bound tree_search's own exhaustively-
                        // explored guides proved (see its 'local_outputs'/
                        // 'tssibs_output.optimal' handling): once it matches
                        // the best profit/cost found, this sets
                        // 'end_boolean()' the same way a full pack already
                        // does, letting the loop below stop growing on a
                        // genuinely axle-weight-infeasible instance instead
                        // of only being able to give up when tree_search
                        // isn't tried at all.
                        algorithm_formatter.update_bounds(ts_output);
                    };
                tree_search(instance, ts_parameters);

                auto bs_end = std::chrono::steady_clock::now();
                output.tree_search_time += std::chrono::duration_cast<
                    std::chrono::duration<double>>(bs_end - bs_begin).count();
                output.number_of_tree_search_calls++;
                if (output.solution_pool.best().number_of_items() == instance.number_of_items()) {
                    output.number_of_tree_search_perfect++;
                } else if (output.solution_pool.best().profit() > sor_output.solution_pool.best().profit()) {
                    output.number_of_tree_search_better++;
                }
            } else if (growth_factor == 1) {
                output.number_of_sequential_onedimensional_rectangle_good++;
            }
        }

        if (growth_factor == 1
                && output.solution_pool.best().number_of_items() == instance.number_of_items())
            output.number_of_sequential_onedimensional_rectangle_perfect++;

        // Check end.
        if (algorithm_formatter.end_boolean())
            break;
        if (parameters.timer.needs_to_end())
            break;
        if (last_level)
            break;
        // A larger rectangle queue size is guaranteed not to change SOR's
        // result (see 'rectangle_subproblem_explored_exhaustively''s own
        // doc comment). That alone isn't enough to stop, though: tree_search
        // (a distinct search space) may still benefit from growing further,
        // so only stop here when it isn't even being tried - otherwise
        // growing further would be pointless on both fronts, and on a
        // small/easy instance could otherwise keep doubling long after
        // every call above returns in microseconds, until 'growth_factor'
        // overflows.
        if (!run_boxstacks_branching_scheme
                && sor_output.rectangle_subproblem_explored_exhaustively)
            break;

        growth_factor = std::max(
                growth_factor + 1,
                (Counter)(growth_factor * 2));
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

    // Instance reduction (see 'Reduction'): applied once, upfront, wrapping
    // the whole dispatch logic below uniformly for every algorithm -
    // mirrors 'rectangle::optimize''s own wrapping of its own 'Reduction'.
    // 'reduced_parameters.reduction_parameters.reduce' is set to 'false'
    // below to avoid re-running the reduction recursively on the
    // already-reduced instance.
    if (parameters.reduction_parameters.reduce) {
        ReductionParameters reduction_parameters = parameters.reduction_parameters;
        reduction_parameters.timer = parameters.timer;
        Reduction reduction(instance, reduction_parameters);

        // Forwards a solution/bound found for the reduced instance to the
        // original 'algorithm_formatter', in original-instance coordinates.
        // Neither reduction operation this class performs can ever hide an
        // item in a dedicated bin or prove the instance infeasible by
        // itself (unlike 'rectangle::Reduction'), so no bound needs any
        // reduction-specific translation: 'reduced_output's bounds already
        // are the original instance's bounds directly.
        auto report_reduced_output = [&reduction, &algorithm_formatter](
                const boxstacks::Output& reduced_output)
            {
                algorithm_formatter.update_solution(
                        reduction.unreduce_solution(reduced_output.solution_pool.best()),
                        reduced_output.solution_pool.best_label());
                algorithm_formatter.update_bounds(reduced_output);
            };

        OptimizeParameters reduced_parameters = parameters;
        reduced_parameters.verbosity_level = 0;
        reduced_parameters.reduction_parameters.reduce = false;
        // Forward every solution/bound the recursive solve finds to the
        // original 'algorithm_formatter' as soon as it is found, rather
        // than only once at the very end.
        reduced_parameters.new_solution_callback = report_reduced_output;
        Output reduced_output = optimize(reduction.instance(), reduced_parameters);
        // Also report the final result explicitly: some sub-solves never
        // call 'new_solution_callback' at all - this guarantees the answer
        // is still reported once regardless. Harmless to call again since
        // 'update_solution'/'update_bounds' are themselves no-ops for
        // anything that doesn't improve on what is already recorded.
        report_reduced_output(reduced_output);

        algorithm_formatter.end();
        return output;
    }

    if (parameters.use_box_bounds
            && (instance.objective() == Objective::Knapsack
                || instance.objective() == Objective::BinPacking
                || instance.objective() == Objective::VariableSizedBinPacking)) {
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

        optimize_sequential_onedimensional_rectangle(
                instance,
                parameters,
                algorithm_formatter,
                output);

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
