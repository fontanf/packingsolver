#include "packingsolver/irregular/optimize.hpp"

#include "packingsolver/irregular/algorithm_formatter.hpp"
#include "packingsolver/irregular/instance_builder.hpp"
#include "irregular/trivial.hpp"
#include "irregular/tree_search.hpp"
#include "irregular/tree_search_periodic_packing.hpp"
#include "irregular/milp_raster.hpp"
#include "irregular/local_search.hpp"
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

void optimize_trivial_bound(
        const Instance& instance,
        AlgorithmFormatter& algorithm_formatter)
{
    if (instance.objective() == Objective::Knapsack) {
        // 1D continuous relaxation (area-based Dantzig bound): sort items
        // by decreasing profit/area ratio and greedily fill the total
        // available bin area, taking the last item fractionally. Always a
        // valid, cheap (O(n log n), no search) upper bound, and much
        // tighter than the trivial "sum of all profits" whenever item
        // profits aren't roughly proportional to their area.
        // Items whose bounding box (over every allowed discrete rotation)
        // doesn't fit within any bin's bounding box can never be packed
        // (a necessary, if not sufficient, condition for actually fitting
        // the polygon), so they are excluded entirely rather than counted
        // as fractionally packable area. Items with a continuous rotation
        // range are conservatively always kept: proving they don't fit
        // would require an exact geometric optimization over the range,
        // and wrongly excluding a fitting item would make the bound
        // unsound.
        AreaDbl total_capacity = 0;
        for (BinTypeId bin_type_id = 0;
                bin_type_id < instance.number_of_bin_types();
                ++bin_type_id) {
            const BinType& bin_type = instance.bin_type(bin_type_id);
            total_capacity += bin_type.area_orig * bin_type.copies;
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
                    return item_type_1.profit * item_type_2.area_orig
                        > item_type_2.profit * item_type_1.area_orig;
                });
        Profit bound = 0.0;
        AreaDbl remaining_capacity = total_capacity;
        for (ItemTypeId item_type_id: sorted_item_types) {
            if (remaining_capacity <= 0)
                break;
            const ItemType& item_type = instance.item_type(item_type_id);
            if (item_type.area_orig <= 0)
                continue;
            AreaDbl item_total_area = item_type.area_orig * item_type.copies;
            if (item_total_area <= remaining_capacity) {
                bound += item_type.profit * item_type.copies;
                remaining_capacity -= item_total_area;
            } else {
                bound += item_type.profit
                    * (remaining_capacity / item_type.area_orig);
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
        // Area-based bound: fill bin types in the order they are provided
        // (as bins are used for this objective) until enough area is
        // available to fit all the items. Cheap (linear in the number of
        // bin/item types).
        AreaDbl remaining_item_area = instance.item_area();
        BinPos bound = 0;
        for (BinTypeId bin_type_id = 0;
                bin_type_id < instance.number_of_bin_types();
                ++bin_type_id) {
            if (remaining_item_area <= 0)
                break;
            const BinType& bin_type = instance.bin_type(bin_type_id);
            if (bin_type.area_orig <= 0)
                continue;
            BinPos bins_needed = (BinPos)std::ceil(remaining_item_area / bin_type.area_orig);
            BinPos bins_used = std::min(bins_needed, bin_type.copies);
            bound += bins_used;
            remaining_item_area -= bins_used * bin_type.area_orig;
        }
        algorithm_formatter.update_bin_packing_bound(bound);
        return;
    }

    if (instance.objective() != Objective::OpenDimensionX
            && instance.objective() != Objective::OpenDimensionY) {
        return;
    }

    // Area-based bound: the open dimension cannot be smaller than what is
    // required to fit the total area of the items in the fixed dimension of
    // the bin's bounding box.
    const auto& bin_type = instance.bin_type(0);
    LengthDbl fixed_dimension = (instance.objective() == Objective::OpenDimensionX)?
        bin_type.aabb_orig.y_max - bin_type.aabb_orig.y_min:
        bin_type.aabb_orig.x_max - bin_type.aabb_orig.x_min;
    if (fixed_dimension <= 0)
        return;
    LengthDbl bound = instance.item_area() / fixed_dimension;

    if (instance.objective() == Objective::OpenDimensionX) {
        algorithm_formatter.update_open_dimension_x_bound(bound);
    } else {
        algorithm_formatter.update_open_dimension_y_bound(bound);
    }
}

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
        BinTypeId onedim_bin_type_id = onedim_instance_builder.add_bin_type(
                std::ceil(bin_type.area_scaled));
        onedim_instance_builder.set_bin_type_cost(onedim_bin_type_id, bin_type.cost);
        onedim_instance_builder.set_bin_type_copies(onedim_bin_type_id, bin_type.copies);
        onedim_instance_builder.set_bin_type_copies_min(onedim_bin_type_id, bin_type.copies_min);
    }
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        Length length = std::floor(item_type.area_scaled);
        if (length == 0)
            continue;
        ItemTypeId onedim_item_type_id = onedim_instance_builder.add_item_type(length);
        onedim_instance_builder.set_item_type_profit(onedim_item_type_id, item_type.profit);
        onedim_instance_builder.set_item_type_copies(onedim_item_type_id, item_type.copies);
    }
    onedimensional::Instance onedim_instance = onedim_instance_builder.build();

    // Solve the instance.
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

    std::stringstream ss("1D");
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
            const irregular::Output& output) {
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
            const irregular::Output& ts_output)
    {
        algorithm_formatter.update_solution(ts_output.solution_pool.best(), "TS " + ts_output.solution_pool.best_label());
    };
    tree_search(instance, ts_parameters);
}


/**
 * Post-process for BinPackingWithLeftovers in a not-anytime mode: if the
 * tree search didn't already use guides {2, 3}, re-optimize just the last
 * bin of 'solution_best' (as its own single-bin sub-instance) with the tree
 * search with guides {2, 3}, in case that guide combination packs it into
 * less leftover space: the used area of the last bin doesn't have to span
 * the whole width or the whole height of the bin, whichever algorithm found
 * the solution.
 *
 * It is applied to the best solution of each algorithm, not only to the
 * best solution overall: the re-optimized last bin of a solution which isn't
 * the best one may make it better than the best one.
 */
bool last_bin_post_process_applies(
        const Instance& instance,
        const OptimizeParameters& parameters)
{
    return instance.objective() == Objective::BinPackingWithLeftovers
        && parameters.optimization_mode != OptimizationMode::Anytime
        && parameters.tree_search_guides != std::vector<GuideId>({2, 3});
}

void optimize_last_bin_post_process(
        const Instance& instance,
        const OptimizeParameters& parameters,
        const Solution& solution_best,
        AlgorithmFormatter& algorithm_formatter)
{
    if (last_bin_post_process_applies(instance, parameters)
            && solution_best.number_of_bins() > 0
            && solution_best.bin(solution_best.number_of_different_bins() - 1).copies == 1) {

        InstanceBuilder last_bin_instance_builder;
        last_bin_instance_builder.set_objective(Objective::BinPackingWithLeftovers);
        last_bin_instance_builder.set_parameters(instance.parameters());

        // Add bin types.
        const SolutionBin& last_bin = solution_best.bin(solution_best.number_of_different_bins() - 1);
        BinTypeId last_bin_type_id = last_bin_instance_builder.add_bin_type(instance, last_bin.bin_type_id);
        last_bin_instance_builder.set_bin_type_copies(last_bin_type_id, 1);
        last_bin_instance_builder.set_bin_type_copies_min(last_bin_type_id, 0);

        // Add item types.
        std::vector<ItemPos> last_bin_item_copies(instance.number_of_item_types(), 0);
        for (const SolutionItem& solution_item: last_bin.items)
            last_bin_item_copies[solution_item.item_type_id]++;

        std::vector<ItemTypeId> last_bin_to_orig;
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            if (last_bin_item_copies[item_type_id] > 0) {
                ItemTypeId sub_item_type_id = last_bin_instance_builder.add_item_type(instance, item_type_id);
                last_bin_instance_builder.set_item_type_copies(sub_item_type_id, last_bin_item_copies[item_type_id]);
                last_bin_to_orig.push_back(item_type_id);
            }
        }

        // Build instance.
        Instance last_bin_instance = last_bin_instance_builder.build();

        // Solve instance. Only the tree search is run: the guides {2, 3}
        // are specific to it.
        OptimizeParameters last_bin_parameters;
        last_bin_parameters.seed = parameters.seed;
        last_bin_parameters.verbosity_level = 0;
        last_bin_parameters.timer = parameters.timer;
        last_bin_parameters.optimization_mode = parameters.optimization_mode;
        last_bin_parameters.not_anytime_maximum_approximation_ratio = parameters.not_anytime_maximum_approximation_ratio;
        last_bin_parameters.not_anytime_tree_search_queue_size = parameters.not_anytime_tree_search_queue_size;
        last_bin_parameters.tree_search_guides = {2, 3};
        last_bin_parameters.linear_programming_solver_name = parameters.linear_programming_solver_name;
        last_bin_parameters.use_tree_search = true;
        auto last_bin_output = optimize(last_bin_instance, last_bin_parameters);

        if (last_bin_output.solution_pool.best().feasible()) {

            // Retrieve solution.
            Solution solution(instance);
            // Add the bins of the current best solution, except the last
            // one, which is replaced by the optimized one.
            for (BinPos bin_pos = 0;
                    bin_pos < solution_best.number_of_different_bins() - 1;
                    ++bin_pos) {
                const SolutionBin& solution_bin = solution_best.bin(bin_pos);
                solution.append_bin(solution_best, bin_pos, solution_bin.copies);
            }
            // Add last optimized bin.
            solution.append_bin(
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
}

void optimize_local_search(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter,
        LocalSearchEngine engine)
{
    LocalSearchParameters ls_parameters;
    ls_parameters.engine = engine;
    ls_parameters.verbosity_level = 0;
    ls_parameters.timer = parameters.timer;
    ls_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    if (parameters.optimization_mode == OptimizationMode::Anytime) {
        ls_parameters.maximum_number_of_failed_separations = -1;
    } else {
        ls_parameters.maximum_number_of_failed_separations
            = parameters.not_anytime_local_search_maximum_number_of_failed_separations;
        if (parameters.not_anytime_local_search_maximum_number_of_moves_factor >= 0) {
            ls_parameters.maximum_number_of_moves
                = parameters.not_anytime_local_search_maximum_number_of_moves_factor
                * instance.number_of_items();
        }
    }
    if (parameters.optimization_mode == OptimizationMode::NotAnytimeSequential)
        ls_parameters.separator_parameters.number_of_workers = 1;
    ls_parameters.new_solution_callback = [&algorithm_formatter, engine](
            const irregular::Output& ps_output)
    {
        std::stringstream ss;
        ss << ((engine == LocalSearchEngine::LinearProgramming)? "LS-LP":
                (engine == LocalSearchEngine::NonlinearProgramming)? "LS-NLP":
                "LS-DT");
        if (!ps_output.solution_pool.best_label().empty())
            ss << " " << ps_output.solution_pool.best_label();
        algorithm_formatter.update_solution(ps_output.solution_pool.best(), ss.str());
    };
    std::mt19937_64 generator(parameters.seed);
    local_search(instance, generator, nullptr, ls_parameters);
}

void optimize_local_search_linear_programming(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter)
{
    optimize_local_search(
            instance,
            parameters,
            algorithm_formatter,
            LocalSearchEngine::LinearProgramming);
}

void optimize_local_search_nonlinear_programming(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter)
{
    optimize_local_search(
            instance,
            parameters,
            algorithm_formatter,
            LocalSearchEngine::NonlinearProgramming);
}

void optimize_local_search_direct_trigonometry(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter)
{
    optimize_local_search(
            instance,
            parameters,
            algorithm_formatter,
            LocalSearchEngine::DirectTrigonometry);
}

void optimize_sequential_single_knapsack(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter,
        Counter queue_size_max)
{
    double maximum_approximation_ratio = parameters.initial_maximum_approximation_ratio;
    for (Counter iteration = 0, growth_factor = 1;; ++iteration) {
        // The effort of each algorithm of the subproblems grows from its own
        // initial value, see
        // 'anytime_sequential_single_knapsack_subproblem_tree_search_initial_queue_size'.
        NodeId tree_search_queue_size
            = parameters.anytime_sequential_single_knapsack_subproblem_tree_search_initial_queue_size
            * growth_factor;
        NodeId tree_search_periodic_packing_queue_size
            = parameters.anytime_sequential_single_knapsack_subproblem_tree_search_periodic_packing_initial_queue_size
            * growth_factor;
        Counter local_search_maximum_number_of_moves_factor
            = parameters.anytime_sequential_single_knapsack_subproblem_local_search_initial_maximum_number_of_moves_factor
            * growth_factor;
        if (parameters.optimization_mode != OptimizationMode::Anytime) {
            tree_search_queue_size = parameters
                .not_anytime_sequential_single_knapsack_subproblem_tree_search_queue_size;
            tree_search_periodic_packing_queue_size = parameters
                .not_anytime_sequential_single_knapsack_subproblem_tree_search_periodic_packing_queue_size;
            local_search_maximum_number_of_moves_factor = parameters
                .not_anytime_sequential_single_knapsack_subproblem_local_search_maximum_number_of_moves_factor;
            maximum_approximation_ratio = parameters.not_anytime_maximum_approximation_ratio;
        }

        if (queue_size_max != -1
                && tree_search_queue_size > queue_size_max) {
            break;
        }

        SequentialValueCorrectionFunction<Instance, Solution> kp_solve
            = [&algorithm_formatter, &parameters,
            tree_search_queue_size,
            tree_search_periodic_packing_queue_size,
            local_search_maximum_number_of_moves_factor,
            maximum_approximation_ratio](const Instance& kp_instance)
            {
                OptimizeParameters kp_parameters;
                kp_parameters.seed = parameters.seed;
                kp_parameters.verbosity_level = 0;
                kp_parameters.timer = parameters.timer;
                kp_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
                kp_parameters.optimization_mode
                    = (parameters.optimization_mode == OptimizationMode::NotAnytimeSequential)?
                    OptimizationMode::NotAnytimeSequential:
                    OptimizationMode::NotAnytimeDeterministic;
                kp_parameters.not_anytime_maximum_approximation_ratio = maximum_approximation_ratio;
                kp_parameters.not_anytime_tree_search_queue_size = tree_search_queue_size;
                kp_parameters.not_anytime_tree_search_periodic_packing_queue_size
                    = tree_search_periodic_packing_queue_size;
                kp_parameters.not_anytime_local_search_maximum_number_of_moves_factor
                    = local_search_maximum_number_of_moves_factor;
                kp_parameters.linear_programming_solver_name = parameters.linear_programming_solver_name;
                auto kp_output = optimize(kp_instance, kp_parameters);
                return kp_output.solution_pool;
            };
        SequentialValueCorrectionParameters<Instance, Solution, irregular::Output> svc_parameters;
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
                    const irregular::Output& ps_output)
            {
                const SequentialValueCorrectionOutput<Instance, Solution, irregular::Output>& pssvc_output
                    = static_cast<const SequentialValueCorrectionOutput<Instance, Solution, irregular::Output>&>(ps_output);
                std::stringstream ss;
                ss << "SSK it " << iteration;
                algorithm_formatter.update_solution(pssvc_output.solution_pool.best(), ss.str());
            };
        sequential_value_correction<Instance, InstanceBuilder, Solution, AlgorithmFormatter, irregular::Output>(instance, kp_solve, svc_parameters);

        // Check end.
        if (algorithm_formatter.end_boolean())
            break;
        if (parameters.timer.needs_to_end())
            break;

        if (parameters.optimization_mode != OptimizationMode::Anytime)
            break;

        // Update growth factor.
        growth_factor = std::max(
                growth_factor + 1,
                (Counter)(growth_factor * 2));
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
            kp_parameters.seed = parameters.seed;
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
    SequentialValueCorrectionParameters<Instance, Solution, irregular::Output> svc_parameters;
    svc_parameters.verbosity_level = 0;
    svc_parameters.timer = parameters.timer;
    svc_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    svc_parameters.initial_profit_exponent = 1.1;
    if (parameters.optimization_mode != OptimizationMode::Anytime)
        svc_parameters.maximum_number_of_iterations = parameters.not_anytime_sequential_value_correction_number_of_iterations;
    svc_parameters.new_solution_callback = [&algorithm_formatter](
            const irregular::Output& ps_output)
    {
        const SequentialValueCorrectionOutput<Instance, Solution, irregular::Output>& pssvc_output
            = static_cast<const SequentialValueCorrectionOutput<Instance, Solution, irregular::Output>&>(ps_output);
        std::stringstream ss;
        ss << "SVC it " << pssvc_output.number_of_iterations;
        algorithm_formatter.update_solution(pssvc_output.solution_pool.best(), ss.str());
    };
    sequential_value_correction<Instance, InstanceBuilder, Solution, AlgorithmFormatter, irregular::Output>(instance, kp_solve, svc_parameters);
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
                bpp_parameters.seed = parameters.seed;
                bpp_parameters.verbosity_level = 0;
                bpp_parameters.timer = parameters.timer;
                bpp_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
                bpp_parameters.optimization_mode
                    = (parameters.optimization_mode == OptimizationMode::NotAnytimeSequential)?
                    OptimizationMode::NotAnytimeSequential:
                    OptimizationMode::NotAnytimeDeterministic;
                bpp_parameters.not_anytime_maximum_approximation_ratio = maximum_approximation_ratio;
                bpp_parameters.not_anytime_tree_search_queue_size = queue_size;
                bpp_parameters.linear_programming_solver_name = parameters.linear_programming_solver_name;
                auto bpp_output = optimize(bpp_instance, bpp_parameters);
                return bpp_output.solution_pool;
            };
        DichotomicSearchParameters<Instance, Solution, irregular::Output> ds_parameters;
        ds_parameters.verbosity_level = 0;
        ds_parameters.timer = parameters.timer;
        ds_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
        ds_parameters.initial_waste_percentage_upper_bound = waste_percentage_upper_bound;
        ds_parameters.new_solution_callback = [
            &algorithm_formatter, &queue_size](
                    const irregular::Output& ps_output)
            {
                const DichotomicSearchOutput<Instance, Solution, irregular::Output>& psds_output
                    = static_cast<const DichotomicSearchOutput<Instance, Solution, irregular::Output>&>(ps_output);
                std::stringstream ss;
                ss << "DS q " << queue_size
                    << " w " << psds_output.waste_percentage;
                algorithm_formatter.update_solution(psds_output.solution_pool.best(), ss.str());
            };
        auto ds_output = dichotomic_search<Instance, InstanceBuilder, Solution, AlgorithmFormatter, irregular::Output>(instance, bpp_solve, ds_parameters);

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
    ColumnGenerationPricingFunction<Instance, InstanceBuilder, Solution, irregular::Output> pricing_function
        = [&algorithm_formatter, &parameters](const Instance& kp_instance, PricingType)
        {
            OptimizeParameters kp_parameters;
            kp_parameters.seed = parameters.seed;
            kp_parameters.verbosity_level = 0;
            kp_parameters.timer = parameters.timer;
            kp_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
            kp_parameters.optimization_mode
                = (parameters.optimization_mode == OptimizationMode::NotAnytimeSequential)?
                OptimizationMode::NotAnytimeSequential:
                OptimizationMode::NotAnytimeDeterministic;
            kp_parameters.not_anytime_maximum_approximation_ratio
                = parameters.not_anytime_maximum_approximation_ratio;
            kp_parameters.not_anytime_tree_search_queue_size
                = parameters.column_generation_subproblem_tree_search_queue_size;
            kp_parameters.linear_programming_solver_name = parameters.linear_programming_solver_name;
            return optimize(kp_instance, kp_parameters);
        };

    ColumnGenerationParameters<Instance, Solution, irregular::Output> cg_parameters;
    cg_parameters.verbosity_level = 0;
    cg_parameters.timer = parameters.timer;
    cg_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    cg_parameters.optimization_mode = parameters.optimization_mode;
    cg_parameters.linear_programming_solver_name = parameters.linear_programming_solver_name;
    cg_parameters.new_solution_callback = [&algorithm_formatter](
            const irregular::Output& ps_output)
    {
        algorithm_formatter.update_solution(ps_output.solution_pool.best(), "CG " + ps_output.solution_pool.best_label());
        algorithm_formatter.update_bounds(ps_output);
    };
    column_generation<Instance, InstanceBuilder, Solution, AlgorithmFormatter, irregular::Output>(instance, pricing_function, cg_parameters);
}

void optimize_tree_search_periodic_packing(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter)
{
    TreeSearchPeriodicPackingParameters ts_pp_parameters;
    ts_pp_parameters.verbosity_level = 0;
    ts_pp_parameters.timer = parameters.timer;
    ts_pp_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    ts_pp_parameters.optimization_mode = parameters.optimization_mode;
    ts_pp_parameters.not_anytime_tree_search_queue_size
        = parameters.not_anytime_tree_search_periodic_packing_queue_size;
    ts_pp_parameters.maximum_approximation_ratio
        = parameters.not_anytime_maximum_approximation_ratio;
    ts_pp_parameters.new_solution_callback = [&algorithm_formatter](
            const packingsolver::Output<Instance, Solution>& ts_output)
    {
        algorithm_formatter.update_solution(
                ts_output.solution_pool.best(),
                "TSPP " + ts_output.solution_pool.best_label());
    };
    tree_search_periodic_packing(instance, ts_pp_parameters);
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
            const irregular::Output& output) {
        algorithm_formatter.update_solution(
                output.solution_pool.best(),
                "MILP raster");
        algorithm_formatter.update_bounds(output);
    };
    milp_raster(instance, milp_raster_parameters);
}

/**
 * Area of the bin of a single-bin instance which is left for the items to
 * place: its area minus the areas of its fixed items and of its defects
 * (approximate if defects overlap each other or go out of the bin).
 */
AreaDbl bin_free_area(const Instance& instance)
{
    const BinType& bin_type = instance.bin_type(0);
    AreaDbl free_area = bin_type.space();
    for (const FixedItem& fixed_item: bin_type.fixed_items)
        free_area -= instance.item_type(fixed_item.item_type_id).space();
    for (const Defect& defect: bin_type.defects)
        free_area -= defect.shape_orig.compute_area();
    return (std::max)(free_area, 0.0);
}

/**
 * Estimate the maximum number of items that could fit in the (single) bin
 * of the instance, using only their areas: sort item types by increasing
 * item area and greedily count copies (smallest items first) until their
 * cumulative area reaches the bin's free area (see 'bin_free_area'). Only
 * the copies which are not fixed are counted.
 */
ItemPos max_number_of_items_in_bin(const Instance& instance)
{
    std::vector<std::pair<AreaDbl, ItemPos>> items_by_area;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        ItemPos copies = item_type.copies - item_type.copies_fixed;
        if (copies > 0)
            items_by_area.push_back({item_type.space(), copies});
    }
    std::sort(items_by_area.begin(), items_by_area.end());

    AreaDbl bin_area = bin_free_area(instance);
    AreaDbl cumulative_area = 0.0;
    ItemPos number_of_items = 0;
    for (const auto& p: items_by_area) {
        AreaDbl area = p.first;
        ItemPos copies = p.second;
        if (!packingsolver::strictly_lesser(cumulative_area, bin_area))
            break;
        if (area <= 0.0) {
            number_of_items += copies;
            continue;
        }
        ItemPos whole_fit = (ItemPos)((bin_area - cumulative_area) / area);
        if (whole_fit >= copies) {
            number_of_items += copies;
            cumulative_area += copies * area;
        } else {
            number_of_items += whole_fit + 1;
            break;
        }
    }
    return number_of_items;
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

}

packingsolver::irregular::Output packingsolver::irregular::optimize(
        const Instance& instance,
        const OptimizeParameters& parameters)
{
    Output output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    optimize_item_types_fit(instance, algorithm_formatter);
    if (output.is_proven_infeasible) {
        algorithm_formatter.end();
        return output;
    }

    // Instance reduction (see 'Reduction'): applied once, upfront,
    // wrapping the whole dispatch logic below uniformly for every
    // algorithm - mirrors 'rectangle::optimize''s own wiring. Gated
    // directly on 'parameters.reduction_parameters.reduce', rather than
    // constructing a 'Reduction' unconditionally and letting its
    // constructor no-op, to skip paying for (and discarding) a full pass
    // building its working representation when disabled.
    // 'reduced_parameters.reduction_parameters.reduce' is set to 'false'
    // below to avoid re-running the reduction recursively on the
    // already-reduced instance.
    if (parameters.reduction_parameters.reduce) {
        ReductionParameters reduction_parameters = parameters.reduction_parameters;
        reduction_parameters.timer = parameters.timer;
        Reduction reduction(instance, reduction_parameters);

        auto report_reduced_output = [&reduction, &algorithm_formatter](
                const irregular::Output& reduced_output)
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
        // than only once at the very end - this is what lets an anytime
        // run on the reduced instance report live progress (in original-
        // instance coordinates) the same way a direct, unreduced run
        // would.
        reduced_parameters.new_solution_callback = report_reduced_output;
        Output reduced_output = optimize(reduction.instance(), reduced_parameters);
        // Also report the final result explicitly: some sub-solves never
        // call 'new_solution_callback' at all (e.g. nothing improved on
        // the initial empty solution) - this guarantees the answer is
        // still reported once regardless. Harmless to call again since
        // 'update_solution'/'update_bounds' are themselves no-ops for
        // anything that doesn't improve on what is already recorded.
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

    // Select algorithms to run.
    ItemPos mean_number_of_items_in_bins
        = largest_bin_space(instance) / mean_item_space(instance);
    bool use_tree_search = parameters.use_tree_search;
    bool use_tree_search_periodic_packing = parameters.use_tree_search_periodic_packing;
    bool use_local_search_linear_programming = parameters.use_local_search_linear_programming;
    bool use_local_search_nonlinear_programming = parameters.use_local_search_nonlinear_programming;
    bool use_local_search_direct_trigonometry = parameters.use_local_search_direct_trigonometry;
    bool use_sequential_single_knapsack = parameters.use_sequential_single_knapsack;
    bool use_sequential_value_correction = parameters.use_sequential_value_correction;
    bool use_dichotomic_search = parameters.use_dichotomic_search;
    bool use_column_generation = parameters.use_column_generation;
    bool use_milp_raster = parameters.use_milp_raster;
    if (instance.number_of_bins() <= 1) {
        // Disable algorithms which are not available for this objective.
        use_sequential_single_knapsack = false;
        use_sequential_value_correction = false;
        use_dichotomic_search = false;
        use_column_generation = false;
        if (instance.objective() != Objective::Knapsack
                && instance.objective() != Objective::Feasibility) {
            use_milp_raster = false;
        }
        if (instance.objective() == Objective::Knapsack) {
            use_local_search_linear_programming = false;
            use_local_search_nonlinear_programming = false;
            use_local_search_direct_trigonometry = false;
        }
        // Automatic selection.
        if (!use_tree_search
                && !use_tree_search_periodic_packing
                && !use_local_search_linear_programming
                && !use_local_search_nonlinear_programming
                && !use_local_search_direct_trigonometry
                && !use_milp_raster) {
            // tree_search_periodic_packing is worth running when the
            // instance is dominated by item types with many copies: either
            // these alone already exceed the bin's free area (so a periodic
            // tiling of them is going to be needed regardless of the other
            // item types), or they make up the entire instance (no other
            // item type contributes any area). The fixed items and the
            // defects are not counted: they leave less area to fill.
            AreaDbl bin_area = bin_free_area(instance);
            AreaDbl high_copies_area = 0.0;
            AreaDbl total_item_area = 0.0;
            for (ItemTypeId item_type_id = 0;
                    item_type_id < instance.number_of_item_types();
                    ++item_type_id) {
                const ItemType& item_type = instance.item_type(item_type_id);
                ItemPos copies = item_type.copies - item_type.copies_fixed;
                AreaDbl item_type_area = copies * item_type.space();
                total_item_area += item_type_area;
                if (copies > parameters.periodic_packing_copies_threshold)
                    high_copies_area += item_type_area;
            }
            bool periodic_packing_runnable
                = packingsolver::strictly_greater(high_copies_area, 0.8 * bin_area)
                || packingsolver::strictly_greater(high_copies_area, 0.8 * total_item_area);

            use_tree_search_periodic_packing = periodic_packing_runnable;
            use_tree_search
                = !periodic_packing_runnable
                || max_number_of_items_in_bin(instance)
                    < parameters.periodic_packing_max_items_threshold;
            if (instance.objective() != Objective::Knapsack)
                use_local_search_direct_trigonometry = true;
        }
    } else if (instance.objective() == Objective::Feasibility) {
        // Disable algorithms which are not available for this objective.
        use_local_search_linear_programming = false;
        use_local_search_nonlinear_programming = false;
        use_dichotomic_search = false;
        use_tree_search_periodic_packing = false;
        // Automatic selection.
        if (!use_tree_search
                && !use_local_search_direct_trigonometry
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
                use_local_search_direct_trigonometry = true;
            }
        }
    } else if (instance.objective() == Objective::Knapsack) {
        // Disable algorithms which are not available for this objective.
        use_local_search_linear_programming = false;
        use_local_search_nonlinear_programming = false;
        use_local_search_direct_trigonometry = false;
        use_dichotomic_search = false;
        use_tree_search_periodic_packing = false;
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
    } else if (instance.objective() == Objective::BinPacking
            || instance.objective() == Objective::BinPackingWithLeftovers) {
        // Disable algorithms which are not available for this objective.
        // tree_search_periodic_packing only supports a single bin; here we
        // are in the multi-bin case (it will still be available for the
        // per-bin Knapsack subproblems of sequential_single_knapsack /
        // sequential_value_correction, via their own recursive optimize()
        // call on a single-bin instance).
        use_tree_search_periodic_packing = false;
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
        use_milp_raster = false;
        // Automatic selection.
        if (!use_tree_search
                && !use_tree_search_periodic_packing
                && !use_local_search_linear_programming
                && !use_local_search_nonlinear_programming
                && !use_local_search_direct_trigonometry
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
                    if (instance.objective() == Objective::BinPacking)
                        use_column_generation = true;
                }
            } else {
                use_tree_search = true;
                use_local_search_direct_trigonometry = true;
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
        use_milp_raster = false;
        use_local_search_linear_programming = false;
        use_local_search_nonlinear_programming = false;
        use_local_search_direct_trigonometry = false;
        use_tree_search_periodic_packing = false;
        if (instance.number_of_bin_types() == 1) {
            if (use_dichotomic_search) {
                use_dichotomic_search = false;
                use_tree_search = true;
            }
        }
        // Automatic selection.
        if (!use_tree_search
                && !use_local_search_linear_programming
                && !use_local_search_nonlinear_programming
                && !use_local_search_direct_trigonometry
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
    // messages, no JSON output, and no new solution callback, except the one
    // forwarding to the shared formatter, see below), and whose timer
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
    bool local = deterministic || last_bin_post_process_applies(instance, parameters);
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
        tasks.push_back([&exception_ptr, &instance, context]() {
            wrapper<decltype(&optimize_tree_search), optimize_tree_search>(
                    exception_ptr,
                    instance,
                    *context.first,
                    *context.second);
        });
    }
    // Tree search periodic packing.
    if (use_tree_search_periodic_packing) {
        exception_ptr_list.push_front(std::exception_ptr());
        std::exception_ptr& exception_ptr = exception_ptr_list.front();
        auto context = task_context();
        tasks.push_back([&exception_ptr, &instance, context]() {
            wrapper<decltype(&optimize_tree_search_periodic_packing), optimize_tree_search_periodic_packing>(
                    exception_ptr,
                    instance,
                    *context.first,
                    *context.second);
        });
    }
    // MILP raster.
    if (use_milp_raster) {
        exception_ptr_list.push_front(std::exception_ptr());
        std::exception_ptr& exception_ptr = exception_ptr_list.front();
        auto context = task_context();
        tasks.push_back([&exception_ptr, &instance, context]() {
            wrapper<decltype(&optimize_milp_raster), optimize_milp_raster>(
                    exception_ptr,
                    instance,
                    *context.first,
                    *context.second);
        });
    }
    // Local search (linear programming engine).
    if (use_local_search_linear_programming) {
        exception_ptr_list.push_front(std::exception_ptr());
        std::exception_ptr& exception_ptr = exception_ptr_list.front();
        auto context = task_context();
        tasks.push_back([&exception_ptr, &instance, context]() {
            wrapper<decltype(&optimize_local_search_linear_programming), optimize_local_search_linear_programming>(
                    exception_ptr,
                    instance,
                    *context.first,
                    *context.second);
        });
    }
    // Local search (nonlinear programming engine).
    if (use_local_search_nonlinear_programming) {
        exception_ptr_list.push_front(std::exception_ptr());
        std::exception_ptr& exception_ptr = exception_ptr_list.front();
        auto context = task_context();
        tasks.push_back([&exception_ptr, &instance, context]() {
            wrapper<decltype(&optimize_local_search_nonlinear_programming), optimize_local_search_nonlinear_programming>(
                    exception_ptr,
                    instance,
                    *context.first,
                    *context.second);
        });
    }
    // Local search (direct trigonometry engine).
    if (use_local_search_direct_trigonometry) {
        exception_ptr_list.push_front(std::exception_ptr());
        std::exception_ptr& exception_ptr = exception_ptr_list.front();
        auto context = task_context();
        tasks.push_back([&exception_ptr, &instance, context]() {
            wrapper<decltype(&optimize_local_search_direct_trigonometry), optimize_local_search_direct_trigonometry>(
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
                    *context.second,
                    -1);
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
    if (last_bin_post_process_applies(instance, parameters)) {
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

    algorithm_formatter.end();
    return output;
}
