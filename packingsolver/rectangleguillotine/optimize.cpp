#include "packingsolver/rectangleguillotine/optimize.hpp"

#include "packingsolver/rectangleguillotine/branching_scheme.hpp"
#include "packingsolver/algorithms/iterative_beam_search.hpp"
#include "packingsolver/algorithms/vbpp2bpp.hpp"
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

    Algorithm algorithm = Algorithm::TreeSearch;
    if (instance.objective() == Objective::BinPacking) {
        if (parameters.bpp_algorithm != Algorithm::Auto) {
            algorithm = parameters.bpp_algorithm;
        } else {
#if defined(COINOR_FOUND) || defined(CPLEX_FOUND)
            if (instance.number_of_bin_types() == 1
                    && largest_bin_space(instance) / mean_item_space(instance) < 16) {
                algorithm = Algorithm::ColumnGeneration;
            }
#endif
        }
    } else if (instance.objective() == Objective::VariableSizedBinPacking) {
        if (parameters.vbpp_algorithm != Algorithm::Auto) {
            algorithm = parameters.vbpp_algorithm;
#if defined(COINOR_FOUND) || defined(CPLEX_FOUND)
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
        if (instance.objective() != Objective::Knapsack) {
            guides = {0, 1};
        } else {
            guides = {4, 5};
        }

        std::vector<CutOrientation> first_stage_orientations = {instance.first_stage_orientation()};
        if (instance.number_of_bins() == 1
                && instance.bin(0).rect.w != instance.bin(0).rect.h
                && instance.first_stage_orientation() == CutOrientation::Any) {
            first_stage_orientations = {CutOrientation::Vertical, CutOrientation::Horinzontal};
        }

        std::vector<double> growth_factors = {1.5};
        if (guides.size() * first_stage_orientations.size() < 4)
            growth_factors = {1.33, 1.5};
        if (parameters.tree_search_queue_size != -1)
            growth_factors = {1.5};

        std::vector<BranchingScheme> branching_schemes;
        std::vector<IterativeBeamSearchOptionalParameters> ibs_parameterss;
        for (double growth_factor: growth_factors) {
            for (GuideId guide_id: guides) {
                for (CutOrientation first_stage_orientation: first_stage_orientations) {
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
            threads.push_back(std::thread(
                        iterative_beam_search<Instance, Solution, BranchingScheme>,
                        std::ref(branching_schemes[i]),
                        std::ref(output.solution_pool),
                        ibs_parameterss[i]));
        }
        for (Counter i = 0; i < (Counter)threads.size(); ++i)
            threads[i].join();

    } else if (algorithm == Algorithm::ColumnGeneration) {

        if (instance.number_of_items() < 1024) {
            Vbpp2BppFunction<Instance, Solution> bpp_solve
                = [&parameters](const Instance& bpp_instance)
                {
                    OptimizeOptionalParameters bpp_parameters;
                    bpp_parameters.bpp_algorithm = Algorithm::TreeSearch;
                    bpp_parameters.tree_search_queue_size = parameters.column_generation_queue_size;
                    bpp_parameters.info = Info(parameters.info, false, "");
                    //bpp_parameters.info.set_verbosity_level(1);
                    auto bpp_output = optimize(bpp_instance, bpp_parameters);
                    return bpp_output.solution_pool;
                };
            Vbpp2BppOptionalParameters<Instance, Solution> vbpp2bpp_op;
            auto vbpp2bpp_output = vbpp2bpp(instance, bpp_solve, vbpp2bpp_op);
            std::stringstream ss;
            ss << "vbpp2bpp";
            output.solution_pool.add(vbpp2bpp_output.solution_pool.best(), ss, parameters.info);
        }

        VariableSizeBinPackingPricingFunction<rectangleguillotine::Instance, rectangleguillotine::Solution> pricing_function
            = [&parameters](const rectangleguillotine::Instance& kp_instance)
            {
                OptimizeOptionalParameters kp_parameters;
                kp_parameters.tree_search_queue_size = parameters.column_generation_queue_size;
                kp_parameters.info = Info(parameters.info, false, "");
                //kp_parameters.info.set_verbosity_level(1);
                auto kp_output = optimize(kp_instance, kp_parameters);
                return kp_output.solution_pool;
            };

        columngenerationsolver::Parameters p = get_parameters(instance, pricing_function);
        columngenerationsolver::LimitedDiscrepancySearchOptionalParameters op;
        op.new_bound_callback = [&instance, &parameters, &output](
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
                    std::shared_ptr<VariableSizeBinPackingColumnExtra<Solution>> extra
                        = std::static_pointer_cast<VariableSizeBinPackingColumnExtra<Solution>>(column.extra);
                    solution.append(extra->solution, {extra->bin_type_id}, extra->kp2vbpp, value);
                }
                std::stringstream ss;
                ss << "discrepancy " << o.solution_discrepancy;
                output.solution_pool.add(solution, ss, parameters.info);
            }
        };
#if defined(CPLEX_FOUND)
        op.column_generation_parameters.linear_programming_solver
            = columngenerationsolver::LinearProgrammingSolver::CPLEX;
#endif
#if defined(COINOR_FOUND)
        op.column_generation_parameters.linear_programming_solver
            = columngenerationsolver::LinearProgrammingSolver::CLP;
#endif
        op.info = Info(parameters.info, false, "");
        columngenerationsolver::limited_discrepancy_search(p, op);

    } else if (algorithm == Algorithm::DichotomicSearch) {

        DichotomicSearchFunction<Instance, Solution> bpp_solve
            = [&parameters](const Instance& bpp_instance)
            {
                OptimizeOptionalParameters bpp_parameters;
                bpp_parameters.tree_search_queue_size = parameters.dichotomic_search_queue_size;
                bpp_parameters.info = Info(parameters.info, false, "");
                //bpp_parameters.info.set_verbosity_level(1);
                auto bpp_output = optimize(bpp_instance, bpp_parameters);
                return bpp_output.solution_pool;
            };
        DichotomicSearchOptionalParameters<Instance, Solution> op;
        op.new_solution_callback = [&parameters, &output](
                const DichotomicSearchOutput<Instance, Solution>& o)
        {
            std::stringstream ss;
            ss << "waste percentage " << o.waste_percentage;
            output.solution_pool.add(o.solution_pool.best(), ss, parameters.info);
        };
        op.info = Info(parameters.info, false, "");
        dichotomic_search(instance, bpp_solve, op);

    }

    output.solution_pool.best().algorithm_end(parameters.info);
    return output;
}

