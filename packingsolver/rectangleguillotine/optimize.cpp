#include "packingsolver/rectangleguillotine/optimize.hpp"

#include "packingsolver/rectangleguillotine/algorithm_formatter.hpp"
#include "packingsolver/rectangleguillotine/instance_builder.hpp"
#include "packingsolver/rectangleguillotine/branching_scheme.hpp"
#include "packingsolver/algorithms/iterative_beam_search.hpp"
#include "packingsolver/algorithms/dichotomic_search.hpp"
#include "packingsolver/algorithms/sequential_value_correction.hpp"
#include "packingsolver/algorithms/column_generation.hpp"

#include <thread>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

const packingsolver::rectangleguillotine::Output packingsolver::rectangleguillotine::optimize(
        const Instance& instance,
        const OptimizeParameters& parameters)
{
    Output output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    ItemPos mean_number_of_items_in_bins
        = largest_bin_space(instance) / mean_item_space(instance);

    if (instance.number_of_bins() == 1) {

        std::vector<GuideId> guides;
        if (!parameters.tree_search_guides.empty()) {
            guides = parameters.tree_search_guides;
        } else if (instance.objective() == Objective::Knapsack) {
            guides = {4, 5};
        } else {
            guides = {0, 2};
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
        if (!parameters.anytime)
            growth_factors = {1.5};

        std::vector<BranchingScheme> branching_schemes;
        std::vector<IterativeBeamSearchParameters> ibs_parameterss;
        for (double growth_factor: growth_factors) {
            for (GuideId guide_id: guides) {
                for (CutOrientation first_stage_orientation: first_stage_orientations) {
                    //std::cout << growth_factor << " " << guide_id << " " << first_stage_orientation << std::endl;
                    Counter i = branching_schemes.size();
                    BranchingScheme::Parameters branching_scheme_parameters;
                    branching_scheme_parameters.guide_id = guide_id;
                    branching_scheme_parameters.first_stage_orientation = first_stage_orientation;
                    branching_schemes.push_back(BranchingScheme(instance, branching_scheme_parameters));
                    IterativeBeamSearchParameters ibs_parameters;
                    ibs_parameters.verbosity_level = 0;
                    ibs_parameters.timer = parameters.timer;
                    ibs_parameters.growth_factor = growth_factor;
                    ibs_parameters.thread_id = i + 1;
                    if (!parameters.anytime) {
                        ibs_parameters.queue_size_min
                            = parameters.not_anytime_tree_search_queue_size;
                        ibs_parameters.queue_size_max
                            = parameters.not_anytime_tree_search_queue_size;
                    }
                    ibs_parameterss.push_back(ibs_parameters);
                }
            }
        }

        std::vector<std::thread> threads;
        for (Counter i = 0; i < (Counter)branching_schemes.size(); ++i) {
            if (!parameters.sequential && branching_schemes.size() > 1) {
                threads.push_back(std::thread(
                            iterative_beam_search<BranchingScheme>,
                            std::ref(branching_schemes[i]),
                            std::ref(algorithm_formatter),
                            ibs_parameterss[i]));
            } else {
                iterative_beam_search<BranchingScheme>(
                        branching_schemes[i],
                        algorithm_formatter,
                        ibs_parameterss[i]);
            }
        }
        for (Counter i = 0; i < (Counter)threads.size(); ++i)
            threads[i].join();


    } else {

        if (mean_number_of_items_in_bins
                > parameters.many_items_in_bins_threshold) {

            if (mean_item_type_copies(instance)
                    > parameters.many_item_type_copies_factor
                    * mean_number_of_items_in_bins) {

                // Sequential single knapsack.

                for (Counter queue_size = 1;;) {

                    if (!parameters.anytime)
                        queue_size = parameters.not_anytime_dichotomic_search_subproblem_beam_size;

                    SequentialValueCorrectionFunction<Instance, Solution> kp_solve
                        = [&parameters, &queue_size](const Instance& kp_instance)
                        {
                            OptimizeParameters kp_parameters;
                            kp_parameters.verbosity_level = 0;
                            kp_parameters.timer = parameters.timer;
                            kp_parameters.sequential = parameters.sequential;
                            kp_parameters.anytime = false;
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
                        ss << "q " << queue_size;
                        algorithm_formatter.update_solution(pssvc_output.solution_pool.best(), ss.str());
                    };
                    sequential_value_correction<Instance, InstanceBuilder, Solution, AlgorithmFormatter>(instance, kp_solve, svc_parameters);

                    // Check end.
                    if (parameters.timer.needs_to_end())
                        break;

                    if (!parameters.anytime)
                        break;

                    queue_size = std::max(
                            queue_size + 1,
                            (NodeId)(queue_size * 2));
                }

            } else {

                if (instance.objective() == Objective::Knapsack
                        || instance.objective() == Objective::BinPacking
                        || instance.objective() == Objective::BinPackingWithLeftovers
                        || instance.number_of_bin_types() == 1) {

                    // Tree search.

                    std::vector<GuideId> guides;
                    if (!parameters.tree_search_guides.empty()) {
                        guides = parameters.tree_search_guides;
                    } else if (instance.objective() == Objective::BinPackingWithLeftovers) {
                        guides = {0, 1};
                    } else {
                        guides = {0, 2};
                    }
                    //guides = {0};

                    std::vector<CutOrientation> first_stage_orientations = {instance.first_stage_orientation()};

                    std::vector<double> growth_factors = {1.5};
                    if (guides.size() * first_stage_orientations.size() * 2 <= 4)
                        growth_factors = {1.33, 1.5};
                    if (!parameters.anytime)
                        growth_factors = {1.5};
                    //growth_factors = {1.5};

                    std::vector<BranchingScheme> branching_schemes;
                    std::vector<IterativeBeamSearchParameters> ibs_parameterss;
                    for (double growth_factor: growth_factors) {
                        for (GuideId guide_id: guides) {
                            for (CutOrientation first_stage_orientation: first_stage_orientations) {
                                //std::cout << growth_factor << " " << guide_id << " " << first_stage_orientation << std::endl;
                                Counter i = branching_schemes.size();
                                BranchingScheme::Parameters branching_scheme_parameters;
                                branching_scheme_parameters.guide_id = guide_id;
                                branching_scheme_parameters.first_stage_orientation = first_stage_orientation;
                                branching_schemes.push_back(BranchingScheme(instance, branching_scheme_parameters));
                                IterativeBeamSearchParameters ibs_parameters;
                                ibs_parameters.verbosity_level = 0;
                                ibs_parameters.timer = parameters.timer;
                                ibs_parameters.growth_factor = growth_factor;
                                ibs_parameters.thread_id = i + 1;
                                if (!parameters.anytime) {
                                    ibs_parameters.queue_size_min
                                        = parameters.not_anytime_tree_search_queue_size;
                                    ibs_parameters.queue_size_max
                                        = parameters.not_anytime_tree_search_queue_size;
                                }
                                ibs_parameterss.push_back(ibs_parameters);
                            }
                        }
                    }

                    std::vector<std::thread> threads;
                    for (Counter i = 0; i < (Counter)branching_schemes.size(); ++i) {
                        if (!parameters.sequential && branching_schemes.size() > 1) {
                            threads.push_back(std::thread(
                                        iterative_beam_search<BranchingScheme>,
                                        std::ref(branching_schemes[i]),
                                        std::ref(algorithm_formatter),
                                        ibs_parameterss[i]));
                        } else {
                            iterative_beam_search<BranchingScheme>(
                                    branching_schemes[i],
                                    algorithm_formatter,
                                    ibs_parameterss[i]);
                        }
                    }
                    for (Counter i = 0; i < (Counter)threads.size(); ++i)
                        threads[i].join();

                } else {

                    // Dichotomic search.

                    for (Counter queue_size = 1;;) {

                        if (!parameters.anytime)
                            queue_size = parameters.not_anytime_dichotomic_search_subproblem_beam_size;

                        DichotomicSearchFunction<Instance, Solution> bpp_solve
                            = [&parameters, &queue_size](const Instance& bpp_instance)
                            {
                                OptimizeParameters bpp_parameters;
                                bpp_parameters.verbosity_level = 0;
                                bpp_parameters.timer = parameters.timer;
                                bpp_parameters.sequential = parameters.sequential;
                                bpp_parameters.anytime = false;
                                bpp_parameters.not_anytime_tree_search_queue_size = queue_size;
                                auto bpp_output = optimize(bpp_instance, bpp_parameters);
                                return bpp_output.solution_pool;
                            };
                        DichotomicSearchParameters<Instance, Solution> ds_parameters;
                        ds_parameters.verbosity_level = 0;
                        ds_parameters.timer = parameters.timer;
                        ds_parameters.new_solution_callback = [
                            &algorithm_formatter, &queue_size](
                                const packingsolver::Output<Instance, Solution>& ps_output)
                        {
                            const DichotomicSearchOutput<Instance, Solution>& psds_output
                                = static_cast<const DichotomicSearchOutput<Instance, Solution>&>(ps_output);
                            std::stringstream ss;
                            ss << "q " << queue_size
                                << " w " << psds_output.waste_percentage;
                            algorithm_formatter.update_solution(psds_output.solution_pool.best(), ss.str());
                        };
                        dichotomic_search<Instance, InstanceBuilder, Solution, AlgorithmFormatter>(instance, bpp_solve, ds_parameters);

                        // Check end.
                        if (parameters.timer.needs_to_end())
                            break;

                        if (!parameters.anytime)
                            break;

                        queue_size = std::max(
                                queue_size + 1,
                                (NodeId)(queue_size * 2));
                    }

                }

            }

        } else {

            if ((instance.objective() == Objective::BinPacking
                        && (instance.number_of_bin_types() > 1
                            || instance.number_of_stacks() < instance.number_of_item_types()))
                    || (instance.objective() == Objective::BinPackingWithLeftovers
                        && (instance.number_of_bin_types() > 1
                            || instance.number_of_stacks() < instance.number_of_item_types()))) {

                SequentialValueCorrectionFunction<Instance, Solution> kp_solve
                    = [&parameters](const Instance& kp_instance)
                    {
                        OptimizeParameters kp_parameters;
                        kp_parameters.verbosity_level = 0;
                        kp_parameters.timer = parameters.timer;
                        kp_parameters.sequential = parameters.sequential;
                        kp_parameters.anytime = false;
                        kp_parameters.not_anytime_tree_search_queue_size
                            = parameters.sequential_value_correction_subproblem_queue_size;
                        auto kp_output = optimize(kp_instance, kp_parameters);
                        return kp_output.solution_pool;
                    };
                SequentialValueCorrectionParameters<Instance, Solution> svc_parameters;
                svc_parameters.verbosity_level = 0;
                svc_parameters.timer = parameters.timer;
                svc_parameters.new_solution_callback = [&algorithm_formatter](
                        const packingsolver::Output<Instance, Solution>& ps_output)
                {
                    const SequentialValueCorrectionOutput<Instance, Solution>& pssvc_output
                        = static_cast<const SequentialValueCorrectionOutput<Instance, Solution>&>(ps_output);
                    std::stringstream ss;
                    ss << "it " << pssvc_output.number_of_iterations;
                    algorithm_formatter.update_solution(pssvc_output.solution_pool.best(), ss.str());
                };
                sequential_value_correction<Instance, InstanceBuilder, Solution, AlgorithmFormatter>(instance, kp_solve, svc_parameters);

            } else if ((instance.objective() == Objective::BinPacking
                        && instance.number_of_bin_types() == 1)
                    || (instance.objective() == Objective::VariableSizedBinPacking)
                    || (instance.objective() == Objective::Knapsack
                        && instance.number_of_bins() > 1)) {

                // Sequential value correction.

                SequentialValueCorrectionFunction<Instance, Solution> kp_solve
                    = [&parameters](const Instance& kp_instance)
                    {
                        OptimizeParameters kp_parameters;
                        kp_parameters.verbosity_level = 0;
                        kp_parameters.timer = parameters.timer;
                        kp_parameters.sequential = parameters.sequential;
                        kp_parameters.anytime = false;
                        kp_parameters.not_anytime_tree_search_queue_size
                            = parameters.sequential_value_correction_subproblem_queue_size;
                        auto kp_output = optimize(kp_instance, kp_parameters);
                        return kp_output.solution_pool;
                    };
                SequentialValueCorrectionParameters<Instance, Solution> svc_parameters;
                svc_parameters.verbosity_level = 0;
                svc_parameters.timer = parameters.timer;
                svc_parameters.maximum_number_of_iterations = parameters.sequential_value_correction_number_of_iterations;
                svc_parameters.new_solution_callback = [&algorithm_formatter](
                        const packingsolver::Output<Instance, Solution>& ps_output)
                {
                    const SequentialValueCorrectionOutput<Instance, Solution>& pssvc_output
                        = static_cast<const SequentialValueCorrectionOutput<Instance, Solution>&>(ps_output);
                    std::stringstream ss;
                    ss << "it " << pssvc_output.number_of_iterations;
                    algorithm_formatter.update_solution(pssvc_output.solution_pool.best(), ss.str());
                };
                sequential_value_correction<Instance, InstanceBuilder, Solution, AlgorithmFormatter>(instance, kp_solve, svc_parameters);

                // Column generation.

                ColumnGenerationPricingFunction<Instance, InstanceBuilder, Solution> pricing_function
                    = [&parameters](const Instance& kp_instance)
                    {
                        OptimizeParameters kp_parameters;
                        kp_parameters.verbosity_level = 0;
                        kp_parameters.timer = parameters.timer;
                        kp_parameters.sequential = parameters.sequential;
                        kp_parameters.anytime = false;
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
                if (!parameters.anytime)
                    cgslds_parameters.automatic_stop = true;
                if (output.solution_pool.best().full())
                    cgslds_parameters.initial_columns = solution2column(output.solution_pool.best());
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
                        ss << "n " << cgslds_output.number_of_nodes;
                        algorithm_formatter.update_solution(solution, ss.str());
                    }
                };
                cgslds_parameters.column_generation_parameters.linear_programming_solver
                    = parameters.linear_programming_solver;
                columngenerationsolver::limited_discrepancy_search(cgs_model, cgslds_parameters);

            }

        }

    }

    algorithm_formatter.end();
    return output;
}
