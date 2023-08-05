/**
 * Column Generation algorithm
 *
 * Algorithm for Variable-sized Bin Packing and Bin Packing Problems.
 *
 * Input:
 * - m bin types with lower bounds lᵢ, upper bounds uᵢ and costs cᵢ (i = 0..m)
 * - n item types with qᵢ copies
 * Problem:
 * - pack all items in a subset of the bins.
 * Objective:
 * - minimize the sum of the cost of the bins used.
 *
 * The linear programming formulation of the Variable-sized Bin Packing Problem
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
 * qⱼ <= ∑ₖ xⱼᵏ yᵢᵏ <= qⱼ     for all item types j
 *                                      (each item selected exactly qⱼ times)
 *                                                         Dual variables: vⱼ
 *
 * The pricing problem consists in finding a variable of negative reduced cost.
 * The reduced cost of a variable yᵏ is given by:
 * rc(yᵢᵏ) = cᵢ - uᵢ - ∑ⱼ xⱼᵏ vⱼ
 *
 * Therefore, finding a variable of minium reduced cost reduces to solving
 * a Bounded Knapsack Problems with items with profit vⱼ.
 *
 */

#pragma once

#include "packingsolver/algorithms/common.hpp"

#include "columngenerationsolver/algorithms/heuristic_tree_search.hpp"
#include "columngenerationsolver/algorithms/limited_discrepancy_search.hpp"

namespace packingsolver
{

using RowIdx = columngenerationsolver::RowIdx;
using ColIdx = columngenerationsolver::ColIdx;
using Value = columngenerationsolver::Value;
using Column = columngenerationsolver::Column;

template <typename Instance, typename InstanceBuilder, typename Solution>
using VariableSizeBinPackingPricingFunction = std::function<SolutionPool<Instance, Solution>(const Instance&)>;

template <typename Instance, typename InstanceBuilder, typename Solution>
class VariableSizeBinPackingPricingSolver: public columngenerationsolver::PricingSolver
{

public:

    VariableSizeBinPackingPricingSolver(
            const Instance& instance,
            const VariableSizeBinPackingPricingFunction<Instance, InstanceBuilder, Solution>& pricing_function):
        instance_(instance),
        pricing_function_(pricing_function),
        fixed_bin_types_(instance.number_of_bin_types()),
        filled_demands_(instance.number_of_item_types())
    { }

    virtual std::vector<ColIdx> initialize_pricing(
            const std::vector<Column>& columns,
            const std::vector<std::pair<ColIdx, Value>>& fixed_columns);

    virtual std::vector<Column> solve_pricing(
            const std::vector<Value>& duals);

private:

    const Instance& instance_;
    VariableSizeBinPackingPricingFunction<Instance, InstanceBuilder, Solution> pricing_function_;

    std::vector<BinPos> fixed_bin_types_;
    std::vector<ItemPos> filled_demands_;

};

template <typename Instance, typename InstanceBuilder, typename Solution>
columngenerationsolver::Parameters get_parameters(
        const Instance& instance,
        const VariableSizeBinPackingPricingFunction<Instance, InstanceBuilder, Solution>& pricing_function)
{
    BinTypeId m = instance.number_of_bin_types();
    ItemTypeId n = instance.number_of_item_types();
    columngenerationsolver::Parameters p(m + n);

    Profit maximum_bin_type_cost = 0;
    for (BinTypeId bin_type_id = 0; bin_type_id < m; ++bin_type_id)
        if (maximum_bin_type_cost < instance.bin_type(bin_type_id).cost)
            maximum_bin_type_cost = instance.bin_type(bin_type_id).cost;

    ItemPos maximum_item_type_demand = 0;
    for (ItemTypeId item_type_id = 0; item_type_id < n; ++item_type_id)
        if (maximum_item_type_demand < instance.item_type(item_type_id).copies)
            maximum_item_type_demand = instance.item_type(item_type_id).copies;

    p.objective_sense = columngenerationsolver::ObjectiveSense::Min;
    p.column_lower_bound = 0;
    p.column_upper_bound = maximum_item_type_demand;
    // Row bounds.
    for (BinTypeId bin_type_id = 0; bin_type_id < m; ++bin_type_id) {
        p.row_lower_bounds[bin_type_id] = instance.bin_type(bin_type_id).copies_min;
        p.row_upper_bounds[bin_type_id] = instance.bin_type(bin_type_id).copies;
        p.row_coefficient_lower_bounds[bin_type_id] = 0;
        p.row_coefficient_upper_bounds[bin_type_id] = 1;
    }
    for (ItemTypeId item_type_id = 0; item_type_id < n; ++item_type_id) {
        p.row_lower_bounds[m + item_type_id] = instance.item_type(item_type_id).copies;
        p.row_upper_bounds[m + item_type_id] = instance.item_type(item_type_id).copies;
        p.row_coefficient_lower_bounds[m + item_type_id] = 0;
        p.row_coefficient_upper_bounds[m + item_type_id] = instance.item_type(item_type_id).copies;
    }
    // Dummy column objective coefficient.
    p.dummy_column_objective_coefficient = 10 * maximum_bin_type_cost * maximum_item_type_demand;
    //std::cout << "dummy_column_objective_coefficient " << p.dummy_column_objective_coefficient << std::endl;
    // Pricing solver.
    p.pricing_solver = std::unique_ptr<columngenerationsolver::PricingSolver>(
            new VariableSizeBinPackingPricingSolver<Instance, InstanceBuilder, Solution>(instance, pricing_function));
    return p;
}

template <typename Instance, typename InstanceBuilder, typename Solution>
std::vector<ColIdx> VariableSizeBinPackingPricingSolver<Instance, InstanceBuilder, Solution>::initialize_pricing(
            const std::vector<Column>& columns,
            const std::vector<std::pair<ColIdx, Value>>& fixed_columns)
{
    //std::cout << "initialize_pricing " << fixed_columns.size() << std::endl;
    ItemTypeId m = instance_.number_of_bin_types();
    std::fill(fixed_bin_types_.begin(), fixed_bin_types_.end(), 0);
    std::fill(filled_demands_.begin(), filled_demands_.end(), 0);
    for (auto p: fixed_columns) {
        const Column& column = columns[p.first];
        Value value = p.second;
        if (value < 0.5)
            continue;
        for (RowIdx row_pos = 0; row_pos < (RowIdx)column.row_indices.size(); ++row_pos) {
            RowIdx row_index = column.row_indices[row_pos];
            Value row_coefficient = column.row_coefficients[row_pos];
            if (row_index < m) {
                fixed_bin_types_[row_index] += value;
            } else {
                filled_demands_[row_index - m] += value * row_coefficient;
            }
        }
    }
    //std::cout << "initialize_pricing end" << std::endl;
    return {};
}

template <typename Solution>
std::vector<Column> solution2column(
        const Solution& solution)
{
    std::vector<Column> columns;
    BinPos m = solution.instance().number_of_bin_types();
    for (BinPos bin_pos = 0; bin_pos < solution.number_of_different_bins(); ++bin_pos) {
        BinTypeId bin_type_id = solution.bin(bin_pos).bin_type_id;
        Solution extra_solution(solution.instance());
        extra_solution.append(solution, bin_pos, 1);
        Column column;
        column.objective_coefficient = solution.instance().bin_type(bin_type_id).cost;
        column.row_indices.push_back(bin_type_id);
        column.row_coefficients.push_back(1);
        for (ItemTypeId item_type_id = 0;
                item_type_id < solution.instance().number_of_item_types();
                ++item_type_id) {
            if (extra_solution.item_copies(item_type_id) > 0) {
                column.row_indices.push_back(m + item_type_id);
                column.row_coefficients.push_back(
                        extra_solution.item_copies(item_type_id));
            }
        }
        column.extra = std::shared_ptr<void>(new Solution(extra_solution));
        columns.push_back(column);
    }
    return columns;
}

template <typename Instance, typename InstanceBuilder, typename Solution>
std::vector<Column> VariableSizeBinPackingPricingSolver<Instance, InstanceBuilder, Solution>::solve_pricing(
            const std::vector<Value>& duals)
{
    //std::cout << "solve_pricing" << std::endl;
    ItemTypeId m = instance_.number_of_bin_types();
    ItemTypeId n = instance_.number_of_item_types();
    std::vector<Column> columns;

    for (BinTypeId bin_type_id = 0; bin_type_id < m; ++bin_type_id) {
        if (fixed_bin_types_[bin_type_id] == instance_.bin_type(bin_type_id).copies)
            continue;
        // Build knapsack instance.
        InstanceBuilder kp_instance_builder = InstanceBuilder();
        kp_instance_builder.set_objective(Objective::Knapsack);
        kp_instance_builder.set_parameters(instance_.parameters());
        kp_instance_builder.add_bin_type(
                instance_.bin_type(bin_type_id),
                1);
        std::vector<ItemTypeId> kp2vbpp;
        for (ItemTypeId item_type_id = 0;
                item_type_id < n;
                ++item_type_id) {
            Profit profit = duals[m + item_type_id];
            //std::cout << "j " << j << " profit " << profit << std::endl;
            if (profit <= 0)
                continue;
            ItemPos copies = instance_.item_type(item_type_id).copies - filled_demands_[item_type_id];
            kp_instance_builder.add_item_type(
                    instance_.item_type(item_type_id),
                    profit,
                    copies);
            kp2vbpp.push_back(item_type_id);
        }
        Instance kp_instance = kp_instance_builder.build();

        // Solve knapsack instance.
        //std::cout << "pricing_function" << std::endl;
        SolutionPool<Instance, Solution> kp_solution_pool = pricing_function_(kp_instance);
        //std::cout << "pricing_function end" << std::endl;

        // Retrieve column.
        for (const Solution& kp_solution: kp_solution_pool.solutions()) {
            if (kp_solution.number_of_bins() == 0)
                continue;

            Column column;
            column.objective_coefficient = instance_.bin_type(bin_type_id).cost;
            column.row_indices.push_back(bin_type_id);
            column.row_coefficients.push_back(1);
            //std::cout << duals[i] << std::endl;
            //std::cout << "number_of_items " << extra.solution.number_of_items() << std::endl;
            for (ItemTypeId kp_item_type_id = 0;
                    kp_item_type_id < kp_instance.number_of_item_types();
                    ++kp_item_type_id) {
                if (kp_solution.item_copies(kp_item_type_id) > 0) {
                    column.row_indices.push_back(m + kp2vbpp[kp_item_type_id]);
                    column.row_coefficients.push_back(kp_solution.item_copies(kp_item_type_id));
                    //std::cout << duals[m + extra->kp2vbpp[kp_j]] << std::endl;
                }
            }
            Solution extra_solution(instance_);
            extra_solution.append(
                    kp_solution,
                    0,
                    1,
                    {bin_type_id},
                    kp2vbpp);
            column.extra = std::shared_ptr<void>(new Solution(extra_solution));
            //std::cout << column << std::endl;
            columns.push_back(column);
        }
    }
    //std::cout << "solve_pricing end" << std::endl;
    return columns;
}

}

