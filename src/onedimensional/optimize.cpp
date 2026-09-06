#include "packingsolver/onedimensional/optimize.hpp"

#include "packingsolver/onedimensional/algorithm_formatter.hpp"
#include "packingsolver/onedimensional/instance_builder.hpp"
#include "onedimensional/solution_builder.hpp"
#include "onedimensional/tree_search.hpp"
#include "onedimensional/milp_assignment.hpp"
#include "onedimensional/dual_feasible_functions.hpp"
#include "algorithms/dichotomic_search.hpp"
#include "algorithms/sequential_value_correction.hpp"
#include "algorithms/column_generation.hpp"
#include "algorithms/thread_pool.hpp"

#include "knapsacksolver/instance_builder.hpp"
#include "knapsacksolver/algorithms/dynamic_programming_primal_dual.hpp"


using namespace packingsolver;
using namespace packingsolver::onedimensional;

namespace
{

void optimize_trivial_bound(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter)
{
    if (instance.objective() == Objective::Knapsack) {
        // 1D continuous relaxation (length-based Dantzig bound): pool the
        // length of all bins together (ignoring the constraint that items
        // must be split across discrete, separate bins), sort items by
        // decreasing profit/length ratio, and greedily fill that pooled
        // capacity, taking the last item fractionally. Always a valid,
        // cheap (O(n log n), no search) upper bound, and much tighter than
        // the trivial "sum of all profits" whenever item profits aren't
        // roughly proportional to their length. Items with a positive
        // nesting length take up less space than their full length once
        // packed next to another copy of the same type, so that reduced
        // length is used instead, matching the bin packing bound below.
        // Items longer than every bin type can never be packed (even a
        // single, un-nested copy needs its full length to fit), so they
        // must be excluded entirely rather than counted as fractionally
        // packable length.
        Length total_capacity = 0;
        for (BinTypeId bin_type_id = 0;
                bin_type_id < instance.number_of_bin_types();
                ++bin_type_id) {
            const BinType& bin_type = instance.bin_type(bin_type_id);
            total_capacity += bin_type.length * bin_type.copies;
        }
        std::vector<ItemTypeId> sorted_item_types;
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            if (instance.fits_some_bin(item_type_id))
                sorted_item_types.push_back(item_type_id);
        }
        std::sort(
                sorted_item_types.begin(),
                sorted_item_types.end(),
                [&instance](ItemTypeId item_type_id_1, ItemTypeId item_type_id_2) -> bool
                {
                    const ItemType& item_type_1 = instance.item_type(item_type_id_1);
                    const ItemType& item_type_2 = instance.item_type(item_type_id_2);
                    Length length_1 = item_type_1.length - std::max(item_type_1.nesting_length, (Length)0);
                    Length length_2 = item_type_2.length - std::max(item_type_2.nesting_length, (Length)0);
                    return item_type_1.profit * length_2
                        > item_type_2.profit * length_1;
                });
        Profit bound = 0.0;
        Length remaining_capacity = total_capacity;
        for (ItemTypeId item_type_id: sorted_item_types) {
            if (remaining_capacity <= 0)
                break;
            const ItemType& item_type = instance.item_type(item_type_id);
            Length length = item_type.length - std::max(item_type.nesting_length, (Length)0);
            if (length <= 0)
                continue;
            Length item_total_length = length * item_type.copies;
            if (item_total_length <= remaining_capacity) {
                bound += item_type.profit * item_type.copies;
                remaining_capacity -= item_total_length;
            } else {
                bound += item_type.profit
                    * ((double)remaining_capacity / length);
                remaining_capacity = 0;
            }
        }
        // This bound ignores resources entirely. A 'penalize' resource with
        // a negative penalty *increases* the reported profit when
        // triggered (see 'Resource'), so add back the worst case - every
        // such resource triggering at once - to keep the bound valid.
        bound += negative_penalty_sum(instance);
        algorithm_formatter.update_knapsack_bound(bound);
        return;
    }

    if (instance.objective() == Objective::BinPacking) {
        // Length-based bound: fill bin types in the order they are
        // provided (as bins are used for this objective) until enough
        // length is available to fit all the items. Cheap (linear in the
        // number of bin/item types). Items with a positive nesting length
        // take up less space than their full length once packed next to
        // another copy of the same type, so that reduced length is used
        // instead.
        Length remaining_item_length = 0;
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = instance.item_type(item_type_id);
            remaining_item_length += item_type.copies
                * (item_type.length - std::max(item_type.nesting_length, (Length)0));
        }
        BinPos bound = 0;
        for (BinTypeId bin_type_id = 0;
                bin_type_id < instance.number_of_bin_types();
                ++bin_type_id) {
            if (remaining_item_length <= 0)
                break;
            const BinType& bin_type = instance.bin_type(bin_type_id);
            if (bin_type.length <= 0)
                continue;
            BinPos bins_needed = (BinPos)((remaining_item_length + bin_type.length - 1) / bin_type.length);
            BinPos bins_used = std::min(bins_needed, bin_type.copies);
            bound += bins_used;
            remaining_item_length -= bins_used * bin_type.length;
        }
        algorithm_formatter.update_bin_packing_bound(bound);
        return;
    }

    if (instance.objective() == Objective::VariableSizedBinPacking) {
        // Same bin-selection knapsack as the one solved by the dichotomic
        // search (see 'algorithms/dichotomic_search.hpp'), but solved once
        // at zero waste instead of iteratively: a real solution can never
        // have negative waste, so the minimum cost of a bin selection whose
        // total length covers the (nesting-reduced) item lengths exactly is
        // a valid lower bound on the actual solution's cost.
        //
        // Bin copies beyond 'copies_min' are knapsack items (weight =
        // length, profit = cost); leaving a bin out of the knapsack means
        // using it, so maximizing the profit (cost) of the bins put in the
        // knapsack (i.e. left unused) is equivalent to minimizing the cost
        // of the bins actually used.
        Length bin_length = 0;
        Length bin_min_length = 0;
        Profit total_cost = 0;
        for (BinTypeId bin_type_id = 0;
                bin_type_id < instance.number_of_bin_types();
                ++bin_type_id) {
            const BinType& bin_type = instance.bin_type(bin_type_id);
            bin_length += bin_type.length * bin_type.copies;
            bin_min_length += bin_type.length * bin_type.copies_min;
            total_cost += bin_type.cost * bin_type.copies;
        }
        Length item_length = 0;
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = instance.item_type(item_type_id);
            item_length += item_type.copies
                * (item_type.length - std::max(item_type.nesting_length, (Length)0));
        }

        Length kp_capacity = bin_length - bin_min_length - item_length;
        if (kp_capacity <= 0) {
            // No bin can be left unused: they are all needed just to cover
            // the item lengths.
            algorithm_formatter.update_variable_sized_bin_packing_bound(total_cost);
            return;
        }

        InstanceBuilder kp_instance_builder;
        kp_instance_builder.set_objective(Objective::Knapsack);
        kp_instance_builder.add_bin_type(kp_capacity);
        for (BinTypeId bin_type_id = 0;
                bin_type_id < instance.number_of_bin_types();
                ++bin_type_id) {
            const BinType& bin_type = instance.bin_type(bin_type_id);
            BinPos optional_copies = bin_type.copies - bin_type.copies_min;
            if (optional_copies <= 0
                    || bin_type.length <= 0
                    || bin_type.length > kp_capacity) {
                continue;
            }
            ItemTypeId kp_item_type_id = kp_instance_builder.add_item_type(bin_type.length);
            kp_instance_builder.set_item_type_profit(kp_item_type_id, bin_type.cost);
            kp_instance_builder.set_item_type_copies(kp_item_type_id, optional_copies);
        }
        Instance kp_instance = kp_instance_builder.build();

        OptimizeParameters kp_parameters;
        kp_parameters.verbosity_level = 0;
        kp_parameters.timer = parameters.timer;
        kp_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
        kp_parameters.optimization_mode = OptimizationMode::NotAnytime;
        kp_parameters.linear_programming_solver_name = parameters.linear_programming_solver_name;
        auto kp_output = optimize(kp_instance, kp_parameters);

        algorithm_formatter.update_variable_sized_bin_packing_bound(
                total_cost - kp_output.solution_pool.best().profit());
        return;
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
                const onedimensional::Output& dff_output)
        {
            algorithm_formatter.update_bounds(dff_output);
        };
    dual_feasible_functions(instance, dff_parameters);
}

void optimize_dynamic_programming(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter)
{
    const BinType& bin_type = instance.bin_type(0);
    knapsacksolver::InstanceFromFloatProfitsBuilder kp_instance_builder;
    std::vector<std::pair<ItemTypeId, ItemPos>> kp2ps;
    knapsacksolver::Weight kp_capacity = bin_type.length;
    kp_instance_builder.set_capacity(kp_capacity);
    for (ItemTypeId item_type_id: bin_type.item_type_ids) {
        const ItemType& item_type = instance.item_type(item_type_id);
        ItemPos total_copies = 0;
        ItemPos copies = 1;
        while (total_copies < item_type.copies) {
            if (total_copies + copies > item_type.copies)
                copies = item_type.copies - total_copies;
            knapsacksolver::Weight kp_weight = copies * (
                    item_type.length
                    - (std::max)(item_type.nesting_length, (Length)0));
            if (kp_weight > kp_capacity)
                break;
            kp2ps.push_back({item_type_id, copies});
            kp_instance_builder.add_item(
                    copies * item_type.profit,
                    kp_weight);
            total_copies += copies;
            copies *= 2;
        }
    }
    knapsacksolver::Instance kp_instance = kp_instance_builder.build();

    knapsacksolver::DynamicProgrammingPrimalDualParameters kp_parameters;
    kp_parameters.verbosity_level = 0;
    auto kp_output = knapsacksolver::dynamic_programming_primal_dual(
            kp_instance,
            kp_parameters);

    SolutionBuilder solution_builder(instance);
    solution_builder.add_bin(0, 1);
    for (knapsacksolver::ItemId kp_item_type_id = 0;
            kp_item_type_id < kp_instance.number_of_items();
            ++kp_item_type_id) {
        if (kp_output.solution.contains(kp_item_type_id)) {
            ItemTypeId item_type_id = kp2ps[kp_item_type_id].first;
            ItemPos copies = kp2ps[kp_item_type_id].second;
            for (ItemPos copy = 0; copy < copies; ++copy)
                solution_builder.add_item(0, item_type_id);
        }
    }
    Solution solution = solution_builder.build();

    if (solution.feasible()) {
        std::stringstream ss;
        ss << "DP";
        algorithm_formatter.update_solution(solution, ss.str());
    }
    algorithm_formatter.update_knapsack_bound(solution.profit());
}

void optimize_tree_search(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter,
        onedimensional::Output* local_output)
{
    TreeSearchParameters ts_parameters;
    ts_parameters.verbosity_level = 0;
    ts_parameters.timer = parameters.timer;
    ts_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    ts_parameters.guides = parameters.tree_search_guides;
    ts_parameters.optimization_mode = parameters.optimization_mode;
    ts_parameters.not_anytime_tree_search_queue_size = parameters.not_anytime_tree_search_queue_size;
    ts_parameters.json_search_tree_path = parameters.json_search_tree_path;
    ts_parameters.new_solution_callback = [&algorithm_formatter, local_output](
            const onedimensional::Output& ts_output)
    {
        if (local_output != nullptr) {
            local_output->solution_pool.add(ts_output.solution_pool.best(), "TS " + ts_output.solution_pool.best_label());
            local_output->update_bounds(ts_output);
        } else {
            algorithm_formatter.update_solution(ts_output.solution_pool.best(), "TS " + ts_output.solution_pool.best_label());
            algorithm_formatter.update_bounds(ts_output);
        }
    };
    tree_search(instance, ts_parameters);
}

void optimize_sequential_single_knapsack(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter,
        onedimensional::Output* local_output)
{
    for (Counter queue_size = 1;;) {

        if (parameters.optimization_mode != OptimizationMode::Anytime)
            queue_size = parameters.not_anytime_sequential_single_knapsack_subproblem_tree_search_queue_size;

        SequentialValueCorrectionFunction<Instance, Solution> kp_solve
            = [&algorithm_formatter, &parameters, &queue_size](const Instance& kp_instance)
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
                kp_parameters.linear_programming_solver_name = parameters.linear_programming_solver_name;
                auto kp_output = optimize(kp_instance, kp_parameters);
                return kp_output.solution_pool;
            };
        SequentialValueCorrectionParameters<Instance, Solution, onedimensional::Output> svc_parameters;
        svc_parameters.verbosity_level = 0;
        svc_parameters.timer = parameters.timer;
        svc_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
        svc_parameters.maximum_number_of_iterations = 1;
        // No later iteration to adjust profits away from this initial
        // value - see 'SequentialValueCorrectionParameters::
        // initial_profit_exponent''s own doc comment.
        svc_parameters.initial_profit_exponent = 1.0;
        svc_parameters.new_solution_callback = [
            &algorithm_formatter, local_output, &queue_size](
                    const onedimensional::Output& ps_output)
            {
                const SequentialValueCorrectionOutput<Instance, Solution, onedimensional::Output>& pssvc_output
                    = static_cast<const SequentialValueCorrectionOutput<Instance, Solution, onedimensional::Output>&>(ps_output);
                std::stringstream ss;
                ss << "SSK q " << queue_size;
                if (local_output != nullptr) {
                    local_output->solution_pool.add(pssvc_output.solution_pool.best(), ss.str());
                } else {
                    algorithm_formatter.update_solution(pssvc_output.solution_pool.best(), ss.str());
                }
            };
        sequential_value_correction<Instance, InstanceBuilder, Solution, AlgorithmFormatter, onedimensional::Output>(instance, kp_solve, svc_parameters);

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
        onedimensional::Output* local_output)
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
            kp_parameters.linear_programming_solver_name = parameters.linear_programming_solver_name;
            auto kp_output = optimize(kp_instance, kp_parameters);
            return kp_output.solution_pool;
        };
    SequentialValueCorrectionParameters<Instance, Solution, onedimensional::Output> svc_parameters;
    svc_parameters.verbosity_level = 0;
    svc_parameters.timer = parameters.timer;
    svc_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    svc_parameters.initial_profit_exponent = 1.1;
    if (parameters.optimization_mode != OptimizationMode::Anytime)
        svc_parameters.maximum_number_of_iterations = parameters.not_anytime_sequential_value_correction_number_of_iterations;
    svc_parameters.new_solution_callback = [&algorithm_formatter, local_output](
            const onedimensional::Output& ps_output)
    {
        const SequentialValueCorrectionOutput<Instance, Solution, onedimensional::Output>& pssvc_output
            = static_cast<const SequentialValueCorrectionOutput<Instance, Solution, onedimensional::Output>&>(ps_output);
        std::stringstream ss;
        ss << "SVC it " << pssvc_output.number_of_iterations;
        if (local_output != nullptr) {
            local_output->solution_pool.add(pssvc_output.solution_pool.best(), ss.str());
        } else {
            algorithm_formatter.update_solution(pssvc_output.solution_pool.best(), ss.str());
        }
    };
    sequential_value_correction<Instance, InstanceBuilder, Solution, AlgorithmFormatter, onedimensional::Output>(instance, kp_solve, svc_parameters);
}

void optimize_dichotomic_search(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter,
        onedimensional::Output* local_output)
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
        DichotomicSearchParameters<Instance, Solution, onedimensional::Output> ds_parameters;
        ds_parameters.verbosity_level = 0;
        ds_parameters.timer = parameters.timer;
        ds_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
        ds_parameters.initial_waste_percentage_upper_bound = waste_percentage_upper_bound;
        ds_parameters.new_solution_callback = [
            &algorithm_formatter, local_output, &queue_size](
                    const onedimensional::Output& ps_output)
            {
                const DichotomicSearchOutput<Instance, Solution, onedimensional::Output>& psds_output
                    = static_cast<const DichotomicSearchOutput<Instance, Solution, onedimensional::Output>&>(ps_output);
                std::stringstream ss;
                ss << "DS q " << queue_size
                    << " w " << psds_output.waste_percentage;
                if (local_output != nullptr) {
                    local_output->solution_pool.add(psds_output.solution_pool.best(), ss.str());
                } else {
                    algorithm_formatter.update_solution(psds_output.solution_pool.best(), ss.str());
                }
            };
        auto ds_output = dichotomic_search<Instance, InstanceBuilder, Solution, AlgorithmFormatter, onedimensional::Output>(instance, bpp_solve, ds_parameters);

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
        onedimensional::Output* local_output)
{
    ColumnGenerationPricingFunction<Instance, InstanceBuilder, Solution, onedimensional::Output> pricing_function
        = [&parameters](const Instance& kp_instance, PricingType)
        {
            OptimizeParameters kp_parameters;
            kp_parameters.verbosity_level = 0;
            kp_parameters.timer = parameters.timer;
            kp_parameters.optimization_mode
                = (parameters.optimization_mode == OptimizationMode::NotAnytimeSequential)?
                OptimizationMode::NotAnytimeSequential:
                OptimizationMode::NotAnytimeDeterministic;
            kp_parameters.not_anytime_tree_search_queue_size
                = parameters.column_generation_subproblem_tree_search_queue_size;
            kp_parameters.linear_programming_solver_name = parameters.linear_programming_solver_name;
            return optimize(kp_instance, kp_parameters);
        };

    ColumnGenerationParameters<Instance, Solution, onedimensional::Output> cg_parameters;
    cg_parameters.verbosity_level = 0;
    cg_parameters.timer = parameters.timer;
    cg_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    cg_parameters.optimization_mode = parameters.optimization_mode;
    cg_parameters.internal_diving = columngenerationsolver::Activation::Never;
    cg_parameters.linear_programming_solver_name = parameters.linear_programming_solver_name;
    // Unlike the other domains, this one already has its own dedicated,
    // purpose-built sequential feasibility scheme for 'BinPacking' (see
    // 'optimize_milp_assignment'/'MilpAssignmentParameters::
    // use_sequential_feasibility'), so column generation's own doesn't need
    // to run here too - it would only add a second, less specialized search
    // for the same thing, called with no proper 'lower_bound' to seed it
    // from (unlike 'optimize_milp_assignment''s own call site below). This
    // matters beyond the top-level solve: this function also runs as a
    // supposedly cheap sub-step of other domains' own bound computations
    // (e.g. 'rectangle::optimize_onedimensional_bound'), where a slow,
    // from-scratch sequential search would be a disproportionate cost for
    // what's meant to be a fast approximate bound.
    cg_parameters.use_sequential_feasibility = false;
    cg_parameters.new_solution_callback = [&algorithm_formatter, local_output](
            const onedimensional::Output& ps_output)
    {
        if (local_output != nullptr) {
            local_output->solution_pool.add(ps_output.solution_pool.best(), "CG " + ps_output.solution_pool.best_label());
            local_output->update_bounds(ps_output);
        } else {
            algorithm_formatter.update_solution(ps_output.solution_pool.best(), "CG " + ps_output.solution_pool.best_label());
            algorithm_formatter.update_bounds(ps_output);
        }
    };
    column_generation<Instance, InstanceBuilder, Solution, AlgorithmFormatter, onedimensional::Output>(instance, pricing_function, cg_parameters);
}

void optimize_milp_assignment(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter,
        onedimensional::Output* local_output,
        BinPos lower_bound)
{
    MilpAssignmentParameters ma_parameters;
    ma_parameters.verbosity_level = 0;
    ma_parameters.timer = parameters.timer;
    ma_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    ma_parameters.optimization_mode = parameters.optimization_mode;
    ma_parameters.new_solution_callback = [&algorithm_formatter, local_output](
            const onedimensional::Output& ps_output)
    {
        if (local_output != nullptr) {
            local_output->solution_pool.add(ps_output.solution_pool.best(), "MA " + ps_output.solution_pool.best_label());
            local_output->update_bounds(ps_output);
        } else {
            algorithm_formatter.update_solution(ps_output.solution_pool.best(), "MA " + ps_output.solution_pool.best_label());
            algorithm_formatter.update_bounds(ps_output);
        }
    };
    milp_assignment(instance, ma_parameters, lower_bound);
}

}

packingsolver::onedimensional::Output packingsolver::onedimensional::optimize(
        const Instance& instance,
        const OptimizeParameters& parameters)
{
    Output output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    optimize_trivial_bound(instance, parameters, algorithm_formatter);

    if (instance.objective() == Objective::BinPacking
            || instance.objective() == Objective::Feasibility
            || instance.objective() == Objective::Knapsack) {
        if (instance.number_of_bin_types() == 1
                && (parameters.use_dual_feasible_functions
                    || instance.number_of_items() <= 100)) {
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

    if (instance.number_of_bins() == 1
            && instance.objective() == Objective::Knapsack) {
        optimize_dynamic_programming(
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

    // Select algorithms to run.
    ItemPos mean_number_of_items_in_bins
        = largest_bin_space(instance) / mean_item_space(instance);
    bool use_tree_search = parameters.use_tree_search;
    bool use_sequential_single_knapsack = parameters.use_sequential_single_knapsack;
    bool use_sequential_value_correction = parameters.use_sequential_value_correction;
    bool use_dichotomic_search = parameters.use_dichotomic_search;
    bool use_column_generation = parameters.use_column_generation;
    bool use_milp_assignment = parameters.use_milp_assignment;
    if (instance.number_of_bins() <= 1) {
        use_sequential_single_knapsack = false;
        use_sequential_value_correction = false;
        use_dichotomic_search = false;
        use_column_generation = false;
        // Automatic selection.
        if (!use_tree_search
                && !use_milp_assignment) {
            use_tree_search = true;
        }
    } else if (instance.objective() == Objective::Feasibility) {
        // Disable algorithms which are not available for this objective.
        use_dichotomic_search = false;
        // Automatic selection.
        if (!use_tree_search
                && !use_sequential_single_knapsack
                && !use_sequential_value_correction
                && !use_column_generation
                && !use_milp_assignment) {
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
    } else if (instance.objective() == Objective::Knapsack) {
        // Disable algorithms which are not available for this objective.
        use_dichotomic_search = false;
        // Automatic selection.
        if (!use_tree_search
                && !use_sequential_single_knapsack
                && !use_sequential_value_correction
                && !use_column_generation
                && !use_milp_assignment) {
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
    } else if (instance.objective() == Objective::BinPacking
            || instance.objective() == Objective::BinPackingWithLeftovers) {
        // Disable algorithms which are not available for this objective.
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
        if (instance.objective() == Objective::BinPackingWithLeftovers)
            use_milp_assignment = false;
        use_dichotomic_search = false;
        // Automatic selection.
        if (!use_tree_search
                && !use_sequential_single_knapsack
                && !use_sequential_value_correction
                && !use_column_generation
                && !use_milp_assignment) {
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
                use_tree_search = true;
                if (mean_number_of_items_in_bins
                        > parameters.many_items_in_bins_threshold) {
                    use_sequential_single_knapsack = true;
                } else {
                    use_sequential_value_correction = true;
                    if (instance.objective() == Objective::BinPacking)
                        use_column_generation = true;
                }
            }
        }
    } else if (instance.objective() == Objective::VariableSizedBinPacking) {
        // Disable algorithms which are not available for this objective.
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
                && !use_column_generation
                && !use_milp_assignment) {
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

    // Run selected algorithms.
    // In 'NotAnytimeDeterministic' mode, algorithms still run in parallel, but
    // each writes its solutions to its own 'local_output' instead of the
    // shared 'algorithm_formatter', so that they can be replayed into it in a
    // fixed, deterministic order once every algorithm has terminated
    // ('run(tasks, ...)' does not guarantee a deterministic finish order).
    // 'local_outputs' owns these; a 'unique_ptr' is used so that it growing
    // does not invalidate the raw pointers captured by the tasks below.
    bool deterministic = (parameters.optimization_mode == OptimizationMode::NotAnytimeDeterministic);
    std::vector<std::unique_ptr<onedimensional::Output>> local_outputs;
    std::vector<std::function<void()>> tasks;
    std::forward_list<std::exception_ptr> exception_ptr_list;
    // Tree search.
    if (use_tree_search) {
        exception_ptr_list.push_front(std::exception_ptr());
        std::exception_ptr& exception_ptr = exception_ptr_list.front();
        std::unique_ptr<onedimensional::Output> local_output;
        if (deterministic)
            local_output = std::make_unique<onedimensional::Output>(instance);
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
    // Sequential single knapsack.
    if (use_sequential_single_knapsack) {
        exception_ptr_list.push_front(std::exception_ptr());
        std::exception_ptr& exception_ptr = exception_ptr_list.front();
        std::unique_ptr<onedimensional::Output> local_output;
        if (deterministic)
            local_output = std::make_unique<onedimensional::Output>(instance);
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
        std::unique_ptr<onedimensional::Output> local_output;
        if (deterministic)
            local_output = std::make_unique<onedimensional::Output>(instance);
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
        std::unique_ptr<onedimensional::Output> local_output;
        if (deterministic)
            local_output = std::make_unique<onedimensional::Output>(instance);
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
        std::unique_ptr<onedimensional::Output> local_output;
        if (deterministic)
            local_output = std::make_unique<onedimensional::Output>(instance);
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
    // MILP assignment.
    if (use_milp_assignment) {
        exception_ptr_list.push_front(std::exception_ptr());
        std::exception_ptr& exception_ptr = exception_ptr_list.front();
        std::unique_ptr<onedimensional::Output> local_output;
        if (deterministic)
            local_output = std::make_unique<onedimensional::Output>(instance);
        // Snapshot the bound established so far (currently just
        // 'optimize_trivial_bound', which already ran synchronously above):
        // seeds the sequential feasibility scheme's starting candidate
        // instead of it always restarting from scratch.
        BinPos milp_assignment_lower_bound = output.bin_packing_bound;
        tasks.push_back([&exception_ptr, &instance, &parameters, &algorithm_formatter, local_output = local_output.get(), milp_assignment_lower_bound]() {
            wrapper<decltype(&optimize_milp_assignment), optimize_milp_assignment>(
                    exception_ptr,
                    instance,
                    parameters,
                    algorithm_formatter,
                    local_output,
                    milp_assignment_lower_bound);
        });
        local_outputs.push_back(std::move(local_output));
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

    algorithm_formatter.end();
    return output;
}
