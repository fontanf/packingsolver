#include "onedimensional/milp_assignment.hpp"

#include "packingsolver/onedimensional/algorithm_formatter.hpp"
#include "packingsolver/onedimensional/instance_builder.hpp"
#include "onedimensional/solution_builder.hpp"

#ifdef CBC_FOUND
#include "mathoptsolverscmake/mathopt_cbc.hpp"
#endif
#ifdef HIGHS_FOUND
#include "mathoptsolverscmake/mathopt_highs.hpp"
#endif
#ifdef XPRESS_FOUND
#include "mathoptsolverscmake/mathopt_xpress.hpp"
#endif

#include <algorithm>
#include <cmath>
#include <sstream>

using namespace packingsolver;
using namespace packingsolver::onedimensional;

namespace
{

/**
 * Compute an upper bound on the number of bin instances of a given bin type
 * that could be used by an optimal solution: pack, via tree search, all the
 * item types that fit this bin type into bins of this type alone. Since any
 * solution never needs more bins of a given type than would be required to
 * pack (using only that type) every item type that fits it, this is a valid
 * bound.
 *
 * When this bin type is 'instance's *only* bin type and every item type
 * fits it, the sub-solve above is solving the exact same problem as
 * 'instance' itself: its dual bound is then reported as a sound lower
 * bound on 'instance's own objective (via 'algorithm_formatter'), and its
 * primal solution - converted back onto 'instance' via 'Solution::append''s
 * bin/item type id remapping - is reported too (as the new best solution,
 * retrievable via 'algorithm_formatter''s 'Output::solution_pool'), in
 * case the caller's own solve is cut short or wants to reuse it (e.g. as a
 * MILP warm start).
 */
BinPos compute_bin_instance_upper_bound(
        const Instance& instance,
        BinTypeId bin_type_id,
        const MilpAssignmentParameters& parameters,
        AlgorithmFormatter& algorithm_formatter)
{
    const BinType& bin_type = instance.bin_type(bin_type_id);

    InstanceBuilder sub_instance_builder;
    sub_instance_builder.set_objective(Objective::BinPacking);
    sub_instance_builder.set_parameters(instance.parameters());
    BinTypeId sub_bin_type_id = sub_instance_builder.add_bin_type(instance, bin_type_id);
    sub_instance_builder.set_bin_type_copies(sub_bin_type_id, -1);
    sub_instance_builder.set_bin_type_copies_min(sub_bin_type_id, 0);

    // Maps 'sub_instance's (compacted) item type ids back to 'instance's
    // own: item types that don't fit this bin type are skipped while
    // building 'sub_instance', so its item type ids only line up 1:1 with
    // 'instance's when every item type fits (the common case is that they
    // don't, e.g. for other bin types in a multi-bin-type instance).
    std::vector<ItemTypeId> sub_item_type_ids;
    // Sum of every compatible item type's own copies: since compatibility
    // means a single copy always fits this bin type on its own, packing one
    // item per bin trivially fits every compatible copy - a valid, if
    // typically very loose, fallback upper bound that does not depend on
    // 'sub_output' below being a complete packing (see its own use further
    // down).
    ItemPos total_compatible_item_copies = 0;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        if (!instance.item_type_fits_bin_type(item_type_id, bin_type_id))
            continue;
        sub_instance_builder.add_item_type(instance, item_type_id);
        sub_item_type_ids.push_back(item_type_id);
        total_compatible_item_copies += instance.item_type(item_type_id).copies;
    }
    if (sub_item_type_ids.empty())
        return 0;
    Instance sub_instance = sub_instance_builder.build();

    OptimizeParameters sub_parameters;
    sub_parameters.verbosity_level = 0;
    sub_parameters.timer = parameters.timer;
    sub_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    sub_parameters.optimization_mode
        = (parameters.optimization_mode == OptimizationMode::NotAnytimeSequential)?
        OptimizationMode::NotAnytimeSequential:
        OptimizationMode::NotAnytimeDeterministic;
    sub_parameters.use_column_generation = true;
    auto sub_output = optimize(sub_instance, sub_parameters);

    bool is_whole_instance = (instance.number_of_bin_types() == 1)
        && ((ItemTypeId)sub_item_type_ids.size() == instance.number_of_item_types());
    if (is_whole_instance) {
        // 'sub_instance' has a single bin type (index 0) mapping back to
        // 'bin_type_id' in 'instance'; 'sub_item_type_ids' maps its item
        // type ids back likewise.
        Solution solution(instance);
        solution.append_bins(sub_output.solution_pool.best(), {bin_type_id}, sub_item_type_ids);
        algorithm_formatter.update_solution(solution, "single bin type");
        if (instance.objective() == Objective::BinPacking) {
            algorithm_formatter.update_bin_packing_bound(sub_output.bin_packing_bound);
        } else if (instance.objective() == Objective::VariableSizedBinPacking) {
            // A single bin type: cost is exactly bins used * that type's
            // cost, so a lower bound on bins used gives one on cost too.
            algorithm_formatter.update_variable_sized_bin_packing_bound(
                    (Profit)sub_output.bin_packing_bound * bin_type.cost);
        }
    }

    // This function's own soundness argument (see its doc comment) requires
    // a *complete* packing of every compatible item type: 'sub_instance'
    // always has a feasible solution (unlimited bin copies), but the search
    // for it shares this function's own timer with its caller, so it can be
    // cut short before finishing. In that case, 'sub_output''s bin count
    // would undercount what packing everything actually needs, making it an
    // unsound (too tight) bound - fall back to one that does not depend on
    // completeness instead.
    if (!sub_output.solution_pool.best().full()) {
        return (bin_type.copies != -1)?
            bin_type.copies:
            total_compatible_item_copies;
    }

    BinPos bound = sub_output.solution_pool.best().number_of_bins();
    if (bin_type.copies != -1)
        bound = std::min(bound, bin_type.copies);
    return bound;
}

/**
 * The classical assignment ("Kantorovich") MILP model of the
 * Variable-sized Bin Packing Problem (or, for the Knapsack objective, of the
 * Multiple Knapsack Problem, or, for the BinPacking objective, of the Bin
 * Packing Problem with a fixed bin-type usage order), together with the
 * bindings between its variables and the instance's bin/item types.
 */
struct MilpModel
{
    /** Underlying generic MILP model. */
    mathoptsolverscmake::MathOptModel model;

    /**
     * Power-of-two divisor applied to every length value (item lengths, bin
     * lengths) appearing in the length-capacity and bin-instance-load-
     * ordering rows, for numerical conditioning only; does not change the
     * optimal solution.
     */
    double multiplier_length = 1.0;

    /**
     * Power-of-two divisor applied to item profits in the objective
     * (Knapsack objective only), for numerical conditioning only; any
     * profit-based bound retrieved from the solver must be multiplied back
     * by this value before being reported.
     */
    double multiplier_profit = 1.0;

    /** Number of bin instances of each bin type considered in the model. */
    std::vector<BinPos> bin_type_upper_bounds;

    /**
     * y[bin_type_id][bin_instance_pos]: bin instance is used, or -1 if the
     * bin instance has no 'y' variable, meaning it is always available (this
     * is the case for every bin instance for the 'Knapsack' and
     * 'Feasibility' objectives).
     */
    std::vector<std::vector<int>> y;

    /**
     * x[item_type_id][bin_type_id][bin_instance_pos][copy]: binary variable,
     * 'true' iff at least 'copy + 1' copies of the item type are packed in
     * the bin instance.
     *
     * Copies are exploded into one binary variable each (rather than a
     * single bounded integer count variable) so that "at least a fixed
     * number of copies of this item type are used" is a plain existing
     * variable ('x[...][a - 1]'), not something that would otherwise need
     * an auxiliary clamp variable to express. This is what makes the
     * "dominated copies" ordering below exact, and lets combinatorial cuts
     * added by callers (e.g. the rectangle Benders decomposition's no-good
     * cuts and pairwise-incompatibility cuts) be expressed as plain
     * resources instead of needing their own mechanism - see
     * 'Resource::item_consumptions'.
     */
    std::vector<std::vector<std::vector<std::vector<int>>>> x;
};

/**
 * Build a full initial (warm-start) MILP solution vector from any solution
 * to 'instance' (e.g. one obtained via 'compute_bin_instance_upper_bound'
 * and converted back onto 'instance', though nothing here assumes that
 * origin or that it covers only a single bin type). Returns an empty
 * vector if the solution doesn't fit within the MILP's modeled
 * bin-instance bounds for some bin type - either because it needs more bin
 * instances than that type's own 'copies' allows, or fewer than
 * 'copies_min' mandates - or if some item type's packed copies fall short
 * of its own 'copies_min'.
 */
std::vector<double> build_initial_solution(
        const Instance& instance,
        const MilpModel& milp_model,
        const Solution& solution)
{
    // Bin instances used per bin type, checked against the MILP's own
    // per-type bounds.
    std::vector<BinPos> number_of_bin_instances(instance.number_of_bin_types(), 0);
    for (BinPos solution_bin_pos = 0;
            solution_bin_pos < solution.number_of_different_bins();
            ++solution_bin_pos) {
        const SolutionBin& solution_bin = solution.bin(solution_bin_pos);
        number_of_bin_instances[solution_bin.bin_type_id] += solution_bin.copies;
    }
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        if (bin_type.copies != -1 && number_of_bin_instances[bin_type_id] > bin_type.copies)
            return {};
        if (number_of_bin_instances[bin_type_id] < bin_type.copies_min)
            return {};
        if (number_of_bin_instances[bin_type_id] > (BinPos)milp_model.y[bin_type_id].size())
            return {};
    }
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        if (solution.item_copies(item_type_id) < item_type.copies_min)
            return {};
    }

    // Sort key for each distinct solution bin (used below), matching the
    // pigeonhole-based bound on 'x_{i,t,k,c}' in 'build_milp_model': the
    // (copies, item_type_id) of whichever of its item types is smallest in
    // that order (i.e. the one that would top out its cumulative-supply
    // bound soonest, so it needs the lowest-ranked, earliest bin instance),
    // and, to break ties between bins sharing that same item type, its
    // count of that type in this bin, descending (a bin with more copies of
    // that type needs a lower-ranked instance to leave room for the bound
    // to hold - see 'build_milp_model'). Bins with no items sort last
    // (unconstrained).
    struct BinSortKey
    {
        bool empty = true;
        ItemPos item_type_copies = 0;
        ItemTypeId item_type_id = 0;
        ItemPos item_type_count = 0;
    };
    std::vector<BinSortKey> bin_sort_keys(solution.number_of_different_bins());
    std::vector<ItemPos> sort_key_item_counts(instance.number_of_item_types());
    for (BinPos solution_bin_pos = 0;
            solution_bin_pos < solution.number_of_different_bins();
            ++solution_bin_pos) {
        const SolutionBin& solution_bin = solution.bin(solution_bin_pos);
        std::fill(sort_key_item_counts.begin(), sort_key_item_counts.end(), 0);
        for (const SolutionItem& solution_item: solution_bin.items)
            ++sort_key_item_counts[solution_item.item_type_id];
        BinSortKey& key = bin_sort_keys[solution_bin_pos];
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            if (sort_key_item_counts[item_type_id] == 0)
                continue;
            ItemPos copies = instance.item_type(item_type_id).copies;
            if (key.empty
                    || copies < key.item_type_copies
                    || (copies == key.item_type_copies && item_type_id < key.item_type_id)) {
                key.empty = false;
                key.item_type_copies = copies;
                key.item_type_id = item_type_id;
                key.item_type_count = sort_key_item_counts[item_type_id];
            }
        }
    }

    // One entry per physical bin instance (a distinct bin's assignment may
    // be shared by several physical instances via 'SolutionBin::copies'),
    // grouped by bin type and sorted (within each type) by the key above,
    // to satisfy 'build_milp_model's pigeonhole-based bound on 'x_{i,t,k,c}'.
    std::vector<std::vector<BinPos>> solution_bin_positions(instance.number_of_bin_types());
    for (BinPos solution_bin_pos = 0;
            solution_bin_pos < solution.number_of_different_bins();
            ++solution_bin_pos) {
        const SolutionBin& solution_bin = solution.bin(solution_bin_pos);
        for (BinPos copy = 0; copy < solution_bin.copies; ++copy)
            solution_bin_positions[solution_bin.bin_type_id].push_back(solution_bin_pos);
    }
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        std::stable_sort(
                solution_bin_positions[bin_type_id].begin(),
                solution_bin_positions[bin_type_id].end(),
                [&bin_sort_keys](BinPos solution_bin_pos_1, BinPos solution_bin_pos_2)
                {
                    const BinSortKey& key_1 = bin_sort_keys[solution_bin_pos_1];
                    const BinSortKey& key_2 = bin_sort_keys[solution_bin_pos_2];
                    if (key_1.empty != key_2.empty)
                        return !key_1.empty;
                    if (key_1.empty)
                        return false;
                    if (key_1.item_type_copies != key_2.item_type_copies)
                        return key_1.item_type_copies < key_2.item_type_copies;
                    if (key_1.item_type_id != key_2.item_type_id)
                        return key_1.item_type_id < key_2.item_type_id;
                    return key_1.item_type_count > key_2.item_type_count;
                });
    }

    std::vector<double> initial_solution(milp_model.model.variables_lower_bounds.size(), 0.0);
    std::vector<ItemPos> item_counts(instance.number_of_item_types(), 0);
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const std::vector<BinPos>& positions = solution_bin_positions[bin_type_id];
        for (BinPos bin_instance_pos = 0;
                bin_instance_pos < (BinPos)positions.size();
                ++bin_instance_pos) {
            int y_variable_id = milp_model.y[bin_type_id][bin_instance_pos];
            if (y_variable_id != -1)
                initial_solution[y_variable_id] = 1.0;
            const SolutionBin& solution_bin = solution.bin(positions[bin_instance_pos]);
            std::fill(item_counts.begin(), item_counts.end(), 0);
            for (const SolutionItem& solution_item: solution_bin.items)
                ++item_counts[solution_item.item_type_id];
            for (ItemTypeId item_type_id = 0;
                    item_type_id < instance.number_of_item_types();
                    ++item_type_id) {
                ItemPos copies = item_counts[item_type_id];
                if (copies == 0)
                    continue;
                if (milp_model.x[item_type_id][bin_type_id].empty())
                    continue;
                const std::vector<int>& slots = milp_model.x[item_type_id][bin_type_id][bin_instance_pos];
                for (ItemPos copy = 0; copy < copies && copy < (ItemPos)slots.size(); ++copy)
                    initial_solution[slots[copy]] = 1.0;
            }
        }
    }
    return initial_solution;
}

/**
 * Add the MILP encoding of a 'penalize' resource (see 'Resource::penalize')
 * to 'milp_model', for every bin instance of 'bin_type_id' - one binary
 * "excess" variable psi_k per bin instance k, subtracting the resource's
 * penalty from the objective whenever it is 1, plus the constraints forcing
 * it to 1 whenever the resource's threshold is reached.
 *
 * Only a Jepsen et al. (2008)-style "at least N of a set of item-type units
 * are packed together" shape is supported, for any non-negative integer
 * 'resource.capacity' (threshold 'capacity + 1'), with every item type it
 * involves capped at a contribution of 1 per copy up to some bound N, via a
 * 'threshold_schedule(N)' schedule (N ones followed by a single trailing 0 -
 * see 'threshold_schedule' in the rectangle Benders decomposition, or
 * 'Resource::item_consumptions' for the general semantics). Throws if the
 * resource does not have this shape.
 *
 * Since every copy of an item type is already its own separate binary
 * variable (see 'build_milp_model's own "Variables: x_{i,t,k,c}" section:
 * 'x_{i,t,k,c} == 1' iff at least 'c + 1' copies of item type i are packed,
 * so under the "dominated copies" ordering, 'x_{i,t,k,0} + ... + x_{i,t,k,N-1}'
 * is exactly the number - from 0 to N - of type i's copies present), each of
 * the resource's first N copies of each item type is already a plain 0/1
 * "is this particular unit present" indicator, exactly like Wang et al.
 * (2025)'s individual items. "At least 'threshold' (of any mix of units,
 * whether from the same item type or different ones) are packed together"
 * is then the direct multi-unit generalization of their G2KP formulation's
 * pairwise/clique linearization (their constraint (5c), which is exactly
 * this for 'threshold == 2'): applied to the flattened list of every (item
 * type, copy) unit the resource involves, for every subset S of size
 * 'threshold' of that list,
 *     sum_{u in S} x_u - psi_k <= threshold - 1,
 * which forces 'psi_k' to 1 exactly when every unit of some size-'threshold'
 * subset is packed together, with no slack/tolerance needed (unlike a
 * general excess-vs-capacity row, which would need an upper bound on the
 * resource's own maximum possible consumption to relax by) - this holds for
 * any finite set of 0/1 variables, regardless of whether some of them
 * happen to be different copies of the same item type. Since these
 * resources only ever come from subset-row cuts (cardinality at most 7, so
 * at most 7 units and 'threshold' at most 4), the number of size-'threshold'
 * subsets stays small (at most C(7, 4) = 35) even though it grows
 * combinatorially with 'threshold' in general. A bin instance whose
 * available units (excluded by eligibility or by the "pigeonhole" bound)
 * number fewer than 'threshold' has no subset of the required size at all,
 * so it is skipped entirely - not even 'psi_k' is created for it, since
 * nothing could ever force 'psi_k' away from its natural optimum of 0 (0.5
 * for a threshold-crossing check that is trivially unsatisfiable there
 * would otherwise leave 'psi_k' free to be set to 1 for a spurious profit
 * whenever 'resource.penalty' is negative - see 'Resource''s own doc
 * comment on negative penalties).
 */
void add_penalize_resource_constraints(
        const Instance& instance,
        BinTypeId bin_type_id,
        ResourceId resource_id,
        const Resource& resource,
        BinPos number_of_bin_instances,
        double multiplier_profit,
        MilpModel& milp_model)
{
    double rounded_capacity = std::round(resource.capacity);
    if (resource.capacity < 0.0
            || std::abs(resource.capacity - rounded_capacity) > 1e-6) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "'penalize' resource " + std::to_string(resource_id) + " of bin type " +
                std::to_string(bin_type_id) + " has capacity " + std::to_string(resource.capacity) +
                "; only 'at least N of a set of item-type units' penalize resources "
                "(a non-negative integer capacity, every item type's consumption a "
                "'threshold_schedule(N)' for some N) are currently supported by the "
                "MILP model.");
    }
    ItemPos threshold = (ItemPos)rounded_capacity + 1;

    // Every (item type, copy) unit the resource involves, flattened.
    std::vector<std::pair<ItemTypeId, ItemPos>> resource_units;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        if (resource.item_consumption(item_type_id, 0) == 0.0)
            continue;
        // Validate the 'threshold_schedule(N)' shape: N ones then a single
        // trailing 0 - anything else (e.g. an uncapped uniform consumption,
        // or a non-0/1 value) is not expressible as 0/1 "unit" presence.
        // Bounded by the item type's own total copies: no valid schedule
        // needs to cap beyond that, and an uncapped (all-1) schedule would
        // otherwise make this scan run forever ('item_consumption' repeats
        // a schedule's last entry for every copy past its end).
        ItemPos copies_bound = instance.item_type(item_type_id).copies;
        ItemPos item_type_threshold = 0;
        while (item_type_threshold <= copies_bound
                && resource.item_consumption(item_type_id, item_type_threshold) == 1.0) {
            ++item_type_threshold;
        }
        if (resource.item_consumption(item_type_id, item_type_threshold) != 0.0) {
            throw std::invalid_argument(
                    FUNC_SIGNATURE + ": "
                    "'penalize' resource " + std::to_string(resource_id) + " of bin type " +
                    std::to_string(bin_type_id) + " does not use a 'threshold_schedule(N)' "
                    "consumption for item type " + std::to_string(item_type_id) + "; only "
                    "'at least N of a set of item-type units' penalize resources are "
                    "currently supported by the MILP model.");
        }
        for (ItemPos copy = 0; copy < item_type_threshold; ++copy)
            resource_units.push_back({item_type_id, copy});
    }
    if ((ItemPos)resource_units.size() < threshold)
        return;

    for (BinPos bin_instance_pos = 0;
            bin_instance_pos < number_of_bin_instances;
            ++bin_instance_pos) {
        // Presence variable of every unit in the resource, for this bin
        // instance, or -1 if that unit has no variable there at all.
        std::vector<int> presence_variable_ids;
        for (const auto& unit: resource_units) {
            ItemTypeId item_type_id = unit.first;
            ItemPos copy = unit.second;
            const std::vector<std::vector<int>>& x_bin_type
                = milp_model.x[item_type_id][bin_type_id];
            int presence_variable_id = (!x_bin_type.empty()
                    && copy < (ItemPos)x_bin_type[bin_instance_pos].size())?
                x_bin_type[bin_instance_pos][copy]:
                -1;
            presence_variable_ids.push_back(presence_variable_id);
        }

        // Units that actually have a variable for this bin instance - see
        // this function's own doc comment on why a bin instance with fewer
        // of these than 'threshold' is skipped entirely, 'psi_k' included.
        std::vector<int> valid_presence_variable_ids;
        for (int presence_variable_id: presence_variable_ids) {
            if (presence_variable_id != -1)
                valid_presence_variable_ids.push_back(presence_variable_id);
        }
        if ((ItemPos)valid_presence_variable_ids.size() < threshold)
            continue;

        // psi_k variable.
        int psi_variable_id = milp_model.model.variables_lower_bounds.size();
        milp_model.model.variables_lower_bounds.push_back(0.0);
        milp_model.model.variables_upper_bounds.push_back(1.0);
        milp_model.model.variables_types.push_back(mathoptsolverscmake::VariableType::Binary);
        milp_model.model.objective_coefficients.push_back(-resource.penalty / multiplier_profit);

        // One constraint per size-'threshold' subset of
        // 'valid_presence_variable_ids' (see this function's own doc
        // comment) - enumerated via the standard "combination indices"
        // odometer: 'combination_indices' always holds 'threshold' strictly
        // increasing indices into 'valid_presence_variable_ids', starting at
        // '0, 1, ..., threshold - 1'; each step advances the rightmost index
        // that still has room to grow, then resets every index after it to
        // the tightest strictly-increasing sequence following it.
        std::vector<std::size_t> combination_indices(threshold);
        for (ItemPos index = 0; index < threshold; ++index)
            combination_indices[index] = index;
        while (true) {
            // Initialize new row.
            milp_model.model.constraints_starts.push_back(milp_model.model.elements_variables.size());
            // Add row elements.
            for (std::size_t index: combination_indices) {
                milp_model.model.elements_variables.push_back(valid_presence_variable_ids[index]);
                milp_model.model.elements_coefficients.push_back(1.0);
            }
            milp_model.model.elements_variables.push_back(psi_variable_id);
            milp_model.model.elements_coefficients.push_back(-1.0);
            // Add row bounds.
            milp_model.model.constraints_lower_bounds.push_back(-std::numeric_limits<double>::infinity());
            milp_model.model.constraints_upper_bounds.push_back((double)threshold - 1.0);

            // Advance to the next combination, or stop once none is left.
            int position = (int)threshold - 1;
            while (position >= 0
                    && combination_indices[position]
                        == valid_presence_variable_ids.size() - threshold + position) {
                --position;
            }
            if (position < 0)
                break;
            ++combination_indices[position];
            for (std::size_t next_position = position + 1; next_position < (std::size_t)threshold; ++next_position)
                combination_indices[next_position] = combination_indices[next_position - 1] + 1;
        }
    }
}

/**
 * Build the classical assignment MILP model of the instance. If 'incumbent'
 * is non-empty (it has at least one bin - it need not be full, e.g. for the
 * 'Knapsack' objective), it is also used to fill in the model's
 * 'variables_initial_values' (warm start); see 'build_initial_solution'.
 */
MilpModel build_milp_model(
        const Instance& instance,
        const std::vector<BinPos>& bin_type_upper_bounds,
        const Solution& incumbent)
{
    bool is_knapsack = (instance.objective() == Objective::Knapsack);
    bool is_bin_packing = (instance.objective() == Objective::BinPacking);
    // 'Knapsack' and 'Feasibility' both work on the instance's fixed set of
    // bins with no activation decision at all (every bin instance is simply
    // available); 'VariableSizedBinPacking' and 'BinPacking' both decide how
    // many bins to use via a 'y' variable (the former to minimize cost, the
    // latter to minimize count, additionally constrained to use bin types in
    // the order they are provided).
    bool needs_y = (instance.objective() == Objective::VariableSizedBinPacking
            || is_bin_packing);

    MilpModel milp_model;
    // Numerical conditioning only (power-of-two coefficient scaling); does
    // not change the optimal solution.
    Length largest_bin_length = 0;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        largest_bin_length = std::max(
                largest_bin_length,
                instance.bin_type(bin_type_id).length);
    }
    milp_model.multiplier_length = largest_power_of_two_lesser_or_equal(largest_bin_length);
    milp_model.multiplier_profit = largest_power_of_two_lesser_or_equal(instance.largest_item_profit());
    double multiplier_length = milp_model.multiplier_length;
    double multiplier_profit = milp_model.multiplier_profit;
    milp_model.bin_type_upper_bounds = bin_type_upper_bounds;
    milp_model.model.objective_direction = is_knapsack?
        mathoptsolverscmake::ObjectiveDirection::Maximize:
        mathoptsolverscmake::ObjectiveDirection::Minimize;
    milp_model.y.resize(instance.number_of_bin_types());
    milp_model.x.resize(instance.number_of_item_types());
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        milp_model.x[item_type_id].resize(instance.number_of_bin_types());
    }

    // Maximum number of copies of an item type that could ever fit (by
    // length alone) in a single bin instance of a given type: the number of
    // per-copy binaries created for that (item type, bin type) pair. Capped
    // at the item type's own total number of copies.
    std::vector<std::vector<ItemPos>> item_copies_bound(instance.number_of_item_types());
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        item_copies_bound[item_type_id].resize(instance.number_of_bin_types(), 0);
        for (BinTypeId bin_type_id = 0;
                bin_type_id < instance.number_of_bin_types();
                ++bin_type_id) {
            if (!instance.item_type_fits_bin_type(item_type_id, bin_type_id))
                continue;
            const BinType& bin_type = instance.bin_type(bin_type_id);
            item_copies_bound[item_type_id][bin_type_id] = std::min(
                    item_type.copies,
                    (ItemPos)(bin_type.length / item_type.length));
        }
    }

    // Variables: y_{t,k}.
    // For 'VariableSizedBinPacking' and 'BinPacking', a 'y' variable is
    // created for every candidate bin instance: whether to use it is a real
    // decision (its lower bound is forced to 1 for the mandatory instances,
    // i.e. those within the bin type's 'copies_min'), costed by the bin
    // type's cost for 'VariableSizedBinPacking' and by 1 (a plain count) for
    // 'BinPacking'. For 'Knapsack' and 'Feasibility', bin instances are
    // already fixed/available ('copies_min' does not apply): no 'y' variable
    // is created at all (every entry stays at its -1 sentinel).
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        BinPos number_of_bin_instances = bin_type_upper_bounds[bin_type_id];
        milp_model.y[bin_type_id] = std::vector<int>(number_of_bin_instances, -1);
        if (!needs_y)
            continue;
        for (BinPos bin_instance_pos = 0;
                bin_instance_pos < number_of_bin_instances;
                ++bin_instance_pos) {
            bool mandatory = (bin_instance_pos < bin_type.copies_min);
            milp_model.y[bin_type_id][bin_instance_pos] = milp_model.model.variables_lower_bounds.size();
            // Force the mandatory instances of the type to be used.
            milp_model.model.variables_lower_bounds.push_back(mandatory? 1.0: 0.0);
            milp_model.model.variables_upper_bounds.push_back(1.0);
            milp_model.model.variables_types.push_back(mathoptsolverscmake::VariableType::Binary);
            milp_model.model.objective_coefficients.push_back(is_bin_packing? 1.0: bin_type.cost);
        }
    }

    // Cumulative supply, per bin type, for the pigeonhole-based symmetry
    // breaking below: order item types globally by ascending number of
    // copies (ties broken by item type id) - a fixed order independent of
    // bin type, only which item types are eligible for a given bin type
    // varies. 'item_cumulative_supply[item_type_id][bin_type_id]' is then
    // the total number of copies of item types up to and including
    // 'item_type_id' in that order, restricted to types eligible for
    // 'bin_type_id'.
    //
    // Bin instances of the same type are interchangeable: relabel them, for
    // any feasible solution, by increasing order of the smallest "global
    // label" they contain, where labels 1..copies_i of item type i's own
    // copies occupy the block right after the previous (in the above order)
    // eligible item type's own block. Since a used instance's labels are all
    // taken from distinct items and instance ranks are relabelled by
    // increasing minimum label, rank r's bin only ever contains labels >= r;
    // as item type i's own labels top out at 'item_cumulative_supply[i][t]',
    // no bin instance ranked (0-indexed) 'k' can hold more than
    // 'max(0, item_cumulative_supply[i][t] - k)' copies of it - so
    // 'x_{i,t,k,c}' (at least 'c + 1' copies) never needs to exist once
    // 'c + 1' exceeds that bound. This holds for every eligible item type
    // simultaneously (no single "primary" type needed), and needs no
    // separate bin-instance ordering constraint: fixing (here, simply not
    // creating) the excluded variables is already a sound and complete
    // account of it.
    std::vector<ItemTypeId> item_type_order(instance.number_of_item_types());
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        item_type_order[item_type_id] = item_type_id;
    }
    std::sort(
            item_type_order.begin(),
            item_type_order.end(),
            [&instance](ItemTypeId item_type_id_1, ItemTypeId item_type_id_2)
            {
                ItemPos copies_1 = instance.item_type(item_type_id_1).copies;
                ItemPos copies_2 = instance.item_type(item_type_id_2).copies;
                if (copies_1 != copies_2)
                    return copies_1 < copies_2;
                return item_type_id_1 < item_type_id_2;
            });
    std::vector<std::vector<ItemPos>> item_cumulative_supply(instance.number_of_item_types());
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        item_cumulative_supply[item_type_id].resize(instance.number_of_bin_types(), 0);
    }
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        ItemPos cumulative_supply = 0;
        for (ItemTypeId item_type_id: item_type_order) {
            if (!instance.item_type_fits_bin_type(item_type_id, bin_type_id))
                continue;
            cumulative_supply += instance.item_type(item_type_id).copies;
            item_cumulative_supply[item_type_id][bin_type_id] = cumulative_supply;
        }
    }

    // Variables: x_{i,t,k,c}.
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        for (BinTypeId bin_type_id = 0;
                bin_type_id < instance.number_of_bin_types();
                ++bin_type_id) {
            if (!instance.item_type_fits_bin_type(item_type_id, bin_type_id))
                continue;
            BinPos number_of_bin_instances = bin_type_upper_bounds[bin_type_id];
            ItemPos copies_bound = item_copies_bound[item_type_id][bin_type_id];
            ItemPos cumulative_supply = item_cumulative_supply[item_type_id][bin_type_id];
            milp_model.x[item_type_id][bin_type_id] = std::vector<std::vector<int>>(number_of_bin_instances);
            for (BinPos bin_instance_pos = 0;
                    bin_instance_pos < number_of_bin_instances;
                    ++bin_instance_pos) {
                std::vector<int>& slots = milp_model.x[item_type_id][bin_type_id][bin_instance_pos];
                ItemPos pigeonhole_bound = std::max(
                        (ItemPos)0,
                        cumulative_supply - (ItemPos)bin_instance_pos);
                slots.resize(std::min(copies_bound, pigeonhole_bound));
                for (ItemPos copy = 0; copy < (ItemPos)slots.size(); ++copy) {
                    slots[copy] = milp_model.model.variables_lower_bounds.size();
                    milp_model.model.variables_lower_bounds.push_back(0.0);
                    milp_model.model.variables_upper_bounds.push_back(1.0);
                    milp_model.model.variables_types.push_back(mathoptsolverscmake::VariableType::Binary);
                    milp_model.model.objective_coefficients.push_back(is_knapsack? item_type.profit / multiplier_profit: 0.0);
                }
            }
        }
    }

    // Constraints: dominated copies.
    // x_{i,t,k,c+1} <= x_{i,t,k,c}
    // <=> 0 <= x_{i,t,k,c} - x_{i,t,k,c+1} <= inf
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        for (BinTypeId bin_type_id = 0;
                bin_type_id < instance.number_of_bin_types();
                ++bin_type_id) {
            if (milp_model.x[item_type_id][bin_type_id].empty())
                continue;
            BinPos number_of_bin_instances = bin_type_upper_bounds[bin_type_id];
            for (BinPos bin_instance_pos = 0;
                    bin_instance_pos < number_of_bin_instances;
                    ++bin_instance_pos) {
                const std::vector<int>& slots = milp_model.x[item_type_id][bin_type_id][bin_instance_pos];
                for (ItemPos copy = 0; copy + 1 < (ItemPos)slots.size(); ++copy) {
                    // Initialize new row.
                    milp_model.model.constraints_starts.push_back(milp_model.model.elements_variables.size());
                    // Add row elements.
                    milp_model.model.elements_variables.push_back(slots[copy]);
                    milp_model.model.elements_coefficients.push_back(1.0);
                    milp_model.model.elements_variables.push_back(slots[copy + 1]);
                    milp_model.model.elements_coefficients.push_back(-1.0);
                    // Add row bounds.
                    milp_model.model.constraints_lower_bounds.push_back(0.0);
                    milp_model.model.constraints_upper_bounds.push_back(std::numeric_limits<double>::infinity());
                }
            }
        }
    }

    // Constraints: demand.
    // copies_min_i <= sum_{t,k,c} x_{i,t,k,c} <= copies_i
    // 'VariableSizedBinPacking', 'BinPacking' and 'Feasibility': copies_min_i == copies_i
    // (every item must be packed); 'Knapsack': copies_min_i is 0 unless
    // explicitly forced.
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        // Initialize new row.
        milp_model.model.constraints_starts.push_back(milp_model.model.elements_variables.size());
        // Add row elements.
        for (BinTypeId bin_type_id = 0;
                bin_type_id < instance.number_of_bin_types();
                ++bin_type_id) {
            for (const std::vector<int>& slots: milp_model.x[item_type_id][bin_type_id]) {
                for (int variable_id: slots) {
                    milp_model.model.elements_variables.push_back(variable_id);
                    milp_model.model.elements_coefficients.push_back(1.0);
                }
            }
        }
        // Add row bounds.
        milp_model.model.constraints_lower_bounds.push_back((double)item_type.copies_min);
        milp_model.model.constraints_upper_bounds.push_back((double)item_type.copies);
    }

    // Constraints: length capacity.
    // With a 'y' variable:    sum_{i,c} length_i * x_{i,t,k,c} <= length_t * y_{t,k}
    //                     <=> sum_{i,c} length_i * x_{i,t,k,c} - length_t * y_{t,k} <= 0
    // Without a 'y' variable: sum_{i,c} length_i * x_{i,t,k,c} <= length_t
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        BinPos number_of_bin_instances = bin_type_upper_bounds[bin_type_id];
        for (BinPos bin_instance_pos = 0;
                bin_instance_pos < number_of_bin_instances;
                ++bin_instance_pos) {
            // Initialize new row.
            milp_model.model.constraints_starts.push_back(milp_model.model.elements_variables.size());
            // Add row elements.
            for (ItemTypeId item_type_id = 0;
                    item_type_id < instance.number_of_item_types();
                    ++item_type_id) {
                const ItemType& item_type = instance.item_type(item_type_id);
                if (milp_model.x[item_type_id][bin_type_id].empty())
                    continue;
                for (int variable_id: milp_model.x[item_type_id][bin_type_id][bin_instance_pos]) {
                    milp_model.model.elements_variables.push_back(variable_id);
                    milp_model.model.elements_coefficients.push_back((double)item_type.length / multiplier_length);
                }
            }
            int y_variable_id = milp_model.y[bin_type_id][bin_instance_pos];
            if (y_variable_id != -1) {
                milp_model.model.elements_variables.push_back(y_variable_id);
                milp_model.model.elements_coefficients.push_back(-(double)bin_type.length / multiplier_length);
                // Add row bounds.
                milp_model.model.constraints_lower_bounds.push_back(-std::numeric_limits<double>::infinity());
                milp_model.model.constraints_upper_bounds.push_back(0.0);
            } else {
                // Add row bounds.
                milp_model.model.constraints_lower_bounds.push_back(-std::numeric_limits<double>::infinity());
                milp_model.model.constraints_upper_bounds.push_back((double)bin_type.length / multiplier_length);
            }
        }
    }

    // Constraints: weight capacity.
    // With a 'y' variable:    sum_{i,c} weight_i * x_{i,t,k,c} <= maximum_weight_t * y_{t,k}
    //                     <=> sum_{i,c} weight_i * x_{i,t,k,c} - maximum_weight_t * y_{t,k} <= 0
    // Without a 'y' variable: sum_{i,c} weight_i * x_{i,t,k,c} <= maximum_weight_t
    // Skipped for bin types without a weight limit.
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        if (bin_type.maximum_weight == std::numeric_limits<Weight>::infinity())
            continue;
        BinPos number_of_bin_instances = bin_type_upper_bounds[bin_type_id];
        for (BinPos bin_instance_pos = 0;
                bin_instance_pos < number_of_bin_instances;
                ++bin_instance_pos) {
            // Initialize new row.
            milp_model.model.constraints_starts.push_back(milp_model.model.elements_variables.size());
            // Add row elements.
            for (ItemTypeId item_type_id = 0;
                    item_type_id < instance.number_of_item_types();
                    ++item_type_id) {
                const ItemType& item_type = instance.item_type(item_type_id);
                if (item_type.weight == 0.0)
                    continue;
                if (milp_model.x[item_type_id][bin_type_id].empty())
                    continue;
                for (int variable_id: milp_model.x[item_type_id][bin_type_id][bin_instance_pos]) {
                    milp_model.model.elements_variables.push_back(variable_id);
                    milp_model.model.elements_coefficients.push_back(item_type.weight);
                }
            }
            int y_variable_id = milp_model.y[bin_type_id][bin_instance_pos];
            if (y_variable_id != -1) {
                milp_model.model.elements_variables.push_back(y_variable_id);
                milp_model.model.elements_coefficients.push_back(-bin_type.maximum_weight);
                // Add row bounds.
                milp_model.model.constraints_lower_bounds.push_back(-std::numeric_limits<double>::infinity());
                milp_model.model.constraints_upper_bounds.push_back(0.0);
            } else {
                // Add row bounds.
                milp_model.model.constraints_lower_bounds.push_back(-std::numeric_limits<double>::infinity());
                milp_model.model.constraints_upper_bounds.push_back(bin_type.maximum_weight);
            }
        }
    }

    // Constraints: resource capacity.
    // With a 'y' variable:    sum_{i,c} consumption_{i,r,c} * x_{i,t,k,c} <= capacity_{t,r} * y_{t,k}
    //                     <=> sum_{i,c} consumption_{i,r,c} * x_{i,t,k,c} - capacity_{t,r} * y_{t,k} <= 0
    // Without a 'y' variable: sum_{i,c} consumption_{i,r,c} * x_{i,t,k,c} <= capacity_{t,r}
    //
    // The consumption of the 'c'-th copy of an item type can depend on 'c'
    // (see 'Resource::item_consumptions'): a schedule that is 1 for
    // the first 'a' copies and 0 after makes the row's contribution from
    // that item type equal to 'min(count, a)', which keeps growing only
    // while count < a. This is what lets combinatorial cuts (no-good cuts,
    // pairwise-incompatibility cuts - see the rectangle Benders
    // decomposition) be expressed exactly as resources: a uniform
    // "cost per unit" resource could only cap the *combined* total of the
    // item types involved, wrongly excluding unrelated combinations using
    // more of one item type and none of another.
    //
    // 'penalize' resources (see 'Resource::penalize') are handled
    // separately, below, only for the 'Knapsack' objective: a 'penalize'
    // resource never blocks packing and never affects any other objective
    // (see 'Solution::update_indicators'), so it is simply skipped when
    // '!is_knapsack' - there would be nothing for it to constrain or cost.
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        BinPos number_of_bin_instances = bin_type_upper_bounds[bin_type_id];
        for (ResourceId resource_id = 0;
                resource_id < bin_type.number_of_resources();
                ++resource_id) {
            const Resource& resource = bin_type.resource(resource_id);
            if (resource.penalize) {
                if (!is_knapsack)
                    continue;
                add_penalize_resource_constraints(
                        instance,
                        bin_type_id,
                        resource_id,
                        resource,
                        number_of_bin_instances,
                        multiplier_profit,
                        milp_model);
                continue;
            }
            double capacity = resource.capacity;
            for (BinPos bin_instance_pos = 0;
                    bin_instance_pos < number_of_bin_instances;
                    ++bin_instance_pos) {
                // Initialize new row.
                milp_model.model.constraints_starts.push_back(milp_model.model.elements_variables.size());
                // Add row elements.
                for (ItemTypeId item_type_id = 0;
                        item_type_id < instance.number_of_item_types();
                        ++item_type_id) {
                    if (milp_model.x[item_type_id][bin_type_id].empty())
                        continue;
                    const std::vector<int>& slots = milp_model.x[item_type_id][bin_type_id][bin_instance_pos];
                    for (ItemPos copy = 0; copy < (ItemPos)slots.size(); ++copy) {
                        double consumption = resource.item_consumption(item_type_id, copy);
                        if (consumption == 0.0)
                            continue;
                        milp_model.model.elements_variables.push_back(slots[copy]);
                        milp_model.model.elements_coefficients.push_back(consumption);
                    }
                }
                int y_variable_id = milp_model.y[bin_type_id][bin_instance_pos];
                if (y_variable_id != -1) {
                    milp_model.model.elements_variables.push_back(y_variable_id);
                    milp_model.model.elements_coefficients.push_back(-capacity);
                    // Add row bounds.
                    milp_model.model.constraints_lower_bounds.push_back(-std::numeric_limits<double>::infinity());
                    milp_model.model.constraints_upper_bounds.push_back(0.0);
                } else {
                    // Add row bounds.
                    milp_model.model.constraints_lower_bounds.push_back(-std::numeric_limits<double>::infinity());
                    milp_model.model.constraints_upper_bounds.push_back(capacity);
                }
            }
        }
    }

    // Constraints: item type precedence.
    // See 'Precedence': each one means no unit of the dominated item type
    // may be used unless the dominating item type uses all of its copies,
    // globally, across every bin type and instance it can be packed in
    // (not just one specific bin).
    //
    // With more than one candidate bin, this needs an auxiliary binary
    // indicator 'z' (not tied to any specific bin) plus two constraints:
    //   sum(all dominating copies) >= copies_dominating * z
    //   sum(all dominated copies)  <= copies_dominated * z
    // For equality-demand objectives ('BinPacking', 'VariableSizedBinPacking',
    // 'Feasibility') 'sum(all dominating copies)' is already pinned to
    // 'copies_dominating' by the item type's own demand constraint
    // regardless of 'z', so 'z' is left free and the solver can always set
    // it to 1: the pair becomes an automatic no-op there, exactly as it
    // should (the underlying exchange argument - swap one dominated unit
    // for one dominating unit, which always fits in the freed space since
    // the dominating item type is no larger, and never decreases profit -
    // only has force where the demand constraint allows some copies to go
    // unpacked, i.e. 'Knapsack'). No explicit objective check is needed.
    //
    // With a single candidate bin, no new variable is needed at all: the
    // dominating item type's own last achievable copy in that bin is
    // already a binary meaning exactly "fully used there" (thanks to the
    // "dominated copies" ordering above), so it is reused directly as 'z'
    // (this is also exactly why a single-bin instance can never need more
    // than this: with only one candidate bin, "used everywhere" and "used
    // in this one bin" coincide).
    {
        bool single_bin = (instance.number_of_bins() <= 1);
        BinTypeId single_bin_type_id = -1;
        if (single_bin) {
            for (BinTypeId bin_type_id = 0;
                    bin_type_id < instance.number_of_bin_types();
                    ++bin_type_id) {
                if (bin_type_upper_bounds[bin_type_id] > 0) {
                    single_bin_type_id = bin_type_id;
                    break;
                }
            }
        }
        for (const Precedence& precedence: instance.precedences()) {
            ItemTypeId dominated_item_type_id = precedence.dominated_item_type_id;
            ItemTypeId dominating_item_type_id = precedence.dominating_item_type_id;
            const ItemType& dominated_item_type = instance.item_type(dominated_item_type_id);
            const ItemType& dominating_item_type = instance.item_type(dominating_item_type_id);

            if (single_bin) {
                if (single_bin_type_id == -1
                        || milp_model.x[dominated_item_type_id][single_bin_type_id].empty()
                        || milp_model.x[dominating_item_type_id][single_bin_type_id].empty()) {
                    // Either no bin at all, or one of the two item types
                    // cannot be packed in the (only) candidate bin type: the
                    // precedence is vacuous here.
                    continue;
                }
                const std::vector<int>& dominated_slots = milp_model.x[dominated_item_type_id][single_bin_type_id][0];
                const std::vector<int>& dominating_slots = milp_model.x[dominating_item_type_id][single_bin_type_id][0];
                int z_variable_id = dominating_slots.back();
                // sum_c x_{dominated,c} <= copies_dominated * z
                // <=> sum_c x_{dominated,c} - copies_dominated * z <= 0
                // Initialize new row.
                milp_model.model.constraints_starts.push_back(milp_model.model.elements_variables.size());
                // Add row elements.
                for (int variable_id: dominated_slots) {
                    milp_model.model.elements_variables.push_back(variable_id);
                    milp_model.model.elements_coefficients.push_back(1.0);
                }
                milp_model.model.elements_variables.push_back(z_variable_id);
                milp_model.model.elements_coefficients.push_back(-(double)dominated_item_type.copies);
                // Add row bounds.
                milp_model.model.constraints_lower_bounds.push_back(-std::numeric_limits<double>::infinity());
                milp_model.model.constraints_upper_bounds.push_back(0.0);
                continue;
            }

            // Multiple candidate bins: auxiliary indicator variable.
            int z_variable_id = milp_model.model.variables_lower_bounds.size();
            milp_model.model.variables_lower_bounds.push_back(0.0);
            milp_model.model.variables_upper_bounds.push_back(1.0);
            milp_model.model.variables_types.push_back(mathoptsolverscmake::VariableType::Binary);
            milp_model.model.objective_coefficients.push_back(0.0);

            // sum(all dominating copies) >= copies_dominating * z
            // <=> sum(all dominating copies) - copies_dominating * z >= 0
            {
                milp_model.model.constraints_starts.push_back(milp_model.model.elements_variables.size());
                for (BinTypeId bin_type_id = 0;
                        bin_type_id < instance.number_of_bin_types();
                        ++bin_type_id) {
                    if (milp_model.x[dominating_item_type_id][bin_type_id].empty())
                        continue;
                    for (const std::vector<int>& slots: milp_model.x[dominating_item_type_id][bin_type_id]) {
                        for (int variable_id: slots) {
                            milp_model.model.elements_variables.push_back(variable_id);
                            milp_model.model.elements_coefficients.push_back(1.0);
                        }
                    }
                }
                milp_model.model.elements_variables.push_back(z_variable_id);
                milp_model.model.elements_coefficients.push_back(-(double)dominating_item_type.copies);
                milp_model.model.constraints_lower_bounds.push_back(0.0);
                milp_model.model.constraints_upper_bounds.push_back(std::numeric_limits<double>::infinity());
            }

            // sum(all dominated copies) <= copies_dominated * z
            // <=> sum(all dominated copies) - copies_dominated * z <= 0
            {
                milp_model.model.constraints_starts.push_back(milp_model.model.elements_variables.size());
                for (BinTypeId bin_type_id = 0;
                        bin_type_id < instance.number_of_bin_types();
                        ++bin_type_id) {
                    if (milp_model.x[dominated_item_type_id][bin_type_id].empty())
                        continue;
                    for (const std::vector<int>& slots: milp_model.x[dominated_item_type_id][bin_type_id]) {
                        for (int variable_id: slots) {
                            milp_model.model.elements_variables.push_back(variable_id);
                            milp_model.model.elements_coefficients.push_back(1.0);
                        }
                    }
                }
                milp_model.model.elements_variables.push_back(z_variable_id);
                milp_model.model.elements_coefficients.push_back(-(double)dominated_item_type.copies);
                milp_model.model.constraints_lower_bounds.push_back(-std::numeric_limits<double>::infinity());
                milp_model.model.constraints_upper_bounds.push_back(0.0);
            }
        }
    }

    if (is_bin_packing) {
        // Constraints: bin usage order.
        // 'BinPacking' bin types must be used in the order they are
        // provided: all instances of a type must be used before any
        // instance of the next type is, matching how bins become physically
        // available one at a time in a fixed sequence. This is a single
        // chain across the whole flat bin sequence (all instances of type
        // 0, then all instances of type 1, and so on), which also subsumes
        // the plain within-type ordering used below for
        // 'VariableSizedBinPacking'.
        // y_k >= y_{k+1}
        // <=> 0 <= y_k - y_{k+1} <= inf
        int previous_y_variable_id = -1;
        for (BinTypeId bin_type_id = 0;
                bin_type_id < instance.number_of_bin_types();
                ++bin_type_id) {
            BinPos number_of_bin_instances = bin_type_upper_bounds[bin_type_id];
            for (BinPos bin_instance_pos = 0;
                    bin_instance_pos < number_of_bin_instances;
                    ++bin_instance_pos) {
                int y_variable_id = milp_model.y[bin_type_id][bin_instance_pos];
                if (y_variable_id == -1)
                    continue;
                if (previous_y_variable_id != -1) {
                    // Initialize new row.
                    milp_model.model.constraints_starts.push_back(milp_model.model.elements_variables.size());
                    // Add row elements.
                    milp_model.model.elements_variables.push_back(previous_y_variable_id);
                    milp_model.model.elements_coefficients.push_back(1.0);
                    milp_model.model.elements_variables.push_back(y_variable_id);
                    milp_model.model.elements_coefficients.push_back(-1.0);
                    // Add row bounds.
                    milp_model.model.constraints_lower_bounds.push_back(0.0);
                    milp_model.model.constraints_upper_bounds.push_back(std::numeric_limits<double>::infinity());
                }
                previous_y_variable_id = y_variable_id;
            }
        }
    } else {
        // Constraints: symmetry breaking ('VariableSizedBinPacking' only:
        // bin instances of the same type are interchangeable, so this only
        // cuts otherwise-symmetric equivalent solutions within a type; it
        // does not link different bin types together, unlike the ordering
        // constraints above).
        // y_{t,k} >= y_{t,k+1}
        // <=> 0 <= y_{t,k} - y_{t,k+1} <= inf
        for (BinTypeId bin_type_id = 0;
                bin_type_id < instance.number_of_bin_types();
                ++bin_type_id) {
            BinPos number_of_bin_instances = bin_type_upper_bounds[bin_type_id];
            for (BinPos bin_instance_pos = 0;
                    bin_instance_pos < number_of_bin_instances - 1;
                    ++bin_instance_pos) {
                int y_variable_id_1 = milp_model.y[bin_type_id][bin_instance_pos];
                int y_variable_id_2 = milp_model.y[bin_type_id][bin_instance_pos + 1];
                if (y_variable_id_1 == -1 || y_variable_id_2 == -1)
                    continue;
                // Initialize new row.
                milp_model.model.constraints_starts.push_back(milp_model.model.elements_variables.size());
                // Add row elements.
                milp_model.model.elements_variables.push_back(y_variable_id_1);
                milp_model.model.elements_coefficients.push_back(1.0);
                milp_model.model.elements_variables.push_back(y_variable_id_2);
                milp_model.model.elements_coefficients.push_back(-1.0);
                // Add row bounds.
                milp_model.model.constraints_lower_bounds.push_back(0.0);
                milp_model.model.constraints_upper_bounds.push_back(std::numeric_limits<double>::infinity());
            }
        }
    }

    if (incumbent.number_of_bins() > 0) {
        std::vector<double> initial_solution = build_initial_solution(
                instance, milp_model, incumbent);
        if (!initial_solution.empty())
            milp_model.model.variables_initial_values = initial_solution;
    }

    return milp_model;
}

/** Build a 'Solution' from the values of a MILP solution. */
Solution retrieve_solution(
        const Instance& instance,
        const MilpModel& milp_model,
        const std::vector<double>& milp_solution)
{
    SolutionBuilder solution_builder(instance);
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        BinPos number_of_bin_instances = milp_model.bin_type_upper_bounds[bin_type_id];
        for (BinPos bin_instance_pos = 0;
                bin_instance_pos < number_of_bin_instances;
                ++bin_instance_pos) {
            int y_variable_id = milp_model.y[bin_type_id][bin_instance_pos];
            if (y_variable_id != -1 && milp_solution[y_variable_id] < 0.5)
                continue;

            std::vector<std::pair<ItemTypeId, ItemPos>> bin_items;
            for (ItemTypeId item_type_id = 0;
                    item_type_id < instance.number_of_item_types();
                    ++item_type_id) {
                if (milp_model.x[item_type_id][bin_type_id].empty())
                    continue;
                ItemPos copies = 0;
                for (int variable_id: milp_model.x[item_type_id][bin_type_id][bin_instance_pos]) {
                    if (milp_solution[variable_id] > 0.5)
                        ++copies;
                }
                if (copies > 0)
                    bin_items.push_back({item_type_id, copies});
            }
            // No 'y' variable: the bin instance is only included when
            // non-empty (it was never "activated", it is simply available).
            if (y_variable_id == -1 && bin_items.empty())
                continue;

            BinPos solution_bin_pos = solution_builder.add_bin(bin_type_id, 1);
            for (const std::pair<ItemTypeId, ItemPos>& item_entry: bin_items) {
                for (ItemPos copy = 0; copy < item_entry.second; ++copy)
                    solution_builder.add_item(solution_bin_pos, item_entry.first);
            }
        }
    }
    Solution solution = solution_builder.build();

    // Check that the constraints modeled by the MILP are satisfied.
    // Constraints not modeled by the MILP (e.g. nesting length, maximum
    // stackability, maximum weight after) are not checked here: the MILP
    // assignment algorithm does not support them, so a solution may
    // legitimately violate them.
    if (!solution.item_copies_feasible()) {
        throw std::logic_error(
                FUNC_SIGNATURE + ": solution doesn't satisfy item copies.");
    }
    if (instance.objective() != Objective::Knapsack && !solution.full()) {
        throw std::logic_error(
                FUNC_SIGNATURE + ": solution doesn't satisfy item demand.");
    }
    if (!solution.capacity_feasible()) {
        throw std::logic_error(
                FUNC_SIGNATURE + ": solution doesn't satisfy bin length capacity.");
    }
    if (!solution.weight_feasible()) {
        throw std::logic_error(
                FUNC_SIGNATURE + ": solution doesn't satisfy bin weight capacity.");
    }
    if (!solution.resource_feasible()) {
        throw std::logic_error(
                FUNC_SIGNATURE + ": solution doesn't satisfy resource capacity.");
    }
    if (!solution.eligibility_feasible()) {
        throw std::logic_error(
                FUNC_SIGNATURE + ": solution doesn't satisfy eligibility.");
    }
    if (!solution.bin_type_order_feasible()) {
        throw std::logic_error(
                FUNC_SIGNATURE + ": solution doesn't satisfy bin type usage order.");
    }

    return solution;
}

/**
 * Bin type upper bounds for the 'VariableSizedBinPacking' objective: how
 * many bins of each type to use is a decision, so each one is only an
 * estimated upper bound, computed independently per bin type via
 * 'compute_bin_instance_upper_bound'. Stops early (returning whatever has
 * been computed so far) if the timer ends partway through, since the
 * caller checks 'algorithm_formatter.end_boolean()' right after calling
 * this and bails out of 'milp_assignment' entirely in that case.
 */
std::vector<BinPos> compute_bin_type_upper_bounds_variable_sized_bin_packing(
        const Instance& instance,
        const MilpAssignmentParameters& parameters,
        AlgorithmFormatter& algorithm_formatter)
{
    std::vector<BinPos> bin_type_upper_bounds(instance.number_of_bin_types());
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        bin_type_upper_bounds[bin_type_id] = compute_bin_instance_upper_bound(
                instance,
                bin_type_id,
                parameters,
                algorithm_formatter);
        if (algorithm_formatter.end_boolean())
            break;
    }
    return bin_type_upper_bounds;
}

/**
 * Bin type upper bounds for the 'BinPacking' objective: the instance's own
 * bin counts are already exact (unlike 'VariableSizedBinPacking', it has
 * nothing to bound), but its own objective - minimize the number of bins -
 * is exactly what a tree search over the instance itself (every bin type
 * together, with their real, possibly limited copies - not a per-bin-type
 * sub-instance with unlimited copies, since bin supply is already fixed
 * and part of what is being solved for here) already optimizes: it might
 * find a good feasible solution and/or bound before the MILP below does,
 * and might even reveal that fewer bin types or bins are actually needed.
 *
 * If it finds a *complete* solution, its own per-bin-type bin counts are
 * then used as the returned bounds, tighter than (and replacing) the
 * instance's own bin type copies. This is sound because bin types must be
 * used in the order they are provided (see the "bin usage order"
 * constraint in 'build_milp_model'): any feasible solution's bin-type
 * breakdown is therefore a deterministic function of its own total bin
 * count alone (exhaust type 0's copies before touching type 1, and so
 * on), and that function only ever needs *more* of an earlier type as the
 * total grows. So a solution using fewer bins overall (in particular, any
 * optimal one, since this solution's total is a valid upper bound on it)
 * can never need more of any given type than this one, already known
 * feasible, does - and since tree search enumerates bin positions in that
 * exact same fixed, per-type order, this solution's own per-type bin
 * counts are already exactly that breakdown. This is also what turns an
 * unlimited ('copies == -1') bin type into a finite, usable bound for the
 * MILP below.
 */
std::vector<BinPos> compute_bin_type_upper_bounds_bin_packing(
        const Instance& instance,
        const MilpAssignmentParameters& parameters,
        AlgorithmFormatter& algorithm_formatter)
{
    std::vector<BinPos> bin_type_upper_bounds(instance.number_of_bin_types());
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        bin_type_upper_bounds[bin_type_id] = instance.bin_type(bin_type_id).copies;
    }

    OptimizeParameters sub_parameters;
    sub_parameters.verbosity_level = 0;
    sub_parameters.timer = parameters.timer;
    sub_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    sub_parameters.optimization_mode
        = (parameters.optimization_mode == OptimizationMode::NotAnytimeSequential)?
        OptimizationMode::NotAnytimeSequential:
        OptimizationMode::NotAnytimeDeterministic;
    if (instance.number_of_bin_types() == 1) {
        sub_parameters.use_column_generation = true;
    } else {
        sub_parameters.use_tree_search = true;
        sub_parameters.not_anytime_tree_search_queue_size = parameters.bin_count_subproblem_tree_search_queue_size;
    }
    auto sub_output = optimize(instance, sub_parameters);
    algorithm_formatter.update_solution(sub_output.solution_pool.best(), "tree search");
    // Via 'update_bounds' rather than 'update_bin_packing_bound' directly,
    // so that a proven-infeasible 'sub_output' (its own 'bin_packing_bound'
    // would otherwise carry no such signal) is correctly propagated too.
    algorithm_formatter.update_bounds(sub_output);
    if (algorithm_formatter.end_boolean())
        return bin_type_upper_bounds;

    if (sub_output.solution_pool.best().full()) {
        const Solution& sub_solution = sub_output.solution_pool.best();
        std::vector<BinPos> solution_bin_type_counts(instance.number_of_bin_types(), 0);
        for (BinPos solution_bin_pos = 0;
                solution_bin_pos < sub_solution.number_of_different_bins();
                ++solution_bin_pos) {
            const SolutionBin& solution_bin = sub_solution.bin(solution_bin_pos);
            solution_bin_type_counts[solution_bin.bin_type_id] += solution_bin.copies;
        }
        bin_type_upper_bounds = solution_bin_type_counts;
    }

    return bin_type_upper_bounds;
}

/**
 * Bin type upper bounds, dispatching on the objective: see
 * 'compute_bin_type_upper_bounds_variable_sized_bin_packing' and
 * 'compute_bin_type_upper_bounds_bin_packing'. For 'Knapsack' and
 * 'Feasibility', the bins are already fixed by the instance, so the exact
 * instance counts are used directly - there is nothing to bound.
 */
std::vector<BinPos> compute_bin_type_upper_bounds(
        const Instance& instance,
        const MilpAssignmentParameters& parameters,
        AlgorithmFormatter& algorithm_formatter)
{
    if (instance.objective() == Objective::VariableSizedBinPacking) {
        return compute_bin_type_upper_bounds_variable_sized_bin_packing(
                instance, parameters, algorithm_formatter);
    }
    if (instance.objective() == Objective::BinPacking) {
        return compute_bin_type_upper_bounds_bin_packing(
                instance, parameters, algorithm_formatter);
    }

    std::vector<BinPos> bin_type_upper_bounds(instance.number_of_bin_types());
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        bin_type_upper_bounds[bin_type_id] = instance.bin_type(bin_type_id).copies;
    }
    return bin_type_upper_bounds;
}

/**
 * Build the 'Feasibility' sub-instance for a candidate bin count of the
 * sequential feasibility scheme (see 'milp_assignment'): the first
 * 'number_of_bins' bins of 'instance', in the order bin types must be used
 * (mirrors the "bin usage order" constraint of the full 'BinPacking' MILP
 * model in 'build_milp_model'). Bin types are added in the same order as
 * 'instance's own, starting from the first one, so the sub-instance's bin
 * type ids line up 1:1 with 'instance's - no remapping is needed to relate
 * a sub-solution back to 'instance' (see 'enforce_bin_type_order', used
 * against this 1:1 correspondence once the sub-instance has been solved).
 */
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
        const BinType& bin_type = instance.bin_type(bin_type_id);
        BinPos copies = (bin_type.copies == -1)?
            remaining_bins:
            std::min(bin_type.copies, remaining_bins);
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

}

MilpAssignmentOutput packingsolver::onedimensional::milp_assignment(
        const Instance& instance,
        const MilpAssignmentParameters& parameters,
        BinPos lower_bound)
{
    MilpAssignmentOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    if (instance.objective() != Objective::VariableSizedBinPacking
            && instance.objective() != Objective::Knapsack
            && instance.objective() != Objective::Feasibility
            && instance.objective() != Objective::BinPacking) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": only the 'VariableSizedBinPacking', "
                "'Knapsack', 'Feasibility' and 'BinPacking' objectives are "
                "currently supported.");
    }

    // Sequential feasibility scheme (see 'MilpAssignmentParameters::
    // use_sequential_feasibility'): try candidate bin counts in increasing
    // order starting at 'sequential_feasibility_lower_bound' (the better of
    // 'output.bin_packing_bound' and the 'lower_bound' argument - see its
    // own doc comment; both are genuine lower bounds, not proof that
    // anything below them is infeasible, so the bound itself must be tried
    // too, not skipped), each as a 'Feasibility' MILP (solved by a recursive
    // call to 'milp_assignment' itself, which already fully supports that
    // objective), stopping as soon as one is feasible or the timer ends.
    if (instance.objective() == Objective::BinPacking
            && parameters.use_sequential_feasibility) {
        BinPos sequential_feasibility_lower_bound = std::max(output.bin_packing_bound, lower_bound);
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
        for (BinPos number_of_bins = sequential_feasibility_lower_bound;
                number_of_bins <= sequential_feasibility_bins_available;
                ++number_of_bins) {
            if (algorithm_formatter.end_boolean() || parameters.timer.needs_to_end())
                break;

            Instance sub_instance = build_sequential_feasibility_sub_instance(
                    instance, number_of_bins);

            MilpAssignmentParameters sub_parameters;
            sub_parameters.solver = parameters.solver;
            sub_parameters.verbosity_level = 0;
            sub_parameters.timer = parameters.timer;
            sub_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
            MilpAssignmentOutput sub_output = milp_assignment(sub_instance, sub_parameters);

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
                ss << "MILP-A SF " << (number_of_bins - sequential_feasibility_lower_bound)
                    << " " << sub_output.solution_pool.best_label();
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

    // Determine, for each bin type, the number of bin instances of that type
    // to consider in the MILP; see 'compute_bin_type_upper_bounds' and the
    // functions it dispatches to.
    std::vector<BinPos> bin_type_upper_bounds = compute_bin_type_upper_bounds(
            instance, parameters, algorithm_formatter);
    if (algorithm_formatter.end_boolean()) {
        algorithm_formatter.end();
        return output;
    }

    // If some mandatory item type ('copies_min' > 0) has no eligible,
    // capacity-fitting bin instance among 'bin_type_upper_bounds', the
    // instance is trivially infeasible: 'build_milp_model' would otherwise
    // give that item's "demand" constraint row zero variables (and, if no
    // other item/bin pair creates any variable either - e.g. a single-item,
    // single-bin-type 'Feasibility' sub-instance from the sequential
    // feasibility scheme - the whole MILP ends up with zero columns, which
    // HiGHS reports as 'kModelEmpty' rather than 'kInfeasible'). Detect and
    // report this case directly instead of building and solving a model
    // that can't answer it.
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        if (item_type.copies_min == 0)
            continue;
        bool has_eligible_bin = false;
        for (BinTypeId bin_type_id = 0;
                bin_type_id < instance.number_of_bin_types();
                ++bin_type_id) {
            if (bin_type_upper_bounds[bin_type_id] == 0)
                continue;
            if (instance.item_type_fits_bin_type(item_type_id, bin_type_id)) {
                has_eligible_bin = true;
                break;
            }
        }
        if (!has_eligible_bin) {
            algorithm_formatter.update_is_proven_infeasible();
            algorithm_formatter.end();
            return output;
        }
    }

    MilpModel milp_model = build_milp_model(
            instance, bin_type_upper_bounds, output.solution_pool.best());

    // Solve.
    std::vector<double> milp_solution;
    int number_of_nodes = 0;
    if (parameters.solver == mathoptsolverscmake::SolverName::Highs) {
#ifdef HIGHS_FOUND
        Highs highs;
        mathoptsolverscmake::reduce_printout(highs);
        mathoptsolverscmake::set_time_limit(highs, parameters.timer.remaining_time());
        mathoptsolverscmake::load(highs, milp_model.model);
        highs.setCallback([
                &instance,
                &milp_model,
                &parameters,
                &output,
                &algorithm_formatter](
                    const int callback_type,
                    const std::string&,
                    const HighsCallbackOutput* highs_output,
                    HighsCallbackInput* highs_input,
                    void*)
                {
                    if (!highs_output->mip_solution.empty()) {
                        Solution solution = retrieve_solution(
                                instance,
                                milp_model,
                                highs_output->mip_solution);
                        algorithm_formatter.update_solution(
                                solution,
                                "MILP-A n " + std::to_string(highs_output->mip_node_count));
                    }

                    // Update the bound from the solver's own current dual
                    // bound. Safe regardless of callback type (unlike
                    // requesting an interrupt below, this has no HiGHS-side
                    // restriction), so it is done on every invocation for
                    // the most up-to-date reporting - including from
                    // 'kCallbackMipImprovingSolution', not just
                    // 'kCallbackMipInterrupt'.
                    if (instance.objective() == Objective::VariableSizedBinPacking) {
                        algorithm_formatter.update_variable_sized_bin_packing_bound(
                                highs_output->mip_dual_bound);
                    } else if (instance.objective() == Objective::Knapsack) {
                        algorithm_formatter.update_knapsack_bound(
                                highs_output->mip_dual_bound * milp_model.multiplier_profit);
                    } else if (instance.objective() == Objective::BinPacking) {
                        // The '-0.001' guards against floating-point noise
                        // from the solve nudging the ceiling up past the
                        // true value, which would make the bound unsound
                        // (same convention as 'algorithms/column_generation.hpp').
                        algorithm_formatter.update_bin_packing_bound(
                                (BinPos)std::ceil(highs_output->mip_dual_bound - 0.001));
                    }

                    // HiGHS only allows requesting an interrupt from the
                    // dedicated 'kCallbackMipInterrupt' callback: it asserts
                    // internally that 'user_interrupt' is never set from an
                    // "informational" callback type such as this one's other
                    // registration, 'kCallbackMipImprovingSolution' (see
                    // 'HighsCallback::callbackAction' in HiGHS itself).
                    if (callback_type != HighsCallbackType::kCallbackMipInterrupt)
                        return;

                    // 'is_proven_optimal' already encodes, per objective,
                    // exactly the "found solution matches the current
                    // bound" condition that used to be checked ad hoc here -
                    // including 'Feasibility', whose own case
                    // ('best().full() && best().feasible()') is exactly
                    // "the demand constraints (equalities) are met by an
                    // already-found incumbent", the same condition the
                    // former inline check captured.
                    if (output.is_proven_optimal())
                        highs_input->user_interrupt = 1;

                    // Check end.
                    if (parameters.timer.needs_to_end())
                        highs_input->user_interrupt = 1;
                },
                nullptr);
        HighsStatus highs_status;
        highs_status = highs.startCallback(HighsCallbackType::kCallbackMipImprovingSolution);
        highs_status = highs.startCallback(HighsCallbackType::kCallbackMipInterrupt);
        mathoptsolverscmake::solve(highs);
        if (parameters.timer.needs_to_end()) {
            algorithm_formatter.end();
            return output;
        }
        // 'getSolution().col_value' (what 'get_solution' returns) is not
        // cleared by HiGHS when the model is infeasible, so emptiness alone
        // cannot be used to detect that case: the model status must be
        // checked explicitly.
        bool proven_infeasible =
            (highs.getModelStatus() == HighsModelStatus::kInfeasible
             || highs.getModelStatus() == HighsModelStatus::kUnboundedOrInfeasible);
        if (!proven_infeasible)
            milp_solution = mathoptsolverscmake::get_solution(highs);
        number_of_nodes = mathoptsolverscmake::get_number_of_nodes(highs);
        if (!milp_solution.empty()) {
            double bound = mathoptsolverscmake::get_bound(highs);
            if (instance.objective() == Objective::VariableSizedBinPacking) {
                algorithm_formatter.update_variable_sized_bin_packing_bound(bound);
            } else if (instance.objective() == Objective::Knapsack) {
                algorithm_formatter.update_knapsack_bound(bound * milp_model.multiplier_profit);
            } else if (instance.objective() == Objective::BinPacking) {
                algorithm_formatter.update_bin_packing_bound((BinPos)std::ceil(bound - 1e-6));
            }
        } else if (proven_infeasible) {
            // For Feasibility/Knapsack, the MILP exactly encodes the
            // instance's fixed set of bins (no upper-bound approximation -
            // see 'compute_bin_type_upper_bounds''s own doc comment), so its
            // infeasibility is a genuine proof that the instance itself is
            // infeasible. For BinPacking, 'compute_bin_type_upper_bounds_
            // bin_packing' can instead tighten the bounds to an
            // already-found *complete* solution's own per-bin-type counts -
            // but per that function's own doc comment, any solution using
            // fewer or equal total bins (in particular any optimal one)
            // provably never needs more of a given type than that solution
            // does, so that solution itself remains a feasible point of the
            // MILP built with those tightened bounds: this MILP cannot
            // become infeasible in that case (short of an actual model bug),
            // so whenever it does, the bounds in use must be the untightened
            // (exact) ones instead, making the infeasibility genuine here
            // too. For VariableSizedBinPacking, 'compute_bin_instance_
            // upper_bound' computes each bin type's bound independently, but
            // always soundly regardless of how it was derived (falling back
            // to a completeness-independent bound - the bin type's own real
            // capacity, or one bin per compatible item copy when
            // unlimited - whenever its own sub-search wasn't a complete
            // packing), so its own infeasibility is genuine too.
            algorithm_formatter.update_is_proven_infeasible();
        }
#else
        throw std::invalid_argument(FUNC_SIGNATURE);
#endif
    } else {
        throw std::invalid_argument(FUNC_SIGNATURE);
    }

    if (!milp_solution.empty()) {
        Solution solution = retrieve_solution(instance, milp_model, milp_solution);
        algorithm_formatter.update_solution(
                solution,
                "MILP-A n " + std::to_string(number_of_nodes));
    }

    algorithm_formatter.end();
    return output;
}
