#include "rectangle/conservative_scales.hpp"

#include "packingsolver/rectangle/algorithm_formatter.hpp"
#include "packingsolver/onedimensional/instance_builder.hpp"
#include "packingsolver/onedimensional/optimize.hpp"

#include "mathoptsolverscmake/mathopt.hpp"
#ifdef HIGHS_FOUND
#include "mathoptsolverscmake/mathopt_highs.hpp"
#endif

#include <cmath>

using namespace packingsolver;
using namespace packingsolver::rectangle;

namespace
{

/**
 * Solve one of the two alternating rescaling LPs (equations (21)/(22) of
 * Belov, Kartak, Rohling & Scheithauer 2013, as used in Cote, Haouari &
 * Iori's 'L_BKRS') via a cutting-plane loop.
 *
 * The LP maximizes 'sum_j objective_weights[j] * w_j' subject to
 * 'sum_{j in S} w_j <= capacity' for every subset S of item types (with
 * multiplicities up to their 'copies') whose *original* dimensions
 * ('original_dims') sum to at most 'capacity'. There are exponentially many
 * such constraints, but the most violated one for a candidate solution is
 * found by solving a 0/1 knapsack (maximize the candidate's rescaled
 * dimensions as "value", subject to the original dimensions as "weight"
 * staying within 'capacity') - if its optimal value exceeds 'capacity', that
 * knapsack solution is the violated subset; if not, the candidate is already
 * optimal for the full constraint set.
 *
 * Returns the rescaled dimension for every item type (0 for item types that
 * do not individually fit 'capacity' - they can never appear in any
 * feasible packing along this axis, so excluding them avoids an unbounded
 * LP).
 */
std::vector<double> solve_conservative_scale_lp(
        const Instance& instance,
        const std::vector<Length>& original_dims,
        Length capacity,
        const std::vector<double>& objective_weights)
{
    ItemTypeId number_of_item_types = instance.number_of_item_types();
    std::vector<double> result(number_of_item_types, 0.0);

    std::vector<ItemTypeId> eligible_item_type_ids;
    for (ItemTypeId item_type_id = 0;
            item_type_id < number_of_item_types;
            ++item_type_id) {
        if (original_dims[item_type_id] <= capacity)
            eligible_item_type_ids.push_back(item_type_id);
    }
    if (eligible_item_type_ids.empty())
        return result;

    std::vector<int> variable_pos(number_of_item_types, -1);
    for (size_t pos = 0; pos < eligible_item_type_ids.size(); ++pos)
        variable_pos[eligible_item_type_ids[pos]] = (int)pos;

    mathoptsolverscmake::MathOptModel model;
    model.objective_direction = mathoptsolverscmake::ObjectiveDirection::Maximize;
    for (ItemTypeId item_type_id: eligible_item_type_ids) {
        model.variables_lower_bounds.push_back(0.0);
        // The singleton {item_type_id} always belongs to the constraint
        // family (its own original dimension trivially satisfies the
        // capacity bound), so this already encodes that particular
        // constraint as a variable bound, without needing an explicit row.
        model.variables_upper_bounds.push_back((double)capacity);
        model.variables_types.push_back(mathoptsolverscmake::VariableType::Continuous);
        model.objective_coefficients.push_back(objective_weights[item_type_id]);
    }

    for (Counter cutting_plane_iteration = 0; ; ++cutting_plane_iteration) {
        // Defensive cap only: there are finitely many distinct violated
        // subsets, each added at most once, so this loop must terminate on
        // its own well before this.
        if (cutting_plane_iteration >= 10000)
            break;

#ifdef HIGHS_FOUND
        Highs highs;
        mathoptsolverscmake::reduce_printout(highs);
        mathoptsolverscmake::load(highs, model);
        mathoptsolverscmake::solve(highs);
        std::vector<double> solution = mathoptsolverscmake::get_solution(highs);
#else
        throw std::invalid_argument(FUNC_SIGNATURE);
#endif

        // Separation: 0/1 knapsack maximizing the sum of the candidate's
        // rescaled dimensions subject to the original dimensions summing to
        // at most 'capacity'. Built and solved as a one-dimensional Knapsack
        // instance (rather than calling a knapsack solver library
        // directly), which handles item copies natively and takes the
        // rescaled dimensions directly as (double) profits, matching how
        // higher-level domains solve their own sub-problems elsewhere in
        // this codebase (e.g. 'onedimensional::optimize_dynamic_programming').
        onedimensional::InstanceBuilder kp_instance_builder;
        kp_instance_builder.set_objective(Objective::Knapsack);
        kp_instance_builder.add_bin_type(capacity);
        std::vector<ItemTypeId> kp_to_item_type;
        for (ItemTypeId item_type_id: eligible_item_type_ids) {
            double value = solution[variable_pos[item_type_id]];
            // A zero-profit item can never improve the knapsack objective
            // whether selected or not, so it is simply omitted here -
            // 'selected_copies' below naturally stays 0 for it, exactly as
            // if it had been included and left unselected.
            if (value <= 0.0)
                continue;
            const ItemType& item_type = instance.item_type(item_type_id);
            ItemTypeId kp_item_type_id = kp_instance_builder.add_item_type(
                    original_dims[item_type_id]);
            kp_instance_builder.set_item_type_profit(kp_item_type_id, value);
            kp_instance_builder.set_item_type_copies(kp_item_type_id, item_type.copies);
            kp_to_item_type.push_back(item_type_id);
        }
        // Recompute the achieved sum (and per-item-type selected copy
        // count) ourselves, in our own precision, rather than trusting the
        // knapsack solve's own profit accounting.
        std::vector<ItemPos> selected_copies(number_of_item_types, 0);
        if (!kp_to_item_type.empty()) {
            onedimensional::Instance kp_instance = kp_instance_builder.build();
            onedimensional::OptimizeParameters kp_parameters;
            kp_parameters.verbosity_level = 0;
            kp_parameters.optimization_mode = OptimizationMode::NotAnytime;
            onedimensional::Output kp_output = onedimensional::optimize(kp_instance, kp_parameters);
            const onedimensional::Solution& kp_solution = kp_output.solution_pool.best();
            for (ItemTypeId kp_item_type_id = 0;
                    kp_item_type_id < (ItemTypeId)kp_to_item_type.size();
                    ++kp_item_type_id) {
                selected_copies[kp_to_item_type[kp_item_type_id]]
                    = kp_solution.item_copies(kp_item_type_id);
            }
        }
        double achieved = 0.0;
        for (ItemTypeId item_type_id: eligible_item_type_ids)
            achieved += selected_copies[item_type_id] * solution[variable_pos[item_type_id]];

        if (achieved <= (double)capacity + 1e-6) {
            // No violated constraint found: 'solution' is optimal for the
            // full (exponential) constraint set.
            for (ItemTypeId item_type_id: eligible_item_type_ids)
                result[item_type_id] = solution[variable_pos[item_type_id]];
            return result;
        }

        // Add the violated constraint and resolve.
        model.constraints_starts.push_back((int)model.elements_variables.size());
        for (ItemTypeId item_type_id: eligible_item_type_ids) {
            if (selected_copies[item_type_id] == 0)
                continue;
            model.elements_variables.push_back(variable_pos[item_type_id]);
            model.elements_coefficients.push_back((double)selected_copies[item_type_id]);
        }
        model.constraints_lower_bounds.push_back(-std::numeric_limits<double>::infinity());
        model.constraints_upper_bounds.push_back((double)capacity);
    }

    // Unreachable in practice (see the defensive cap above); return the
    // original dimensions as a safe (if possibly non-optimal) fallback.
    for (ItemTypeId item_type_id: eligible_item_type_ids)
        result[item_type_id] = (double)original_dims[item_type_id];
    return result;
}

/**
 * Area-bound contribution of a single '(width_iteration, height_iteration)'
 * pair, ceiled (with a small downward epsilon: this bound is derived from
 * floating-point LP solves, and rounding noise must never push the ceiling
 * *up* past the true value, which would make the bound unsound - only ever
 * down, which only costs a fraction of tightness in rare edge cases).
 */
BinPos bound_for_pair(
        const Instance& instance,
        const std::vector<double>& lengths_1,
        const std::vector<double>& lengths_2,
        double bin_area)
{
    double sum = 0.0;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        sum += (double)item_type.copies
            * lengths_1[item_type_id]
            * lengths_2[item_type_id];
    }
    return (BinPos)std::ceil(sum / bin_area - 1e-6);
}

}

ConservativeScalesOutput packingsolver::rectangle::conservative_scales(
        const Instance& instance,
        const ConservativeScalesParameters& parameters)
{
    if (instance.objective() != Objective::BinPacking
            && instance.objective() != Objective::Feasibility) {
        throw std::invalid_argument(FUNC_SIGNATURE);
    }
    if (instance.number_of_bin_types() != 1) {
        throw std::invalid_argument(FUNC_SIGNATURE);
    }
    if (!instance.all_item_types_oriented()) {
        throw std::invalid_argument(FUNC_SIGNATURE);
    }

    const BinType& bin_type = instance.bin_type(0);
    ItemTypeId number_of_item_types = instance.number_of_item_types();

    ConservativeScalesOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    std::vector<Length> original_widths(number_of_item_types);
    std::vector<Length> original_heights(number_of_item_types);
    for (ItemTypeId item_type_id = 0;
            item_type_id < number_of_item_types;
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        original_widths[item_type_id] = item_type.rect.x;
        original_heights[item_type_id] = item_type.rect.y;
    }

    // 'widths[width_iteration]'/'heights[height_iteration]' hold w^k/h^l
    // (doubles throughout, even for iteration 0, so every iterate can be
    // compared/combined uniformly).
    std::vector<std::vector<double>> widths;
    std::vector<std::vector<double>> heights;
    widths.push_back(std::vector<double>(original_widths.begin(), original_widths.end()));
    heights.push_back(std::vector<double>(original_heights.begin(), original_heights.end()));

    double bin_area = (double)bin_type.rect.x * (double)bin_type.rect.y;
    BinPos bound = bound_for_pair(
            instance,
            widths.front(),
            heights.front(),
            bin_area);
    if (instance.objective() == Objective::BinPacking) {
        algorithm_formatter.update_bin_packing_bound(bound);
    } else if (instance.objective() == Objective::Feasibility) {
        if (bound > instance.number_of_bins())
            algorithm_formatter.update_is_proven_infeasible();
    }

    for (Counter iteration = 1;
            iteration <= parameters.number_of_iterations;
            ++iteration) {
        if (parameters.timer.needs_to_end())
            break;

        // Jacobi-style update: both use iteration 'iteration - 1''s
        // vectors, never the just-computed other one.
        std::vector<double> new_widths = solve_conservative_scale_lp(
                instance, original_widths, bin_type.rect.x, heights[iteration - 1]);
        for (const std::vector<double>& old_heights: heights) {
            BinPos bound = bound_for_pair(
                    instance,
                    new_widths,
                    old_heights,
                    bin_area);
            if (instance.objective() == Objective::BinPacking) {
                algorithm_formatter.update_bin_packing_bound(bound);
            } else if (instance.objective() == Objective::Feasibility) {
                if (bound > instance.number_of_bins())
                    algorithm_formatter.update_is_proven_infeasible();
            }
        }
        widths.push_back(std::move(new_widths));

        std::vector<double> new_heights = solve_conservative_scale_lp(
                instance, original_heights, bin_type.rect.y, widths[iteration - 1]);
        for (const std::vector<double>& old_widths: widths) {
            BinPos bound = bound_for_pair(
                    instance,
                    new_heights,
                    old_widths,
                    bin_area);
            if (instance.objective() == Objective::BinPacking) {
                algorithm_formatter.update_bin_packing_bound(bound);
            } else if (instance.objective() == Objective::Feasibility) {
                if (bound > instance.number_of_bins())
                    algorithm_formatter.update_is_proven_infeasible();
            }
        }
        heights.push_back(std::move(new_heights));

        bool unchanged = (widths[iteration] == widths[iteration - 1])
            && (heights[iteration] == heights[iteration - 1]);
        if (unchanged)
            break;
    }

    algorithm_formatter.end();
    return output;
}
