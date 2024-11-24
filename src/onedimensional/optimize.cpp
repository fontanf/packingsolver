#include "packingsolver/onedimensional/optimize.hpp"

#include "packingsolver/onedimensional/algorithm_formatter.hpp"
#include "packingsolver/onedimensional/instance_builder.hpp"
#include "onedimensional/branching_scheme.hpp"
#include "algorithms/dichotomic_search.hpp"
#include "algorithms/sequential_value_correction.hpp"
#include "algorithms/column_generation.hpp"

#include "treesearchsolver/iterative_beam_search_2.hpp"

#include "knapsacksolver/knapsack/instance_builder.hpp"
#include "knapsacksolver/knapsack/algorithms/dynamic_programming_primal_dual.hpp"

#include <thread>

using namespace packingsolver;
using namespace packingsolver::onedimensional;

namespace
{

void optimize_dynamic_programming(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter)
{
    if (instance.number_of_bins() != 1)
        return;
    if (instance.objective() != Objective::Knapsack)
        return;

    knapsacksolver::knapsack::InstanceFromFloatProfitsBuilder kp_instance_builder;
    std::vector<std::pair<ItemTypeId, ItemPos>> kp2ps;
    knapsacksolver::knapsack::Weight kp_capacity = instance.bin_type(0).length;
    kp_instance_builder.set_capacity(kp_capacity);
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        ItemPos total_copies = 0;
        ItemPos copies = 1;
        while (total_copies < item_type.copies) {
            if (total_copies + copies > item_type.copies)
                copies = item_type.copies - total_copies;
            knapsacksolver::knapsack::Weight kp_weight = copies * (
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
    knapsacksolver::knapsack::Instance kp_instance = kp_instance_builder.build();

    knapsacksolver::knapsack::DynamicProgrammingPrimalDualParameters kp_parameters;
    kp_parameters.verbosity_level = 0;
    auto kp_output = knapsacksolver::knapsack::dynamic_programming_primal_dual(
            kp_instance,
            kp_parameters);

    Solution solution(instance);
    solution.add_bin(0, 1);
    for (knapsacksolver::knapsack::ItemId kp_item_type_id = 0;
            kp_item_type_id < kp_instance.number_of_items();
            ++kp_item_type_id) {
        if (kp_output.solution.contains(kp_item_type_id)) {
            ItemTypeId item_type_id = kp2ps[kp_item_type_id].first;
            ItemPos copies = kp2ps[kp_item_type_id].second;
            for (ItemPos copy = 0; copy < copies; ++copy)
                solution.add_item(0, item_type_id);
        }
    }

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
        AlgorithmFormatter& algorithm_formatter)
{
    // Try dynamic programming.
    optimize_dynamic_programming(instance, parameters, algorithm_formatter);
    if (algorithm_formatter.end_boolean())
        return;

    std::vector<GuideId> guides;
    if (!parameters.tree_search_guides.empty()) {
        guides = parameters.tree_search_guides;
    } else if (instance.objective() == Objective::Knapsack) {
        guides = {4, 5};
    } else if (instance.objective() == Objective::BinPackingWithLeftovers) {
        guides = {0, 1};
    } else {
        guides = {0, 2};
    }

    std::vector<double> growth_factors = {1.5};
    if (guides.size() * 2 <= 4)
        growth_factors = {1.33, 1.5};
    if (parameters.optimization_mode != OptimizationMode::Anytime)
        growth_factors = {1.5};

    std::vector<BranchingScheme> branching_schemes;
    std::vector<treesearchsolver::IterativeBeamSearch2Parameters<BranchingScheme>> ibs_parameters_list;
    std::vector<onedimensional::Output> outputs;
    for (double growth_factor: growth_factors) {
        for (GuideId guide_id: guides) {
            //std::cout << growth_factor << " " << guide_id << std::endl;
            BranchingScheme::Parameters branching_scheme_parameters;
            branching_scheme_parameters.guide_id = guide_id;
            branching_schemes.push_back(BranchingScheme(instance, branching_scheme_parameters));
            treesearchsolver::IterativeBeamSearch2Parameters<BranchingScheme> ibs_parameters;
            ibs_parameters.verbosity_level = 0;
            ibs_parameters.timer = parameters.timer;
            if (parameters.optimization_mode == OptimizationMode::Anytime)
                ibs_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
            ibs_parameters.growth_factor = growth_factor;
            if (parameters.optimization_mode != OptimizationMode::Anytime) {
                ibs_parameters.minimum_size_of_the_queue
                    = parameters.not_anytime_tree_search_queue_size;
                ibs_parameters.maximum_size_of_the_queue
                    = parameters.not_anytime_tree_search_queue_size;
            }
            ibs_parameters_list.push_back(ibs_parameters);
            outputs.push_back(onedimensional::Output(instance));
        }
    }

    std::vector<std::thread> threads;
    std::forward_list<std::exception_ptr> exception_ptr_list;
    for (Counter i = 0; i < (Counter)branching_schemes.size(); ++i) {
        if (parameters.optimization_mode == OptimizationMode::Anytime) {
            ibs_parameters_list[i].new_solution_callback
                = [&algorithm_formatter, &branching_schemes, i](
                        const treesearchsolver::Output<BranchingScheme>& tss_output)
                {
                    const treesearchsolver::IterativeBeamSearch2Output<BranchingScheme>& tssibs_output
                        = static_cast<const treesearchsolver::IterativeBeamSearch2Output<BranchingScheme>&>(tss_output);
                    Solution solution = branching_schemes[i].to_solution(
                            tssibs_output.solution_pool.best());
                    std::stringstream ss;
                    ss << "TS g " << branching_schemes[i].parameters().guide_id
                        << " q " << tssibs_output.maximum_size_of_the_queue;
                    algorithm_formatter.update_solution(solution, ss.str());
                };
        } else {
            ibs_parameters_list[i].new_solution_callback
                = [&outputs, &branching_schemes, i](
                        const treesearchsolver::Output<BranchingScheme>& tss_output)
                {
                    const treesearchsolver::IterativeBeamSearch2Output<BranchingScheme>& tssibs_output
                        = static_cast<const treesearchsolver::IterativeBeamSearch2Output<BranchingScheme>&>(tss_output);
                    Solution solution = branching_schemes[i].to_solution(
                            tssibs_output.solution_pool.best());
                    outputs[i].solution_pool.add(solution);
                };
        }
        if (parameters.optimization_mode != OptimizationMode::NotAnytimeSequential) {
            exception_ptr_list.push_front(std::exception_ptr());
            threads.push_back(std::thread(
                        wrapper<decltype(&treesearchsolver::iterative_beam_search_2<BranchingScheme>), treesearchsolver::iterative_beam_search_2<BranchingScheme>>,
                        std::ref(exception_ptr_list.front()),
                        std::ref(branching_schemes[i]),
                        ibs_parameters_list[i]));
        } else {
            treesearchsolver::iterative_beam_search_2<BranchingScheme>(
                    branching_schemes[i],
                    ibs_parameters_list[i]);
        }
    }
    for (Counter i = 0; i < (Counter)threads.size(); ++i)
        threads[i].join();
    for (const std::exception_ptr& exception_ptr: exception_ptr_list)
        if (exception_ptr)
            std::rethrow_exception(exception_ptr);
    if (parameters.optimization_mode != OptimizationMode::Anytime) {
        for (Counter i = 0; i < (Counter)branching_schemes.size(); ++i) {
            std::stringstream ss;
            ss << "TS g " << branching_schemes[i].parameters().guide_id;
            algorithm_formatter.update_solution(outputs[i].solution_pool.best(), ss.str());
        }
    }
}

void optimize_sequential_single_knapsack(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter)
{
    for (Counter queue_size = 1;;) {

        if (parameters.optimization_mode != OptimizationMode::Anytime)
            queue_size = parameters.not_anytime_sequential_single_knapsack_subproblem_queue_size;

        SequentialValueCorrectionFunction<Instance, Solution> kp_solve
            = [&parameters, &queue_size](const Instance& kp_instance)
            {
                OptimizeParameters kp_parameters;
                kp_parameters.verbosity_level = 0;
                kp_parameters.timer = parameters.timer;
                kp_parameters.optimization_mode
                    = (parameters.optimization_mode == OptimizationMode::NotAnytimeSequential)?
                    OptimizationMode::NotAnytimeSequential:
                    OptimizationMode::NotAnytime;
                kp_parameters.not_anytime_tree_search_queue_size = queue_size;
                auto kp_output = optimize(kp_instance, kp_parameters);
                return kp_output.solution_pool;
            };
        SequentialValueCorrectionParameters<Instance, Solution> svc_parameters;
        svc_parameters.verbosity_level = 0;
        svc_parameters.timer = parameters.timer;
        if (parameters.optimization_mode == OptimizationMode::Anytime)
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
        = [&parameters](const Instance& kp_instance)
        {
            OptimizeParameters kp_parameters;
            kp_parameters.verbosity_level = 0;
            kp_parameters.timer = parameters.timer;
            kp_parameters.optimization_mode
                = (parameters.optimization_mode == OptimizationMode::NotAnytimeSequential)?
                OptimizationMode::NotAnytimeSequential:
                OptimizationMode::NotAnytime;
            kp_parameters.not_anytime_tree_search_queue_size
                = parameters.sequential_value_correction_subproblem_queue_size;
            auto kp_output = optimize(kp_instance, kp_parameters);
            return kp_output.solution_pool;
        };
    SequentialValueCorrectionParameters<Instance, Solution> svc_parameters;
    svc_parameters.verbosity_level = 0;
    svc_parameters.timer = parameters.timer;
    if (parameters.optimization_mode == OptimizationMode::Anytime)
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
    for (Counter queue_size = 1;;) {

        if (parameters.optimization_mode != OptimizationMode::Anytime)
            queue_size = parameters.not_anytime_dichotomic_search_subproblem_queue_size;

        DichotomicSearchFunction<Instance, Solution> bpp_solve
            = [&parameters, &queue_size](const Instance& bpp_instance)
            {
                OptimizeParameters bpp_parameters;
                bpp_parameters.verbosity_level = 0;
                bpp_parameters.timer = parameters.timer;
                bpp_parameters.optimization_mode
                    = (parameters.optimization_mode == OptimizationMode::NotAnytimeSequential)?
                    OptimizationMode::NotAnytimeSequential:
                    OptimizationMode::NotAnytime;
                bpp_parameters.use_tree_search = 1;
                bpp_parameters.not_anytime_tree_search_queue_size = queue_size;
                auto bpp_output = optimize(bpp_instance, bpp_parameters);
                return bpp_output.solution_pool;
            };
        DichotomicSearchParameters<Instance, Solution> ds_parameters;
        ds_parameters.verbosity_level = 0;
        ds_parameters.timer = parameters.timer;
        if (parameters.optimization_mode == OptimizationMode::Anytime)
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
    ColumnGenerationPricingFunction<Instance, InstanceBuilder, Solution> pricing_function
        = [&parameters](const Instance& kp_instance)
        {
            OptimizeParameters kp_parameters;
            kp_parameters.verbosity_level = 0;
            kp_parameters.timer = parameters.timer;
            kp_parameters.optimization_mode
                = (parameters.optimization_mode == OptimizationMode::NotAnytimeSequential)?
                OptimizationMode::NotAnytimeSequential:
                OptimizationMode::NotAnytime;
            kp_parameters.not_anytime_tree_search_queue_size
                = parameters.column_generation_subproblem_queue_size;
            return optimize(kp_instance, kp_parameters);
        };

    columngenerationsolver::Model cgs_model = get_model<Instance, InstanceBuilder, Solution>(instance, pricing_function);
    columngenerationsolver::LimitedDiscrepancySearchParameters cgslds_parameters;
    cgslds_parameters.verbosity_level = 0;
    cgslds_parameters.timer = parameters.timer;
    if (parameters.optimization_mode == OptimizationMode::Anytime)
        cgslds_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    cgslds_parameters.internal_diving = 0;
    cgslds_parameters.dummy_column_objective_coefficient = (std::max)(
            2 * instance.maximum_bin_cost() * instance.maximum_item_copies(),
            (Profit)1);
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
    cgslds_parameters.new_bound_callback = [&instance, &algorithm_formatter](
            const columngenerationsolver::Output& cgs_output)
    {
        const columngenerationsolver::LimitedDiscrepancySearchOutput& cgslds_output
            = static_cast<const columngenerationsolver::LimitedDiscrepancySearchOutput&>(cgs_output);
        if (instance.objective() == Objective::VariableSizedBinPacking) {
            algorithm_formatter.update_variable_sized_bin_packing_bound(cgslds_output.bound);
        } else if (instance.objective() == Objective::Knapsack) {
            algorithm_formatter.update_knapsack_bound(cgslds_output.bound);
        } else if (instance.objective() == Objective::BinPacking) {
            BinPos bin_packing_bound = std::ceil(cgslds_output.bound / instance.bin_type(0).space() - 0.001);
            algorithm_formatter.update_bin_packing_bound(bin_packing_bound);
        }
    };
    cgslds_parameters.column_generation_parameters.linear_programming_solver
        = parameters.linear_programming_solver;
    columngenerationsolver::limited_discrepancy_search(cgs_model, cgslds_parameters);
}

}

const packingsolver::onedimensional::Output packingsolver::onedimensional::optimize(
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
    bool use_sequential_single_knapsack = parameters.use_sequential_single_knapsack;
    bool use_sequential_value_correction = parameters.use_sequential_value_correction;
    bool use_dichotomic_search = parameters.use_dichotomic_search;
    bool use_column_generation = parameters.use_column_generation;
    if (instance.number_of_bins() <= 1) {
        use_tree_search = true;
        use_sequential_single_knapsack = false;
        use_sequential_value_correction = false;
        use_dichotomic_search = false;
        use_column_generation = false;
    } else if (instance.objective() == Objective::Knapsack) {
        // Disable algorithms which are not available for this objective.
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
                use_tree_search = true;
                use_column_generation = true;
            }
        }
    } else if (instance.objective() == Objective::BinPacking
            || instance.objective() == Objective::BinPackingWithLeftovers) {
        // Disable algorithms which are not available for this objective.
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
    if (parameters.optimization_mode != OptimizationMode::NotAnytimeSequential) {
        std::vector<std::thread> threads;
        std::forward_list<std::exception_ptr> exception_ptr_list;
        // Tree search.
        if (use_tree_search) {
            exception_ptr_list.push_front(std::exception_ptr());
            threads.push_back(std::thread(
                        wrapper<decltype(&optimize_tree_search), optimize_tree_search>,
                        std::ref(exception_ptr_list.front()),
                        std::ref(instance),
                        std::ref(parameters),
                        std::ref(algorithm_formatter)));
        }
        // Sequential single knapsack.
        if (use_sequential_single_knapsack) {
            exception_ptr_list.push_front(std::exception_ptr());
            threads.push_back(std::thread(
                        wrapper<decltype(&optimize_sequential_single_knapsack), optimize_sequential_single_knapsack>,
                        std::ref(exception_ptr_list.front()),
                        std::ref(instance),
                        std::ref(parameters),
                        std::ref(algorithm_formatter)));
        }
        // Sequential value correction.
        if (use_sequential_value_correction) {
            exception_ptr_list.push_front(std::exception_ptr());
            threads.push_back(std::thread(
                        wrapper<decltype(&optimize_sequential_value_correction), optimize_sequential_value_correction>,
                        std::ref(exception_ptr_list.front()),
                        std::ref(instance),
                        std::ref(parameters),
                        std::ref(algorithm_formatter)));
        }
        // Dichotomic search.
        if (use_dichotomic_search) {
            exception_ptr_list.push_front(std::exception_ptr());
            threads.push_back(std::thread(
                        wrapper<decltype(&optimize_dichotomic_search), optimize_dichotomic_search>,
                        std::ref(exception_ptr_list.front()),
                        std::ref(instance),
                        std::ref(parameters),
                        std::ref(algorithm_formatter)));
        }
        // Column generation.
        if (use_column_generation) {
            exception_ptr_list.push_front(std::exception_ptr());
            threads.push_back(std::thread(
                        wrapper<decltype(&optimize_column_generation), optimize_column_generation>,
                        std::ref(exception_ptr_list.front()),
                        std::ref(instance),
                        std::ref(parameters),
                        std::ref(algorithm_formatter)));
        }
        for (Counter i = 0; i < (Counter)threads.size(); ++i)
            threads[i].join();
        for (std::exception_ptr exception_ptr: exception_ptr_list)
            if (exception_ptr)
                std::rethrow_exception(exception_ptr);
    } else {
        // Tree search.
        if (use_tree_search)
            optimize_tree_search(
                    instance,
                    parameters,
                    algorithm_formatter);
        // Sequential single knapsack.
        if (use_sequential_single_knapsack)
            optimize_sequential_single_knapsack(
                    instance,
                    parameters,
                    algorithm_formatter);
        // Sequential value correction.
        if (use_sequential_value_correction)
            optimize_sequential_value_correction(
                    instance,
                    parameters,
                    algorithm_formatter);
        // Dichotomic search.
        if (use_dichotomic_search)
            optimize_dichotomic_search(
                    instance,
                    parameters,
                    algorithm_formatter);
        // Column generation.
        if (use_column_generation)
            optimize_column_generation(
                    instance,
                    parameters,
                    algorithm_formatter);
    }

    algorithm_formatter.end();
    return output;
}
