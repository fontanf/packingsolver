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

#include "optimizationtools/utils/utils.hpp"

#include "columngenerationsolver/algorithms/limited_discrepancy_search.hpp"

#include <algorithm>
#include <array>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace packingsolver
{

using Value = columngenerationsolver::Value;
using Column = columngenerationsolver::Column;
using Cut = columngenerationsolver::Cut;
using CutIdx = columngenerationsolver::CutIdx;
using PricingOutput = columngenerationsolver::PricingSolver::PricingOutput;
using PricingType = columngenerationsolver::PricingSolver::PricingType;

/**
 * Data specific to a subset-row cut of generalized cardinality (Jepsen,
 * Petersen, Spoorendonk & Pisinger 2008 introduce the cardinality-three case,
 * used for the SR cuts of wang2025_bin_packing; the same multiplier-1/2
 * Chvatal-Gomory rounding argument generalizes to any larger subset, as in
 * the "extended" subset-row cuts used in VRP branch-cut-and-price): given a
 * subset of item types, summing their demand-exactly-1 row equalities with
 * multiplier 1/2 each and rounding down gives
 * sum_{p: pattern p contains >= 2 of the subset} floor(|subset ∩ p| / 2) xi_p
 *     <= floor(|item_type_ids.size()| / 2).
 * The cardinality-three case is the special case where the right-hand side
 * is 1 and the left-hand side coefficient is 0/1 valued ("at most one
 * pattern contains >= 2 of the triplet").
 *
 * Stored in 'Cut::extra' (tagged by 'Cut::name == subset_row_cut_name'):
 * 'columngenerationsolver::Cut' is a concrete, uniform type - cut families
 * are told apart by 'extra', not by subclassing (see 'Cut' in
 * columngenerationsolver). 'item_type_ids' is always kept sorted (by
 * 'build_subset_row_cut' below), so two cuts over the same subset compare
 * equal by simple vector comparison - see 'ColumnGenerationPricingSolver::
 * equal' below.
 */
struct SubsetRowCutExtra
{
    std::vector<ItemTypeId> item_type_ids;
};

/** Tag used to recognize a subset-row cut among 'Cut::extra' payloads. */
const std::string subset_row_cut_name = "subset_row";

/**
 * Build a subset-row cut over the given item type subset (cardinality >= 3
 * - see 'SubsetRowCutExtra' above for the general formula; cardinality 3 is
 * the original Jepsen et al. triple).
 *
 * Coefficient computation is not attached to the cut itself (see 'Cut' in
 * columngenerationsolver): 'ColumnGenerationPricingSolver::coefficient'
 * below reads 'SubsetRowCutExtra' back out of 'Cut::extra' and checks item
 * presence via the packed solution stashed in every column's 'extra' field
 * (see 'solution_to_columns' and 'solve_pricing' below, the only two places
 * that build a 'Column' for this template - both always set 'extra') with
 * an O(1) 'Solution::item_copies' lookup per item type. Works unmodified
 * for every domain that uses this column generation template. Enforcing
 * the cut during pricing (as opposed to just rejecting cut-violating
 * columns after the fact) is handled separately in 'solve_pricing' below,
 * for domains whose 'InstanceBuilder' supports resources.
 */
inline std::shared_ptr<Cut> build_subset_row_cut(
        std::vector<ItemTypeId> item_type_ids)
{
    std::sort(item_type_ids.begin(), item_type_ids.end());

    auto extra = std::make_shared<SubsetRowCutExtra>();
    extra->item_type_ids = item_type_ids;

    auto cut = std::make_shared<Cut>();
    cut->name = subset_row_cut_name;
    cut->upper_bound = (Value)(item_type_ids.size() / 2);
    cut->extra = extra;
    return cut;
}

template <typename Instance, typename InstanceBuilder, typename Solution, typename Output = packingsolver::Output<Instance, Solution>>
using ColumnGenerationPricingFunction = std::function<Output(const Instance&, PricingType)>;

template <typename Instance, typename InstanceBuilder, typename Solution, typename Output = packingsolver::Output<Instance, Solution>>
class ColumnGenerationPricingSolver: public columngenerationsolver::PricingSolver
{

public:

    ColumnGenerationPricingSolver(
            const Instance& instance,
            const Output& output,
            const ColumnGenerationPricingFunction<Instance, InstanceBuilder, Solution, Output>& pricing_function,
            bool has_pricing_bound = false):
        instance_(instance),
        output_(output),
        pricing_function_(pricing_function),
        has_pricing_bound_(has_pricing_bound)
    { }

    virtual PricingOutput solve_pricing(
            bool solve_feasibility,
            const std::vector<std::pair<std::shared_ptr<const columngenerationsolver::Column>, columngenerationsolver::Value>>& fixed_columns,
            const std::vector<std::shared_ptr<const columngenerationsolver::BranchingDecision>>&,
            const std::unordered_set<std::shared_ptr<const columngenerationsolver::Column>>& tabu,
            const std::vector<columngenerationsolver::Value>& duals,
            const std::vector<std::pair<std::shared_ptr<const columngenerationsolver::Cut>, columngenerationsolver::Value>>& cut_duals,
            PricingType pricing_type) override;

    /**
     * Whether 'pricing_function_' has something genuinely useful to offer
     * for 'PricingType::Dual' calls - set from the constructor's
     * 'has_pricing_bound' argument, which only a domain whose
     * 'pricing_function' actually branches on 'PricingType::Dual' (e.g.
     * rectangle, forcing an exact Benders-decomposition solve - see
     * 'rectangle::optimize_column_generation') should pass as 'true'. See
     * 'columngenerationsolver::PricingSolver::has_pricing_bound' for what
     * this gates.
     */
    virtual bool has_pricing_bound() const override
    {
        return has_pricing_bound_;
    }

    /**
     * Subset-row cut (Jepsen et al. 2008) separation - see
     * 'SubsetRowCutExtra' above.
     *
     * Fully generic: only relies on the row layout built by 'get_model'
     * (bin type rows first, then item type rows), so this works unmodified
     * for every domain that uses this column generation template, not just
     * rectangle.
     *
     * Two passes:
     * - Cardinality 3: candidate triples are restricted to those built from
     *   a pair of item types directly co-occurring in some positive-valued
     *   column of 'solution', extended by a third item type that co-occurs
     *   with either one of the pair in some (possibly different) column: a
     *   triple with no such pairwise evidence anywhere contributes nothing
     *   to any cut's violation yet, so this is a sound (if not exhaustive)
     *   pruning of the full O(n^3) triple search space.
     * - Cardinality 5, then 7 (see 'generalized_subset_row_cardinalities'
     *   below): every co-occurring pair (seeded from pairs rather than
     *   triples - see this pass's own comment in 'separate_cuts' for why
     *   ranking by triple weight would miss some violated larger subsets)
     *   is greedily grown, one item type at a time, up through cardinality
     *   5 and then 7, checking for a violation at each cardinality along
     *   the way - picking at each growth step whichever candidate
     *   maximizes the resulting subset's violation.
     */
    virtual std::vector<std::shared_ptr<const Cut>> separate_cuts(
            const columngenerationsolver::Solution& solution) override;

    /** Coefficient of a subset-row cut on a column - see 'Cut' in columngenerationsolver. */
    virtual Value coefficient(
            const Cut& cut,
            const Column& column) const override;

private:

    /**
     * Upper bound on the number of bins any 'VariableSizedBinPacking'
     * solution at least as good as the current incumbent could use; see
     * its definition for the derivation. Cached against the incumbent's
     * cost - see 'has_cached_maximum_number_of_bins_'.
     */
    BinPos maximum_number_of_bins_for_variable_sized_bin_packing();

    const Instance& instance_;

    const Output& output_;

    ColumnGenerationPricingFunction<Instance, InstanceBuilder, Solution, Output> pricing_function_;

    /** See 'has_pricing_bound' above. */
    bool has_pricing_bound_ = false;

    /**
     * Cache for the 'VariableSizedBinPacking' 'maximum_number_of_bins'
     * computation below: 'true' once it has been computed at least once,
     * so it is only recomputed when the incumbent's cost has actually
     * improved since.
     */
    bool has_cached_maximum_number_of_bins_ = false;

    /** Incumbent cost the cached value below was computed from. */
    Profit cached_maximum_number_of_bins_cost_ = -1;

    /** See 'has_cached_maximum_number_of_bins_'. */
    BinPos cached_maximum_number_of_bins_ = -1;

};

template <typename Instance, typename InstanceBuilder, typename Solution, typename Output>
columngenerationsolver::Model get_model(
        const Instance& instance,
        const Output& output,
        const ColumnGenerationPricingFunction<Instance, InstanceBuilder, Solution, Output>& pricing_function,
        bool pricing_function_has_dual_bound = false)
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
            new ColumnGenerationPricingSolver<Instance, InstanceBuilder, Solution, Output>(
                instance, output, pricing_function, pricing_function_has_dual_bound));
    return model;
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
        const std::vector<std::pair<std::shared_ptr<const columngenerationsolver::Column>, columngenerationsolver::Value>>& fixed_columns,
        const std::vector<std::shared_ptr<const columngenerationsolver::BranchingDecision>>&,
        const std::unordered_set<std::shared_ptr<const columngenerationsolver::Column>>& tabu,
        const std::vector<Value>& duals,
        const std::vector<std::pair<std::shared_ptr<const columngenerationsolver::Cut>, Value>>& cut_duals,
        PricingType pricing_type)
{
    double multiplier_cost = largest_power_of_two_lesser_or_equal(instance_.largest_bin_cost());
    double multiplier_profit = largest_power_of_two_lesser_or_equal(instance_.largest_item_profit());

    // 'fixed_columns' and 'tabu' are the same on every call within one
    // column generation attempt (see 'columngenerationsolver::PricingSolver
    // ::solve_pricing''s own doc comment on why they're passed here rather
    // than via a separate one-time setup call), but cheap enough to derive
    // from unconditionally on every call anyway - no need to cache them
    // across calls.
    std::vector<BinPos> fixed_bin_types(instance_.number_of_bin_types(), 0);
    std::vector<double> filled_demands(instance_.number_of_item_types(), 0);
    // Fixed copies already covered by selected LP columns (per item type).
    std::vector<double> filled_fixed_demands(instance_.number_of_item_types(), 0);
    for (const auto& p: fixed_columns) {
        const Column& column = *(p.first);
        Value value = p.second;
        if (value < 0.5)
            continue;
        for (const columngenerationsolver::LinearTerm& element: column.elements) {
            if (element.row < instance_.number_of_bin_types()) {
                BinTypeId bin_type_id = element.row;
                fixed_bin_types[bin_type_id] += value;
                for (const auto& fixed_item: instance_.bin_type(bin_type_id).fixed_items)
                    filled_fixed_demands[fixed_item.item_type_id] += value;
            } else {
                filled_demands[element.row - instance_.number_of_bin_types()] += value * element.coefficient;
            }
        }
    }

    //std::cout << "solve_pricing" << std::endl;
    PricingOutput output;
    Value reduced_cost_bound = 0.0;

    std::vector<ItemPos> bin_fixed_copies(instance_.number_of_item_types(), 0);
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance_.number_of_bin_types();
            ++bin_type_id) {
        const auto& bin_type = instance_.bin_type(bin_type_id);
        if (fixed_bin_types[bin_type_id] == bin_type.copies)
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
                    - (filled_demands[item_type_id] - filled_fixed_demands[item_type_id])
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
        // Map from vbpp item type id to this round's kp item type id ('-1'
        // for a vbpp item type excluded from this round's kp instance -
        // see the 'copies <= 0' skip above). Built once, shared by the
        // subset-row cut resources below and the tabu no-good cut
        // resources further down.
        std::vector<ItemTypeId> vbpp2kp;
        if (!cut_duals.empty() || !tabu.empty()) {
            vbpp2kp.assign(instance_.number_of_item_types(), -1);
            for (ItemTypeId kp_item_type_id = 0;
                    kp_item_type_id < (ItemTypeId)kp2vbpp.size();
                    ++kp_item_type_id) {
                vbpp2kp[kp2vbpp[kp_item_type_id]] = kp_item_type_id;
            }
        }

        // Apply active subset-row cuts as penalizing resources on the
        // pricing sub-instance's bin type (see 'Resource' in
        // 'packingsolver/algorithms/common.hpp'), so that the tree search
        // actually searches for the best *reduced-cost* pattern instead of
        // just the best raw-profit one.
        if (!cut_duals.empty()) {
            for (const auto& cut_dual: cut_duals) {
                if (cut_dual.first->name != subset_row_cut_name)
                    continue;
                const SubsetRowCutExtra& extra
                    = *std::static_pointer_cast<SubsetRowCutExtra>(cut_dual.first->extra);
                // The SR cut is a one-sided '<=' row (see
                // 'build_subset_row_cut': only 'upper_bound' is set, so
                // 'lower_bound' stays at its default of -infinity), so its
                // dual has a fixed sign, set by the master's objective
                // sense (see 'get_model'): non-positive when minimizing
                // (BinPacking/VariableSizedBinPacking/Feasibility),
                // non-negative when maximizing (Knapsack) - the standard
                // convention for any '<=' row. 'Resource' always computes
                // 'profit -= penalty', so 'penalty' must be converted into
                // the non-negative quantity that reproduces the pricing
                // contribution derived from the reduced cost formula at
                // the top of this file: minimizing wants 'profit +=
                // cut_dual' (i.e. 'penalty = -cut_dual'); maximizing wants
                // 'profit -= cut_dual * multiplier_profit' (i.e. 'penalty
                // = cut_dual * multiplier_profit', scaled the same way the
                // item duals just above are).
                // The cut's coefficient on a generated column is
                // floor(count / 2), where 'count' is how many *distinct*
                // item types of 'item_type_ids' the column contains - not
                // how many copies (see 'coefficient' below, which counts
                // presence, not copies) - reproduced here as one soft
                // resource per unit of that coefficient: resource i
                // (capacities 1, 3, 5, ...)
                // is the first to cross its capacity exactly when 'count'
                // reaches 2*(i+1), so the number of resources that end up
                // crossed equals floor(count / 2) exactly, each
                // independently contributing one 'penalty' ('Resource'
                // triggers its penalty only once per bin, the first time
                // consumption crosses its own capacity - see its own doc
                // comment). This is why the cardinality-3 case (a single
                // resource of capacity 1) is enough to cap its coefficient
                // at 1: floor(3 / 2) is still 1, so a second resource would
                // never be reachable there anyway.
                //
                // If this bin type's pricing sub-instance excludes 2 or
                // more of the subset's item types already (e.g. because
                // their remaining demand is 0 this round - see the
                // 'copies <= 0' skip above), no column from it could ever
                // reach count 2, so no resource here could ever be crossed
                // and building any would be dead weight in the tree search
                // state for no benefit - same reasoning caps the number of
                // resources built at 'present_kp_item_type_ids.size() / 2':
                // no column could ever reach a count high enough to cross
                // any resource beyond that.
                std::vector<ItemTypeId> present_kp_item_type_ids;
                for (ItemTypeId item_type_id: extra.item_type_ids) {
                    if (item_type_id < 0 || item_type_id >= (ItemTypeId)vbpp2kp.size())
                        continue;
                    ItemTypeId kp_item_type_id = vbpp2kp[item_type_id];
                    if (kp_item_type_id == -1)
                        continue;
                    present_kp_item_type_ids.push_back(kp_item_type_id);
                }
                if (present_kp_item_type_ids.size() < 2)
                    continue;

                double penalty = (instance_.objective() == Objective::Knapsack)?
                    cut_dual.second * multiplier_profit:
                    -cut_dual.second;
                ItemPos number_of_thresholds = (ItemPos)present_kp_item_type_ids.size() / 2;
                for (ItemPos threshold = 0; threshold < number_of_thresholds; ++threshold) {
                    ResourceId resource_id = kp_instance_builder.add_bin_type_resource(
                            kp_bin_type_id,
                            /* capacity */ 2.0 * threshold + 1.0,
                            /* penalize */ true,
                            /* penalty */ penalty);
                    for (ItemTypeId kp_item_type_id: present_kp_item_type_ids) {
                        // Consumption schedule '[1.0, 0.0]': the item
                        // type's first copy consumes 1.0 (counts as
                        // "present"), every further copy consumes 0.0 (see
                        // 'Resource::item_consumption' - a copy past the
                        // end of the schedule repeats its last entry, so
                        // without this trailing 0 every extra copy would
                        // keep adding 1.0, making the resource count total
                        // copies instead of distinct present item types,
                        // unlike 'coefficient' above).
                        kp_instance_builder.add_resource_consumption(
                                kp_bin_type_id,
                                resource_id,
                                kp_item_type_id,
                                0,
                                1.0);
                        kp_instance_builder.add_resource_consumption(
                                kp_bin_type_id,
                                resource_id,
                                kp_item_type_id,
                                1,
                                0.0);
                    }
                }
            }
        }

        // Forbid regenerating any tabu column's pattern for this bin type,
        // via a no-good cut expressed as a hard resource (see 'Resource'
        // in 'packingsolver/algorithms/common.hpp', and the identical
        // technique - 'threshold_schedule'/'ResourceCut' - already used by
        // 'rectangle::benders_decomposition' for the same purpose on its
        // own master problem): a per-item-type schedule capping that item
        // type's contribution at its copy count in the tabu pattern (a run
        // of 1's the length of that copy count, followed by a trailing 0
        // so it stops growing there), with the resource's capacity set to
        // one less than the sum of all the pattern's item copies, forbids
        // exactly that pattern - and any pattern using at least as many
        // copies of every item type in it - without excluding anything
        // else. Unlike the subset-row resources above, this is a hard
        // exclusion ('penalize' left at its default 'false'): a tabu
        // column isn't a priced-in cut with a dual value to trade off
        // against, it is a pattern the search has already branched away
        // from and must never regenerate.
        for (const auto& tabu_column: tabu) {
            // Identify the bin type this tabu column was generated for -
            // every column built by this template has exactly one bin
            // type element, with coefficient 1 (see 'solution_to_columns'
            // and this function's own column-building loop below).
            BinTypeId tabu_bin_type_id = -1;
            for (const columngenerationsolver::LinearTerm& element: tabu_column->elements) {
                if (element.row < instance_.number_of_bin_types()) {
                    tabu_bin_type_id = element.row;
                    break;
                }
            }
            if (tabu_bin_type_id != bin_type_id)
                continue;

            std::vector<std::pair<ItemTypeId, ItemPos>> present_kp_item_thresholds;
            double capacity = -1.0;
            for (const columngenerationsolver::LinearTerm& element: tabu_column->elements) {
                if (element.row < instance_.number_of_bin_types())
                    continue;
                ItemTypeId item_type_id = element.row - instance_.number_of_bin_types();
                ItemPos threshold = (ItemPos)std::round(element.coefficient);
                if (threshold <= 0)
                    continue;
                capacity += threshold;
                ItemTypeId kp_item_type_id = vbpp2kp[item_type_id];
                if (kp_item_type_id == -1)
                    continue;
                present_kp_item_thresholds.push_back({kp_item_type_id, threshold});
            }
            // If none of the tabu pattern's item types survived into this
            // round's kp instance, the pattern can't be regenerated this
            // round regardless (see the 'copies <= 0' skip above), so the
            // cut would be a dead weight in the tree search state for no
            // benefit - same reasoning as the subset-row cut's own
            // '< 2 present' skip above.
            if (present_kp_item_thresholds.empty())
                continue;

            ResourceId resource_id = kp_instance_builder.add_bin_type_resource(
                    kp_bin_type_id,
                    /* capacity */ capacity);
            for (const auto& present_kp_item_threshold: present_kp_item_thresholds) {
                ItemTypeId kp_item_type_id = present_kp_item_threshold.first;
                ItemPos threshold = present_kp_item_threshold.second;
                for (ItemPos copy = 0; copy < threshold; ++copy) {
                    kp_instance_builder.add_resource_consumption(
                            kp_bin_type_id,
                            resource_id,
                            kp_item_type_id,
                            copy,
                            1.0);
                }
                kp_instance_builder.add_resource_consumption(
                        kp_bin_type_id,
                        resource_id,
                        kp_item_type_id,
                        threshold,
                        0.0);
            }
        }

        Instance kp_instance = kp_instance_builder.build();

        // Solve knapsack instance.
        //std::cout << "pricing_function" << std::endl;
        auto kp_output = pricing_function_(kp_instance, pricing_type);
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
    // be optimal). For VariableSizedBinPacking, the incumbent's own bin
    // count isn't directly comparable this way (bins have different
    // costs), but its total cost still bounds how many bins any
    // at-least-as-good solution could afford - see
    // 'maximum_number_of_bins_for_variable_sized_bin_packing'.
    BinPos maximum_number_of_bins = (std::min)((BinPos)instance_.number_of_items(), instance_.number_of_bins());
    if (instance_.objective() == Objective::BinPacking
            && output_.solution_pool.best().feasible()) {
        maximum_number_of_bins = output_.solution_pool.best().number_of_bins();
    } else if (instance_.objective() == Objective::VariableSizedBinPacking
            && output_.solution_pool.best().feasible()) {
        maximum_number_of_bins = (std::min)(
                maximum_number_of_bins,
                maximum_number_of_bins_for_variable_sized_bin_packing());
    }
    output.overcost
        = maximum_number_of_bins * reduced_cost_bound;

    //std::cout << "solve_pricing end" << std::endl;
    return output;
}

template <typename Instance, typename InstanceBuilder, typename Solution, typename Output>
BinPos ColumnGenerationPricingSolver<Instance, InstanceBuilder, Solution, Output>::maximum_number_of_bins_for_variable_sized_bin_packing()
{
    Profit best_cost = output_.solution_pool.best().cost();
    if (has_cached_maximum_number_of_bins_
            && best_cost == cached_maximum_number_of_bins_cost_) {
        return cached_maximum_number_of_bins_;
    }

    // Any solution at least as good as the incumbent must have total
    // cost <= 'best_cost'. Maximizing the number of bins selected
    // subject only to that cost budget (each bin type bounded by its own
    // 'copies', with 'copies_min' bins mandatory) is a
    // maximum-cardinality-under-a-single-capacity problem: take the
    // mandatory copies of every bin type first (forced regardless of
    // cost), then greedily fill the remaining budget with the cheapest
    // bin types first. Sorting by increasing cost and greedily filling
    // is optimal for maximizing the count of bins selected subject to a
    // sum constraint - same argument as 'greedy_maximum_cardinality' in
    // the dual feasible functions bound.
    BinPos count = 0;
    Profit remaining_budget = best_cost;
    std::vector<std::pair<Profit, BinPos>> optional_bin_types;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance_.number_of_bin_types();
            ++bin_type_id) {
        const auto& bin_type = instance_.bin_type(bin_type_id);
        count += bin_type.copies_min;
        remaining_budget -= bin_type.cost * bin_type.copies_min;
        BinPos optional_copies = bin_type.copies - bin_type.copies_min;
        if (optional_copies > 0)
            optional_bin_types.push_back({bin_type.cost, optional_copies});
    }
    if (remaining_budget < 0)
        remaining_budget = 0;
    std::sort(optional_bin_types.begin(), optional_bin_types.end());
    for (const auto& p: optional_bin_types) {
        Profit cost = p.first;
        BinPos copies = p.second;
        if (cost <= 0) {
            count += copies;
            continue;
        }
        BinPos affordable = (BinPos)(remaining_budget / cost);
        BinPos take = (std::min)(copies, affordable);
        count += take;
        remaining_budget -= take * cost;
    }

    cached_maximum_number_of_bins_ = (std::min)(count, (BinPos)instance_.number_of_items());
    cached_maximum_number_of_bins_cost_ = best_cost;
    has_cached_maximum_number_of_bins_ = true;
    return cached_maximum_number_of_bins_;
}

template <typename Instance, typename InstanceBuilder, typename Solution, typename Output>
std::vector<std::shared_ptr<const Cut>> ColumnGenerationPricingSolver<Instance, InstanceBuilder, Solution, Output>::separate_cuts(
        const columngenerationsolver::Solution& solution)
{
    // A cut per separation round is plenty to make progress without
    // overwhelming the master LP; a co-packing candidate triple that is
    // still violated next round will simply be found again.
    const CutIdx maximum_number_of_cuts = 10000;
    // Safety cap on the total number of candidate triples considered, in
    // case some column happens to cover an unusually large number of item
    // types.
    const ItemPos maximum_number_of_candidate_triples = 10000;

    // Extract, for each non-negligible column, the sorted list of item
    // type ids it covers (rows past the bin type rows with coefficient >
    // 0.5 - see 'get_model' for the row layout), restricted to item types
    // with exactly 1 copy in the instance.
    //
    // The cut sum_{p: >=2 of the triplet present} xi_p <= 1 is only a valid
    // inequality when each of the triplet's 3 items has demand exactly 1:
    // that is what makes the underlying Chvatal-Gomory rounding argument
    // (summing the 3 row equalities with multiplier 1/2 each) produce
    // floor(3/2) = 1 on the right-hand side and match "item type present"
    // to the rounded left-hand-side coefficient. With copies > 1, a single
    // column's master variable is not bounded by 1 (a pattern can
    // legitimately be selected many times), so restricting it to <= 1
    // whenever it happens to cover 2 of the triplet's *types* - regardless
    // of copy count - can cut off the true optimum. Excluding item types
    // with copies > 1 from candidate triples entirely keeps every
    // generated cut within the regime the derivation actually covers.
    //
    // Two item types are also never included, both decided by a first pass
    // over 'solution.columns()' before any 'ColumnItems' entry is built:
    //
    // - an item type present in fewer than 2 non-negligible columns. Split
    //   a candidate triple's violation into: 'A', the mass of columns
    //   containing both of the other two item types (independent of this
    //   one), and 'B', the mass of columns containing this item type and
    //   exactly one of the other two. An item type present in only one
    //   column can contribute at most that column's value to 'B', so it can
    //   never beat a third item type reachable via a richer pair as the
    //   "extra" beyond 'A' - and using it as a seed pair member instead is
    //   only ever needed when its one column has exactly two items (the
    //   only way to reach that specific pair), a low-value case not worth
    //   special-casing.
    //
    // - an item type whose every non-negligible appearance has value ~= 1
    //   (an "integral" column: with the item's own copies == 1, this
    //   saturates that item's entire row capacity, leaving no room for any
    //   other column to also cover it). This applies to every cardinality k
    //   this function generates cuts for (3, 5 and 7): a set of such
    //   integral columns is necessarily pairwise item-disjoint (two
    //   sharing an item would both need that item's full capacity at once),
    //   so their contributions floor(count_1/2), floor(count_2/2), ...
    //   sum to at most floor(k/2) (by floor(a/2)+floor(b/2) <=
    //   floor((a+b)/2), summed count_i <= k) - exactly the cut's own
    //   right-hand side. Reaching a violation (> floor(k/2)) therefore
    //   always requires at least one genuinely fractional contribution, so
    //   an item type with no fractional appearance can never be part of a
    //   violated cut.
    struct ColumnItems
    {
        Value value;
        std::vector<ItemTypeId> item_type_ids;
    };
    std::vector<ItemPos> item_type_number_of_columns(instance_.number_of_item_types(), 0);
    for (const auto& p: solution.columns()) {
        // Only a genuinely fractional column (as opposed to merely
        // non-negligible) can ever contribute to a violated cut here, and
        // restricting this scan (and 'column_items' below) to fractional
        // columns changes nothing else: for an item type with 'copies ==
        // 1', the row's upper bound is exactly 1, so a single column with
        // value ~= 1 already saturates it entirely, leaving zero room for
        // any other column - integral or fractional - to also carry that
        // item type with positive value. Equivalently, such an item type
        // has exactly one non-negligible occurrence total (that one
        // integral column), so it is already excluded below regardless
        // (via 'number_of_columns < 2'); an item type that does have
        // another, distinct occurrence can therefore never have an
        // integral one in the first place - every one of its occurrences
        // is fractional. Skipping integral columns up front thus loses no
        // item type this scan would otherwise have kept, while skipping
        // exactly the columns that would end up contributing nothing to
        // 'column_items' below anyway (an integral column's every
        // 'copies == 1' item type is, by the same argument, excluded from
        // its own 'entry.item_type_ids', making it an inert entry there).
        if (p.second < 1e-6 || p.second >= 1.0 - 1e-6)
            continue;
        for (const columngenerationsolver::LinearTerm& element: p.first->elements) {
            if (element.row < instance_.number_of_bin_types())
                continue;
            if (element.coefficient <= 0.5)
                continue;
            ItemTypeId item_type_id = element.row - instance_.number_of_bin_types();
            if (instance_.item_type(item_type_id).copies != 1)
                continue;
            item_type_number_of_columns[item_type_id]++;
        }
    }
    std::vector<ColumnItems> column_items;
    for (const auto& p: solution.columns()) {
        if (p.second < 1e-6 || p.second >= 1.0 - 1e-6)
            continue;
        ColumnItems entry;
        entry.value = p.second;
        for (const columngenerationsolver::LinearTerm& element: p.first->elements) {
            if (element.row < instance_.number_of_bin_types())
                continue;
            if (element.coefficient <= 0.5)
                continue;
            ItemTypeId item_type_id = element.row - instance_.number_of_bin_types();
            if (instance_.item_type(item_type_id).copies != 1)
                continue;
            if (item_type_number_of_columns[item_type_id] < 2)
                continue;
            entry.item_type_ids.push_back(item_type_id);
        }
        std::sort(entry.item_type_ids.begin(), entry.item_type_ids.end());
        column_items.push_back(std::move(entry));
    }

    // Build, in a single pass:
    // - a co-occurrence graph (item_type_id -> set of item type ids that
    //   appear together with it in at least one non-negligible column),
    //   used below to generate candidate triples;
    // - for each pair of item types that co-occurs in some column,
    //   'pair_weight' (the total value of columns containing both) and
    //   'pair_columns' (the sorted list of indices - into 'column_items' -
    //   of those columns), used further down to compute each candidate
    //   triple's violation without rescanning every column;
    // - for each item type, 'item_columns' (the sorted list of indices -
    //   into 'column_items' - of every column that contains it), used by
    //   'subset_violation' below to compute a larger (cardinality 5 or 7)
    //   candidate's violation the same way - via a merge over just the
    //   columns its own item types actually appear in - rather than
    //   rescanning every active column regardless of whether it has any
    //   overlap with the candidate at all.
    std::vector<std::vector<ItemTypeId>> neighbors(instance_.number_of_item_types());
    std::vector<std::vector<ItemPos>> item_columns(instance_.number_of_item_types());
    using ItemTypeIdPair = std::pair<ItemTypeId, ItemTypeId>;
    struct ItemTypeIdPairHasher
    {
        std::size_t operator()(const ItemTypeIdPair& pair) const
        {
            std::size_t hash = 0;
            optimizationtools::hash_combine(hash, std::hash<ItemTypeId>{}(pair.first));
            optimizationtools::hash_combine(hash, std::hash<ItemTypeId>{}(pair.second));
            return hash;
        }
    };
    std::unordered_map<ItemTypeIdPair, Value, ItemTypeIdPairHasher> pair_weight;
    std::unordered_map<ItemTypeIdPair, std::vector<ItemPos>, ItemTypeIdPairHasher> pair_columns;
    for (ItemPos column_pos = 0; column_pos < (ItemPos)column_items.size(); ++column_pos) {
        const ColumnItems& entry = column_items[column_pos];
        // 'column_pos' increases monotonically across this loop and
        // 'entry.item_type_ids' is sorted, so each 'item_columns' list ends
        // up sorted by construction - no separate sort needed afterwards
        // (unlike 'neighbors' below, which collects across different
        // columns in a different order).
        for (ItemTypeId item_type_id: entry.item_type_ids)
            item_columns[item_type_id].push_back(column_pos);
        for (ItemPos pos_1 = 0; pos_1 < (ItemPos)entry.item_type_ids.size(); ++pos_1) {
            for (ItemPos pos_2 = pos_1 + 1; pos_2 < (ItemPos)entry.item_type_ids.size(); ++pos_2) {
                ItemTypeId item_type_id_1 = entry.item_type_ids[pos_1];
                ItemTypeId item_type_id_2 = entry.item_type_ids[pos_2];
                neighbors[item_type_id_1].push_back(item_type_id_2);
                neighbors[item_type_id_2].push_back(item_type_id_1);
                // 'item_type_ids' is sorted, so item_type_id_1 < item_type_id_2.
                ItemTypeIdPair pair(item_type_id_1, item_type_id_2);
                pair_weight[pair] += entry.value;
                pair_columns[pair].push_back(column_pos);
            }
        }
    }
    for (std::vector<ItemTypeId>& item_neighbors: neighbors) {
        std::sort(item_neighbors.begin(), item_neighbors.end());
        item_neighbors.erase(
                std::unique(item_neighbors.begin(), item_neighbors.end()),
                item_neighbors.end());
    }

    // Generate candidate triples: for every pair (item_type_id_1,
    // item_type_id_2) directly co-occurring in some column, extend it with
    // every third item type that co-occurs with either one in *some*
    // (possibly different) column. A triple's violation only needs a
    // column to cover 2 of its 3 item types, not all 3, so this also
    // catches triples whose pairwise evidence is split across different
    // columns (e.g. item_type_id_1/item_type_id_2 co-packed in one column,
    // item_type_id_1/item_type_id_3 in another) - which enumerating only
    // full triples already present within a single column would miss.
    std::set<std::array<ItemTypeId, 3>> candidate_triples;
    bool candidates_capped = false;
    auto add_candidate = [&](ItemTypeId item_type_id_1, ItemTypeId item_type_id_2, ItemTypeId item_type_id_3)
    {
        if (item_type_id_3 == item_type_id_1 || item_type_id_3 == item_type_id_2)
            return;
        std::array<ItemTypeId, 3> triple = {item_type_id_1, item_type_id_2, item_type_id_3};
        std::sort(triple.begin(), triple.end());
        candidate_triples.insert(triple);
        if ((ItemPos)candidate_triples.size() >= maximum_number_of_candidate_triples)
            candidates_capped = true;
    };
    for (const ColumnItems& entry: column_items) {
        if (candidates_capped)
            break;
        for (ItemPos pos_1 = 0;
                pos_1 < (ItemPos)entry.item_type_ids.size() && !candidates_capped;
                ++pos_1) {
            for (ItemPos pos_2 = pos_1 + 1;
                    pos_2 < (ItemPos)entry.item_type_ids.size() && !candidates_capped;
                    ++pos_2) {
                ItemTypeId item_type_id_1 = entry.item_type_ids[pos_1];
                ItemTypeId item_type_id_2 = entry.item_type_ids[pos_2];
                for (ItemTypeId item_type_id_3: neighbors[item_type_id_1]) {
                    add_candidate(item_type_id_1, item_type_id_2, item_type_id_3);
                    if (candidates_capped)
                        break;
                }
                for (ItemTypeId item_type_id_3: neighbors[item_type_id_2]) {
                    if (candidates_capped)
                        break;
                    add_candidate(item_type_id_1, item_type_id_2, item_type_id_3);
                }
            }
        }
    }

    // Compute the violation of each candidate triple {a, b, c} (a < b < c)
    // as pair_weight(a,b) + pair_weight(a,c) + pair_weight(b,c) - 2 *
    // triple_weight(a,b,c), where triple_weight is the total value of
    // columns containing all 3 (obtained via a 3-way merge of the -
    // already sorted - 'pair_columns' index lists, rather than rescanning
    // every column). This is exactly the inclusion-exclusion count of
    // "columns containing >= 2 of the 3": a column with exactly 2 present
    // is counted once (by the one relevant pair_weight term), a column
    // with all 3 present is counted 3 times by the pair terms and then
    // corrected back down to 1 by the subtraction. The triple's cut (sum
    // <= 1) is violated iff that exceeds 1.
    //
    // No need to skip triples already covered by an active cut: 'solution'
    // is the master LP's current (feasible) relaxation, which already
    // enforces every active cut's row to within its feasibility tolerance,
    // and this violation is exactly that row's value minus its upper bound
    // (same 'coefficient' definition, same columns) - so an already-active
    // triple's violation comes out <= 0 here on its own. Re-adding it would
    // only ever produce a redundant row, and 'columngenerationsolver'
    // already relies on 'PricingSolver::equal' (not on 'separate_cuts'
    // never repeating itself) to recognize a cut it previously removed for
    // being inactive.
    auto pair_weight_of = [&pair_weight](ItemTypeId item_type_id_1, ItemTypeId item_type_id_2) -> Value
    {
        auto it = pair_weight.find({item_type_id_1, item_type_id_2});
        return (it == pair_weight.end())? 0.0: it->second;
    };
    static const std::vector<ItemPos> empty_columns;
    auto pair_columns_of = [&pair_columns](ItemTypeId item_type_id_1, ItemTypeId item_type_id_2) -> const std::vector<ItemPos>&
    {
        auto it = pair_columns.find({item_type_id_1, item_type_id_2});
        return (it == pair_columns.end())? empty_columns: it->second;
    };

    struct ViolatedSubset
    {
        std::vector<ItemTypeId> item_type_ids;
        Value violation;
    };
    std::vector<ViolatedSubset> violated_subsets;
    for (const std::array<ItemTypeId, 3>& triple: candidate_triples) {
        ItemTypeId item_type_id_a = triple[0];
        ItemTypeId item_type_id_b = triple[1];
        ItemTypeId item_type_id_c = triple[2];

        // 3-way merge intersection of the pair column-index lists to
        // compute triple_weight (the columns containing all 3).
        const std::vector<ItemPos>& columns_ab = pair_columns_of(item_type_id_a, item_type_id_b);
        const std::vector<ItemPos>& columns_ac = pair_columns_of(item_type_id_a, item_type_id_c);
        const std::vector<ItemPos>& columns_bc = pair_columns_of(item_type_id_b, item_type_id_c);
        Value triple_weight = 0.0;
        ItemPos pos_ab = 0;
        ItemPos pos_ac = 0;
        ItemPos pos_bc = 0;
        while (pos_ab < (ItemPos)columns_ab.size()
                && pos_ac < (ItemPos)columns_ac.size()
                && pos_bc < (ItemPos)columns_bc.size()) {
            ItemPos column_ab = columns_ab[pos_ab];
            ItemPos column_ac = columns_ac[pos_ac];
            ItemPos column_bc = columns_bc[pos_bc];
            if (column_ab == column_ac && column_ac == column_bc) {
                triple_weight += column_items[column_ab].value;
                ++pos_ab;
                ++pos_ac;
                ++pos_bc;
            } else {
                ItemPos column_max = (std::max)({column_ab, column_ac, column_bc});
                if (column_ab < column_max)
                    ++pos_ab;
                if (column_ac < column_max)
                    ++pos_ac;
                if (column_bc < column_max)
                    ++pos_bc;
            }
        }

        Value violation
            = pair_weight_of(item_type_id_a, item_type_id_b)
            + pair_weight_of(item_type_id_a, item_type_id_c)
            + pair_weight_of(item_type_id_b, item_type_id_c)
            - 2.0 * triple_weight
            - 1.0;
        if (violation > 1e-6) {
            violated_subsets.push_back({
                    {item_type_id_a, item_type_id_b, item_type_id_c},
                    violation});
        }
    }

    // Cardinality-5/7 passes (see 'generalized_subset_row_cardinalities'
    // and the class-level doc comment on this function): grow every
    // co-occurring pair, one item type at a time, into a subset of the
    // target cardinality, always adding whichever co-occurrence neighbor
    // of the current subset maximizes its resulting violation. Evaluated
    // by direct column scan ('subset_violation' below) rather than the
    // pairwise inclusion-exclusion trick above: that trick is specific to
    // counting "exactly 2 vs exactly 3" of a *triple*, it does not
    // generalize to distinguishing the coefficient 0/1/2/3 bands of a
    // larger subset.
    //
    // Seeded from pairs, not from the strongest candidate triples: ranking
    // triples by their own combined pairwise weight is biased toward
    // "triangles" (all 3 pairs co-occurring), but a violated larger subset
    // need not contain any triangle at all - e.g. a cycle of item types
    // where only consecutive pairs co-occur has every one of its own
    // triples scoring worse than an unrelated triangle elsewhere, even
    // though the full subset is the one that is actually violated. Seeding
    // from every co-occurring pair instead (there are typically far fewer
    // distinct pairs than triples, so this stays cheap) avoids that bias:
    // every edge of such a cycle is its own seed.
    const std::array<ItemPos, 2> generalized_subset_row_cardinalities = {5, 7};
    const ItemPos maximum_number_of_extension_seeds = 200;
    // 'k'-way merge (k == 'subset.size()', 5 or 7) over 'item_columns'
    // instead of a rescan of every active column: mirrors the pairwise
    // merge 'pair_columns_of' above already uses for the cardinality-3
    // pass, generalized from 2 lists to k. 'positions[i]' is how far
    // 'item_columns[subset[i]]' has been consumed so far; each iteration
    // advances every list currently at the smallest not-yet-consumed
    // column index (there may be several, if that column contains more
    // than one of 'subset''s item types), so every iteration retires at
    // least one list entry - total iterations, and total work (each
    // iteration does O(k) work to find the minimum and advance matching
    // lists), are bounded by the combined length of the k lists, i.e. by
    // how often 'subset''s own item types actually appear, not by how
    // many active columns exist overall (most of which have no overlap
    // with 'subset' at all and would contribute nothing to a full rescan
    // anyway).
    auto subset_violation = [&column_items, &item_columns](const std::vector<ItemTypeId>& subset) -> Value
    {
        std::size_t subset_size = subset.size();
        std::vector<std::size_t> positions(subset_size, 0);
        Value lhs = 0.0;
        while (true) {
            ItemPos min_column = -1;
            for (std::size_t i = 0; i < subset_size; ++i) {
                const std::vector<ItemPos>& list = item_columns[subset[i]];
                if (positions[i] < list.size()
                        && (min_column == -1 || list[positions[i]] < min_column)) {
                    min_column = list[positions[i]];
                }
            }
            if (min_column == -1)
                break;
            int count = 0;
            for (std::size_t i = 0; i < subset_size; ++i) {
                const std::vector<ItemPos>& list = item_columns[subset[i]];
                if (positions[i] < list.size() && list[positions[i]] == min_column) {
                    ++count;
                    ++positions[i];
                }
            }
            if (count >= 2)
                lhs += (Value)(count / 2) * column_items[min_column].value;
        }
        return lhs - (Value)(subset.size() / 2);
    };
    struct PairScore
    {
        ItemTypeId item_type_id_1;
        ItemTypeId item_type_id_2;
        Value weight;
    };
    std::vector<PairScore> pair_scores;
    pair_scores.reserve(pair_weight.size());
    for (const auto& p: pair_weight)
        pair_scores.push_back({p.first.first, p.first.second, p.second});
    std::sort(
            pair_scores.begin(),
            pair_scores.end(),
            [](const PairScore& pair_score_1, const PairScore& pair_score_2)
            {
                return pair_score_1.weight > pair_score_2.weight;
            });
    if ((ItemPos)pair_scores.size() > maximum_number_of_extension_seeds)
        pair_scores.resize(maximum_number_of_extension_seeds);
    // Beam width for the growth itself: pure greedy (width 1) reliably gets
    // trapped on a tie among several "clique-like" candidates (e.g. three
    // item types that all pairwise co-occur with the seed, none of them
    // pairwise with each other) that score equally well one step ahead but
    // lead nowhere, while the extension actually needed to reach a violated
    // quintuple scores no better at that same first step - a real example
    // from the fractional point this cardinality-5 extension targets. A
    // small beam recovers this: even if the needed extension is one of
    // several tied candidates, it survives into the next step instead of
    // being discarded for not going first in the tie.
    const ItemPos extension_beam_width = 10;
    for (const PairScore& seed: pair_scores) {
        std::vector<std::vector<ItemTypeId>> beam = {{seed.item_type_id_1, seed.item_type_id_2}};
        // Grow the same beam through every target cardinality in increasing
        // order (5, then 7), checking for violations at each checkpoint
        // along the way: the first 3 growth steps (2 -> 5) are shared work
        // for both, no need to restart from the pair for cardinality 7.
        for (ItemPos target_cardinality: generalized_subset_row_cardinalities) {
            while ((ItemPos)beam.front().size() < target_cardinality) {
                std::vector<std::pair<std::vector<ItemTypeId>, Value>> candidates;
                std::set<std::vector<ItemTypeId>> seen;
                for (const std::vector<ItemTypeId>& partial: beam) {
                    std::set<ItemTypeId> candidate_pool;
                    for (ItemTypeId item_type_id: partial) {
                        for (ItemTypeId neighbor: neighbors[item_type_id])
                            candidate_pool.insert(neighbor);
                    }
                    for (ItemTypeId item_type_id: partial)
                        candidate_pool.erase(item_type_id);
                    for (ItemTypeId candidate: candidate_pool) {
                        std::vector<ItemTypeId> trial = partial;
                        trial.push_back(candidate);
                        std::sort(trial.begin(), trial.end());
                        if (!seen.insert(trial).second)
                            continue;
                        candidates.push_back({trial, subset_violation(trial)});
                    }
                }
                // No co-occurrence neighbor left to grow into from any
                // subset in the beam: stop here, this seed will be
                // discarded below (and skipped for any larger target
                // cardinality still to come) for not reaching this one.
                if (candidates.empty())
                    break;
                std::sort(
                        candidates.begin(),
                        candidates.end(),
                        [](const std::pair<std::vector<ItemTypeId>, Value>& candidate_1, const std::pair<std::vector<ItemTypeId>, Value>& candidate_2)
                        {
                            return candidate_1.second > candidate_2.second;
                        });
                if ((ItemPos)candidates.size() > extension_beam_width)
                    candidates.resize(extension_beam_width);
                beam.clear();
                for (auto& candidate: candidates)
                    beam.push_back(std::move(candidate.first));
            }
            if ((ItemPos)beam.front().size() != target_cardinality)
                break;
            for (const std::vector<ItemTypeId>& subset: beam) {
                Value violation = subset_violation(subset);
                if (violation > 1e-6)
                    violated_subsets.push_back({subset, violation});
            }
        }
    }

    std::sort(
            violated_subsets.begin(),
            violated_subsets.end(),
            [](const ViolatedSubset& violated_subset_1, const ViolatedSubset& violated_subset_2)
            {
                return violated_subset_1.violation > violated_subset_2.violation;
            });

    std::vector<std::shared_ptr<const Cut>> new_cuts;
    for (const ViolatedSubset& violated_subset: violated_subsets) {
        if ((CutIdx)new_cuts.size() >= maximum_number_of_cuts)
            break;
        new_cuts.push_back(build_subset_row_cut(violated_subset.item_type_ids));
    }
    return new_cuts;
}

template <typename Instance, typename InstanceBuilder, typename Solution, typename Output>
Value ColumnGenerationPricingSolver<Instance, InstanceBuilder, Solution, Output>::coefficient(
        const Cut& cut,
        const Column& column) const
{
    // Only ever builds subset-row cuts - see 'build_subset_row_cut' above.
    const SubsetRowCutExtra& extra = *std::static_pointer_cast<SubsetRowCutExtra>(cut.extra);
    const Solution& extra_solution = *std::static_pointer_cast<Solution>(column.extra);
    int number_of_items_present = 0;
    for (ItemTypeId item_type_id: extra.item_type_ids) {
        if (extra_solution.item_copies(item_type_id) > 0)
            number_of_items_present++;
    }
    int coefficient = number_of_items_present / 2;
    return (Value)coefficient;
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

    /**
     * Enable subset-row cutting planes (see 'SubsetRowCutExtra' and
     * 'ColumnGenerationPricingSolver::separate_cuts'/'solve_pricing' above)
     * at the root of the limited discrepancy search. Off by default: cuts
     * only tighten the bound if the pricing solver also enforces them
     * during search, otherwise they only add master LP overhead for
     * comparatively little benefit.
     */
    int use_cutting_planes = 0;

    /**
     * Whether 'pricing_function' has something genuinely useful to offer
     * for 'columngenerationsolver::PricingSolver::PricingType::Dual' calls
     * - i.e. whether it actually branches on the 'PricingType' it is given
     * to run a more expensive, exact algorithm there instead of its
     * regular cheap search (e.g. rectangle forcing a Benders-decomposition
     * solve - see 'rectangle::optimize_column_generation'). Off by
     * default: passed straight through to 'get_model', which only grants
     * the pricing solver a genuine 'has_pricing_bound' when this is set,
     * so 'columngenerationsolver::column_generation' never wastes a
     * 'PricingType::Dual' call on a 'pricing_function' that would just
     * redo its regular search.
     */
    bool pricing_function_has_dual_bound = false;
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
 *
 * 'initial_columns'/'initial_cuts': unlike 'column_pool' above (checked
 * lazily, only once the master LP actually needs a new entry), these seed
 * the root master LP and root active cut set directly from the start (no
 * analogous cross-call pool exists for cuts on its own - a cut is either
 * currently active or forgotten, never kept around inactive on the chance
 * it is needed again later; see 'columngenerationsolver::Parameters::
 * initial_cuts''s own doc comment). If non-null, seeded with
 * '*initial_columns'/'*initial_cuts' before solving; on return, overwritten
 * (not appended - there is only ever one root relaxation per call) with the
 * columns of this call's own root node relaxation solution and its own
 * root active cuts (see 'columngenerationsolver::
 * LimitedDiscrepancySearchOutput::root_relaxation_solution'/'root_cuts').
 * Passing the same pointers to a sequence of calls (as the sequential
 * feasibility scheme does) thus warm-starts each candidate's root
 * relaxation with what was already (near-)optimal one bin fewer, the same
 * way 'column_pool' lets pricing pick up where the previous call left off.
 * Left untouched when this call takes the sequential feasibility branch
 * itself instead of actually solving a single master LP - only a call that
 * reaches 'columngenerationsolver::limited_discrepancy_search' below has a
 * root relaxation of its own to report.
 */
template <typename Instance, typename InstanceBuilder, typename Solution, typename AlgorithmFormatter, typename Output = packingsolver::Output<Instance, Solution>>
Output column_generation(
        const Instance& instance,
        const ColumnGenerationPricingFunction<Instance, InstanceBuilder, Solution, Output>& pricing_function,
        const ColumnGenerationParameters<Instance, Solution, Output>& parameters = {},
        BinPos lower_bound = 0,
        std::vector<std::shared_ptr<const Column>>* column_pool = nullptr,
        std::vector<std::shared_ptr<const Column>>* initial_columns = nullptr,
        std::vector<std::shared_ptr<const Cut>>* initial_cuts = nullptr)
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
        // 'column_pool''s own doc comment above): every candidate shares
        // the same bin and item types, only the number of bins available
        // of each type changes. Likewise,
        // 'sequential_feasibility_initial_columns'/'..._initial_cuts' carry
        // each candidate's own root relaxation solution and root active
        // cuts forward as the *next* candidate's own seed (see
        // 'initial_columns'/'initial_cuts' above): 'column_generation'
        // both reads them (to warm-start this call) and overwrites them
        // (with this call's own root relaxation, for the call after), so
        // passing the same two vectors through the whole loop is enough -
        // no per-iteration assignment needed.
        std::vector<std::shared_ptr<const Column>> sequential_feasibility_column_pool;
        std::vector<std::shared_ptr<const Column>> sequential_feasibility_initial_columns;
        std::vector<std::shared_ptr<const Cut>> sequential_feasibility_initial_cuts;
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
            sub_parameters.use_cutting_planes = parameters.use_cutting_planes;
            sub_parameters.linear_programming_solver_name = parameters.linear_programming_solver_name;
            sub_parameters.pricing_function_has_dual_bound = parameters.pricing_function_has_dual_bound;
            Output sub_output = column_generation<Instance, InstanceBuilder, Solution, AlgorithmFormatter, Output>(
                    sub_instance, pricing_function, sub_parameters, 0,
                    &sequential_feasibility_column_pool,
                    &sequential_feasibility_initial_columns,
                    &sequential_feasibility_initial_cuts);

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
        = get_model<Instance, InstanceBuilder, Solution, Output>(
                instance, output, pricing_function, parameters.pricing_function_has_dual_bound);
    columngenerationsolver::LimitedDiscrepancySearchParameters cgslds_parameters;
    cgslds_parameters.verbosity_level = 1;
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
        // By the extended reals convention, the optimal value of an
        // infeasible problem is +inf for a minimization objective
        // (VariableSizedBinPacking, BinPacking, Feasibility) or -inf for a
        // maximization one (Knapsack, the only Maximize objective here -
        // see 'get_model''s own 'objective_sense' assignment above). This
        // can happen for any objective (e.g. not enough bin copies to
        // satisfy the item / bin count bounds, or item_type.copies_min too
        // large to fit), so it is checked once up front here, before
        // computing any objective-specific bound below - in particular
        // before 'BinPacking''s own, since converting +inf to its integer
        // 'BinPos' bound would be undefined behavior.
        bool bound_proves_infeasible = (instance.objective() == Objective::Knapsack)?
            (cgslds_output.bound == -std::numeric_limits<double>::infinity()):
            (cgslds_output.bound == std::numeric_limits<double>::infinity());
        if (bound_proves_infeasible) {
            algorithm_formatter.update_is_proven_infeasible();
        } else if (instance.objective() == Objective::VariableSizedBinPacking) {
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
        }
    };
    cgslds_parameters.column_generation_parameters.solver_name
        = parameters.linear_programming_solver_name;
    // '1': enabled at the root node only (see 'ColumnGenerationParameters::
    // use_cutting_planes' above).
    cgslds_parameters.cutting_planes = parameters.use_cutting_planes;
    if (column_pool != nullptr)
        cgslds_parameters.column_pool = *column_pool;
    if (initial_columns != nullptr)
        cgslds_parameters.initial_columns = *initial_columns;
    if (initial_cuts != nullptr)
        cgslds_parameters.initial_cuts = *initial_cuts;
    columngenerationsolver::LimitedDiscrepancySearchOutput cgslds_search_output
        = columngenerationsolver::limited_discrepancy_search(cgs_model, cgslds_parameters);
    if (column_pool != nullptr)
        *column_pool = cgslds_search_output.columns;
    if (initial_columns != nullptr) {
        initial_columns->clear();
        for (const auto& p: cgslds_search_output.root_relaxation_solution.columns())
            initial_columns->push_back(p.first);
    }
    if (initial_cuts != nullptr)
        *initial_cuts = cgslds_search_output.root_cuts;

    algorithm_formatter.end();
    return output;
}

}
