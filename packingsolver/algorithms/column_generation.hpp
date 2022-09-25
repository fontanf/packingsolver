#pragma once

#include "packingsolver/algorithms/common.hpp"

#include "columngenerationsolver/algorithms/heuristic_tree_search.hpp"
#include "columngenerationsolver/algorithms/limited_discrepancy_search.hpp"

/**
 * Variable-sized Bin Packing Problem.
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

namespace packingsolver
{

using RowIdx = columngenerationsolver::RowIdx;
using ColIdx = columngenerationsolver::ColIdx;
using Value = columngenerationsolver::Value;
using Column = columngenerationsolver::Column;

template <typename Instance, typename Solution>
using VariableSizeBinPackingPricingFunction = std::function<SolutionPool<Instance, Solution>(const Instance&)>;

template <typename Instance, typename Solution>
class VariableSizeBinPackingPricingSolver: public columngenerationsolver::PricingSolver
{

public:

    VariableSizeBinPackingPricingSolver(
            const Instance& instance,
            const VariableSizeBinPackingPricingFunction<Instance, Solution>& pricing_function):
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
    VariableSizeBinPackingPricingFunction<Instance, Solution> pricing_function_;

    std::vector<BinPos> fixed_bin_types_;
    std::vector<ItemPos> filled_demands_;

};

template <typename Instance, typename Solution>
columngenerationsolver::Parameters get_parameters(
        const Instance& instance,
        const VariableSizeBinPackingPricingFunction<Instance, Solution>& pricing_function)
{
    BinTypeId m = instance.number_of_bin_types();
    ItemTypeId n = instance.number_of_item_types();
    columngenerationsolver::Parameters p(m + n);

    Profit maximum_bin_type_cost = 0;
    for (BinTypeId i = 0; i < m; ++i)
        if (maximum_bin_type_cost < instance.bin_type(i).cost)
            maximum_bin_type_cost = instance.bin_type(i).cost;

    ItemPos maximum_item_type_demand = 0;
    for (ItemTypeId j = 0; j < n; ++j)
        if (maximum_item_type_demand < instance.item_type(j).copies)
            maximum_item_type_demand = instance.item_type(j).copies;

    p.objective_sense = columngenerationsolver::ObjectiveSense::Min;
    p.column_lower_bound = 0;
    p.column_upper_bound = maximum_item_type_demand;
    // Row bounds.
    for (BinTypeId i = 0; i < m; ++i) {
        p.row_lower_bounds[i] = instance.bin_type(i).copies_min;
        p.row_upper_bounds[i] = instance.bin_type(i).copies;
        p.row_coefficient_lower_bounds[i] = 0;
        p.row_coefficient_upper_bounds[i] = 1;
    }
    for (ItemTypeId j = 0; j < n; ++j) {
        p.row_lower_bounds[m + j] = instance.item_type(j).copies;
        p.row_upper_bounds[m + j] = instance.item_type(j).copies;
        p.row_coefficient_lower_bounds[m + j] = 0;
        p.row_coefficient_upper_bounds[m + j] = instance.item_type(j).copies;
    }
    // Dummy column objective coefficient.
    p.dummy_column_objective_coefficient = 10 * maximum_bin_type_cost * maximum_item_type_demand;
    //std::cout << "dummy_column_objective_coefficient " << p.dummy_column_objective_coefficient << std::endl;
    // Pricing solver.
    p.pricing_solver = std::unique_ptr<columngenerationsolver::PricingSolver>(
            new VariableSizeBinPackingPricingSolver<Instance, Solution>(instance, pricing_function));
    return p;
}

template <typename Instance, typename Solution>
std::vector<ColIdx> VariableSizeBinPackingPricingSolver<Instance, Solution>::initialize_pricing(
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
struct VariableSizeBinPackingColumnExtra
{
    Solution solution;
    BinTypeId bin_type_id;
    std::vector<ItemTypeId> kp2vbpp;
};

template <typename Instance, typename Solution>
std::vector<Column> VariableSizeBinPackingPricingSolver<Instance, Solution>::solve_pricing(
            const std::vector<Value>& duals)
{
    //std::cout << "solve_pricing" << std::endl;
    ItemTypeId m = instance_.number_of_bin_types();
    ItemTypeId n = instance_.number_of_item_types();
    std::vector<Column> columns;

    for (BinTypeId i = 0; i < m; ++i) {
        if (fixed_bin_types_[i] == instance_.bin_type(i).copies)
            continue;
        // Build knapsack instance.
        Instance instance_kp = Instance();
        instance_kp.set_objective(Objective::Knapsack);
        instance_kp.set_parameters(instance_.parameters());
        instance_kp.add_bin_type(instance_.bin_type(i), 1);
        std::vector<ItemTypeId> kp2vbpp;
        for (ItemTypeId j = 0; j < n; ++j) {
            Profit profit = duals[m + j];
            //std::cout << "j " << j << " profit " << profit << std::endl;
            if (profit <= 0)
                continue;
            ItemPos copies = instance_.item_type(j).copies - filled_demands_[j];
            instance_kp.add_item_type(instance_.item_type(j), profit, copies);
            kp2vbpp.push_back(j);
        }

        // Solve knapsack instance.
        //std::cout << "pricing_function" << std::endl;
        SolutionPool<Instance, Solution> solution_pool = pricing_function_(instance_kp);
        //std::cout << "pricing_function end" << std::endl;

        // Retrieve column.
        for (const Solution& solution: solution_pool.solutions()) {
            VariableSizeBinPackingColumnExtra<Solution> extra {solution, i, kp2vbpp};
            Column column;
            column.objective_coefficient = instance_.bin_type(i).cost;
            column.row_indices.push_back(i);
            column.row_coefficients.push_back(1);
            //std::cout << duals[i] << std::endl;
            //std::cout << "number_of_items " << extra.solution.number_of_items() << std::endl;
            for (ItemTypeId j_kp = 0; j_kp < instance_kp.number_of_item_types(); ++j_kp) {
                if (extra.solution.item_copies(j_kp) > 0) {
                    //std::cout << "j " << extra.kp2vbpp[j_kp] << " copies " << extra.solution.item_copies(j_kp) << std::endl;
                    column.row_indices.push_back(m + extra.kp2vbpp[j_kp]);
                    column.row_coefficients.push_back(extra.solution.item_copies(j_kp));
                    //std::cout << duals[m + extra->kp2vbpp[j_kp]] << std::endl;
                }
            }
            column.extra = std::shared_ptr<void>(new VariableSizeBinPackingColumnExtra<Solution>(extra));
            //std::cout << column << std::endl;
            columns.push_back(column);
        }
    }
    //std::cout << "solve_pricing end" << std::endl;
    return columns;
}

}

