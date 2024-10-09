/**
 * Sequential value correction algorithm
 *
 * Algorithm for multiple knapsack, bin packing and variable-sized bin packing
 * problems.
 *
 * For variable-sized bin packing, it doesn't handle the minimum number of
 * copies of each bin type to use.
 *
 * The algorithm solves a single knapsack subproblem for each bin until either
 * all items are packed, or there is no bin left.
 * Then, it restarts but increases the profits of the items that where packed
 * in bins with high waste, such that they get packed earlier and hopefully
 * generate less waste.
 *
 * For Variable-sized bin packing, this algorithm should require less
 * computational effort to find a good solution than the other algorithms for
 * this objective.
 * For multiple knapsck and bin packing, it is useful for boxstacks problem
 * type since, in this case, tree search might not be applied directly to get a
 * good solution.
 *
 * Some references from which this implementation is inspired:
 * - "Linear one-dimensional cutting-packing problems: numerical experiments
 *   with the sequential value correction method (SVC) and a modified
 *   branch-and-bound method (MBB)" (Mukhacheva et al., 2000)
 *   https://doi.org/10.1590/S0101-74382000000200002
 * - "Parallelized sequential value correction procedure for the
 *   one-dimensional cutting stock problem with multiple stock lengths" (Cui et
 *   Tang, 2014)
 *   https://doi.org/10.1080/0305215X.2013.841903
 * - "Sequential value correction heuristic for the two-dimensional cutting
 *   stock problem with three-staged homogenous patterns" (Chen et al., 2016)
 *   https://doi.org/10.1080/10556788.2015.1048860
 */

#pragma once

#include "packingsolver/algorithms/common.hpp"

#include <functional>
#include <sstream>

namespace packingsolver
{

template <typename Instance, typename Solution>
using SequentialValueCorrectionFunction = std::function<SolutionPool<Instance, Solution>(const Instance&)>;

template <typename Instance, typename Solution>
struct SequentialValueCorrectionOutput: packingsolver::Output<Instance, Solution>
{
    /** Constructor. */
    SequentialValueCorrectionOutput(const Instance& instance):
        packingsolver::Output<Instance, Solution>(instance) { }


    /** Number of iterations. */
    Counter number_of_iterations = 0;

    /** List of all patterns generated during the algorithm. */
    std::vector<Solution> all_patterns;
};

template <typename Instance, typename Solution>
struct SequentialValueCorrectionParameters: packingsolver::Parameters<Instance, Solution>
{
    /** Maximum number of iterations. */
    Counter maximum_number_of_iterations = -1;

    /**
     * If the objective is "BinPacking", stop as soon as a solution using this
     * number of bins is found.
     *
     * If all items fit in the first bin, but the first solution found uses
     * more bins, then it is very unlikely that the solutions of the next
     * iterations manage to pack all items in the first bin only.
     */
    BinPos bin_packing_goal = 2;
};

template <typename Instance, typename InstanceBuilder, typename Solution, typename AlgorithmFormatter>
SequentialValueCorrectionOutput<Instance, Solution> sequential_value_correction(
        const Instance& instance,
        const SequentialValueCorrectionFunction<Instance, Solution>& function,
        const SequentialValueCorrectionParameters<Instance, Solution>& parameters)
{
    SequentialValueCorrectionOutput<Instance, Solution> output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    if (instance.number_of_item_types() == 0)
        return output;

    // Initialize profits.
    std::vector<Profit> profits(instance.number_of_item_types());
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        if (instance.objective() == Objective::Knapsack) {
            profits[item_type_id] = instance.item_type(item_type_id).profit;
        } else {
            profits[item_type_id] = instance.item_type(item_type_id).space();
        }
        //std::cout << "item_type_id " << item_type_id
        //    << " profit " << profits[item_type_id]
        //    << std::endl;
    }
    auto lbs = largest_bin_space(instance);

    for (output.number_of_iterations = 0;; output.number_of_iterations++) {
        //std::cout << "it " << output.number_of_iterations
        //    << " / " << parameters.maximum_number_of_iterations
        //    << std::endl;

        // Check maximum number of iterations.
        if (parameters.maximum_number_of_iterations != -1
                && output.number_of_iterations == parameters.maximum_number_of_iterations)
            return output;

        Solution solution(instance);
        std::vector<double> item_type_adjusted_space(instance.number_of_item_types(), 0.0);

        // For VBPP objective, we store the solutions found for each bin type
        // at each iterations. Thus, at the next iteration, we can check if the
        // previously computed ones are still feasible.
        std::vector<std::pair<Solution, Profit>> solutions_cur(
                instance.number_of_bin_types(),
                {Solution(instance), 0.0});

        for (;;) {

            // Check end.
            if (parameters.timer.needs_to_end())
                return output;

            // Stop if all items have been packed.
            if (solution.number_of_items() == instance.number_of_items())
                break;

            // Stop if all bins have been used.
            if (solution.number_of_bins() == instance.number_of_bins())
                break;

            std::vector<ItemTypeId> kp2orig;
            for (ItemTypeId item_type_id = 0;
                    item_type_id < instance.number_of_item_types();
                    ++item_type_id) {
                ItemPos copies = instance.item_type(item_type_id).copies
                    - solution.item_copies(item_type_id);
                if (copies > 0)
                    kp2orig.push_back(item_type_id);
            }

            // Find bin types to try.
            std::vector<BinTypeId> bin_type_ids;
            if (instance.objective() == Objective::VariableSizedBinPacking) {
                // Start with mandatory bins (bin types with copies_min > 0).
                BinTypeId smallest_mandaotry_bin_type_id = -1;
                for (BinTypeId bin_type_id = 0;
                        bin_type_id < instance.number_of_bin_types();
                        ++bin_type_id) {
                    const auto& bin_type = instance.bin_type(bin_type_id);
                    if (solution.bin_copies(bin_type_id) >= bin_type.copies_min)
                        continue;
                    if (smallest_mandaotry_bin_type_id == -1
                            || bin_type.space() < instance.bin_type(smallest_mandaotry_bin_type_id).space()) {
                        smallest_mandaotry_bin_type_id = bin_type_id;
                    }
                }

                if (smallest_mandaotry_bin_type_id != -1) {
                    bin_type_ids.push_back(smallest_mandaotry_bin_type_id);
                } else {
                    for (BinTypeId bin_type_id = 0;
                            bin_type_id < instance.number_of_bin_types();
                            ++bin_type_id) {
                        const auto& bin_type = instance.bin_type(bin_type_id);
                        if (solution.bin_copies(bin_type_id) == bin_type.copies)
                            continue;
                        bin_type_ids.push_back(bin_type_id);
                    }
                }
            } else {
                bin_type_ids.push_back(instance.bin_type_id(solution.number_of_bins()));
            }

            for (BinTypeId bin_type_id: bin_type_ids) {
                const auto& bin_type = instance.bin_type(bin_type_id);

                if (instance.objective() == Objective::VariableSizedBinPacking
                        && solutions_cur[bin_type_id].first.number_of_items() > 0) {
                    // Check if previous solution is still valid.
                    bool valid = true;
                    for (ItemTypeId item_type_id = 0;
                            item_type_id < instance.number_of_item_types();
                            ++item_type_id) {
                        ItemPos item_remaining_copies
                            = instance.item_type(item_type_id).copies
                            - solution.item_copies(item_type_id);
                        if (item_remaining_copies
                                < solutions_cur[bin_type_id].first.item_copies(item_type_id)) {
                            valid = false;
                            break;
                        }
                    }
                    if (valid)
                        continue;
                }

                // Build Knapsack subproblem instance.
                InstanceBuilder kp_instance_builder = InstanceBuilder();
                kp_instance_builder.set_objective(Objective::Knapsack);
                kp_instance_builder.set_parameters(instance.parameters());
                kp_instance_builder.add_bin_type(bin_type, 1);
                for (ItemTypeId item_type_id: kp2orig) {
                    ItemPos copies
                        = instance.item_type(item_type_id).copies
                        - solution.item_copies(item_type_id);
                    kp_instance_builder.add_item_type(
                            instance.item_type(item_type_id),
                            profits[item_type_id],
                            copies);
                }
                Instance kp_instance = kp_instance_builder.build();

                // Solve Knapsack subproblem instance.
                SolutionPool<Instance, Solution> kp_solution_pool = function(kp_instance);
                if (parameters.timer.needs_to_end())
                    return output;

                auto kp_solution = kp_solution_pool.best();
                Solution solution(instance);
                if (kp_solution.number_of_different_bins() > 0)
                    solution.append(kp_solution, 0, 1, {bin_type_id}, kp2orig);
                solutions_cur[bin_type_id].first = solution;
                solutions_cur[bin_type_id].second = kp_solution.profit();
                output.all_patterns.push_back(solution);
            }

            // Find best solution.

            double ratio_best = 0;
            BinTypeId bin_type_id_best = -1;
            for (BinTypeId bin_type_id: bin_type_ids) {
                const auto& bin_type = instance.bin_type(bin_type_id);

                // Update next solution.
                double ratio = solutions_cur[bin_type_id].second / bin_type.cost;
                //std::cout << "bin_type_id " << bin_type_id
                //    << " cost " << bin_type.cost
                //    << " profit " << solutions_cur[bin_type_id].second
                //    << " ratio " << ratio
                //    << std::endl;
                if (ratio_best < ratio) {
                    ratio_best = ratio;
                    bin_type_id_best = bin_type_id;
                }
            }

            // If no item has been packed, stop.
            if (bin_type_id_best == -1)
                break;
            const Solution& kp_solution_best = solutions_cur[bin_type_id_best].first;

            // Compute the number of copies of the selected Knapsack solution
            // to add.
            BinPos number_of_copies
                = instance.bin_type(bin_type_id_best).copies
                - solution.bin_copies(bin_type_id_best);
            for (ItemTypeId item_type_id = 0;
                    item_type_id < instance.number_of_item_types();
                    ++item_type_id) {
                ItemPos item_remaining_copies
                    = instance.item_type(item_type_id).copies
                    - solution.item_copies(item_type_id);
                ItemPos item_packed_copies = kp_solution_best.item_copies(item_type_id);
                if (item_packed_copies > 0) {
                    number_of_copies = std::min(
                            number_of_copies,
                            (BinPos)(item_remaining_copies / item_packed_copies));
                }
            }

            // Update ratio_sums.
            //auto item_space = instance.item_type(0).space();
            double item_space = 0;
            for (ItemTypeId item_type_id = 0;
                    item_type_id < instance.number_of_item_types();
                    ++item_type_id) {
                item_space += kp_solution_best.item_copies(item_type_id) * instance.item_type(item_type_id).space();
            }
            Area waste = instance.bin_type(bin_type_id_best).space() - item_space;
            for (ItemTypeId item_type_id = 0;
                    item_type_id < instance.number_of_item_types();
                    ++item_type_id) {
                const auto& item_type = instance.item_type(item_type_id);
                ItemPos copies = (double)kp_solution_best.item_copies(item_type_id);
                double ratio = (double)copies * (double)item_type.space() / (double)item_space;
                //std::cout << "item_type_id " << item_type_id
                //    << " space " << item_type.space()
                //    << " ratio " << ratio
                //    << " waste " << waste;
                item_type_adjusted_space[item_type_id]
                    += number_of_copies
                    * ((double)copies * (double)instance.item_type(item_type_id).space()
                            + ratio * waste);
                //std::cout << " adjusted_space " << item_type_adjusted_space[item_type_id] << std::endl;
            }

            // Update current solution.
            solution.append(
                    kp_solution_best,
                    0,
                    number_of_copies);
        }

        //if (output.number_of_iterations > 0) {
        //    std::cout << "it " << output.number_of_iterations
        //        << " old " << output.solution_pool.best().number_of_bins()
        //        << " new " << solution.number_of_bins()
        //        << std::endl;
        //}
        //std::cout << solution.cost() << std::endl;
        //for (ItemTypeId item_type_id = 0;
        //        item_type_id < instance.number_of_item_types();
        //        ++item_type_id) {
        //    if (solution.item_copies(item_type_id) != instance.item_type(item_type_id).copies) {
        //        std::cout << item_type_id << std::endl;
        //    }
        //}
        //std::cout << solution.number_of_items() << " " << solution.number_of_bins() << std::endl;
        //solution.write("solution_svc_" + std::to_string(output.number_of_iterations) + ".json");

        // Update best solution.
        std::stringstream ss;
        ss << "iteration " << output.number_of_iterations;
        algorithm_formatter.update_solution(solution, ss.str());

        // Check BinPacking goal.
        if (instance.objective() == Objective::BinPacking
                && output.solution_pool.best().number_of_items() == instance.number_of_items()
                && output.solution_pool.best().number_of_bins() <= std::max(parameters.bin_packing_goal, (BinPos)2))
            return output;

        // If the objective is "Knapsack" and all items have been packed, stop.
        if (instance.objective() == Objective::Knapsack
                && output.solution_pool.best().number_of_items() == instance.number_of_items())
            return output;

        // Update profits.
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            const auto& item_type = instance.item_type(item_type_id);
            //std::cout << "item_type_id " << item_type_id
            //    << " adjusted_space " << item_type_adjusted_space[item_type_id]
            //    << " profit " << profits[item_type_id];
            Profit profit_new = 0.0;
            if (instance.objective() == Objective::Knapsack) {
                profit_new
                    = item_type.profit
                    / item_type.space()
                    * item_type_adjusted_space[item_type_id]
                    / solution.item_copies(item_type_id);
            } else {
                item_type_adjusted_space[item_type_id]
                    += 100 * lbs * (item_type.copies - solution.item_copies(item_type_id));
                profit_new
                    = item_type_adjusted_space[item_type_id]
                    / item_type.copies;
            }
            profits[item_type_id] = 0.5 * profits[item_type_id] + 0.5 * profit_new;
            //std::cout << " -> " << profits[item_type_id] << std::endl;
        }

    }

    algorithm_formatter.end();
    return output;
}

}
