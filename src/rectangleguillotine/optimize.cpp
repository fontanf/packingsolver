#include "packingsolver/rectangleguillotine/optimize.hpp"

#include "packingsolver/rectangleguillotine/algorithm_formatter.hpp"
#include "packingsolver/rectangleguillotine/instance_builder.hpp"
#include "packingsolver/rectangleguillotine/branching_scheme.hpp"
#include "packingsolver/rectangleguillotine/branching_scheme_n.hpp"
#include "packingsolver/algorithms/dichotomic_search.hpp"
#include "packingsolver/algorithms/sequential_value_correction.hpp"
#include "packingsolver/algorithms/column_generation.hpp"

#include "treesearchsolver/iterative_beam_search_2.hpp"

#include <thread>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

namespace
{

void optimize_tree_search(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter)
{
    std::vector<GuideId> guides;
    if (!parameters.tree_search_guides.empty()) {
        guides = parameters.tree_search_guides;
    } else if (instance.objective() == Objective::Knapsack) {
        guides = {4, 5};
    } else if (instance.objective() == Objective::BinPackingWithLeftovers) {
        guides = {0, 1};
    } else {
        guides = {0, 2, 3};
    }

    std::vector<CutOrientation> first_stage_orientations = {instance.first_stage_orientation()};
    if (instance.number_of_bins() == 1
            && instance.bin_type(0).rect.w != instance.bin_type(0).rect.h
            && instance.first_stage_orientation() == CutOrientation::Any) {
        first_stage_orientations = {CutOrientation::Vertical, CutOrientation::Horinzontal};
    }

    std::vector<double> growth_factors = {1.5};
    if (guides.size() * first_stage_orientations.size() * 2 <= 4)
        growth_factors = {1.33, 1.5};
    if (parameters.optimization_mode != OptimizationMode::Anytime)
        growth_factors = {1.5};

    std::vector<BranchingScheme> branching_schemes;
    std::vector<treesearchsolver::IterativeBeamSearch2Parameters<BranchingScheme>> ibs_parameterss;
    std::vector<rectangleguillotine::Output> outputs;
    for (double growth_factor: growth_factors) {
        for (GuideId guide_id: guides) {
            for (CutOrientation first_stage_orientation: first_stage_orientations) {
                //std::cout << growth_factor << " " << guide_id << " " << first_stage_orientation << std::endl;
                BranchingScheme::Parameters branching_scheme_parameters;
                branching_scheme_parameters.guide_id = guide_id;
                branching_scheme_parameters.first_stage_orientation = first_stage_orientation;
                branching_schemes.push_back(BranchingScheme(instance, branching_scheme_parameters));
                treesearchsolver::IterativeBeamSearch2Parameters<BranchingScheme> ibs_parameters;
                ibs_parameters.verbosity_level = 0;
                ibs_parameters.timer = parameters.timer;
                ibs_parameters.growth_factor = growth_factor;
                if (parameters.optimization_mode != OptimizationMode::Anytime) {
                    ibs_parameters.minimum_size_of_the_queue
                        = parameters.not_anytime_tree_search_queue_size;
                    ibs_parameters.maximum_size_of_the_queue
                        = parameters.not_anytime_tree_search_queue_size + 1;
                }
                ibs_parameterss.push_back(ibs_parameters);
                outputs.push_back(rectangleguillotine::Output(instance));
            }
        }
    }

    std::vector<std::thread> threads;
    for (Counter i = 0; i < (Counter)branching_schemes.size(); ++i) {
        if (parameters.optimization_mode == OptimizationMode::Anytime) {
            ibs_parameterss[i].new_solution_callback
                = [&algorithm_formatter, &branching_schemes, i](
                        const treesearchsolver::Output<BranchingScheme>& tss_output)
                {
                    const treesearchsolver::IterativeBeamSearch2Output<BranchingScheme>& tssibs_output
                        = static_cast<const treesearchsolver::IterativeBeamSearch2Output<BranchingScheme>&>(tss_output);
                    Solution solution = branching_schemes[i].to_solution(
                            tssibs_output.solution_pool.best());
                    std::stringstream ss;
                    ss << "TS g " << branching_schemes[i].parameters().guide_id
                        << " d " << branching_schemes[i].parameters().first_stage_orientation
                        << " q " << tssibs_output.maximum_size_of_the_queue;
                    algorithm_formatter.update_solution(solution, ss.str());
                };
        } else {
            ibs_parameterss[i].new_solution_callback
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
            threads.push_back(std::thread(
                        treesearchsolver::iterative_beam_search_2<BranchingScheme>,
                        std::ref(branching_schemes[i]),
                        ibs_parameterss[i]));
        } else {
            treesearchsolver::iterative_beam_search_2<BranchingScheme>(
                    branching_schemes[i],
                    ibs_parameterss[i]);
        }
    }
    for (Counter i = 0; i < (Counter)threads.size(); ++i)
        threads[i].join();
    if (parameters.optimization_mode != OptimizationMode::Anytime) {
        for (Counter i = 0; i < (Counter)branching_schemes.size(); ++i) {
            std::stringstream ss;
            ss << "TS g " << branching_schemes[i].parameters().guide_id
                << " d " << branching_schemes[i].parameters().first_stage_orientation;
            algorithm_formatter.update_solution(outputs[i].solution_pool.best(), ss.str());
        }
    }
}

void optimize_tree_search_n(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter)
{
    std::vector<GuideId> guides;
    if (!parameters.tree_search_guides.empty()) {
        guides = parameters.tree_search_guides;
    } else if (instance.objective() == Objective::Knapsack) {
        guides = {4, 5};
    } else {
        guides = {0, 2, 3};
    }

    std::vector<CutOrientation> first_stage_orientations = {instance.first_stage_orientation()};
    if (instance.number_of_bins() == 1
            && instance.bin_type(0).rect.w != instance.bin_type(0).rect.h
            && instance.first_stage_orientation() == CutOrientation::Any) {
        first_stage_orientations = {CutOrientation::Vertical, CutOrientation::Horinzontal};
    }

    std::vector<double> growth_factors = {1.5};
    if (guides.size() * first_stage_orientations.size() * 2 <= 4)
        growth_factors = {1.33, 1.5};
    if (parameters.optimization_mode != OptimizationMode::Anytime)
        growth_factors = {1.5};

    std::vector<BranchingSchemeN> branching_schemes;
    std::vector<treesearchsolver::IterativeBeamSearch2Parameters<BranchingSchemeN>> ibs_parameterss;
    std::vector<rectangleguillotine::Output> outputs;
    for (double growth_factor: growth_factors) {
        for (GuideId guide_id: guides) {
            for (CutOrientation first_stage_orientation: first_stage_orientations) {
                //std::cout << growth_factor << " " << guide_id << " " << first_stage_orientation << std::endl;
                BranchingSchemeN::Parameters branching_scheme_parameters;
                branching_scheme_parameters.guide_id = guide_id;
                branching_scheme_parameters.first_stage_orientation = first_stage_orientation;
                branching_schemes.push_back(BranchingSchemeN(instance, branching_scheme_parameters));
                treesearchsolver::IterativeBeamSearch2Parameters<BranchingSchemeN> ibs_parameters;
                ibs_parameters.verbosity_level = 0;
                ibs_parameters.timer = parameters.timer;
                ibs_parameters.growth_factor = growth_factor;
                if (parameters.optimization_mode != OptimizationMode::Anytime) {
                    ibs_parameters.minimum_size_of_the_queue
                        = parameters.not_anytime_tree_search_queue_size;
                    ibs_parameters.maximum_size_of_the_queue
                        = parameters.not_anytime_tree_search_queue_size + 1;
                }
                ibs_parameterss.push_back(ibs_parameters);
                outputs.push_back(rectangleguillotine::Output(instance));
            }
        }
    }

    std::vector<std::thread> threads;
    for (Counter i = 0; i < (Counter)branching_schemes.size(); ++i) {
        if (parameters.optimization_mode == OptimizationMode::Anytime) {
            ibs_parameterss[i].new_solution_callback
                = [&algorithm_formatter, &branching_schemes, i](
                        const treesearchsolver::Output<BranchingSchemeN>& tss_output)
                {
                    const treesearchsolver::IterativeBeamSearch2Output<BranchingSchemeN>& tssibs_output
                        = static_cast<const treesearchsolver::IterativeBeamSearch2Output<BranchingSchemeN>&>(tss_output);
                    Solution solution = branching_schemes[i].to_solution(
                            tssibs_output.solution_pool.best());
                    std::stringstream ss;
                    ss << "TS g " << branching_schemes[i].parameters().guide_id
                        << " q " << tssibs_output.maximum_size_of_the_queue;
                    algorithm_formatter.update_solution(solution, ss.str());
                };
        } else {
            ibs_parameterss[i].new_solution_callback
                = [&outputs, &branching_schemes, i](
                        const treesearchsolver::Output<BranchingSchemeN>& tss_output)
                {
                    const treesearchsolver::IterativeBeamSearch2Output<BranchingSchemeN>& tssibs_output
                        = static_cast<const treesearchsolver::IterativeBeamSearch2Output<BranchingSchemeN>&>(tss_output);
                    Solution solution = branching_schemes[i].to_solution(
                            tssibs_output.solution_pool.best());
                    outputs[i].solution_pool.add(solution);
                };
        }
        if (parameters.optimization_mode != OptimizationMode::NotAnytimeSequential) {
            threads.push_back(std::thread(
                        treesearchsolver::iterative_beam_search_2<BranchingSchemeN>,
                        std::ref(branching_schemes[i]),
                        ibs_parameterss[i]));
        } else {
            treesearchsolver::iterative_beam_search_2<BranchingSchemeN>(
                    branching_schemes[i],
                    ibs_parameterss[i]);
        }
    }
    for (Counter i = 0; i < (Counter)threads.size(); ++i)
        threads[i].join();
    if (parameters.optimization_mode != OptimizationMode::Anytime) {
        for (Counter i = 0; i < (Counter)branching_schemes.size(); ++i) {
            std::stringstream ss;
            ss << "TS g " << branching_schemes[i].parameters().guide_id
                << " d " << branching_schemes[i].parameters().first_stage_orientation;
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
            queue_size = parameters.not_anytime_dichotomic_search_subproblem_queue_size;

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
                bpp_parameters.not_anytime_tree_search_queue_size = queue_size;
                auto bpp_output = optimize(bpp_instance, bpp_parameters);
                return bpp_output.solution_pool;
            };
        DichotomicSearchParameters<Instance, Solution> ds_parameters;
        ds_parameters.verbosity_level = 0;
        ds_parameters.timer = parameters.timer;
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
            auto kp_output = optimize(kp_instance, kp_parameters);
            return kp_output.solution_pool;
        };

    columngenerationsolver::Model cgs_model = get_model<Instance, InstanceBuilder, Solution>(instance, pricing_function);
    columngenerationsolver::LimitedDiscrepancySearchParameters cgslds_parameters;
    cgslds_parameters.verbosity_level = 0;
    cgslds_parameters.timer = parameters.timer;
    cgslds_parameters.internal_diving = 1;
    cgslds_parameters.dummy_column_objective_coefficient = 2 * instance.bin_type(0).cost;
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
    cgslds_parameters.column_generation_parameters.linear_programming_solver
        = parameters.linear_programming_solver;
    columngenerationsolver::limited_discrepancy_search(cgs_model, cgslds_parameters);
}

}

const packingsolver::rectangleguillotine::Output packingsolver::rectangleguillotine::optimize(
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
        use_tree_search = 1;
        use_sequential_single_knapsack = 0;
        use_sequential_value_correction = 0;
        use_dichotomic_search = 0;
        use_column_generation = 0;
    } else if (instance.objective() == Objective::Knapsack) {
        // Disable algorithms which are not available for this objective.
        use_dichotomic_search = 0;
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
                }
            } else {
                use_tree_search = true;
            }
            use_column_generation = true;
        }
    } else if (instance.objective() == Objective::BinPacking
            || instance.objective() == Objective::BinPackingWithLeftovers) {
        // Disable algorithms which are not available for this objective.
        if (instance.number_of_bin_types() > 1)
            use_column_generation = 0;
        if (instance.number_of_stages() >= 4) {
            use_tree_search = 0;
        }
        use_dichotomic_search = 0;
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
                }
            } else {
                if (instance.number_of_stages() <= 3) {
                    use_tree_search = true;
                } else {
                    use_sequential_value_correction = true;
                }
            }
            if (instance.number_of_bin_types() == 1)
                use_column_generation = true;
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
                }
            } else {
                if (instance.number_of_bin_types() == 1) {
                    use_tree_search = true;
                } else {
                    if (mean_number_of_items_in_bins
                            > parameters.many_items_in_bins_threshold) {
                        use_dichotomic_search = true;
                    } else {
                        use_sequential_value_correction = true;
                    }
                }
            }
            use_column_generation = true;
        }
    }

    // Run selected algorithms.
    if (parameters.optimization_mode != OptimizationMode::NotAnytimeSequential) {
        std::vector<std::thread> threads;
        // Tree search.
        if (use_tree_search) {
            if (instance.number_of_stages() <= 3) {
                threads.push_back(std::thread(
                            optimize_tree_search,
                            std::ref(instance),
                            std::ref(parameters),
                            std::ref(algorithm_formatter)));
            } else {
                threads.push_back(std::thread(
                            optimize_tree_search_n,
                            std::ref(instance),
                            std::ref(parameters),
                            std::ref(algorithm_formatter)));
            }
        }
        // Sequential single knapsack.
        if (use_sequential_single_knapsack)
            threads.push_back(std::thread(
                        optimize_sequential_single_knapsack,
                        std::ref(instance),
                        std::ref(parameters),
                        std::ref(algorithm_formatter)));
        // Sequential value correction.
        if (use_sequential_value_correction)
            threads.push_back(std::thread(
                        optimize_sequential_value_correction,
                        std::ref(instance),
                        std::ref(parameters),
                        std::ref(algorithm_formatter)));
        // Dichotomic search.
        if (use_dichotomic_search)
            threads.push_back(std::thread(
                        optimize_dichotomic_search,
                        std::ref(instance),
                        std::ref(parameters),
                        std::ref(algorithm_formatter)));
        // Column generation.
        if (use_column_generation)
            threads.push_back(std::thread(
                        optimize_column_generation,
                        std::ref(instance),
                        std::ref(parameters),
                        std::ref(algorithm_formatter)));
        for (Counter i = 0; i < (Counter)threads.size(); ++i)
            threads[i].join();
    } else {
        // Tree search.
        if (use_tree_search) {
            if (instance.number_of_stages() <= 3) {
                optimize_tree_search(
                        instance,
                        parameters,
                        algorithm_formatter);
            } else {
                optimize_tree_search_n(
                        instance,
                        parameters,
                        algorithm_formatter);
            }
        }
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
