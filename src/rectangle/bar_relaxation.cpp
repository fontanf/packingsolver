#include "rectangle/bar_relaxation.hpp"

#include "packingsolver/rectangle/algorithm_formatter.hpp"
#include "packingsolver/onedimensional/instance_builder.hpp"
#include "packingsolver/onedimensional/optimize.hpp"

#include "columngenerationsolver/algorithms/column_generation.hpp"

using namespace packingsolver;
using namespace packingsolver::rectangle;

namespace
{

using VariantId = int32_t;
using RowIdx = columngenerationsolver::RowIdx;

/**
 * An item type, possibly split into two "oriented variants" if it can
 * rotate: one variant per orientation the item may be packed in, each with
 * its own (width, height).
 */
struct OrientedVariant
{
    ItemTypeId item_type_id;
    Length width;
    Length height;
};

class BarRelaxationPricingSolver: public columngenerationsolver::PricingSolver
{

public:

    BarRelaxationPricingSolver(
            const Instance& instance,
            const std::vector<OrientedVariant>& variants,
            const std::vector<std::vector<VariantId>>& eligible_variants,
            const std::vector<RowIdx>& row_cap_row,
            const std::vector<RowIdx>& col_cap_row,
            const std::vector<std::vector<RowIdx>>& link1_row,
            const std::vector<std::vector<RowIdx>>& link2_row):
        instance_(instance),
        variants_(variants),
        eligible_variants_(eligible_variants),
        row_cap_row_(row_cap_row),
        col_cap_row_(col_cap_row),
        link1_row_(link1_row),
        link2_row_(link2_row)
    { }

    // 'solve_feasibility' unused: every dynamically priced column here
    // already carries a fixed 'objective_coefficient' of 0 (see
    // 'price_bars' below - only the static 'n_{i,t}'/'k_t' columns carry
    // the real objective), so this pricing search's own reduced cost is
    // identical in both phases and needs no phase-specific adjustment.
    // 'fixed_columns'/'branching_decisions'/'tabu' unused: this pricing
    // solver has neither fixed/tabu column bookkeeping nor
    // branching-decision-sensitive behavior of its own.
    columngenerationsolver::PricingSolver::PricingOutput solve_pricing(
            bool solve_feasibility,
            const std::vector<std::pair<std::shared_ptr<const columngenerationsolver::Column>, columngenerationsolver::Value>>&,
            const std::vector<std::shared_ptr<const columngenerationsolver::BranchingDecision>>&,
            const std::unordered_set<std::shared_ptr<const columngenerationsolver::Column>>&,
            const std::vector<columngenerationsolver::Value>& duals,
            const std::vector<std::pair<std::shared_ptr<const columngenerationsolver::Cut>, columngenerationsolver::Value>>&,
            columngenerationsolver::PricingSolver::PricingType) override;

private:

    /**
     * Solve the bounded 0-1 knapsack pricing problem for one bar direction
     * (row-patterns if 'link_row' is 'link1_row_' and 'capacity' is the bin
     * width, column-patterns if it is 'link2_row_' and the bin height), for
     * a single bin type. Returns the best reduced cost found (which may be
     * negative or zero, meaning no improving pattern exists), and appends a
     * new column to 'columns' when it is worth adding one.
     */
    double price_bars(
            BinTypeId bin_type_id,
            bool use_width,
            Length capacity,
            RowIdx cap_row,
            const std::vector<RowIdx>& link_row,
            const std::vector<columngenerationsolver::Value>& duals,
            const std::string& column_name_prefix,
            std::vector<std::shared_ptr<const columngenerationsolver::Column>>& columns) const;

    const Instance& instance_;
    const std::vector<OrientedVariant>& variants_;
    const std::vector<std::vector<VariantId>>& eligible_variants_;
    const std::vector<RowIdx>& row_cap_row_;
    const std::vector<RowIdx>& col_cap_row_;
    const std::vector<std::vector<RowIdx>>& link1_row_;
    const std::vector<std::vector<RowIdx>>& link2_row_;

};

double BarRelaxationPricingSolver::price_bars(
        BinTypeId bin_type_id,
        bool use_width,
        Length capacity,
        RowIdx cap_row,
        const std::vector<RowIdx>& link_row,
        const std::vector<columngenerationsolver::Value>& duals,
        const std::string& column_name_prefix,
        std::vector<std::shared_ptr<const columngenerationsolver::Column>>& columns) const
{
    // Every column priced here has objective coefficient 0 (only the static
    // 'n_{i,t}'/'k_t' columns carry the real objective), so its reduced cost
    // is '-duals[cap_row] - sum_i a_iq * duals[link_row[i]]'. For a Maximize
    // model (Knapsack), the best column maximizes 'sum_i a_iq * (-duals[link_row[i]])';
    // for a Minimize model (Feasibility), the best (most negative) one
    // maximizes 'sum_i a_iq * duals[link_row[i]]' instead - the same sign
    // convention 'algorithms/column_generation.hpp' uses (un-negated duals
    // for Minimize objectives, 'profit - dual' for Maximize).
    onedimensional::InstanceBuilder kp_instance_builder;
    kp_instance_builder.set_objective(Objective::Knapsack);
    kp_instance_builder.add_bin_type(capacity);
    std::vector<VariantId> kp_to_variant;
    for (VariantId variant_id: eligible_variants_[bin_type_id]) {
        double profit = 0.0;
        switch (instance_.objective()) {
        case Objective::Knapsack: {
            profit = -duals[link_row[variant_id]];
            break;
        } case Objective::Feasibility:
          case Objective::BinPacking:
          case Objective::VariableSizedBinPacking: {
            profit = duals[link_row[variant_id]];
            break;
        } default: {
            throw std::invalid_argument(FUNC_SIGNATURE);
        }
        }
        // A zero or negative profit item can never improve the knapsack
        // objective whether selected or not, so it is simply omitted here,
        // the same way 'conservative_scales.cpp' skips them.
        if (profit <= 0.0)
            continue;
        const OrientedVariant& variant = variants_[variant_id];
        const ItemType& item_type = instance_.item_type(variant.item_type_id);
        Length length = (use_width)? variant.width: variant.height;
        ItemTypeId kp_item_type_id = kp_instance_builder.add_item_type(length);
        kp_instance_builder.set_item_type_profit(kp_item_type_id, profit);
        kp_instance_builder.set_item_type_copies(kp_item_type_id, item_type.copies);
        kp_to_variant.push_back(variant_id);
    }

    // Recompute the achieved value (and the per-variant selected copy
    // count) ourselves, in our own precision, rather than trusting the
    // knapsack solve's own profit accounting (same rationale as
    // 'conservative_scales.cpp').
    std::vector<ItemPos> selected_copies(variants_.size(), 0);
    double achieved = 0.0;
    if (!kp_to_variant.empty()) {
        onedimensional::Instance kp_instance = kp_instance_builder.build();
        onedimensional::OptimizeParameters kp_parameters;
        kp_parameters.verbosity_level = 0;
        kp_parameters.optimization_mode = OptimizationMode::NotAnytime;
        onedimensional::Output kp_output = onedimensional::optimize(kp_instance, kp_parameters);
        const onedimensional::Solution& kp_solution = kp_output.solution_pool.best();
        for (ItemTypeId kp_item_type_id = 0;
                kp_item_type_id < (ItemTypeId)kp_to_variant.size();
                ++kp_item_type_id) {
            ItemPos copies = kp_solution.item_copies(kp_item_type_id);
            if (copies <= 0)
                continue;
            VariantId variant_id = kp_to_variant[kp_item_type_id];
            selected_copies[variant_id] = copies;
            double profit = 0.0;
            switch (instance_.objective()) {
            case Objective::Knapsack: {
                profit = -duals[link_row[variant_id]];
                break;
            } case Objective::Feasibility:
              case Objective::BinPacking:
              case Objective::VariableSizedBinPacking: {
                profit = duals[link_row[variant_id]];
                break;
            } default: {
                throw std::invalid_argument(FUNC_SIGNATURE);
            }
            }
            achieved += copies * profit;
        }
    }

    double reduced_cost = 0.0;
    switch (instance_.objective()) {
    case Objective::Knapsack: {
        reduced_cost = achieved - duals[cap_row];
        break;
    } case Objective::Feasibility:
      case Objective::BinPacking:
      case Objective::VariableSizedBinPacking: {
        reduced_cost = -achieved - duals[cap_row];
        break;
    } default: {
        throw std::invalid_argument(FUNC_SIGNATURE);
    }
    }

    columngenerationsolver::Column column;
    column.name = column_name_prefix + std::to_string(bin_type_id);
    column.type = columngenerationsolver::VariableType::Continuous;
    column.lower_bound = 0.0;
    column.upper_bound = std::numeric_limits<double>::infinity();
    column.objective_coefficient = 0.0;
    columngenerationsolver::LinearTerm cap_term;
    cap_term.row = cap_row;
    cap_term.coefficient = 1.0;
    column.elements.push_back(cap_term);
    for (VariantId variant_id: kp_to_variant) {
        if (selected_copies[variant_id] <= 0)
            continue;
        columngenerationsolver::LinearTerm term;
        term.row = link_row[variant_id];
        term.coefficient = (double)selected_copies[variant_id];
        column.elements.push_back(term);
    }
    columns.push_back(std::shared_ptr<const columngenerationsolver::Column>(
            new columngenerationsolver::Column(column)));

    return reduced_cost;
}

columngenerationsolver::PricingSolver::PricingOutput BarRelaxationPricingSolver::solve_pricing(
        bool,
        const std::vector<std::pair<std::shared_ptr<const columngenerationsolver::Column>, columngenerationsolver::Value>>&,
        const std::vector<std::shared_ptr<const columngenerationsolver::BranchingDecision>>&,
        const std::unordered_set<std::shared_ptr<const columngenerationsolver::Column>>&,
        const std::vector<columngenerationsolver::Value>& duals,
        const std::vector<std::pair<std::shared_ptr<const columngenerationsolver::Cut>, columngenerationsolver::Value>>&,
        columngenerationsolver::PricingSolver::PricingType)
{
    columngenerationsolver::PricingSolver::PricingOutput output;
    double overcost = 0.0;

    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance_.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance_.bin_type(bin_type_id);
        if (bin_type.copies <= 0 || eligible_variants_[bin_type_id].empty())
            continue;

        double reduced_cost_row = price_bars(
                bin_type_id,
                /* use_width = */ true,
                bin_type.rect.x,
                row_cap_row_[bin_type_id],
                link1_row_[bin_type_id],
                duals,
                "row_pattern_",
                output.columns);
        // Any feasible LP solution has 'sum_q y_q^t <= H_t * copies_t' (the
        // row-cap row), so no single row-pattern column can ever be worth
        // more (Knapsack, Maximize) or cost less (Feasibility, Minimize)
        // than that, in the whole feasible region - not just at the current
        // relaxation solution.
        switch (instance_.objective()) {
        case Objective::Knapsack: {
            overcost += (std::max)(0.0, reduced_cost_row)
                * (double)bin_type.rect.y * (double)bin_type.copies;
            break;
        } case Objective::Feasibility:
          case Objective::BinPacking:
          case Objective::VariableSizedBinPacking: {
            overcost += (std::min)(0.0, reduced_cost_row)
                * (double)bin_type.rect.y * (double)bin_type.copies;
            break;
        } default: {
            throw std::invalid_argument(FUNC_SIGNATURE);
        }
        }

        double reduced_cost_col = price_bars(
                bin_type_id,
                /* use_width = */ false,
                bin_type.rect.y,
                col_cap_row_[bin_type_id],
                link2_row_[bin_type_id],
                duals,
                "col_pattern_",
                output.columns);
        switch (instance_.objective()) {
        case Objective::Knapsack: {
            overcost += (std::max)(0.0, reduced_cost_col)
                * (double)bin_type.rect.x * (double)bin_type.copies;
            break;
        } case Objective::Feasibility:
          case Objective::BinPacking:
          case Objective::VariableSizedBinPacking: {
            overcost += (std::min)(0.0, reduced_cost_col)
                * (double)bin_type.rect.x * (double)bin_type.copies;
            break;
        } default: {
            throw std::invalid_argument(FUNC_SIGNATURE);
        }
        }
    }

    output.overcost = overcost;
    return output;
}

}

BarRelaxationOutput packingsolver::rectangle::bar_relaxation(
        const Instance& instance,
        const BarRelaxationParameters& parameters)
{
    if (instance.objective() != Objective::Knapsack
            && instance.objective() != Objective::Feasibility
            && instance.objective() != Objective::BinPacking
            && instance.objective() != Objective::VariableSizedBinPacking)
        throw std::invalid_argument(FUNC_SIGNATURE);
    // For 'BinPacking' with several bin types, the objective value (a total
    // cost) would no longer convert to a bin *count* by a single division:
    // that conversion (see the 'BinPacking' case in the final callback
    // below) relies on every bin costing the same amount.
    if (instance.objective() == Objective::BinPacking
            && instance.number_of_bin_types() != 1)
        throw std::invalid_argument(FUNC_SIGNATURE);

    BinTypeId number_of_bin_types = instance.number_of_bin_types();
    ItemTypeId number_of_item_types = instance.number_of_item_types();

    BarRelaxationOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    // Build the oriented variants: one per item type, plus a second one for
    // item types that may rotate.
    std::vector<OrientedVariant> variants;
    for (ItemTypeId item_type_id = 0; item_type_id < number_of_item_types; ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        variants.push_back({item_type_id, item_type.rect.x, item_type.rect.y});
        if (!item_type.oriented)
            variants.push_back({item_type_id, item_type.rect.y, item_type.rect.x});
    }
    VariantId number_of_variants = variants.size();

    // For each bin type, the variants that fit it (and are eligible for it).
    std::vector<std::vector<VariantId>> eligible_variants(number_of_bin_types);
    for (BinTypeId bin_type_id = 0; bin_type_id < number_of_bin_types; ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        for (VariantId variant_id = 0; variant_id < number_of_variants; ++variant_id) {
            const OrientedVariant& variant = variants[variant_id];
            if (variant.width > bin_type.rect.x || variant.height > bin_type.rect.y)
                continue;
            const ItemType& item_type = instance.item_type(variant.item_type_id);
            if (item_type.eligibility_id != -1
                    && std::find(
                        bin_type.eligibility_ids.begin(),
                        bin_type.eligibility_ids.end(),
                        item_type.eligibility_id)
                    == bin_type.eligibility_ids.end()) {
                continue;
            }
            eligible_variants[bin_type_id].push_back(variant_id);
        }
    }

    columngenerationsolver::Model model;
    switch (instance.objective()) {
    case Objective::Knapsack: {
        model.objective_sense = optimizationtools::ObjectiveDirection::Maximize;
        break;
    } case Objective::Feasibility:
      case Objective::BinPacking:
      case Objective::VariableSizedBinPacking: {
        model.objective_sense = optimizationtools::ObjectiveDirection::Minimize;
        break;
    } default: {
        throw std::invalid_argument(FUNC_SIGNATURE);
    }
    }

    // Rows.
    std::vector<RowIdx> row_cap_row(number_of_bin_types);
    std::vector<RowIdx> col_cap_row(number_of_bin_types);
    std::vector<std::vector<RowIdx>> link1_row(
            number_of_bin_types,
            std::vector<RowIdx>(number_of_variants, -1));
    std::vector<std::vector<RowIdx>> link2_row(
            number_of_bin_types,
            std::vector<RowIdx>(number_of_variants, -1));
    std::vector<RowIdx> item_row(number_of_item_types);

    for (BinTypeId bin_type_id = 0; bin_type_id < number_of_bin_types; ++bin_type_id) {
        // These rows mix a static column (the relaxed bin count, with a
        // negative coefficient) with generated bar-pattern columns (with a
        // positive coefficient): 'coefficient_lower_bound' is set to a
        // negative value to disable the column-generation solver's "row
        // already saturated by fixed columns" shortcut, which assumes a row
        // can only be driven towards its upper bound by generated columns -
        // not true here, and this model never fixes any column anyway (no
        // branch-and-price is performed on it), so the shortcut would only
        // ever misfire, never help.
        columngenerationsolver::Row row_cap;
        row_cap.name = "row_cap_" + std::to_string(bin_type_id);
        row_cap.lower_bound = -std::numeric_limits<double>::infinity();
        row_cap.upper_bound = 0.0;
        row_cap.coefficient_lower_bound = -1.0;
        row_cap_row[bin_type_id] = model.rows.size();
        model.rows.push_back(row_cap);

        columngenerationsolver::Row col_cap;
        col_cap.name = "col_cap_" + std::to_string(bin_type_id);
        col_cap.lower_bound = -std::numeric_limits<double>::infinity();
        col_cap.upper_bound = 0.0;
        col_cap.coefficient_lower_bound = -1.0;
        col_cap_row[bin_type_id] = model.rows.size();
        model.rows.push_back(col_cap);

        for (VariantId variant_id: eligible_variants[bin_type_id]) {
            const ItemType& item_type = instance.item_type(variants[variant_id].item_type_id);

            columngenerationsolver::Row link1;
            link1.name = "link1_" + std::to_string(bin_type_id) + "_" + std::to_string(variant_id);
            link1.lower_bound = 0.0;
            link1.upper_bound = 0.0;
            link1.coefficient_lower_bound = -1.0;
            link1.coefficient_upper_bound = (double)item_type.copies;
            link1_row[bin_type_id][variant_id] = model.rows.size();
            model.rows.push_back(link1);

            columngenerationsolver::Row link2;
            link2.name = "link2_" + std::to_string(bin_type_id) + "_" + std::to_string(variant_id);
            link2.lower_bound = 0.0;
            link2.upper_bound = 0.0;
            link2.coefficient_lower_bound = -1.0;
            link2.coefficient_upper_bound = (double)item_type.copies;
            link2_row[bin_type_id][variant_id] = model.rows.size();
            model.rows.push_back(link2);
        }
    }
    for (ItemTypeId item_type_id = 0; item_type_id < number_of_item_types; ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        columngenerationsolver::Row row;
        row.name = "item_" + std::to_string(item_type_id);
        // Knapsack: at least 'copies_min' (0 unless explicitly forced) and at
        // most 'copies' (a subset may be left unpacked).
        // Feasibility/BinPacking/VariableSizedBinPacking: 'copies_min' equals
        // 'copies' (every item must be packed), so this is still exact.
        row.lower_bound = (double)item_type.copies_min;
        row.upper_bound = (double)item_type.copies;
        row.coefficient_lower_bound = -1.0;
        row.coefficient_upper_bound = (double)item_type.copies;
        item_row[item_type_id] = model.rows.size();
        model.rows.push_back(row);
    }

    // Static columns: the relaxed bin count 'k_t' for each bin type, and the
    // relaxed item-copies-assigned-to-bin-type 'n_{i,t}' for each bin type /
    // eligible variant pair.
    for (BinTypeId bin_type_id = 0; bin_type_id < number_of_bin_types; ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);

        columngenerationsolver::Column k_column;
        k_column.name = "k_" + std::to_string(bin_type_id);
        k_column.type = columngenerationsolver::VariableType::Continuous;
        k_column.lower_bound = (double)bin_type.copies_min;
        k_column.upper_bound = (double)bin_type.copies;
        // Knapsack/Feasibility: bin usage costs nothing, only item selection
        // (Knapsack) or the constraint system (Feasibility) matters.
        // BinPacking/VariableSizedBinPacking: minimize the total bin cost.
        switch (instance.objective()) {
        case Objective::Knapsack:
          case Objective::Feasibility: {
            k_column.objective_coefficient = 0.0;
            break;
        } case Objective::BinPacking:
          case Objective::VariableSizedBinPacking: {
            k_column.objective_coefficient = bin_type.cost;
            break;
        } default: {
            throw std::invalid_argument(FUNC_SIGNATURE);
        }
        }
        {
            // A row-pattern (capacity = bin width) is a horizontal
            // unit-height strip: there are as many of them per bin as the
            // bin is tall.
            columngenerationsolver::LinearTerm term;
            term.row = row_cap_row[bin_type_id];
            term.coefficient = -(double)bin_type.rect.y;
            k_column.elements.push_back(term);
        }
        {
            // Symmetrically, a column-pattern (capacity = bin height) is a
            // vertical unit-width strip: there are as many of them per bin
            // as the bin is wide.
            columngenerationsolver::LinearTerm term;
            term.row = col_cap_row[bin_type_id];
            term.coefficient = -(double)bin_type.rect.x;
            k_column.elements.push_back(term);
        }
        model.static_columns.push_back(std::shared_ptr<const columngenerationsolver::Column>(
                new columngenerationsolver::Column(k_column)));

        for (VariantId variant_id: eligible_variants[bin_type_id]) {
            const OrientedVariant& variant = variants[variant_id];
            const ItemType& item_type = instance.item_type(variant.item_type_id);

            columngenerationsolver::Column n_column;
            n_column.name = "n_" + std::to_string(bin_type_id) + "_" + std::to_string(variant_id);
            n_column.type = columngenerationsolver::VariableType::Continuous;
            n_column.lower_bound = 0.0;
            n_column.upper_bound = (double)item_type.copies;
            // Knapsack maximizes profit; the other objectives have no use
            // for item selection in the objective itself (Feasibility has
            // nothing to optimize, BinPacking/VariableSizedBinPacking only
            // cost 'k_t' for the bins used).
            switch (instance.objective()) {
            case Objective::Knapsack: {
                n_column.objective_coefficient = item_type.profit;
                break;
            } case Objective::Feasibility:
              case Objective::BinPacking:
              case Objective::VariableSizedBinPacking: {
                n_column.objective_coefficient = 0.0;
                break;
            } default: {
                throw std::invalid_argument(FUNC_SIGNATURE);
            }
            }
            {
                // A row-pattern (capacity = bin width) is a horizontal
                // unit-height strip, so a placed copy of this variant spans
                // as many of them as its height.
                columngenerationsolver::LinearTerm term;
                term.row = link1_row[bin_type_id][variant_id];
                term.coefficient = -(double)variant.height;
                n_column.elements.push_back(term);
            }
            {
                // Symmetrically, a column-pattern (capacity = bin height) is
                // a vertical unit-width strip, spanned as many times as this
                // variant's width.
                columngenerationsolver::LinearTerm term;
                term.row = link2_row[bin_type_id][variant_id];
                term.coefficient = -(double)variant.width;
                n_column.elements.push_back(term);
            }
            {
                columngenerationsolver::LinearTerm term;
                term.row = item_row[variant.item_type_id];
                term.coefficient = 1.0;
                n_column.elements.push_back(term);
            }
            model.static_columns.push_back(std::shared_ptr<const columngenerationsolver::Column>(
                    new columngenerationsolver::Column(n_column)));
        }
    }

    model.pricing_solver = std::unique_ptr<columngenerationsolver::PricingSolver>(
            new BarRelaxationPricingSolver(
                instance,
                variants,
                eligible_variants,
                row_cap_row,
                col_cap_row,
                link1_row,
                link2_row));

    columngenerationsolver::ColumnGenerationParameters cgs_parameters;
    cgs_parameters.verbosity_level = 0;
    cgs_parameters.timer = parameters.timer;
    cgs_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    cgs_parameters.solver_name = parameters.linear_programming_solver_name;
    cgs_parameters.new_bound_callback = [&instance, &algorithm_formatter](
            const columngenerationsolver::Output& cgs_output)
    {
        // By the extended reals convention, the optimal value of an
        // infeasible problem is +inf for a minimization objective
        // (VariableSizedBinPacking, BinPacking, Feasibility) or -inf for a
        // maximization one (Knapsack, the only Maximize objective here).
        // This can happen for any objective (e.g. not enough bin copies to
        // satisfy the item / bin count bounds, or item_type.copies_min too
        // large to fit), so it is checked once up front here, before
        // computing any objective-specific bound below - in particular
        // before BinPacking's own, since converting +inf to its integer
        // 'BinPos' bound would be undefined behavior.
        bool bound_proves_infeasible = (instance.objective() == Objective::Knapsack)?
            (cgs_output.bound == -std::numeric_limits<double>::infinity()):
            (cgs_output.bound == std::numeric_limits<double>::infinity());
        if (bound_proves_infeasible) {
            algorithm_formatter.update_is_proven_infeasible();
            return;
        }
        switch (instance.objective()) {
        case Objective::Knapsack: {
            // This bound ignores resources entirely. A 'penalize' resource
            // with a negative penalty *increases* the reported profit when
            // triggered (see 'Resource'), so add back the worst case -
            // every such resource triggering at once - to keep the bound
            // valid.
            algorithm_formatter.update_knapsack_bound(
                    cgs_output.bound + negative_penalty_sum(instance));
            break;
        } case Objective::Feasibility: {
            break;
        } case Objective::VariableSizedBinPacking: {
            algorithm_formatter.update_variable_sized_bin_packing_bound(cgs_output.bound);
            break;
        } case Objective::BinPacking: {
            // A single bin type (enforced above), so the total-cost bound
            // converts to a bin-count bound by dividing by that one cost -
            // the same formula 'algorithms/column_generation.hpp' uses for
            // the analogous conversion. The '-0.001' guards against
            // floating-point noise from the LP solve nudging the ceiling up
            // past the true value, which would make the bound unsound.
            BinPos bound = (BinPos)std::ceil(
                    cgs_output.bound / instance.bin_type(0).cost - 0.001);
            algorithm_formatter.update_bin_packing_bound(bound);
            break;
        } default: {
            throw std::invalid_argument(FUNC_SIGNATURE);
        }
        }
    };
    columngenerationsolver::column_generation(model, cgs_parameters);

    algorithm_formatter.end();
    return output;
}
