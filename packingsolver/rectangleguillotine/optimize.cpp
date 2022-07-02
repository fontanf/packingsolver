#include "packingsolver/rectangleguillotine/optimize.hpp"

#include "packingsolver/rectangleguillotine/branching_scheme.hpp"
#include "packingsolver/algorithms/iterative_beam_search.hpp"
#include "packingsolver/algorithms/column_generation.hpp"

#include <thread>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

Output packingsolver::rectangleguillotine::optimize(
        const Instance& instance,
        OptimizeOptionalParameters parameters)
{
    Output output(instance);
    output.solution_pool.best().algorithm_start(parameters.info);

    if (instance.objective() != Objective::VariableSizedBinPacking) {

        std::vector<double> growth_factors = {1.33, 1.5};

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
            growth_factors = {1.5};
        }

        std::vector<BranchingScheme> branching_schemes(
                growth_factors.size() * guides.size() * first_stage_orientations.size(),
                BranchingScheme(instance));
        std::vector<std::thread> threads;
        for (double growth_factor: growth_factors) {
            for (GuideId guide_id: guides) {
                for (CutOrientation first_stage_orientation: first_stage_orientations) {
                    Counter i = threads.size();
                    // Branching scheme.
                    branching_schemes[i].set_guide(guide_id);
                    branching_schemes[i].set_first_stage_orientation(first_stage_orientation);
                    // Beam search.
                    IterativeBeamSearchOptionalParameters parameters_ibs;
                    parameters_ibs.growth_factor = growth_factor;
                    parameters_ibs.thread_id = i + 1;
                    parameters_ibs.info = Info(parameters.info, true, "thread" + std::to_string(i + 1));
                    threads.push_back(std::thread(
                                iterative_beam_search<Instance, Solution, BranchingScheme>,
                                std::ref(branching_schemes[i]),
                                std::ref(output.solution_pool),
                                parameters_ibs));
                }
            }
        }
        for (Counter i = 0; i < (Counter)threads.size(); ++i)
            threads[i].join();

    } else {

        VariableSizeBinPackingPricingFunction<rectangleguillotine::Instance, rectangleguillotine::Solution> pricing_function
            = [&parameters](const rectangleguillotine::Instance& instance_kp)
            {
                std::vector<GuideId> guides = {4, 5};

                std::vector<CutOrientation> first_stage_orientations = {instance_kp.first_stage_orientation()};
                if (instance_kp.bin(0).rect.w != instance_kp.bin(0).rect.h
                        && instance_kp.first_stage_orientation() == CutOrientation::Any) {
                    first_stage_orientations = {CutOrientation::Vertical, CutOrientation::Horinzontal};
                }

                std::vector<SolutionPool<Instance, Solution>> solution_pools(
                        guides.size() * first_stage_orientations.size(),
                        SolutionPool<Instance, Solution>(instance_kp, 10));
                std::vector<BranchingScheme> branching_schemes(
                        guides.size() * first_stage_orientations.size(),
                        BranchingScheme(instance_kp));
                std::vector<std::thread> threads;
                Info info = Info(parameters.info, false, "pricing");
                for (GuideId guide_id: guides) {
                    for (CutOrientation first_stage_orientation: first_stage_orientations) {
                        Counter i = threads.size();
                        // Branching scheme.
                        branching_schemes[i].set_guide(guide_id);
                        branching_schemes[i].set_first_stage_orientation(first_stage_orientation);
                        // Beam search.
                        IterativeBeamSearchOptionalParameters parameters_ibs;
                        parameters_ibs.queue_size_min = 256;
                        parameters_ibs.queue_size_max = 256;
                        parameters_ibs.thread_id = i + 1;
                        parameters_ibs.info = Info(info, true, "thread" + std::to_string(i + 1));
                        threads.push_back(std::thread(
                                    iterative_beam_search<Instance, Solution, BranchingScheme>,
                                    std::ref(branching_schemes[i]),
                                    std::ref(solution_pools[i]),
                                    parameters_ibs));
                    }
                }
                for (Counter i = 0; i < (Counter)threads.size(); ++i)
                    threads[i].join();
                SolutionPool<Instance, Solution> solution_pool(instance_kp, 10);
                std::stringstream ss;
                for (const auto& sp: solution_pools)
                    for (const auto& solution: sp.solutions())
                        solution_pool.add(solution, ss, info);
                return solution_pool;
            };

        columngenerationsolver::Parameters p = get_parameters(instance, pricing_function);
        columngenerationsolver::HeuristicTreeSearchOptionalParameters op;
        op.new_bound_callback = [&instance, &parameters, &output](
                const columngenerationsolver::HeuristicTreeSearchOutput& o)
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
                    solution.append(extra->solution, extra->bin_type_id, extra->kp2vbpp, value);
                }
                std::stringstream ss;
                ss << "iteration " << o.solution_iteration << " node " << o.solution_node;
                output.solution_pool.add(solution, ss, parameters.info);
            }
        };
        op.info.set_time_limit(parameters.info.remaining_time());
        op.column_generation_parameters.linear_programming_solver
            = columngenerationsolver::LinearProgrammingSolver::CPLEX;
        columngenerationsolver::heuristic_tree_search(p, op);
    }

    output.solution_pool.best().algorithm_end(parameters.info);
    return output;
}

