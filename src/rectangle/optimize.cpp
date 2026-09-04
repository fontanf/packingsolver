#include "packingsolver/rectangle/optimize.hpp"

#include "packingsolver/rectangle/algorithm_formatter.hpp"
#include "packingsolver/rectangle/instance_builder.hpp"
#include "rectangle/tree_search.hpp"
#include "rectangle/tree_search_maximal_spaces.hpp"
#include "rectangle/benders_decomposition.hpp"
#include "rectangle/benders_decomposition_contiguity.hpp"
#include "rectangle/dual_feasible_functions.hpp"
#include "rectangle/conservative_scales.hpp"
#include "rectangle/bar_relaxation.hpp"
#include "packingsolver/onedimensional/instance_builder.hpp"
#include "packingsolver/onedimensional/optimize.hpp"
#include "algorithms/dichotomic_search.hpp"
#include "algorithms/sequential_value_correction.hpp"
#include "algorithms/column_generation.hpp"
#include "algorithms/thread_pool.hpp"


using namespace packingsolver;
using namespace packingsolver::rectangle;

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
        //
        // Items that don't fit (in either orientation, if allowed) in any
        // bin type can never be packed, so they must be excluded entirely
        // rather than counted as fractionally packable area: otherwise an
        // oversized item's area alone can make the bound arbitrarily
        // looser than the achievable profit.
        Area total_capacity = 0;
        for (BinTypeId bin_type_id = 0;
                bin_type_id < instance.number_of_bin_types();
                ++bin_type_id) {
            const BinType& bin_type = instance.bin_type(bin_type_id);
            total_capacity += bin_type.area() * bin_type.copies;
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
                    return item_type_1.profit * item_type_2.area()
                        > item_type_2.profit * item_type_1.area();
                });
        Profit bound = 0.0;
        Area remaining_capacity = total_capacity;
        for (ItemTypeId item_type_id: sorted_item_types) {
            if (remaining_capacity <= 0)
                break;
            const ItemType& item_type = instance.item_type(item_type_id);
            if (item_type.area() <= 0)
                continue;
            Area item_total_area = item_type.area() * item_type.copies;
            if (item_total_area <= remaining_capacity) {
                bound += item_type.profit * item_type.copies;
                remaining_capacity -= item_total_area;
            } else {
                bound += item_type.profit
                    * ((double)remaining_capacity / item_type.area());
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
        // bin/item types), so useful when there are too many (small) items
        // for the more expensive dual feasible functions bound to run.
        Area remaining_item_area = instance.item_area();
        BinPos bound = 0;
        for (BinTypeId bin_type_id = 0;
                bin_type_id < instance.number_of_bin_types();
                ++bin_type_id) {
            if (remaining_item_area <= 0)
                break;
            const BinType& bin_type = instance.bin_type(bin_type_id);
            if (bin_type.area() <= 0)
                continue;
            BinPos bins_needed = (BinPos)((remaining_item_area + bin_type.area() - 1) / bin_type.area());
            BinPos bins_used = std::min(bins_needed, bin_type.copies);
            bound += bins_used;
            remaining_item_area -= bins_used * bin_type.area();
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
    // the bin.
    // Item-based bound: the open dimension cannot be smaller than the
    // smallest extent an item can have in that direction (accounting for
    // whether it may be rotated), for the item requiring the most space in
    // that direction.
    const auto& bin_type = instance.bin_type(0);
    Length fixed_dimension = (instance.objective() == Objective::OpenDimensionX)?
        bin_type.rect.y:
        bin_type.rect.x;
    Length bound = (fixed_dimension > 0)?
        (Length)((instance.item_area() + fixed_dimension - 1) / fixed_dimension):
        0;

    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        Length item_min_extent = (instance.objective() == Objective::OpenDimensionX)?
            item_type.rect.x:
            item_type.rect.y;
        if (!item_type.oriented) {
            item_min_extent = std::min(
                    item_min_extent,
                    (instance.objective() == Objective::OpenDimensionX)?
                        item_type.rect.y:
                        item_type.rect.x);
        }
        bound = std::max(bound, item_min_extent);
    }

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
    // Relax the instance to 1D, keeping only bin/item areas: any solution of
    // the original instance is also a solution of this relaxation (with the
    // same cost), so the bound the onedimensional solver finds for it
    // (which runs the same dichotomic search) is a valid lower bound here.
    onedimensional::InstanceBuilder onedim_instance_builder;
    onedim_instance_builder.set_objective(instance.objective());
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        BinTypeId onedim_bin_type_id = onedim_instance_builder.add_bin_type(bin_type.area());
        onedim_instance_builder.set_bin_type_cost(onedim_bin_type_id, bin_type.cost);
        onedim_instance_builder.set_bin_type_copies(onedim_bin_type_id, bin_type.copies);
        onedim_instance_builder.set_bin_type_copies_min(onedim_bin_type_id, bin_type.copies_min);
    }
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        if (item_type.area() <= 0)
            continue;
        ItemTypeId onedim_item_type_id = onedim_instance_builder.add_item_type(item_type.area());
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
    auto onedim_output = optimize(onedim_instance, onedim_parameters);

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
                const rectangle::Output& dff_output)
        {
            algorithm_formatter.update_bounds(dff_output);
        };
    dual_feasible_functions(instance, dff_parameters);
}

void optimize_conservative_scales(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter,
        rectangle::Output* local_output)
{
    ConservativeScalesParameters cs_parameters;
    cs_parameters.verbosity_level = 0;
    cs_parameters.timer = parameters.timer;
    cs_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    cs_parameters.new_solution_callback
        = [&algorithm_formatter, local_output](
                const rectangle::Output& cs_output)
        {
            if (local_output != nullptr) {
                local_output->update_bounds(cs_output);
            } else {
                algorithm_formatter.update_bounds(cs_output);
            }
        };
    conservative_scales(instance, cs_parameters);
}

void optimize_bar_relaxation(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter,
        rectangle::Output* local_output)
{
    BarRelaxationParameters br_parameters;
    br_parameters.verbosity_level = 0;
    br_parameters.timer = parameters.timer;
    br_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    br_parameters.linear_programming_solver_name = parameters.linear_programming_solver_name;
    br_parameters.new_solution_callback
        = [&algorithm_formatter, local_output](
                const rectangle::Output& br_output)
        {
            if (local_output != nullptr) {
                local_output->update_bounds(br_output);
            } else {
                algorithm_formatter.update_bounds(br_output);
            }
        };
    bar_relaxation(instance, br_parameters);
}

void optimize_tree_search(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter,
        rectangle::Output* local_output)
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
    ts_parameters.fixed_items = parameters.fixed_items;
    ts_parameters.new_solution_callback = [&algorithm_formatter, local_output](
            const rectangle::Output& ts_output)
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
        rectangle::Output* local_output)
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
        SequentialValueCorrectionParameters<Instance, Solution, rectangle::Output> svc_parameters;
        svc_parameters.verbosity_level = 0;
        svc_parameters.timer = parameters.timer;
        svc_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
        svc_parameters.maximum_number_of_iterations = 1;
        svc_parameters.new_solution_callback = [
            &algorithm_formatter, local_output, &queue_size](
                    const rectangle::Output& ps_output)
            {
                const SequentialValueCorrectionOutput<Instance, Solution, rectangle::Output>& pssvc_output
                    = static_cast<const SequentialValueCorrectionOutput<Instance, Solution, rectangle::Output>&>(ps_output);
                std::stringstream ss;
                ss << "SSK q " << queue_size;
                if (local_output != nullptr) {
                    local_output->solution_pool.add(pssvc_output.solution_pool.best(), ss.str());
                } else {
                    algorithm_formatter.update_solution(pssvc_output.solution_pool.best(), ss.str());
                }
            };
        sequential_value_correction<Instance, InstanceBuilder, Solution, AlgorithmFormatter, rectangle::Output>(instance, kp_solve, svc_parameters);

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
        rectangle::Output* local_output)
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
    SequentialValueCorrectionParameters<Instance, Solution, rectangle::Output> svc_parameters;
    svc_parameters.verbosity_level = 0;
    svc_parameters.timer = parameters.timer;
    svc_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    if (parameters.optimization_mode != OptimizationMode::Anytime)
        svc_parameters.maximum_number_of_iterations = parameters.not_anytime_sequential_value_correction_number_of_iterations;
    svc_parameters.new_solution_callback = [&algorithm_formatter, local_output](
            const rectangle::Output& ps_output)
    {
        const SequentialValueCorrectionOutput<Instance, Solution, rectangle::Output>& pssvc_output
            = static_cast<const SequentialValueCorrectionOutput<Instance, Solution, rectangle::Output>&>(ps_output);
        std::stringstream ss;
        ss << "SVC it " << pssvc_output.number_of_iterations;
        if (local_output != nullptr) {
            local_output->solution_pool.add(pssvc_output.solution_pool.best(), ss.str());
        } else {
            algorithm_formatter.update_solution(pssvc_output.solution_pool.best(), ss.str());
        }
    };
    sequential_value_correction<Instance, InstanceBuilder, Solution, AlgorithmFormatter, rectangle::Output>(instance, kp_solve, svc_parameters);
}

void optimize_dichotomic_search(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter,
        rectangle::Output* local_output)
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
        DichotomicSearchParameters<Instance, Solution, rectangle::Output> ds_parameters;
        ds_parameters.verbosity_level = 0;
        ds_parameters.timer = parameters.timer;
        ds_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
        ds_parameters.initial_waste_percentage_upper_bound = waste_percentage_upper_bound;
        ds_parameters.new_solution_callback = [
            &algorithm_formatter, local_output, &queue_size](
                    const rectangle::Output& ps_output)
            {
                const DichotomicSearchOutput<Instance, Solution, rectangle::Output>& psds_output
                    = static_cast<const DichotomicSearchOutput<Instance, Solution, rectangle::Output>&>(ps_output);
                std::stringstream ss;
                ss << "DS q " << queue_size
                    << " w " << psds_output.waste_percentage;
                if (local_output != nullptr) {
                    local_output->solution_pool.add(psds_output.solution_pool.best(), ss.str());
                } else {
                    algorithm_formatter.update_solution(psds_output.solution_pool.best(), ss.str());
                }
            };
        auto ds_output = dichotomic_search<Instance, InstanceBuilder, Solution, AlgorithmFormatter, rectangle::Output>(instance, bpp_solve, ds_parameters);

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
        rectangle::Output* local_output,
        BinPos lower_bound)
{
    ColumnGenerationPricingFunction<Instance, InstanceBuilder, Solution, rectangle::Output> pricing_function
        = [&parameters](const Instance& kp_instance, PricingType pricing_type)
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
            kp_parameters.not_anytime_tree_search_maximal_spaces_queue_size
                = parameters.column_generation_subproblem_tree_search_maximal_spaces_queue_size;
            kp_parameters.linear_programming_solver_name = parameters.linear_programming_solver_name;
            // 'PricingType::Dual' calls are rare (at most twice per column
            // generation attempt) and exist specifically to *prove* a bound
            // cheaply enough to justify the cost - see
            // 'columngenerationsolver::PricingSolver::solve_pricing''s own
            // doc comment. 'benders_decomposition' is the one algorithm here
            // that converges to a proven-exact answer (or bound) rather than
            // an incomplete search cut short by a queue size, so force it
            // alone, bypassing the regular (heuristic) automatic algorithm
            // selection entirely.
            if (pricing_type == PricingType::Dual) {
                //kp_parameters.verbosity_level = 1;
                kp_parameters.use_benders_decomposition = true;
            }
            return optimize(kp_instance, kp_parameters);
        };

    ColumnGenerationParameters<Instance, Solution, rectangle::Output> cg_parameters;
    cg_parameters.verbosity_level = 0;
    cg_parameters.timer = parameters.timer;
    cg_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    cg_parameters.optimization_mode = parameters.optimization_mode;
    cg_parameters.linear_programming_solver_name = parameters.linear_programming_solver_name;
    cg_parameters.use_cutting_planes = 2;
    cg_parameters.pricing_function_has_dual_bound = true;
    cg_parameters.new_solution_callback = [&algorithm_formatter, local_output](
            const rectangle::Output& ps_output)
    {
        if (local_output != nullptr) {
            local_output->solution_pool.add(ps_output.solution_pool.best(), "CG " + ps_output.solution_pool.best_label());
            local_output->update_bounds(ps_output);
        } else {
            algorithm_formatter.update_solution(ps_output.solution_pool.best(), "CG " + ps_output.solution_pool.best_label());
            algorithm_formatter.update_bounds(ps_output);
        }
    };
    column_generation<Instance, InstanceBuilder, Solution, AlgorithmFormatter, rectangle::Output>(
            instance, pricing_function, cg_parameters, lower_bound);
}

void optimize_tree_search_maximal_spaces(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter,
        rectangle::Output* local_output)
{
    TreeSearchMaximalSpacesParameters ts_ms_parameters;
    ts_ms_parameters.verbosity_level = 0;
    ts_ms_parameters.timer = parameters.timer;
    ts_ms_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    ts_ms_parameters.optimization_mode = parameters.optimization_mode;
    ts_ms_parameters.not_anytime_tree_search_queue_size = parameters.not_anytime_tree_search_maximal_spaces_queue_size;
    ts_ms_parameters.new_solution_callback = [&algorithm_formatter, local_output](
            const rectangle::Output& ts_output)
    {
        if (local_output != nullptr) {
            local_output->solution_pool.add(ts_output.solution_pool.best(), "TSMS " + ts_output.solution_pool.best_label());
            local_output->update_bounds(ts_output);
        } else {
            algorithm_formatter.update_solution(ts_output.solution_pool.best(), "TSMS " + ts_output.solution_pool.best_label());
            algorithm_formatter.update_bounds(ts_output);
        }
    };
    tree_search_maximal_spaces(instance, ts_ms_parameters);
}

void optimize_benders_decomposition(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter,
        rectangle::Output* local_output)
{
    BendersDecompositionParameters bd_parameters;
    bd_parameters.verbosity_level = 0;
    bd_parameters.timer = parameters.timer;
    bd_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    bd_parameters.optimization_mode = parameters.optimization_mode;
    // Only applied by 'benders_decomposition' itself in non-'Anytime' modes
    // (see 'BendersDecompositionParameters::not_anytime_maximum_number_of_iterations'),
    // so it is harmless to always set it here.
    bd_parameters.not_anytime_maximum_number_of_iterations = parameters.not_anytime_benders_decomposition_number_of_iterations;
    bd_parameters.new_solution_callback = [&algorithm_formatter, local_output](
            const rectangle::Output& ps_output)
    {
        const BendersDecompositionOutput& psbd_output
            = static_cast<const BendersDecompositionOutput&>(ps_output);
        std::stringstream ss;
        ss << "BD " << psbd_output.number_of_iterations;
        if (local_output != nullptr) {
            local_output->solution_pool.add(psbd_output.solution_pool.best(), ss.str());
            local_output->update_bounds(psbd_output);
        } else {
            algorithm_formatter.update_solution(psbd_output.solution_pool.best(), ss.str());
            algorithm_formatter.update_bounds(psbd_output);
        }
    };
    benders_decomposition(instance, bd_parameters);
}

void optimize_benders_decomposition_contiguity(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter,
        rectangle::Output* local_output)
{
    BendersDecompositionContiguityParameters bdc_parameters;
    bdc_parameters.verbosity_level = 0;
    bdc_parameters.timer = parameters.timer;
    bdc_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    bdc_parameters.optimization_mode = parameters.optimization_mode;
    if (parameters.optimization_mode != OptimizationMode::Anytime)
        bdc_parameters.not_anytime_maximum_number_of_iterations = parameters.not_anytime_benders_decomposition_contiguity_number_of_iterations;
    bdc_parameters.new_solution_callback = [&algorithm_formatter, local_output](
            const rectangle::Output& ps_output)
    {
        const BendersDecompositionContiguityOutput& psbdc_output
            = static_cast<const BendersDecompositionContiguityOutput&>(ps_output);
        std::stringstream ss;
        ss << "BDC " << psbdc_output.number_of_iterations;
        if (local_output != nullptr) {
            local_output->solution_pool.add(psbdc_output.solution_pool.best(), ss.str());
            local_output->update_bounds(psbdc_output);
        } else {
            algorithm_formatter.update_solution(psbdc_output.solution_pool.best(), ss.str());
            algorithm_formatter.update_bounds(psbdc_output);
        }
    };
    benders_decomposition_contiguity(instance, bdc_parameters);
}

}

packingsolver::rectangle::Output packingsolver::rectangle::optimize(
        const Instance& instance,
        const OptimizeParameters& parameters)
{
    rectangle::Output output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    // "Packing and removing some items" reduction (see 'Reduction'):
    // applied once, upfront, wrapping the whole dispatch logic below
    // uniformly for every algorithm (mirroring how e.g. setcoveringsolver's
    // 'Reduction' wraps its own algorithms generically). Gated directly on
    // 'parameters.reduction_parameters.reduce', rather than constructing a
    // 'Reduction' unconditionally and letting its constructor no-op: when
    // it is 'false', this skips the reduction entirely instead of paying
    // for (and discarding) a full pass building its working
    // representation. 'reduced_parameters.reduction_parameters.reduce' is
    // set to 'false' below to avoid re-running the reduction recursively
    // on the already-reduced instance - a pass that can only ever find
    // nothing new (the reduction already ran to a fixpoint), just at the
    // cost of a wasted full pass to confirm it.
    if (parameters.reduction_parameters.reduce) {
        ReductionParameters reduction_parameters = parameters.reduction_parameters;
        reduction_parameters.timer = parameters.timer;
        Reduction reduction(instance, reduction_parameters);

        if (reduction.proven_infeasible()) {
            // The reduction alone already proves the instance infeasible
            // (a bin type's dedicated bins exhausted its copies while real
            // items were still left over - see 'Reduction::instance()''s
            // doc): 'reduction.instance()' is not meaningful to solve at
            // all, so skip straight to reporting the answer.
            algorithm_formatter.update_is_proven_infeasible();
            algorithm_formatter.end();
            return output;
        }
        // Forwards a solution/bound found for the reduced instance to the
        // original 'algorithm_formatter', in original-instance coordinates.
        // Most bounds need no reduction-specific translation at all:
        // removing items via this reduction never changes them (cost,
        // feasibility, ...), so 'reduced_output's bounds already are the
        // original instance's bounds, in exactly 'Output's own field
        // layout. The one exception is the dedicated bins set aside
        // outside the reduced instance (see
        // 'Reduction::number_of_dedicated_bins'): entirely absent from the
        // reduced instance, so no solve on it can ever count them - add
        // them back onto the bin-count bounds, in a local copy, before
        // letting 'update_bounds' read off the one relevant to the current
        // objective.
        auto report_reduced_output = [&reduction, &instance, &algorithm_formatter](
                const rectangle::Output& reduced_output)
            {
                algorithm_formatter.update_solution(
                        reduction.unreduce_solution(reduced_output.solution_pool.best()),
                        reduced_output.solution_pool.best_label());
                rectangle::Output translated_output(reduced_output);
                BinPos number_of_dedicated_bins = reduction.number_of_dedicated_bins();
                if (number_of_dedicated_bins > 0) {
                    translated_output.bin_packing_bound += number_of_dedicated_bins;
                    translated_output.variable_sized_bin_packing_bound
                        += number_of_dedicated_bins * instance.bin_type(0).cost;
                }
                algorithm_formatter.update_bounds(translated_output);
            };

        OptimizeParameters reduced_parameters = parameters;
        reduced_parameters.verbosity_level = 0;
        reduced_parameters.reduction_parameters.reduce = false;
        // Forward every solution/bound the recursive solve finds to the
        // original 'algorithm_formatter' as soon as it is found, rather
        // than only once at the very end: this is what lets an anytime
        // run on the reduced instance report live progress (in original-
        // instance coordinates) the same way a direct, unreduced run
        // would.
        reduced_parameters.new_solution_callback = report_reduced_output;
        Output reduced_output = optimize(reduction.instance(), reduced_parameters);
        // Also report the final result explicitly: some sub-solves (e.g. a
        // reduced instance left with zero items, fully captured into
        // dedicated bins) never call 'new_solution_callback' at all, since
        // there is nothing to search for or improve on - this guarantees
        // the answer is still reported once regardless. Harmless to call
        // again even when the callback already reported this exact result,
        // since 'update_solution'/'update_bounds' are themselves no-ops
        // for anything that doesn't improve on what is already recorded.
        report_reduced_output(reduced_output);

        algorithm_formatter.end();
        return output;
    }

    optimize_trivial_bound(instance, algorithm_formatter);

    if (instance.objective() == Objective::BinPacking
            || instance.objective() == Objective::Knapsack
            || instance.objective() == Objective::VariableSizedBinPacking) {
        optimize_onedimensional_bound(
                instance,
                parameters,
                algorithm_formatter);
    }

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

    // Select algorithms to run.
    ItemPos mean_number_of_items_in_bins
        = largest_bin_space(instance) / mean_item_space(instance);
    // Expected number of items in a bin x (resp. y) section - a full-height
    // (resp. full-width) strip as wide (resp. as tall) as the mean item -
    // rather than in the whole bin: what actually drives 'tree_search''s
    // skyline branching factor (see 'many_items_in_section_threshold''s own
    // doc comment).
    double mean_item_width = (double)instance.total_item_width() / instance.number_of_items();
    double mean_item_height = (double)instance.total_item_height() / instance.number_of_items();
    double mean_bin_width = (double)instance.total_bin_width() / instance.number_of_bins();
    double mean_bin_height = (double)instance.total_bin_height() / instance.number_of_bins();
    double mean_number_of_items_in_x_section = mean_bin_height / mean_item_height;
    double mean_number_of_items_in_y_section = mean_bin_width / mean_item_width;
    // Directions 'tree_search' should run with, when the automatic
    // selection below picks it over 'tree_search_maximal_spaces' by
    // restricting to whichever of 'X'/'Y' has few enough items per section
    // (see 'many_items_in_section_threshold''s own doc comment). Empty
    // means "no restriction" (let 'tree_search' decide on its own, as
    // before).
    std::vector<Direction> tree_search_directions_override;
    bool use_tree_search = parameters.use_tree_search;
    bool use_tree_search_maximal_spaces = parameters.use_tree_search_maximal_spaces;
    bool use_sequential_single_knapsack = parameters.use_sequential_single_knapsack;
    bool use_sequential_value_correction = parameters.use_sequential_value_correction;
    bool use_dichotomic_search = parameters.use_dichotomic_search;
    bool use_column_generation = parameters.use_column_generation;
    bool use_benders_decomposition = parameters.use_benders_decomposition;
    bool use_benders_decomposition_contiguity = parameters.use_benders_decomposition_contiguity;
    bool use_bar_relaxation = parameters.use_bar_relaxation;
    if (instance.number_of_bins() <= 1) {
        use_sequential_single_knapsack = false;
        use_sequential_value_correction = false;
        use_dichotomic_search = false;
        use_column_generation = false;
        if (instance.objective() != Objective::Knapsack
                && instance.objective() != Objective::Feasibility)
            use_tree_search_maximal_spaces = false;
        // 'tree_search_maximal_spaces' doesn't implement unloading
        // constraints at all (unlike 'tree_search'): force it off even if
        // the caller explicitly requested it, rather than silently running
        // and ignoring the constraint.
        if (instance.parameters().unloading_constraint != UnloadingConstraint::None)
            use_tree_search_maximal_spaces = false;
        // 'tree_search_maximal_spaces' doesn't implement semi-trailer-truck
        // axle weight distribution constraints either: same reasoning as
        // the unloading constraint above.
        for (BinTypeId bin_type_id = 0;
                use_tree_search_maximal_spaces
                        && bin_type_id < instance.number_of_bin_types();
                ++bin_type_id) {
            if (instance.bin_type(bin_type_id).semi_trailer_truck_data.is)
                use_tree_search_maximal_spaces = false;
        }
        if (instance.objective() != Objective::Knapsack
                && instance.objective() != Objective::Feasibility)
            use_bar_relaxation = false;
        // Automatic selection.
        if (!use_tree_search
                && !use_tree_search_maximal_spaces
                && !use_benders_decomposition
                && !use_bar_relaxation
                && !use_benders_decomposition_contiguity) {
            if ((instance.objective() == Objective::Knapsack
                        || instance.objective() == Objective::Feasibility)
                    && instance.parameters().unloading_constraint == UnloadingConstraint::None) {
                // Restrict 'tree_search' to whichever direction(s) keep few
                // enough items per section - its skyline branching factor
                // stays manageable there - falling back to
                // 'tree_search_maximal_spaces' only if neither direction
                // does.
                std::vector<Direction> restricted_directions;
                if (mean_number_of_items_in_x_section <= parameters.many_items_in_section_threshold)
                    restricted_directions.push_back(Direction::X);
                if (mean_number_of_items_in_y_section <= parameters.many_items_in_section_threshold)
                    restricted_directions.push_back(Direction::Y);
                if (!restricted_directions.empty()) {
                    use_tree_search = true;
                    tree_search_directions_override = restricted_directions;
                } else {
                    use_tree_search_maximal_spaces = true;
                }
            } else {
                use_tree_search = true;
            }
            if ((instance.objective() == Objective::Knapsack
                        || instance.objective() == Objective::Feasibility)
                    && instance.number_of_bin_types() == 1
                    && instance.bin_type(0).rect.x <= 100
                    && instance.bin_type(0).rect.y <= 100) {
                use_bar_relaxation = true;
            }
        }
    } else if (instance.objective() == Objective::Feasibility) {
        // Disable algorithms which are not available for this objective.
        use_tree_search_maximal_spaces = false;
        use_dichotomic_search = false;
        use_benders_decomposition = false;
        use_benders_decomposition_contiguity = false;
        use_bar_relaxation = false;
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
                use_tree_search = true;
                use_column_generation = true;
            }
        }
    } else if (instance.objective() == Objective::Knapsack) {
        // Disable algorithms which are not available for this objective.
        use_tree_search_maximal_spaces = false;
        use_dichotomic_search = false;
        // Unlike 'use_benders_decomposition', 'benders_decomposition_contiguity'
        // requires a single bin type used exactly once (see its own
        // validation) - this branch is only reached when
        // 'instance.number_of_bins() > 1'.
        use_benders_decomposition_contiguity = false;
        // Automatic selection.
        if (!use_tree_search
                && !use_sequential_single_knapsack
                && !use_sequential_value_correction
                && !use_column_generation
                && !use_benders_decomposition
                && !use_bar_relaxation) {
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
        // 'benders_decomposition_contiguity' only supports 'Knapsack'/
        // 'Feasibility' (see its own validation).
        use_benders_decomposition_contiguity = false;
        if (instance.objective() == Objective::BinPackingWithLeftovers)
            use_benders_decomposition = false;
        if (instance.objective() == Objective::BinPackingWithLeftovers
                || instance.number_of_bin_types() > 1) {
            use_bar_relaxation = false;
        }
        // Automatic selection.
        if (!use_tree_search
                && !use_sequential_single_knapsack
                && !use_sequential_value_correction
                && !use_column_generation
                && !use_benders_decomposition
                && !use_bar_relaxation) {
            if (mean_item_type_copies(instance)
                    > parameters.many_item_type_copies_factor
                    * mean_number_of_items_in_bins) {
                if (mean_number_of_items_in_bins
                        > parameters.many_items_in_bins_threshold) {
                    use_sequential_single_knapsack = true;
                } else {
                    use_sequential_value_correction = true;
                    if (instance.objective() == Objective::BinPacking) {
                        use_column_generation = true;
                    }
                }
            } else {
                use_tree_search = true;
                if (mean_number_of_items_in_bins
                        > parameters.many_items_in_bins_threshold) {
                    use_sequential_single_knapsack = true;
                } else {
                    use_sequential_value_correction = true;
                    if (instance.objective() == Objective::BinPacking) {
                        use_column_generation = true;
                    }
                }
            }
        }
    } else if (instance.objective() == Objective::VariableSizedBinPacking) {
        // Disable algorithms which are not available for this objective.
        use_tree_search_maximal_spaces = false;
        // 'benders_decomposition_contiguity' only supports 'Knapsack'/
        // 'Feasibility' (see its own validation).
        use_benders_decomposition_contiguity = false;
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
                && !use_benders_decomposition
                && !use_bar_relaxation) {
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
    std::vector<std::unique_ptr<rectangle::Output>> local_outputs;
    std::vector<std::function<void()>> tasks;
    std::forward_list<std::exception_ptr> exception_ptr_list;
    // Tree search.
    if (use_tree_search) {
        exception_ptr_list.push_front(std::exception_ptr());
        std::exception_ptr& exception_ptr = exception_ptr_list.front();
        std::unique_ptr<rectangle::Output> local_output;
        if (deterministic)
            local_output = std::make_unique<rectangle::Output>(instance);
        // 'tree_search_directions_override' (set by the automatic selection
        // above) takes priority over any direction restriction the caller
        // already set in 'parameters' - the automatic selection only ever
        // runs when the caller left both 'use_tree_search' and
        // 'use_tree_search_maximal_spaces' unset, so there is no explicit
        // choice here to override.
        OptimizeParameters tree_search_parameters = parameters;
        if (!tree_search_directions_override.empty())
            tree_search_parameters.tree_search_directions = tree_search_directions_override;
        tasks.push_back([&exception_ptr, &instance, tree_search_parameters, &algorithm_formatter, local_output = local_output.get()]() {
            wrapper<decltype(&optimize_tree_search), optimize_tree_search>(
                    exception_ptr,
                    instance,
                    tree_search_parameters,
                    algorithm_formatter,
                    local_output);
        });
        local_outputs.push_back(std::move(local_output));
    }
    // Tree search maximal spaces.
    if (use_tree_search_maximal_spaces) {
        exception_ptr_list.push_front(std::exception_ptr());
        std::exception_ptr& exception_ptr = exception_ptr_list.front();
        std::unique_ptr<rectangle::Output> local_output;
        if (deterministic)
            local_output = std::make_unique<rectangle::Output>(instance);
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
    // Bender's decomposition.
    if (use_benders_decomposition) {
        exception_ptr_list.push_front(std::exception_ptr());
        std::exception_ptr& exception_ptr = exception_ptr_list.front();
        std::unique_ptr<rectangle::Output> local_output;
        if (deterministic)
            local_output = std::make_unique<rectangle::Output>(instance);
        tasks.push_back([&exception_ptr, &instance, &parameters, &algorithm_formatter, local_output = local_output.get()]() {
            wrapper<decltype(&optimize_benders_decomposition), optimize_benders_decomposition>(
                    exception_ptr,
                    instance,
                    parameters,
                    algorithm_formatter,
                    local_output);
        });
        local_outputs.push_back(std::move(local_output));
    }
    // Bender's decomposition (contiguity master).
    if (use_benders_decomposition_contiguity) {
        exception_ptr_list.push_front(std::exception_ptr());
        std::exception_ptr& exception_ptr = exception_ptr_list.front();
        std::unique_ptr<rectangle::Output> local_output;
        if (deterministic)
            local_output = std::make_unique<rectangle::Output>(instance);
        tasks.push_back([&exception_ptr, &instance, &parameters, &algorithm_formatter, local_output = local_output.get()]() {
            wrapper<decltype(&optimize_benders_decomposition_contiguity), optimize_benders_decomposition_contiguity>(
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
        std::unique_ptr<rectangle::Output> local_output;
        if (deterministic)
            local_output = std::make_unique<rectangle::Output>(instance);
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
        std::unique_ptr<rectangle::Output> local_output;
        if (deterministic)
            local_output = std::make_unique<rectangle::Output>(instance);
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
        std::unique_ptr<rectangle::Output> local_output;
        if (deterministic)
            local_output = std::make_unique<rectangle::Output>(instance);
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
        std::unique_ptr<rectangle::Output> local_output;
        if (deterministic)
            local_output = std::make_unique<rectangle::Output>(instance);
        // Snapshot the bound established so far (only used by
        // 'column_generation''s own sequential feasibility scheme, for the
        // 'BinPacking' objective - see 'ColumnGenerationParameters::
        // use_sequential_feasibility'): seeds its starting candidate instead
        // of it always restarting from scratch.
        BinPos column_generation_lower_bound = output.bin_packing_bound;
        tasks.push_back([&exception_ptr, &instance, &parameters, &algorithm_formatter, local_output = local_output.get(), column_generation_lower_bound]() {
            wrapper<decltype(&optimize_column_generation), optimize_column_generation>(
                    exception_ptr,
                    instance,
                    parameters,
                    algorithm_formatter,
                    local_output,
                    column_generation_lower_bound);
        });
        local_outputs.push_back(std::move(local_output));
    }
    // Conservative scales.
    if ((instance.objective() == Objective::BinPacking
                || instance.objective() == Objective::Feasibility)
            && instance.number_of_bin_types() == 1
            && instance.all_item_types_oriented()
            && (parameters.use_conservative_scales
                || instance.number_of_items() <= 100)) {
        exception_ptr_list.push_front(std::exception_ptr());
        std::exception_ptr& exception_ptr = exception_ptr_list.front();
        std::unique_ptr<rectangle::Output> local_output;
        if (deterministic)
            local_output = std::make_unique<rectangle::Output>(instance);
        tasks.push_back([&exception_ptr, &instance, &parameters, &algorithm_formatter, local_output = local_output.get()]() {
            wrapper<decltype(&optimize_conservative_scales), optimize_conservative_scales>(
                    exception_ptr,
                    instance,
                    parameters,
                    algorithm_formatter,
                    local_output);
        });
        local_outputs.push_back(std::move(local_output));
    }
    // Bar relaxation.
    if (use_bar_relaxation) {
        exception_ptr_list.push_front(std::exception_ptr());
        std::exception_ptr& exception_ptr = exception_ptr_list.front();
        std::unique_ptr<rectangle::Output> local_output;
        if (deterministic)
            local_output = std::make_unique<rectangle::Output>(instance);
        tasks.push_back([&exception_ptr, &instance, &parameters, &algorithm_formatter, local_output = local_output.get()]() {
            wrapper<decltype(&optimize_bar_relaxation), optimize_bar_relaxation>(
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
        last_bin_parameters.not_anytime_tree_search_queue_size = parameters.not_anytime_tree_search_queue_size;
        last_bin_parameters.tree_search_guides = {2, 3};
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
            ss << "post-process";
            algorithm_formatter.update_solution(solution, ss.str());

        }
    }

    algorithm_formatter.end();
    return output;
}
