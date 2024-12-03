/**
 * Column generation algorithm
 *
 * Algorithm for variable-sized bin packing, bin packing and (multiple)
 * knapsack problems.
 *
 * Input:
 * - m bin types with lower bounds lᵢ, upper bounds uᵢ and costs cᵢ (i = 0..m)
 * - n item types with qⱼ copies
 * Problem:
 * - pack all items in a subset of the bins.
 * Objective:
 * - minimize the sum of the cost of the bins used.
 *
 * The linear programming formulation of the variable-sized bin packing problem
 * based on Dantzig–Wolfe decomposition is written as follows:
 *
 * Variables:
 * - yᵢᵏ ∈ {0, qmax} representing a set of items fitting into bin type i.
 *   yᵢᵏ = q iff the corresponding set of items is selected q times.
 *   xⱼᵢᵏ = q iff yᵏ contains q copies of item type j, otherwise 0.
 *
 * Program:
 *
 * min ∑ᵢ cᵢ ∑ₖ yᵢᵏ
 *
 * lᵢ <= ∑ₖ yᵢᵏ <= uᵢ         for all bin types i
 *                           (bounds on the number of bins for each bin type)
 *                                                         Dual variables: uⱼ
 *
 * qⱼ <= ∑ₖ xⱼᵢᵏ yᵢᵏ <= qⱼ     for all item types j
 *                                      (each item selected exactly qⱼ times)
 *                                                         Dual variables: vⱼ
 *
 * The pricing problem consists in finding a variable of negative reduced cost.
 * The reduced cost of a variable yᵢᵏ is given by:
 * rc(yᵢᵏ) = cᵢ - uᵢ - ∑ⱼ xⱼᵢᵏ vⱼ
 *
 * Therefore, finding a variable of minium reduced cost reduces to solving
 * m bounded knapsack problems with items with profit vⱼ.
 *
 */

#pragma once

#include "packingsolver/algorithms/common.hpp"

#include "columngenerationsolver/algorithms/heuristic_tree_search.hpp"
#include "columngenerationsolver/algorithms/limited_discrepancy_search.hpp"

namespace packingsolver
{

using Value = columngenerationsolver::Value;
using Column = columngenerationsolver::Column;
using PricingOutput = columngenerationsolver::PricingSolver::PricingOutput;

template <typename Instance, typename InstanceBuilder, typename Solution>
using ColumnGenerationPricingFunction = std::function<Output<Instance, Solution>(const Instance&)>;

template <typename Instance, typename InstanceBuilder, typename Solution>
class ColumnGenerationPricingSolver: public columngenerationsolver::PricingSolver
{

public:

    ColumnGenerationPricingSolver(
            const Instance& instance,
            const ColumnGenerationPricingFunction<Instance, InstanceBuilder, Solution>& pricing_function):
        instance_(instance),
        pricing_function_(pricing_function),
        fixed_bin_types_(instance.number_of_bin_types()),
        filled_demands_(instance.number_of_item_types())
    { }

    virtual std::vector<std::shared_ptr<const Column>> initialize_pricing(
            const std::vector<std::pair<std::shared_ptr<const Column>, Value>>& fixed_columns);

    virtual PricingOutput solve_pricing(
            const std::vector<Value>& duals);

private:

    const Instance& instance_;

    ColumnGenerationPricingFunction<Instance, InstanceBuilder, Solution> pricing_function_;

    std::vector<BinPos> fixed_bin_types_;

    std::vector<double> filled_demands_;

};

template <typename Instance, typename InstanceBuilder, typename Solution>
columngenerationsolver::Model get_model(
        const Instance& instance,
        const ColumnGenerationPricingFunction<Instance, InstanceBuilder, Solution>& pricing_function)
{
    columngenerationsolver::Model model;

    Profit maximum_bin_type_cost = 0;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        if (maximum_bin_type_cost < instance.bin_type(bin_type_id).cost)
            maximum_bin_type_cost = instance.bin_type(bin_type_id).cost;
    }

    ItemPos maximum_item_type_demand = 0;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        if (maximum_item_type_demand < instance.item_type(item_type_id).copies)
            maximum_item_type_demand = instance.item_type(item_type_id).copies;
    }

    Profit maximum_item_profit = 0;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        if (maximum_item_profit < instance.item_type(item_type_id).profit)
            maximum_item_profit = instance.item_type(item_type_id).profit;
    }

    if (instance.objective() == Objective::VariableSizedBinPacking
            || instance.objective() == Objective::BinPacking) {
        model.objective_sense = optimizationtools::ObjectiveDirection::Minimize;
    } else if (instance.objective() == Objective::Knapsack) {
        model.objective_sense = optimizationtools::ObjectiveDirection::Maximize;
    }

    // Row bounds.
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        columngenerationsolver::Row row;
        row.lower_bound = instance.bin_type(bin_type_id).copies_min;
        row.upper_bound = instance.bin_type(bin_type_id).copies;
        row.coefficient_lower_bound = 0;
        row.coefficient_upper_bound = 1;
        model.rows.push_back(row);
    }
    if (instance.objective() == Objective::VariableSizedBinPacking
            || instance.objective() == Objective::BinPacking) {
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            columngenerationsolver::Row row;
            row.lower_bound = instance.item_type(item_type_id).copies;
            row.upper_bound = instance.item_type(item_type_id).copies;
            row.coefficient_lower_bound = 0;
            row.coefficient_upper_bound = instance.item_type(item_type_id).copies;
            model.rows.push_back(row);
        }
    } else if (instance.objective() == Objective::Knapsack) {
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            columngenerationsolver::Row row;
            row.lower_bound = 0;
            row.upper_bound = instance.item_type(item_type_id).copies;
            row.coefficient_lower_bound = 0;
            row.coefficient_upper_bound = instance.item_type(item_type_id).copies;
            model.rows.push_back(row);
        }
    }

    // Pricing solver.
    model.pricing_solver = std::unique_ptr<columngenerationsolver::PricingSolver>(
            new ColumnGenerationPricingSolver<Instance, InstanceBuilder, Solution>(instance, pricing_function));
    return model;
}

template <typename Instance, typename InstanceBuilder, typename Solution>
std::vector<std::shared_ptr<const Column>> ColumnGenerationPricingSolver<Instance, InstanceBuilder, Solution>::initialize_pricing(
        const std::vector<std::pair<std::shared_ptr<const Column>, Value>>& fixed_columns)
{
    //std::cout << "initialize_pricing " << fixed_columns.size() << std::endl;
    std::fill(fixed_bin_types_.begin(), fixed_bin_types_.end(), 0);
    std::fill(filled_demands_.begin(), filled_demands_.end(), 0);
    for (auto p: fixed_columns) {
        const Column& column = *(p.first);
        Value value = p.second;
        if (value < 0.5)
            continue;
        for (const columngenerationsolver::LinearTerm& element: column.elements) {
            if (element.row < instance_.number_of_bin_types()) {
                fixed_bin_types_[element.row] += value;
            } else {
                filled_demands_[element.row - instance_.number_of_bin_types()] += value * element.coefficient;
            }
        }
    }
    //std::cout << "initialize_pricing end" << std::endl;
    return {};
}

template <typename Solution>
std::vector<std::shared_ptr<const Column>> solution2column(
        const Solution& solution)
{
    std::vector<std::shared_ptr<const Column>> columns;
    for (BinPos bin_pos = 0;
            bin_pos < solution.number_of_different_bins();
            ++bin_pos) {
        BinTypeId bin_type_id = solution.bin(bin_pos).bin_type_id;
        Solution extra_solution(solution.instance());
        extra_solution.append(solution, bin_pos, 1);
        Column column;
        if (solution.instance().objective() == Objective::VariableSizedBinPacking
                || solution.instance().objective() == Objective::BinPacking) {
            column.objective_coefficient = extra_solution.cost();
        } else if (solution.instance().objective() == Objective::Knapsack) {
            column.objective_coefficient = extra_solution.profit();
        }
        columngenerationsolver::LinearTerm element;
        element.row = bin_type_id;
        element.coefficient = 1;
        column.elements.push_back(element);
        for (ItemTypeId item_type_id = 0;
                item_type_id < solution.instance().number_of_item_types();
                ++item_type_id) {
            if (extra_solution.item_copies(item_type_id) > 0) {
                columngenerationsolver::LinearTerm element;
                element.row = solution.instance().number_of_bin_types() + item_type_id;
                element.coefficient = extra_solution.item_copies(item_type_id);
                column.elements.push_back(element);
            }
        }
        column.extra = std::shared_ptr<void>(new Solution(extra_solution));
        columns.push_back(std::shared_ptr<const Column>(new Column(column)));
    }
    return columns;
}

template <typename Instance, typename InstanceBuilder, typename Solution>
PricingOutput ColumnGenerationPricingSolver<Instance, InstanceBuilder, Solution>::solve_pricing(
            const std::vector<Value>& duals)
{
    //std::cout << "solve_pricing" << std::endl;
    PricingOutput output;
    Value reduced_cost_bound = 0.0;

    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance_.number_of_bin_types();
            ++bin_type_id) {
        const auto& bin_type = instance_.bin_type(bin_type_id);
        if (fixed_bin_types_[bin_type_id] == bin_type.copies)
            continue;

        // Build knapsack instance.
        InstanceBuilder kp_instance_builder = InstanceBuilder();
        kp_instance_builder.set_objective(Objective::Knapsack);
        kp_instance_builder.set_parameters(instance_.parameters());
        kp_instance_builder.add_bin_type(bin_type, 1);
        std::vector<ItemTypeId> kp2vbpp;
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance_.number_of_item_types();
                ++item_type_id) {
            ItemPos copies = instance_.item_type(item_type_id).copies - filled_demands_[item_type_id];
            if (copies == 0)
                continue;

            Profit profit = 0;
            if (instance_.objective() == Objective::VariableSizedBinPacking
                    || instance_.objective() == Objective::BinPacking) {
                profit = duals[instance_.number_of_bin_types() + item_type_id];
            } else if (instance_.objective() == Objective::Knapsack) {
                profit = instance_.item_type(item_type_id).profit
                    - duals[instance_.number_of_bin_types() + item_type_id];
            }

            //std::cout << "j " << j << " profit " << profit << std::endl;
            if (profit <= 0)
                continue;
            kp_instance_builder.add_item_type(
                    instance_.item_type(item_type_id),
                    profit,
                    copies);
            kp2vbpp.push_back(item_type_id);
        }
        Instance kp_instance = kp_instance_builder.build();

        // Solve knapsack instance.
        //std::cout << "pricing_function" << std::endl;
        auto kp_output = pricing_function_(kp_instance);
        //std::cout << "pricing_function end" << std::endl;

        // Retrieve column.
        for (const Solution& kp_solution: kp_output.solution_pool.solutions()) {
            if (kp_solution.number_of_bins() == 0)
                continue;

            Column column;

            Solution extra_solution(instance_);
            extra_solution.append(
                    kp_solution,
                    0,
                    1,
                    {bin_type_id},
                    kp2vbpp);
            column.extra = std::shared_ptr<void>(new Solution(extra_solution));

            if (instance_.objective() == Objective::VariableSizedBinPacking
                    || instance_.objective() == Objective::BinPacking) {
                column.objective_coefficient = extra_solution.cost();
            } else if (instance_.objective() == Objective::Knapsack) {
                column.objective_coefficient = extra_solution.profit();
            }

            columngenerationsolver::LinearTerm element;
            element.row = bin_type_id;
            element.coefficient = 1;
            column.elements.push_back(element);
            //std::cout << duals[i] << std::endl;
            //std::cout << "number_of_items " << extra_solution.number_of_items() << std::endl;
            for (ItemTypeId kp_item_type_id = 0;
                    kp_item_type_id < kp_instance.number_of_item_types();
                    ++kp_item_type_id) {
                if (kp_solution.item_copies(kp_item_type_id) > 0) {
                    columngenerationsolver::LinearTerm element;
                    element.row = instance_.number_of_bin_types() + kp2vbpp[kp_item_type_id];
                    element.coefficient = kp_solution.item_copies(kp_item_type_id);
                    column.elements.push_back(element);
                    //std::cout << duals[m + extra->kp2vbpp[kp_j]] << std::endl;
                }
            }
            //std::cout << column << std::endl;
            output.columns.push_back(std::shared_ptr<const Column>(new Column(column)));
            if (instance_.objective() == Objective::VariableSizedBinPacking
                    || instance_.objective() == Objective::BinPacking) {
                reduced_cost_bound = (std::min)(
                        reduced_cost_bound,
                        bin_type.cost - duals[bin_type_id] - kp_output.knapsack_bound);
            } else if (instance_.objective() == Objective::Knapsack) {
                reduced_cost_bound = (std::max)(
                        reduced_cost_bound,
                        kp_output.knapsack_bound - duals[bin_type_id]);
            }
        }
    }

    output.overcost
        = (std::min)((BinPos)instance_.number_of_items(), instance_.number_of_bins())
        * reduced_cost_bound;

    //std::cout << "solve_pricing end" << std::endl;
    return output;
}

}
