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
        AlgorithmFormatter& algorithm_formatter,
        irregular::Output* local_output)
{
    // Tracks tree_search's own best solution locally (in addition to
    // reporting it via 'local_output'/'algorithm_formatter' as usual), so
    // that the last-bin post-process below -- which is specific to
    // tree_search's own guides -- always has access to it, regardless of
    // whether other algorithms are running alongside it and regardless of
    // the deterministic/non-deterministic reporting mode.
    irregular::Output ts_local_output(instance);

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
    ts_parameters.new_solution_callback = [&algorithm_formatter, local_output, &ts_local_output](
            const irregular::Output& ts_output)
    {
        ts_local_output.solution_pool.add(
                ts_output.solution_pool.best(),
                ts_output.solution_pool.best_label());
        if (local_output != nullptr) {
            local_output->solution_pool.add(ts_output.solution_pool.best(), "TS " + ts_output.solution_pool.best_label());
        } else {
            algorithm_formatter.update_solution(ts_output.solution_pool.best(), "TS " + ts_output.solution_pool.best_label());
        }
    };
    tree_search(instance, ts_parameters);

    // Post-process: for BinPackingWithLeftovers in a not-anytime mode, if
    // tree_search didn't already use guides {2, 3}, re-optimize just the
    // last bin (as its own single-bin sub-instance) with guides {2, 3}
    // forced, in case that guide combination packs it into less leftover
    // space than the one tree_search used.
    const Solution& solution_best = ts_local_output.solution_pool.best();
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

        // Solve instance.
        OptimizeParameters last_bin_parameters;
        last_bin_parameters.verbosity_level = 0;
        last_bin_parameters.timer = parameters.timer;
        last_bin_parameters.optimization_mode = parameters.optimization_mode;
        last_bin_parameters.not_anytime_maximum_approximation_ratio = parameters.not_anytime_maximum_approximation_ratio;
        last_bin_parameters.not_anytime_tree_search_queue_size = parameters.not_anytime_tree_search_queue_size;
        last_bin_parameters.tree_search_guides = {2, 3};
        last_bin_parameters.linear_programming_solver_name = parameters.linear_programming_solver_name;
        // Respect the caller's explicit algorithm selection (if any), rather
        // than always falling back to automatic selection for this
        // single-bin sub-instance.
        last_bin_parameters.use_tree_search = parameters.use_tree_search;
        last_bin_parameters.use_tree_search_periodic_packing = parameters.use_tree_search_periodic_packing;
        last_bin_parameters.use_local_search = parameters.use_local_search;
        last_bin_parameters.use_milp_raster = parameters.use_milp_raster;
        auto last_bin_output = optimize(last_bin_instance, last_bin_parameters);

        if (last_bin_output.solution_pool.best().feasible()) {

            // Retrieve solution.
            Solution solution(instance);
            // Add first bins from current best solution.
            for (BinPos bin_pos = 0;
                    bin_pos < solution_best.number_of_different_bins() - 2;
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
            ss << "TS post-process";
            if (local_output != nullptr) {
                local_output->solution_pool.add(solution, ss.str());
            } else {
                algorithm_formatter.update_solution(solution, ss.str());
            }
        }
    }
}

void optimize_local_search(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter,
        irregular::Output* local_output)
{
    LocalSearchParameters ls_parameters;
    ls_parameters.verbosity_level = 0;
    ls_parameters.timer = parameters.timer;
    ls_parameters.new_solution_callback = [&algorithm_formatter, local_output](
            const irregular::Output& ps_output)
    {
        std::stringstream ss;
        ss << "LS";
        if (local_output != nullptr) {
            local_output->solution_pool.add(ps_output.solution_pool.best(), ss.str());
        } else {
            algorithm_formatter.update_solution(ps_output.solution_pool.best(), ss.str());
        }
    };
    local_search(instance, ls_parameters);
}

void optimize_sequential_single_knapsack(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter,
        Counter queue_size_max,
        irregular::Output* local_output)
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
                kp_parameters.not_anytime_tree_search_periodic_packing_queue_size = queue_size;
                kp_parameters.linear_programming_solver_name = parameters.linear_programming_solver_name;
                auto kp_output = optimize(kp_instance, kp_parameters);
                return kp_output.solution_pool;
            };
        SequentialValueCorrectionParameters<Instance, Solution, irregular::Output> svc_parameters;
        svc_parameters.verbosity_level = 0;
        svc_parameters.timer = parameters.timer;
        svc_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
        svc_parameters.maximum_number_of_iterations = 1;
        svc_parameters.new_solution_callback = [
            &algorithm_formatter, local_output, &queue_size](
                    const irregular::Output& ps_output)
            {
                const SequentialValueCorrectionOutput<Instance, Solution, irregular::Output>& pssvc_output
                    = static_cast<const SequentialValueCorrectionOutput<Instance, Solution, irregular::Output>&>(ps_output);
                std::stringstream ss;
                ss << "SSK q " << queue_size;
                if (local_output != nullptr) {
                    local_output->solution_pool.add(pssvc_output.solution_pool.best(), ss.str());
                } else {
                    algorithm_formatter.update_solution(pssvc_output.solution_pool.best(), ss.str());
                }
            };
        sequential_value_correction<Instance, InstanceBuilder, Solution, AlgorithmFormatter, irregular::Output>(instance, kp_solve, svc_parameters);

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
        AlgorithmFormatter& algorithm_formatter,
        irregular::Output* local_output)
{
    if (parameters.optimization_mode == OptimizationMode::Anytime) {
        optimize_sequential_single_knapsack(
                instance,
                parameters,
                algorithm_formatter,
                parameters.sequential_value_correction_subproblem_tree_search_queue_size - 1,
                local_output);
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
    SequentialValueCorrectionParameters<Instance, Solution, irregular::Output> svc_parameters;
    svc_parameters.verbosity_level = 0;
    svc_parameters.timer = parameters.timer;
    svc_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    if (parameters.optimization_mode != OptimizationMode::Anytime)
        svc_parameters.maximum_number_of_iterations = parameters.not_anytime_sequential_value_correction_number_of_iterations;
    svc_parameters.new_solution_callback = [&algorithm_formatter, local_output](
            const irregular::Output& ps_output)
    {
        const SequentialValueCorrectionOutput<Instance, Solution, irregular::Output>& pssvc_output
            = static_cast<const SequentialValueCorrectionOutput<Instance, Solution, irregular::Output>&>(ps_output);
        std::stringstream ss;
        ss << "SVC it " << pssvc_output.number_of_iterations;
        if (local_output != nullptr) {
            local_output->solution_pool.add(pssvc_output.solution_pool.best(), ss.str());
        } else {
            algorithm_formatter.update_solution(pssvc_output.solution_pool.best(), ss.str());
        }
    };
    sequential_value_correction<Instance, InstanceBuilder, Solution, AlgorithmFormatter, irregular::Output>(instance, kp_solve, svc_parameters);
}

void optimize_dichotomic_search(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter,
        irregular::Output* local_output)
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
            &algorithm_formatter, local_output, &queue_size](
                    const irregular::Output& ps_output)
            {
                const DichotomicSearchOutput<Instance, Solution, irregular::Output>& psds_output
                    = static_cast<const DichotomicSearchOutput<Instance, Solution, irregular::Output>&>(ps_output);
                std::stringstream ss;
                ss << "DS q " << queue_size
                    << " w " << psds_output.waste_percentage;
                if (local_output != nullptr) {
                    local_output->solution_pool.add(psds_output.solution_pool.best(), ss.str());
                } else {
                    algorithm_formatter.update_solution(psds_output.solution_pool.best(), ss.str());
                }
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
        AlgorithmFormatter& algorithm_formatter,
        irregular::Output* local_output)
{
    ColumnGenerationPricingFunction<Instance, InstanceBuilder, Solution, irregular::Output> pricing_function
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
    cg_parameters.new_solution_callback = [&algorithm_formatter, local_output](
            const irregular::Output& ps_output)
    {
        if (local_output != nullptr) {
            local_output->solution_pool.add(ps_output.solution_pool.best(), "CG " + ps_output.solution_pool.best_label());
            local_output->update_bounds(ps_output);
        } else {
            algorithm_formatter.update_solution(ps_output.solution_pool.best(), "CG " + ps_output.solution_pool.best_label());
            algorithm_formatter.update_bounds(ps_output);
        }
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
        AlgorithmFormatter& algorithm_formatter,
        irregular::Output* local_output)
{
    MilpRasterParameters milp_raster_parameters;
    milp_raster_parameters.verbosity_level = 0;
    milp_raster_parameters.timer = parameters.timer;
    milp_raster_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    milp_raster_parameters.new_solution_callback = [&algorithm_formatter, local_output](
            const irregular::Output& output) {
        if (local_output != nullptr) {
            local_output->solution_pool.add(output.solution_pool.best(), "MILP raster");
            local_output->update_bounds(output);
        } else {
            algorithm_formatter.update_solution(
                    output.solution_pool.best(),
                    "MILP raster");
            algorithm_formatter.update_bounds(output);
        }
    };
    milp_raster(instance, milp_raster_parameters);
}

/**
 * Estimate the maximum number of items that could fit in the (single) bin
 * of the instance, using only their areas: sort item types by increasing
 * item area and greedily count copies (smallest items first) until their
 * cumulative area reaches the bin's area.
 */
ItemPos max_number_of_items_in_bin(const Instance& instance)
{
    std::vector<std::pair<AreaDbl, ItemPos>> items_by_area;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        items_by_area.push_back({item_type.space(), item_type.copies});
    }
    std::sort(items_by_area.begin(), items_by_area.end());

    AreaDbl bin_area = instance.bin_type(0).space();
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

}

packingsolver::irregular::Output packingsolver::irregular::optimize(
        const Instance& instance,
        const OptimizeParameters& parameters)
{
    Output output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    optimize_trivial_bound(instance, algorithm_formatter);

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

    // Select algorithms to run.
    ItemPos mean_number_of_items_in_bins
        = largest_bin_space(instance) / mean_item_space(instance);
    bool use_tree_search = parameters.use_tree_search;
    bool use_tree_search_periodic_packing = parameters.use_tree_search_periodic_packing;
    bool use_local_search = parameters.use_local_search;
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
            use_local_search = false;
        }
        // Automatic selection.
        if (!use_tree_search
                && !use_tree_search_periodic_packing
                && !use_local_search
                && !use_milp_raster) {
            // tree_search_periodic_packing is worth running when the
            // instance is dominated by item types with many copies: either
            // these alone already exceed the bin's area (so a periodic
            // tiling of them is going to be needed regardless of the other
            // item types), or they make up the entire instance (no other
            // item type contributes any area).
            AreaDbl bin_area = instance.bin_type(0).space();
            AreaDbl high_copies_area = 0.0;
            AreaDbl total_item_area = 0.0;
            for (ItemTypeId item_type_id = 0;
                    item_type_id < instance.number_of_item_types();
                    ++item_type_id) {
                const ItemType& item_type = instance.item_type(item_type_id);
                AreaDbl item_type_area = item_type.copies * item_type.space();
                total_item_area += item_type_area;
                if (item_type.copies > parameters.periodic_packing_copies_threshold)
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
        }
    } else if (instance.objective() == Objective::Feasibility) {
        // Disable algorithms which are not available for this objective.
        use_local_search = false;
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
    } else if (instance.objective() == Objective::Knapsack) {
        // Disable algorithms which are not available for this objective.
        use_local_search = false;
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
        use_local_search = false;
        use_tree_search_periodic_packing = false;
        if (instance.number_of_bin_types() == 1) {
            if (use_dichotomic_search) {
                use_dichotomic_search = false;
                use_tree_search = true;
            }
        }
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

    // Run selected algorithms.
    // In 'NotAnytimeDeterministic' mode, algorithms still run in parallel, but
    // each writes its solutions to its own 'local_output' instead of the
    // shared 'algorithm_formatter', so that they can be replayed into it in a
    // fixed, deterministic order once every algorithm has terminated
    // ('run(tasks, ...)' does not guarantee a deterministic finish order).
    // 'local_outputs' owns these; a 'unique_ptr' is used so that it growing
    // does not invalidate the raw pointers captured by the tasks below.
    bool deterministic = (parameters.optimization_mode == OptimizationMode::NotAnytimeDeterministic);
    std::vector<std::unique_ptr<irregular::Output>> local_outputs;
    std::vector<std::function<void()>> tasks;
    std::forward_list<std::exception_ptr> exception_ptr_list;
    // Tree search.
    if (use_tree_search) {
        exception_ptr_list.push_front(std::exception_ptr());
        std::exception_ptr& exception_ptr = exception_ptr_list.front();
        std::unique_ptr<irregular::Output> local_output;
        if (deterministic)
            local_output = std::make_unique<irregular::Output>(instance);
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
    // Tree search periodic packing.
    if (use_tree_search_periodic_packing) {
        exception_ptr_list.push_front(std::exception_ptr());
        std::exception_ptr& exception_ptr = exception_ptr_list.front();
        tasks.push_back([&exception_ptr, &instance, &parameters, &algorithm_formatter]() {
            wrapper<decltype(&optimize_tree_search_periodic_packing),
                    optimize_tree_search_periodic_packing>(
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
        std::unique_ptr<irregular::Output> local_output;
        if (deterministic)
            local_output = std::make_unique<irregular::Output>(instance);
        tasks.push_back([&exception_ptr, &instance, &parameters, &algorithm_formatter, local_output = local_output.get()]() {
            wrapper<decltype(&optimize_milp_raster), optimize_milp_raster>(
                    exception_ptr,
                    instance,
                    parameters,
                    algorithm_formatter,
                    local_output);
        });
        local_outputs.push_back(std::move(local_output));
    }
    // Local search.
    if (use_local_search) {
        exception_ptr_list.push_front(std::exception_ptr());
        std::exception_ptr& exception_ptr = exception_ptr_list.front();
        std::unique_ptr<irregular::Output> local_output;
        if (deterministic)
            local_output = std::make_unique<irregular::Output>(instance);
        tasks.push_back([&exception_ptr, &instance, &parameters, &algorithm_formatter, local_output = local_output.get()]() {
            wrapper<decltype(&optimize_local_search), optimize_local_search>(
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
        std::unique_ptr<irregular::Output> local_output;
        if (deterministic)
            local_output = std::make_unique<irregular::Output>(instance);
        tasks.push_back([&exception_ptr, &instance, &parameters, &algorithm_formatter, local_output = local_output.get()]() {
            wrapper<decltype(&optimize_sequential_single_knapsack), optimize_sequential_single_knapsack>(
                    exception_ptr,
                    instance,
                    parameters,
                    algorithm_formatter,
                    -1,
                    local_output);
        });
        local_outputs.push_back(std::move(local_output));
    }
    // Sequential value correction.
    if (use_sequential_value_correction) {
        exception_ptr_list.push_front(std::exception_ptr());
        std::exception_ptr& exception_ptr = exception_ptr_list.front();
        std::unique_ptr<irregular::Output> local_output;
        if (deterministic)
            local_output = std::make_unique<irregular::Output>(instance);
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
        std::unique_ptr<irregular::Output> local_output;
        if (deterministic)
            local_output = std::make_unique<irregular::Output>(instance);
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
        std::unique_ptr<irregular::Output> local_output;
        if (deterministic)
            local_output = std::make_unique<irregular::Output>(instance);
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
