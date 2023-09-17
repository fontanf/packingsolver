#include "packingsolver/rectangleguillotine/optimize.hpp"

#include "packingsolver/rectangleguillotine/branching_scheme.hpp"
#include "packingsolver/algorithms/iterative_beam_search.hpp"
#include "packingsolver/algorithms/vbpp_to_bpp.hpp"
#include "packingsolver/algorithms/column_generation.hpp"
#include "packingsolver/algorithms/dichotomic_search.hpp"

#include <thread>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

Output packingsolver::rectangleguillotine::optimize(
        const Instance& instance,
        OptimizeOptionalParameters parameters)
{
    Output output(instance);

    // Select algorithm.
    Algorithm algorithm = Algorithm::TreeSearch;
    if (instance.objective() == Objective::BinPacking) {
        if (parameters.bpp_algorithm != Algorithm::Auto) {
            algorithm = parameters.bpp_algorithm;
        } else {
#if defined(CLP_FOUND) || defined(CPLEX_FOUND) || defined(XPRESS_FOUND)
            if (instance.number_of_bin_types() == 1
                    && largest_bin_space(instance) / mean_item_space(instance) < 16) {
                algorithm = Algorithm::ColumnGeneration;
            }
#endif
        }
    } else if (instance.objective() == Objective::VariableSizedBinPacking) {
        if (parameters.vbpp_algorithm != Algorithm::Auto) {
            algorithm = parameters.vbpp_algorithm;
#if defined(CLP_FOUND) || defined(CPLEX_FOUND) || defined(XPRESS_FOUND)
        } else if (largest_bin_space(instance) / mean_item_space(instance) < 16) {
            algorithm = Algorithm::ColumnGeneration;
#endif
        } else {
            algorithm = Algorithm::DichotomicSearch;
        }
    }
    std::stringstream ss;
    ss << algorithm;
    parameters.info.add_to_json("Algorithm", "Algorithm", ss.str());

    output.solution_pool.best().algorithm_start(parameters.info, algorithm);

    if (algorithm == Algorithm::TreeSearch) {

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

        std::vector<CutOrientation> first_stage_orientations = {instance.first_stage_orientation()};
        if (instance.number_of_bins() == 1
                && instance.bin_type(0).rect.w != instance.bin_type(0).rect.h
                && instance.first_stage_orientation() == CutOrientation::Any) {
            first_stage_orientations = {CutOrientation::Vertical, CutOrientation::Horinzontal};
        }

        std::vector<double> growth_factors = {1.5};
        if (guides.size() * first_stage_orientations.size() * 2 <= 4)
            growth_factors = {1.33, 1.5};
        if (parameters.tree_search_queue_size != -1)
            growth_factors = {1.5};

        std::vector<BranchingScheme> branching_schemes;
        std::vector<IterativeBeamSearchOptionalParameters> ibs_parameterss;
        for (double growth_factor: growth_factors) {
            for (GuideId guide_id: guides) {
                for (CutOrientation first_stage_orientation: first_stage_orientations) {
                    //std::cout << growth_factor << " " << guide_id << " " << first_stage_orientation << std::endl;
                    Counter i = branching_schemes.size();
                    BranchingScheme::Parameters branching_scheme_parameters;
                    branching_scheme_parameters.guide_id = guide_id;
                    branching_scheme_parameters.first_stage_orientation = first_stage_orientation;
                    branching_schemes.push_back(BranchingScheme(instance, branching_scheme_parameters));
                    IterativeBeamSearchOptionalParameters ibs_parameters;
                    ibs_parameters.growth_factor = growth_factor;
                    ibs_parameters.thread_id = i + 1;
                    if (parameters.tree_search_queue_size != -1) {
                        ibs_parameters.queue_size_min = parameters.tree_search_queue_size;
                        ibs_parameters.queue_size_max = parameters.tree_search_queue_size;
                    }
                    ibs_parameters.info = Info(parameters.info, true, "thread" + std::to_string(i + 1));
                    ibs_parameterss.push_back(ibs_parameters);
                }
            }
        }

        std::vector<std::thread> threads;
        for (Counter i = 0; i < (Counter)branching_schemes.size(); ++i) {
            if (parameters.number_of_threads != 1 && branching_schemes.size() > 1) {
                threads.push_back(std::thread(
                            iterative_beam_search<Instance, Solution, BranchingScheme>,
                            std::ref(branching_schemes[i]),
                            std::ref(output.solution_pool),
                            ibs_parameterss[i]));
            } else {
                iterative_beam_search<Instance, Solution, BranchingScheme>(
                        branching_schemes[i],
                        output.solution_pool,
                        ibs_parameterss[i]);
            }
        }
        for (Counter i = 0; i < (Counter)threads.size(); ++i)
            threads[i].join();

    } else if (algorithm == Algorithm::ColumnGeneration) {

        if (instance.number_of_items() < 1024) {
            VbppToBppFunction<Instance, InstanceBuilder, Solution> bpp_solve
                = [&parameters](const Instance& bpp_instance)
                {
                    OptimizeOptionalParameters bpp_parameters;
                    bpp_parameters.number_of_threads = parameters.number_of_threads;
                    bpp_parameters.bpp_algorithm = Algorithm::TreeSearch;
                    bpp_parameters.tree_search_queue_size = parameters.column_generation_vbpp_to_bpp_queue_size;
                    bpp_parameters.info = Info(parameters.info, false, "");
                    if (parameters.column_generation_vbpp_to_bpp_time_limit >= 0)
                        bpp_parameters.info.set_time_limit(parameters.column_generation_vbpp_to_bpp_time_limit);
                    //bpp_parameters.info.set_verbosity_level(1);
                    auto bpp_output = optimize(bpp_instance, bpp_parameters);
                    return bpp_output.solution_pool;
                };
            VbppToBppOptionalParameters<Instance, InstanceBuilder, Solution> vbpp_to_bpp_parameters;
            auto vbpp_to_bpp_output = vbpp_to_bpp(instance, bpp_solve, vbpp_to_bpp_parameters);
            std::stringstream ss;
            ss << "VBPP to BPP";
            output.solution_pool.add(vbpp_to_bpp_output.solution_pool.best(), ss, parameters.info);
        }

        ColumnGenerationPricingFunction<Instance, InstanceBuilder, rectangleguillotine::Solution> pricing_function
            = [&parameters](const rectangleguillotine::Instance& kp_instance)
            {
                OptimizeOptionalParameters kp_parameters;
                kp_parameters.number_of_threads = parameters.number_of_threads;
                kp_parameters.tree_search_queue_size = parameters.column_generation_pricing_queue_size;
                kp_parameters.info = Info(parameters.info, false, "");
                //kp_parameters.info.set_verbosity_level(1);
                auto kp_output = optimize(kp_instance, kp_parameters);
                return kp_output.solution_pool;
            };

        columngenerationsolver::Parameters cgs_parameters = get_parameters<Instance, InstanceBuilder, Solution>(instance, pricing_function);
        if (output.solution_pool.best().full())
            cgs_parameters.columns = solution2column(output.solution_pool.best());
        columngenerationsolver::LimitedDiscrepancySearchOptionalParameters lds_parameters;
        lds_parameters.new_bound_callback = [&instance, &parameters, &output](
                const columngenerationsolver::LimitedDiscrepancySearchOutput& o)
        {
            if (o.solution.size() > 0) {
                Solution solution(instance);
                for (const auto& pair: o.solution) {
                    const Column& column = pair.first;
                    BinPos value = std::round(pair.second);
                    if (value < 0.5)
                        continue;
                    //std::cout << "append val " << value << " col " << column << std::endl;
                    solution.append(
                            *std::static_pointer_cast<Solution>(column.extra),
                            0,
                            value);
                }
                std::stringstream ss;
                ss << "discrepancy " << o.solution_discrepancy;
                output.solution_pool.add(solution, ss, parameters.info);
            }
        };
        lds_parameters.column_generation_parameters.linear_programming_solver
            = parameters.linear_programming_solver;
        lds_parameters.info = Info(parameters.info, false, "");
        columngenerationsolver::limited_discrepancy_search(cgs_parameters, lds_parameters);

    } else if (algorithm == Algorithm::DichotomicSearch) {

        DichotomicSearchFunction<Instance, InstanceBuilder, Solution> bpp_solve
            = [&parameters](const Instance& bpp_instance)
            {
                OptimizeOptionalParameters bpp_parameters;
                bpp_parameters.number_of_threads = parameters.number_of_threads;
                bpp_parameters.tree_search_queue_size = parameters.dichotomic_search_queue_size;
                bpp_parameters.info = Info(parameters.info, false, "");
                //bpp_parameters.info.set_verbosity_level(1);
                auto bpp_output = optimize(bpp_instance, bpp_parameters);
                return bpp_output.solution_pool;
            };
        DichotomicSearchOptionalParameters<Instance, InstanceBuilder, Solution> ds_parameters;
        ds_parameters.new_solution_callback = [&parameters, &output](
                const DichotomicSearchOutput<Instance, InstanceBuilder, Solution>& o)
        {
            // Lock mutex.
            parameters.info.lock();

            std::stringstream ss;
            ss << "waste percentage " << o.waste_percentage;
            output.solution_pool.add(o.solution_pool.best(), ss, parameters.info);

            // Unlock mutex.
            parameters.info.unlock();
        };
        ds_parameters.info = Info(parameters.info, false, "");
        dichotomic_search(instance, bpp_solve, ds_parameters);

    } else if (algorithm == Algorithm::SequentialValueCorrection) {

        SequentialValueCorrectionFunction<Instance, InstanceBuilder, Solution> kp_solve
            = [&parameters](const Instance& kp_instance)
            {
                OptimizeOptionalParameters kp_parameters;
                kp_parameters.number_of_threads = parameters.number_of_threads;
                kp_parameters.tree_search_queue_size = parameters.sequential_value_correction_queue_size;
                kp_parameters.info = Info(parameters.info, false, "");
                //kp_parameters.info.set_verbosity_level(1);
                auto kp_output = optimize(kp_instance, kp_parameters);
                return kp_output.solution_pool;
            };
        SequentialValueCorrectionOptionalParameters<Instance, InstanceBuilder, Solution> sqv_parameters = parameters.sequential_value_correction_parameters;
        sqv_parameters.new_solution_callback = [&parameters, &output](
                const SequentialValueCorrectionOutput<Instance, InstanceBuilder, Solution>& o)
        {
            // Lock mutex.
            parameters.info.lock();

            std::stringstream ss;
            ss << "iteration " << o.number_of_iterations;
            output.solution_pool.add(o.solution_pool.best(), ss, parameters.info);

            // Unlock mutex.
            parameters.info.unlock();
        };
        sqv_parameters.info = Info(parameters.info, false, "");
        sequential_value_correction(instance, kp_solve, sqv_parameters);

    }

    output.solution_pool.best().algorithm_end(parameters.info);
    return output;
}

