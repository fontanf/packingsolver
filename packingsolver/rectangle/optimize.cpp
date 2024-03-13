#include "packingsolver/rectangle/optimize.hpp"

#include "packingsolver/rectangle/algorithm_formatter.hpp"
#include "packingsolver/rectangle/instance_builder.hpp"
#include "packingsolver/rectangle/branching_scheme.hpp"
#include "packingsolver/algorithms/vbpp_to_bpp.hpp"
#include "packingsolver/algorithms/column_generation.hpp"
#include "packingsolver/algorithms/dichotomic_search.hpp"

#include "treesearchsolver/iterative_beam_search_2.hpp"

#include <thread>

using namespace packingsolver;
using namespace packingsolver::rectangle;

const packingsolver::rectangle::Output packingsolver::rectangle::optimize(
        const Instance& instance,
        const OptimizeParameters& parameters)
{
    Output output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    Algorithm algorithm = Algorithm::TreeSearch;
    if (parameters.algorithm != Algorithm::Auto) {
        algorithm = parameters.algorithm;
    } else if (instance.objective() == Objective::Knapsack) {
        algorithm = Algorithm::TreeSearch;
#if defined(CLP_FOUND) || defined(CPLEX_FOUND) || defined(XPRESS_FOUND)
        if (instance.number_of_bins() > 1
                && largest_bin_space(instance) / mean_item_space(instance) < 16) {
            algorithm = Algorithm::ColumnGeneration;
        }
#endif
    } else if (instance.objective() == Objective::BinPacking) {
#if defined(CLP_FOUND) || defined(CPLEX_FOUND) || defined(XPRESS_FOUND)
        if (instance.number_of_bin_types() == 1
                && largest_bin_space(instance) / mean_item_space(instance) < 16) {
            algorithm = Algorithm::ColumnGeneration;
        }
#endif
    } else if (instance.objective() == Objective::VariableSizedBinPacking) {
        algorithm = Algorithm::DichotomicSearch;
        //algorithm = Algorithm::SequentialValueCorrection;
#if defined(CLP_FOUND) || defined(CPLEX_FOUND) || defined(XPRESS_FOUND)
        if (largest_bin_space(instance) / mean_item_space(instance) < 16) {
            algorithm = Algorithm::ColumnGeneration;
        }
#endif
    }

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
        //guides = {0};

        std::vector<Direction> directions;
        if (instance.objective() == Objective::OpenDimensionX) {
            directions = {Direction::X};
        } else if (instance.objective() == Objective::OpenDimensionY) {
            directions = {Direction::Y};
        } else if (instance.unloading_constraint() == UnloadingConstraint::IncreasingX
                || instance.unloading_constraint() == UnloadingConstraint::OnlyXMovements) {
            directions = {Direction::X};
        } else if (instance.unloading_constraint() == UnloadingConstraint::IncreasingY
                || instance.unloading_constraint() == UnloadingConstraint::OnlyYMovements) {
            directions = {Direction::Y};
        //} else if (instance.number_of_bins() == 1
        //        && instance.bin_type(0).rect.x != instance.bin_type(0).rect.y) {
        //    directions = {Direction::X, Direction::Y};
        } else {
            directions = {Direction::Any};
        }

        std::vector<double> growth_factors = {1.5};
        if (guides.size() * directions.size() * 2 <= 4)
            growth_factors = {1.33, 1.5};
        if (parameters.tree_search_queue_size != -1)
            growth_factors = {1.5};
        //growth_factors = {1.5};

        std::vector<BranchingScheme> branching_schemes;
        std::vector<treesearchsolver::IterativeBeamSearch2Parameters<BranchingScheme>> ibs_parameterss;
        for (double growth_factor: growth_factors) {
            for (GuideId guide_id: guides) {
                for (Direction direction: directions) {
                    //std::cout << growth_factor << " " << guide_id << " " << direction << std::endl;
                    BranchingScheme::Parameters branching_scheme_parameters;
                    branching_scheme_parameters.guide_id = guide_id;
                    branching_scheme_parameters.direction = direction;
                    branching_scheme_parameters.fixed_items = parameters.fixed_items;
                    branching_schemes.push_back(BranchingScheme(instance, branching_scheme_parameters));
                    treesearchsolver::IterativeBeamSearch2Parameters<BranchingScheme> ibs_parameters;
                    ibs_parameters.verbosity_level = 0;
                    ibs_parameters.timer = parameters.timer;
                    ibs_parameters.growth_factor = growth_factor;
                    if (parameters.tree_search_queue_size != -1) {
                        ibs_parameters.minimum_size_of_the_queue = parameters.tree_search_queue_size;
                        ibs_parameters.maximum_size_of_the_queue = parameters.tree_search_queue_size;
                    }
                    //ibs_parameters.info.set_verbosity_level(1);
                    ibs_parameterss.push_back(ibs_parameters);
                }
            }
        }

        std::vector<std::thread> threads;
        for (Counter i = 0; i < (Counter)branching_schemes.size(); ++i) {
            ibs_parameterss[i].new_solution_callback
                = [&algorithm_formatter, &branching_schemes, i](
                        const treesearchsolver::Output<BranchingScheme>& tss_output)
                {
                    const treesearchsolver::IterativeBeamSearch2Output<BranchingScheme>& tssibs_output
                        = static_cast<const treesearchsolver::IterativeBeamSearch2Output<BranchingScheme>&>(tss_output);
                    Solution solution = branching_schemes[i].to_solution(
                            tssibs_output.solution_pool.best());
                    std::stringstream ss;
                    ss << branching_schemes[i].parameters().guide_id
                        << " " << branching_schemes[i].parameters().direction
                        << " " << tssibs_output.maximum_size_of_the_queue;
                    algorithm_formatter.update_solution(solution, ss.str());
                };
            if (parameters.number_of_threads != 1 && branching_schemes.size() > 1) {
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

    } else if (algorithm == Algorithm::ColumnGeneration) {

        if (instance.number_of_items() < 1024) {
            VbppToBppFunction<Instance, Solution> bpp_solve
                = [&parameters](const Instance& bpp_instance)
                {
                    OptimizeParameters bpp_parameters;
                    bpp_parameters.verbosity_level = 0;
                    bpp_parameters.timer = parameters.timer;
                    bpp_parameters.number_of_threads = parameters.number_of_threads;
                    bpp_parameters.algorithm = Algorithm::TreeSearch;
                    bpp_parameters.tree_search_queue_size = parameters.column_generation_vbpp_to_bpp_queue_size;
                    if (parameters.column_generation_vbpp_to_bpp_time_limit >= 0)
                        bpp_parameters.timer.set_time_limit(parameters.column_generation_vbpp_to_bpp_time_limit);
                    auto bpp_output = optimize(bpp_instance, bpp_parameters);
                    return bpp_output.solution_pool;
                };
            VbppToBppParameters<Instance, Solution> vbpp_to_bpp_parameters;
            vbpp_to_bpp_parameters.verbosity_level = 0;
            vbpp_to_bpp_parameters.timer = parameters.timer;
            auto vbpp_to_bpp_output = vbpp_to_bpp<Instance, InstanceBuilder, Solution, AlgorithmFormatter>(instance, bpp_solve, vbpp_to_bpp_parameters);
            std::stringstream ss;
            ss << "VBPP to BPP";
            algorithm_formatter.update_solution(vbpp_to_bpp_output.solution_pool.best(), ss.str());
        }

        ColumnGenerationPricingFunction<Instance, InstanceBuilder, Solution> pricing_function
            = [&parameters](const rectangle::Instance& kp_instance)
            {
                OptimizeParameters kp_parameters;
                kp_parameters.verbosity_level = 0;
                kp_parameters.timer = parameters.timer;
                kp_parameters.number_of_threads = parameters.number_of_threads;
                kp_parameters.tree_search_queue_size = parameters.column_generation_pricing_queue_size;
                auto kp_output = optimize(kp_instance, kp_parameters);
                return kp_output.solution_pool;
            };

        columngenerationsolver::Model cgs_parameters = get_model<Instance, InstanceBuilder, Solution>(instance, pricing_function);
        if (output.solution_pool.best().full())
            cgs_parameters.columns = solution2column(output.solution_pool.best());
        columngenerationsolver::LimitedDiscrepancySearchParameters lds_parameters;
        lds_parameters.verbosity_level = 0;
        lds_parameters.timer = parameters.timer;
        lds_parameters.new_solution_callback = [&instance, &algorithm_formatter](
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
                ss << "discrepancy " << cgslds_output.maximum_discrepancy;
                algorithm_formatter.update_solution(solution, ss.str());
            }
        };
        lds_parameters.column_generation_parameters.linear_programming_solver
            = parameters.linear_programming_solver;
        columngenerationsolver::limited_discrepancy_search(cgs_parameters, lds_parameters);

    } else if (algorithm == Algorithm::DichotomicSearch) {

        DichotomicSearchFunction<Instance, Solution> bpp_solve
            = [&parameters](const Instance& bpp_instance)
            {
                OptimizeParameters bpp_parameters;
                bpp_parameters.verbosity_level = 0;
                bpp_parameters.timer = parameters.timer;
                bpp_parameters.number_of_threads = parameters.number_of_threads;
                bpp_parameters.algorithm = Algorithm::TreeSearch;
                bpp_parameters.tree_search_queue_size = parameters.dichotomic_search_queue_size;
                auto bpp_output = optimize(bpp_instance, bpp_parameters);
                return bpp_output.solution_pool;
            };
        DichotomicSearchParameters<Instance, Solution> ds_parameters;
        ds_parameters.new_solution_callback = [&algorithm_formatter](
                const packingsolver::Output<Instance, Solution>& ps_output)
        {
            const DichotomicSearchOutput<Instance, Solution>& psds_output
                = static_cast<const DichotomicSearchOutput<Instance, Solution>&>(ps_output);
            std::stringstream ss;
            ss << "waste percentage " << psds_output.waste_percentage;
            algorithm_formatter.update_solution(psds_output.solution_pool.best(), ss.str());
        };
        dichotomic_search<Instance, InstanceBuilder, Solution, AlgorithmFormatter>(instance, bpp_solve, ds_parameters);

    }

    algorithm_formatter.end();
    return output;
}
