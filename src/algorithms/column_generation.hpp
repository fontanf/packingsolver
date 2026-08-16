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

#include "columngenerationsolver/algorithms/limited_discrepancy_search.hpp"

#include <sstream>

namespace packingsolver
{

using Value = columngenerationsolver::Value;
using Column = columngenerationsolver::Column;
using PricingOutput = columngenerationsolver::PricingSolver::PricingOutput;

template <typename Instance, typename InstanceBuilder, typename Solution, typename Output = packingsolver::Output<Instance, Solution>>
using ColumnGenerationPricingFunction = std::function<Output(const Instance&)>;

template <typename Instance, typename InstanceBuilder, typename Solution, typename Output = packingsolver::Output<Instance, Solution>>
class ColumnGenerationPricingSolver: public columngenerationsolver::PricingSolver
{

public:

    ColumnGenerationPricingSolver(
            const Instance& instance,
            const Output& output,
            const ColumnGenerationPricingFunction<Instance, InstanceBuilder, Solution, Output>& pricing_function):
        instance_(instance),
        output_(output),
        pricing_function_(pricing_function),
        fixed_bin_types_(instance.number_of_bin_types()),
        filled_demands_(instance.number_of_item_types()),
        filled_fixed_demands_(instance.number_of_item_types())
    { }

    virtual std::vector<std::shared_ptr<const Column>> initialize_pricing(
            const std::vector<std::pair<std::shared_ptr<const columngenerationsolver::Column>, columngenerationsolver::Value>>& fixed_columns,
            const std::vector<std::shared_ptr<const columngenerationsolver::Cut>>&,
            const std::vector<std::shared_ptr<const columngenerationsolver::BranchingDecision>>&) override;

    virtual PricingOutput solve_pricing(
            bool solve_feasibility,
            const std::vector<columngenerationsolver::Value>& duals,
            const std::vector<std::pair<std::shared_ptr<const columngenerationsolver::Cut>, columngenerationsolver::Value>>&) override;

private:

    const Instance& instance_;

    const Output& output_;

    ColumnGenerationPricingFunction<Instance, InstanceBuilder, Solution, Output> pricing_function_;

    std::vector<BinPos> fixed_bin_types_;

    std::vector<double> filled_demands_;

    /** Fixed copies already covered by selected LP columns (per item type). */
    std::vector<double> filled_fixed_demands_;

};

template <typename Instance, typename InstanceBuilder, typename Solution, typename Output>
columngenerationsolver::Model get_model(
        const Instance& instance,
        const Output& output,
        const ColumnGenerationPricingFunction<Instance, InstanceBuilder, Solution, Output>& pricing_function)
{
    columngenerationsolver::Model model;

    if (instance.objective() == Objective::VariableSizedBinPacking
            || instance.objective() == Objective::BinPacking
            || instance.objective() == Objective::Feasibility) {
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
            || instance.objective() == Objective::BinPacking
            || instance.objective() == Objective::Feasibility
            || instance.objective() == Objective::Knapsack) {
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            columngenerationsolver::Row row;
            row.lower_bound = instance.item_type(item_type_id).copies_min;
            row.upper_bound = instance.item_type(item_type_id).copies;
            row.coefficient_lower_bound = 0;
            row.coefficient_upper_bound = instance.item_type(item_type_id).copies;
            model.rows.push_back(row);
        }
    }

    // Pricing solver.
    model.pricing_solver = std::unique_ptr<columngenerationsolver::PricingSolver>(
            new ColumnGenerationPricingSolver<Instance, InstanceBuilder, Solution, Output>(instance, output, pricing_function));
    return model;
}

template <typename Instance, typename InstanceBuilder, typename Solution, typename Output>
std::vector<std::shared_ptr<const Column>> ColumnGenerationPricingSolver<Instance, InstanceBuilder, Solution, Output>::initialize_pricing(
        const std::vector<std::pair<std::shared_ptr<const columngenerationsolver::Column>, columngenerationsolver::Value>>& fixed_columns,
        const std::vector<std::shared_ptr<const columngenerationsolver::Cut>>&,
        const std::vector<std::shared_ptr<const columngenerationsolver::BranchingDecision>>&)
{
    //std::cout << "initialize_pricing " << fixed_columns.size() << std::endl;
    std::fill(fixed_bin_types_.begin(), fixed_bin_types_.end(), 0);
    std::fill(filled_demands_.begin(), filled_demands_.end(), 0);
    std::fill(filled_fixed_demands_.begin(), filled_fixed_demands_.end(), 0);
    for (auto p: fixed_columns) {
        const Column& column = *(p.first);
        Value value = p.second;
        if (value < 0.5)
            continue;
        for (const columngenerationsolver::LinearTerm& element: column.elements) {
            if (element.row < instance_.number_of_bin_types()) {
                BinTypeId bin_type_id = element.row;
                fixed_bin_types_[bin_type_id] += value;
                for (const auto& fixed_item: instance_.bin_type(bin_type_id).fixed_items)
                    filled_fixed_demands_[fixed_item.item_type_id] += value;
            } else {
                filled_demands_[element.row - instance_.number_of_bin_types()] += value * element.coefficient;
            }
        }
    }
    //std::cout << "initialize_pricing end" << std::endl;
    return {};
}

template <typename Solution>
std::vector<std::shared_ptr<const Column>> solution_to_columns(
        const Solution& solution)
{
    const auto& instance = solution.instance();
    double multiplier_cost = largest_power_of_two_lesser_or_equal(instance.largest_bin_cost());
    double multiplier_profit = largest_power_of_two_lesser_or_equal(instance.largest_item_profit());
    std::vector<std::shared_ptr<const Column>> columns;
    for (BinPos bin_pos = 0;
            bin_pos < solution.number_of_different_bins();
            ++bin_pos) {
        BinTypeId bin_type_id = solution.bin(bin_pos).bin_type_id;
        Solution extra_solution(instance);
        extra_solution.append_bin(solution, bin_pos, 1);
        Column column;
        if (instance.objective() == Objective::VariableSizedBinPacking
                || instance.objective() == Objective::BinPacking) {
            column.objective_coefficient = extra_solution.cost() / multiplier_cost;
        } else if (instance.objective() == Objective::Feasibility) {
            // Unlike 'BinPacking'/'VariableSizedBinPacking', a real bin
            // cost isn't the right signal here (see
            // 'ColumnGenerationParameters::use_sequential_feasibility''s
            // own doc comment): it would bias the optimality phase toward
            // whichever bin type is cheapest, not whichever is actually
            // needed for feasibility. A uniform 1 per column instead makes
            // that phase minimize the plain number of bins used, with no
            // such bias.
            column.objective_coefficient = 1;
        } else if (instance.objective() == Objective::Knapsack) {
            column.objective_coefficient = extra_solution.profit() / multiplier_profit;
        }
        columngenerationsolver::LinearTerm element;
        element.row = bin_type_id;
        element.coefficient = 1;
        column.elements.push_back(element);
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            if (extra_solution.item_copies(item_type_id) > 0) {
                columngenerationsolver::LinearTerm element;
                element.row = instance.number_of_bin_types() + item_type_id;
                element.coefficient = extra_solution.item_copies(item_type_id);
                column.elements.push_back(element);
            }
        }
        column.extra = std::shared_ptr<void>(new Solution(extra_solution));
        columns.push_back(std::shared_ptr<const Column>(new Column(column)));
    }
    return columns;
}

template <typename Instance, typename InstanceBuilder, typename Solution, typename Output>
PricingOutput ColumnGenerationPricingSolver<Instance, InstanceBuilder, Solution, Output>::solve_pricing(
        bool solve_feasibility,
        const std::vector<Value>& duals,
        const std::vector<std::pair<std::shared_ptr<const columngenerationsolver::Cut>, Value>>&)
{
    double multiplier_cost = largest_power_of_two_lesser_or_equal(instance_.largest_bin_cost());
    double multiplier_profit = largest_power_of_two_lesser_or_equal(instance_.largest_item_profit());

    //std::cout << "solve_pricing" << std::endl;
    PricingOutput output;
    Value reduced_cost_bound = 0.0;

    std::vector<ItemPos> bin_fixed_copies(instance_.number_of_item_types(), 0);
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance_.number_of_bin_types();
            ++bin_type_id) {
        const auto& bin_type = instance_.bin_type(bin_type_id);
        if (fixed_bin_types_[bin_type_id] == bin_type.copies)
            continue;

        for (const auto& fixed_item: bin_type.fixed_items)
            bin_fixed_copies[fixed_item.item_type_id]++;

        // Build knapsack instance.
        InstanceBuilder kp_instance_builder = InstanceBuilder();
        kp_instance_builder.set_objective(Objective::Knapsack);
        kp_instance_builder.set_parameters(instance_.parameters());
        BinTypeId kp_bin_type_id = kp_instance_builder.add_bin_type(instance_, bin_type_id);
        kp_instance_builder.set_bin_type_copies(kp_bin_type_id, 1);
        kp_instance_builder.set_bin_type_copies_min(kp_bin_type_id, 0);
        std::vector<ItemTypeId> kp2vbpp;
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance_.number_of_item_types();
                ++item_type_id) {
            const auto& item_type = instance_.item_type(item_type_id);

            Profit profit = 0;
            if (instance_.objective() == Objective::VariableSizedBinPacking
                    || instance_.objective() == Objective::BinPacking
                    || instance_.objective() == Objective::Feasibility) {
                profit = duals[instance_.number_of_bin_types() + item_type_id];
            } else if (instance_.objective() == Objective::Knapsack) {
                // During the feasibility phase (see 'solve_feasibility'
                // above), the pricing search must ignore real profit
                // entirely and search purely on duals (matching
                // 'columngenerationsolver::PricingSolver::
                // compute_reduced_cost''s own zeroing of
                // 'column.objective_coefficient') - a column's real profit
                // is the sum of its items' own real profits, so zeroing it
                // decomposes cleanly to zeroing each item's contribution
                // here.
                profit = (solve_feasibility? 0: item_type.profit)
                    - duals[instance_.number_of_bin_types() + item_type_id]
                    * multiplier_profit;
            }

            // If profit <= 0, only add fixed copies (they must be in the bin).
            // If profit > 0, add all available copies.
            ItemPos copies;
            if (profit <= 0) {
                copies = bin_fixed_copies[item_type_id];
            } else {
                copies = item_type.copies
                    - item_type.copies_fixed
                    - (filled_demands_[item_type_id] - filled_fixed_demands_[item_type_id])
                    + bin_fixed_copies[item_type_id];
            }
            //std::cout << "item_type_id " << item_type_id << " profit " << profit << std::endl;
            if (copies <= 0)
                continue;
            ItemTypeId kp_item_type_id = kp_instance_builder.add_item_type(instance_, item_type_id);
            kp_instance_builder.set_item_type_profit(kp_item_type_id, profit);
            kp_instance_builder.set_item_type_copies(kp_item_type_id, copies);
            kp2vbpp.push_back(item_type_id);
        }
        Instance kp_instance = kp_instance_builder.build();

        // Solve knapsack instance.
        //std::cout << "pricing_function" << std::endl;
        auto kp_output = pricing_function_(kp_instance);
        //std::cout << "pricing_function end" << std::endl;

        // Retrieve column.
        for (const auto& kp_entry: kp_output.solution_pool.solutions()) {
            if (kp_entry.solution.number_of_bins() == 0)
                continue;

            Column column;

            Solution extra_solution(instance_);
            extra_solution.append_bin(
                    kp_entry.solution,
                    0,
                    1,
                    {bin_type_id},
                    kp2vbpp);
            column.extra = std::shared_ptr<void>(new Solution(extra_solution));

            if (instance_.objective() == Objective::VariableSizedBinPacking
                    || instance_.objective() == Objective::BinPacking) {
                column.objective_coefficient = extra_solution.cost() / multiplier_cost;
            } else if (instance_.objective() == Objective::Feasibility) {
                // See 'solution_to_columns''s own identical branch above.
                column.objective_coefficient = 1;
            } else if (instance_.objective() == Objective::Knapsack) {
                column.objective_coefficient = extra_solution.profit() / multiplier_profit;
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
                if (kp_entry.solution.item_copies(kp_item_type_id) > 0) {
                    columngenerationsolver::LinearTerm element;
                    element.row = instance_.number_of_bin_types() + kp2vbpp[kp_item_type_id];
                    element.coefficient = kp_entry.solution.item_copies(kp_item_type_id);
                    column.elements.push_back(element);
                    //std::cout << duals[m + extra->kp2vbpp[kp_j]] << std::endl;
                }
            }
            //std::cout << column << std::endl;
            output.columns.push_back(std::shared_ptr<const Column>(new Column(column)));
            if (instance_.objective() == Objective::VariableSizedBinPacking
                    || instance_.objective() == Objective::BinPacking) {
                // Real bin cost zeroed during the feasibility phase, same
                // reasoning as the item profit above; 'kp_output.
                // knapsack_bound' already reflects that phase's own (dual-
                // only, since items carry no real cost for this objective)
                // pricing search, so it needs no separate adjustment here.
                reduced_cost_bound = (std::min)(
                        reduced_cost_bound,
                        (solve_feasibility? 0: bin_type.cost / multiplier_cost) - duals[bin_type_id] - kp_output.knapsack_bound);
            } else if (instance_.objective() == Objective::Feasibility) {
                // Consistent with 'column.objective_coefficient' above: 1
                // per column, not the bin's real cost - zeroed during the
                // feasibility phase like every other real objective term.
                reduced_cost_bound = (std::min)(
                        reduced_cost_bound,
                        (solve_feasibility? 0: 1) - duals[bin_type_id] - kp_output.knapsack_bound);
            } else if (instance_.objective() == Objective::Knapsack) {
                reduced_cost_bound = (std::max)(
                        reduced_cost_bound,
                        kp_output.knapsack_bound / multiplier_profit - duals[bin_type_id]);
            }
        }

        for (const auto& fixed_item: bin_type.fixed_items)
            bin_fixed_copies[fixed_item.item_type_id]--;
    }

    // 'reduced_cost_bound' is a per-bin bound on the reduced cost, so the
    // Lagrangian overcost is that times the largest number of bins any
    // solution could ever use. If a feasible BinPacking solution has
    // already been found, its own number of bins is a tighter such cap
    // than the generic 'min(number_of_items, number_of_bins)' one (no
    // solution using more bins than the best one found so far could ever
    // be optimal).
    BinPos maximum_number_of_bins = (std::min)((BinPos)instance_.number_of_items(), instance_.number_of_bins());
    if (instance_.objective() == Objective::BinPacking
            && output_.solution_pool.best().feasible()) {
        maximum_number_of_bins = output_.solution_pool.best().number_of_bins();
    }
    output.overcost
        = maximum_number_of_bins * reduced_cost_bound;

    //std::cout << "solve_pricing end" << std::endl;
    return output;
}

template <typename Instance, typename Solution, typename Output = packingsolver::Output<Instance, Solution>>
struct ColumnGenerationParameters: packingsolver::Parameters<Instance, Solution, Output>
{
    OptimizationMode optimization_mode = OptimizationMode::Anytime;
    int internal_diving = 1;
    columngenerationsolver::SolverName linear_programming_solver_name
        = columngenerationsolver::SolverName::CLP;

    /**
     * Sequential feasibility scheme for the 'BinPacking' objective - the
     * column generation counterpart of 'onedimensional::milp_assignment's
     * own 'use_sequential_feasibility' scheme (see its doc comment).
     *
     * 'column_generation' converts its master's cost-minimizing LP bound
     * into a bin count by dividing by a single bin type's cost (see the
     * 'BinPacking' branch of its own 'new_bound_callback'), which is only
     * sound when every bin type costs the same. Whenever there is more than
     * one bin type, 'column_generation' therefore always uses this scheme
     * instead, regardless of this parameter.
     *
     * When there is a single bin type, the direct approach is sound too, so
     * this parameter is only needed to opt into the sequential feasibility
     * scheme there as well. Defaults to 'true', so it is used whenever
     * possible (any bin count for the 'BinPacking' objective); set to
     * 'false' to force the direct approach for a single bin type instead.
     *
     * Each 'Feasibility' sub-problem's master LP minimizes the plain number
     * of bins used (every real column gets objective coefficient 1,
     * regardless of bin type - see 'solution_to_columns' and
     * 'ColumnGenerationPricingSolver::solve_pricing') instead of leaving it
     * at the default 0 - only the row bounds differ from the direct
     * approach's own 'BinPacking' LP (the bin-type row is capped at the
     * candidate bin count instead of the instance's own bin availability).
     * 'columngenerationsolver::column_generation' itself already searches
     * for feasibility (a dummy-column-free relaxation) with every real
     * column's objective coefficient zeroed, regardless of what it is set
     * to here, so this doesn't change how feasibility itself gets
     * established; it only shapes the follow-up optimality phase, reached
     * once a candidate bin count is confirmed feasible, into a genuine
     * "minimize bins used" problem instead of a flat one where every
     * feasible combination ties - and unlike using the bin's real cost
     * (which 'BinPacking' needs, since there it's the actual objective), it
     * doesn't bias that phase toward whichever bin type is cheapest when
     * there is more than one, only toward using fewer of them. This doesn't
     * change what counts as feasible - the row bounds are untouched, so
     * this remains a pure feasibility question ("does this many bins
     * suffice?"), not a request for the minimal-cost solution.
     *
     * Not used for other objectives.
     */
    bool use_sequential_feasibility = true;
};

/**
 * Build the 'Feasibility' sub-instance for a candidate bin count of
 * 'column_generation's own sequential feasibility scheme (see
 * 'ColumnGenerationParameters::use_sequential_feasibility'): the first
 * 'number_of_bins' bins of 'instance', in the order bin types must be used
 * (mirrors 'onedimensional::milp_assignment's own sequential feasibility
 * scheme). Bin types are added in the same order as 'instance''s own,
 * starting from the first one, so the sub-instance's bin type ids line up
 * 1:1 with 'instance''s - no remapping is needed to relate a sub-solution
 * back to 'instance' (see 'packingsolver::enforce_bin_type_order', used
 * against this 1:1 correspondence once the sub-instance has been solved).
 */
template <typename Instance, typename InstanceBuilder>
Instance build_sequential_feasibility_sub_instance(
        const Instance& instance,
        BinPos number_of_bins)
{
    InstanceBuilder sub_instance_builder;
    sub_instance_builder.set_objective(Objective::Feasibility);
    sub_instance_builder.set_parameters(instance.parameters());
    BinPos remaining_bins = number_of_bins;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types() && remaining_bins > 0;
            ++bin_type_id) {
        const auto& bin_type = instance.bin_type(bin_type_id);
        BinPos copies = (std::min)(bin_type.copies, remaining_bins);
        BinTypeId sub_bin_type_id = sub_instance_builder.add_bin_type(instance, bin_type_id);
        // Reset 'copies_min' to 0 before 'copies': 'add_bin_type' copied the
        // original bin type's own 'copies_min', which could otherwise be
        // larger than the subset 'copies' computed above and make
        // 'set_bin_type_copies' reject it.
        sub_instance_builder.set_bin_type_copies_min(sub_bin_type_id, 0);
        sub_instance_builder.set_bin_type_copies(sub_bin_type_id, copies);
        remaining_bins -= copies;
    }
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        sub_instance_builder.add_item_type(instance, item_type_id);
    }
    return sub_instance_builder.build();
}

/**
 * 'lower_bound': a known lower bound on the number of bins already
 * established by the caller (e.g. a bound computed by another algorithm
 * before this one runs), used only by the 'ColumnGenerationParameters::
 * use_sequential_feasibility' scheme to seed its starting candidate bin
 * count instead of always starting from scratch. Combined (via 'max') with
 * 'output.bin_packing_bound' itself, so it is never unsound to pass a stale
 * or overly conservative value.
 *
 * 'column_pool': columns are packing patterns for a single bin of a given
 * bin type, so they stay valid regardless of how many bins of that type are
 * available - meaning a column found while solving one instance can be
 * reused unchanged as a starting point for another instance with the same
 * bin and item types (this is exactly the relationship between successive
 * candidates of the sequential feasibility scheme above). If non-null,
 * seeded with '*column_pool' before solving and updated in place with every
 * column found by this call on return, so passing the same pointer to a
 * sequence of calls (as the sequential feasibility scheme does) lets each
 * one pick up where the previous one left off instead of starting its
 * pricing from scratch.
 */
template <typename Instance, typename InstanceBuilder, typename Solution, typename AlgorithmFormatter, typename Output = packingsolver::Output<Instance, Solution>>
Output column_generation(
        const Instance& instance,
        const ColumnGenerationPricingFunction<Instance, InstanceBuilder, Solution, Output>& pricing_function,
        const ColumnGenerationParameters<Instance, Solution, Output>& parameters = {},
        BinPos lower_bound = 0,
        std::vector<std::shared_ptr<const Column>>* column_pool = nullptr)
{
    Output output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    // Sequential feasibility scheme (see 'ColumnGenerationParameters::
    // use_sequential_feasibility'): try candidate bin counts in increasing
    // order starting at 'sequential_feasibility_lower_bound', each as a
    // 'Feasibility' sub-instance containing only the first 'k' bins (see
    // 'build_sequential_feasibility_sub_instance'), solved via a direct
    // recursive call to 'column_generation' itself (never anything else -
    // in particular, this never falls back to a domain's other,
    // non-column-generation algorithms). Stops at the first 'k' found
    // feasible, which is then optimal (every smaller candidate has been
    // shown infeasible).
    if (instance.objective() == Objective::BinPacking
            && (instance.number_of_bin_types() > 1
                    || parameters.use_sequential_feasibility)) {
        BinPos sequential_feasibility_lower_bound = (std::max)(output.bin_packing_bound, lower_bound);
        // No candidate bin count can ever exceed the total number of bins
        // the instance actually offers: past that point,
        // 'build_sequential_feasibility_sub_instance' would just keep
        // returning the same (fully-used) sub-instance over and over,
        // looping forever if it keeps coming back inconclusive (see below).
        BinPos sequential_feasibility_bins_available = 0;
        for (BinTypeId bin_type_id = 0;
                bin_type_id < instance.number_of_bin_types();
                ++bin_type_id) {
            sequential_feasibility_bins_available += instance.bin_type(bin_type_id).copies;
        }
        // Columns are reused from one candidate bin count to the next (see
        // 'column_pool''s own doc comment above): every candidate shares the
        // same bin and item types, only the number of bins available of
        // each type changes.
        std::vector<std::shared_ptr<const Column>> sequential_feasibility_column_pool;
        for (BinPos number_of_bins = sequential_feasibility_lower_bound;
                number_of_bins <= sequential_feasibility_bins_available;
                ++number_of_bins) {
            if (algorithm_formatter.end_boolean() || parameters.timer.needs_to_end())
                break;

            Instance sub_instance = build_sequential_feasibility_sub_instance<Instance, InstanceBuilder>(
                    instance, number_of_bins);

            ColumnGenerationParameters<Instance, Solution, Output> sub_parameters;
            sub_parameters.verbosity_level = 0;
            sub_parameters.timer = parameters.timer;
            sub_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
            sub_parameters.optimization_mode = parameters.optimization_mode;
            sub_parameters.internal_diving = parameters.internal_diving;
            sub_parameters.linear_programming_solver_name = parameters.linear_programming_solver_name;
            Output sub_output = column_generation<Instance, InstanceBuilder, Solution, AlgorithmFormatter, Output>(
                    sub_instance, pricing_function, sub_parameters, 0, &sequential_feasibility_column_pool);

            if (sub_output.solution_pool.best().feasible()) {
                // Found a feasible solution: report it as a new upper bound.
                // Its bin count is not claimed optimal here - proving that
                // would require having proven every smaller candidate
                // infeasible ourselves, but 'sequential_feasibility_lower_bound'
                // is only ever a starting point handed to us (from
                // 'output.bin_packing_bound' or the 'lower_bound' argument),
                // not something this loop has itself established; the bound
                // only ever advances below via our own infeasibility proofs.
                Solution solution(instance);
                solution.append_bins(packingsolver::enforce_bin_type_order(sub_output.solution_pool.best()), {}, {});
                std::stringstream ss;
                ss << "SF " << (number_of_bins - sequential_feasibility_lower_bound);
                algorithm_formatter.update_solution(solution, ss.str());
                break;
            }
            if (sub_output.is_proven_infeasible) {
                // A proof that 'number_of_bins' is infeasible is valid on
                // its own (infeasible with 'number_of_bins' bins available
                // implies infeasible with fewer too, since fewer bins is
                // strictly more restrictive), regardless of whether earlier,
                // smaller candidates were themselves conclusively tested -
                // so the lower bound can always be tightened here.
                algorithm_formatter.update_bin_packing_bound(number_of_bins + 1);
            }
            // Otherwise inconclusive (e.g. cut short by the timer) for this
            // bin count: keep trying larger candidates anyway - a feasible
            // solution found there is still a useful upper bound, even
            // though it can no longer be proven optimal.
        }

        algorithm_formatter.end();
        return output;
    }

    columngenerationsolver::Model cgs_model
        = get_model<Instance, InstanceBuilder, Solution, Output>(instance, output, pricing_function);
    columngenerationsolver::LimitedDiscrepancySearchParameters cgslds_parameters;
    cgslds_parameters.verbosity_level = 0;
    cgslds_parameters.timer = parameters.timer;
    cgslds_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    cgslds_parameters.internal_diving = parameters.internal_diving;
    cgslds_parameters.rounding_heuristic = true;
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
                solution.append_bin(
                        *std::static_pointer_cast<Solution>(column.extra),
                        0,
                        value);
            }
            std::stringstream ss;
            ss << "n " << cgslds_output.number_of_nodes;
            algorithm_formatter.update_solution(solution, ss.str());
        }
    };
    cgslds_parameters.new_bound_callback = [&instance, &algorithm_formatter](
            const columngenerationsolver::Output& cgs_output)
    {
        const columngenerationsolver::LimitedDiscrepancySearchOutput& cgslds_output
            = static_cast<const columngenerationsolver::LimitedDiscrepancySearchOutput&>(cgs_output);
        if (instance.objective() == Objective::VariableSizedBinPacking) {
            double multiplier_cost = largest_power_of_two_lesser_or_equal(instance.largest_bin_cost());
            algorithm_formatter.update_variable_sized_bin_packing_bound(
                    cgslds_output.bound * multiplier_cost);
        } else if (instance.objective() == Objective::Knapsack) {
            double multiplier_profit = largest_power_of_two_lesser_or_equal(instance.largest_item_profit());
            algorithm_formatter.update_knapsack_bound(
                    cgslds_output.bound * multiplier_profit);
        } else if (instance.objective() == Objective::BinPacking) {
            double multiplier_cost = largest_power_of_two_lesser_or_equal(instance.largest_bin_cost());
            algorithm_formatter.update_bin_packing_bound(std::ceil(
                    cgslds_output.bound * multiplier_cost / instance.bin_type(0).cost - 0.001));
        } else if (instance.objective() == Objective::Feasibility) {
            // By the extended reals convention, the optimal value of an
            // infeasible minimization problem is +inf.
            if (cgslds_output.bound == std::numeric_limits<double>::infinity())
                algorithm_formatter.update_is_proven_infeasible();
        }
    };
    cgslds_parameters.column_generation_parameters.solver_name
        = parameters.linear_programming_solver_name;
    if (column_pool != nullptr)
        cgslds_parameters.column_pool = *column_pool;
    columngenerationsolver::LimitedDiscrepancySearchOutput cgslds_search_output
        = columngenerationsolver::limited_discrepancy_search(cgs_model, cgslds_parameters);
    if (column_pool != nullptr)
        *column_pool = cgslds_search_output.columns;

    algorithm_formatter.end();
    return output;
}

}
