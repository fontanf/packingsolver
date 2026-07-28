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
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        if (!instance.item_type_fits_bin_type(item_type_id, bin_type_id))
            continue;
        sub_instance_builder.add_item_type(instance, item_type_id);
        sub_item_type_ids.push_back(item_type_id);
    }
    if (sub_item_type_ids.empty())
        return 0;
    Instance sub_instance = sub_instance_builder.build();

    OptimizeParameters sub_parameters;
    sub_parameters.verbosity_level = 0;
    sub_parameters.timer = parameters.timer;
    sub_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    sub_parameters.optimization_mode = OptimizationMode::NotAnytimeDeterministic;
    if (instance.objective() == Objective::VariableSizedBinPacking
            || instance.number_of_bin_types() == 1) {
        sub_parameters.use_column_generation = true;
    } else {
        sub_parameters.use_tree_search = true;
        sub_parameters.not_anytime_tree_search_queue_size = parameters.bin_count_subproblem_tree_search_queue_size;
    }
    auto sub_output = optimize(sub_instance, sub_parameters);

    bool is_whole_instance = (instance.number_of_bin_types() == 1)
        && ((ItemTypeId)sub_item_type_ids.size() == instance.number_of_item_types());
    if (is_whole_instance) {
        // 'sub_instance' has a single bin type (index 0) mapping back to
        // 'bin_type_id' in 'instance'; 'sub_item_type_ids' maps its item
        // type ids back likewise.
        Solution solution(instance);
        solution.append(sub_output.solution_pool.best(), {bin_type_id}, sub_item_type_ids);
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
     * 'BinType::item_resource_consumptions'.
     */
    std::vector<std::vector<std::vector<std::vector<int>>>> x;
};

/** Build the classical assignment MILP model of the instance. */
MilpModel build_milp_model(
        const Instance& instance,
        const std::vector<BinPos>& bin_type_upper_bounds)
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
            milp_model.x[item_type_id][bin_type_id] = std::vector<std::vector<int>>(number_of_bin_instances);
            for (BinPos bin_instance_pos = 0;
                    bin_instance_pos < number_of_bin_instances;
                    ++bin_instance_pos) {
                std::vector<int>& slots = milp_model.x[item_type_id][bin_type_id][bin_instance_pos];
                slots.resize(copies_bound);
                for (ItemPos copy = 0; copy < copies_bound; ++copy) {
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
    // 'VariableSizedBinPacking', 'BinPacking' and 'Feasibility': sum_{t,k,c} x_{i,t,k,c} = copies_i
    // 'Knapsack':                                                sum_{t,k,c} x_{i,t,k,c} <= copies_i
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
        milp_model.model.constraints_lower_bounds.push_back(is_knapsack? 0.0: (double)item_type.copies);
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
    // (see 'BinType::item_resource_consumptions'): a schedule that is 1 for
    // the first 'a' copies and 0 after makes the row's contribution from
    // that item type equal to 'min(count, a)', which keeps growing only
    // while count < a. This is what lets combinatorial cuts (no-good cuts,
    // pairwise-incompatibility cuts - see the rectangle Benders
    // decomposition) be expressed exactly as resources: a uniform
    // "cost per unit" resource could only cap the *combined* total of the
    // item types involved, wrongly excluding unrelated combinations using
    // more of one item type and none of another.
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        BinPos number_of_bin_instances = bin_type_upper_bounds[bin_type_id];
        for (ResourceId resource_id = 0;
                resource_id < bin_type.number_of_resources();
                ++resource_id) {
            double capacity = bin_type.resource_capacities[resource_id];
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
                        double consumption = bin_type.item_resource_consumption(item_type_id, resource_id, copy);
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

    // Constraints: bin instance load ordering (symmetry breaking).
    // Bin instances of the same type are interchangeable. Breaking this
    // symmetry per item type is unsound in general: two item types' counts
    // cannot always be simultaneously sorted non-increasing by a single
    // shared bin-instance permutation (e.g. instance A = (5, 3), instance
    // B = (3, 5) for two item types: no ordering of A, B makes both item
    // types' counts non-increasing together). A single aggregate key --
    // total length used -- is always totally orderable regardless of item
    // mix, so it is used instead. Valid for every objective (does not
    // involve the 'y' variables).
    // sum_{i,c} length_i * x_{i,t,k,c} >= sum_{i,c} length_i * x_{i,t,k+1,c}
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        BinPos number_of_bin_instances = bin_type_upper_bounds[bin_type_id];
        for (BinPos bin_instance_pos = 0;
                bin_instance_pos < number_of_bin_instances - 1;
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
                double length = (double)item_type.length / multiplier_length;
                for (int variable_id: milp_model.x[item_type_id][bin_type_id][bin_instance_pos]) {
                    milp_model.model.elements_variables.push_back(variable_id);
                    milp_model.model.elements_coefficients.push_back(length);
                }
                for (int variable_id: milp_model.x[item_type_id][bin_type_id][bin_instance_pos + 1]) {
                    milp_model.model.elements_variables.push_back(variable_id);
                    milp_model.model.elements_coefficients.push_back(-length);
                }
            }
            // Add row bounds.
            milp_model.model.constraints_lower_bounds.push_back(0.0);
            milp_model.model.constraints_upper_bounds.push_back(std::numeric_limits<double>::infinity());
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

    return milp_model;
}

/**
 * Build a full initial (warm-start) MILP solution vector from any solution
 * to 'instance' (e.g. one obtained via 'compute_bin_instance_upper_bound'
 * and converted back onto 'instance', though nothing here assumes that
 * origin or that it covers only a single bin type). Returns an empty
 * vector if the solution doesn't fit within the MILP's modeled
 * bin-instance bounds for some bin type - either because it needs more bin
 * instances than that type's own 'copies' allows, or fewer than
 * 'copies_min' mandates.
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

    // Total item length of each distinct bin (the sort key below).
    std::vector<Length> total_item_lengths(solution.number_of_different_bins(), 0);
    for (BinPos solution_bin_pos = 0;
            solution_bin_pos < solution.number_of_different_bins();
            ++solution_bin_pos) {
        const SolutionBin& solution_bin = solution.bin(solution_bin_pos);
        for (const SolutionItem& solution_item: solution_bin.items)
            total_item_lengths[solution_bin_pos] += instance.item_type(solution_item.item_type_id).length;
    }

    // One entry per physical bin instance (a distinct bin's assignment may
    // be shared by several physical instances via 'SolutionBin::copies'),
    // grouped by bin type and sorted (within each type) by descending
    // total item length, to satisfy the MILP's bin-instance load-ordering
    // (symmetry-breaking) constraint.
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
                [&total_item_lengths](BinPos solution_bin_pos_1, BinPos solution_bin_pos_2)
                {
                    return total_item_lengths[solution_bin_pos_1] > total_item_lengths[solution_bin_pos_2];
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

}

MilpAssignmentOutput packingsolver::onedimensional::milp_assignment(
        const Instance& instance,
        const MilpAssignmentParameters& parameters)
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

    // Determine, for each bin type, the number of bin instances of that type
    // to consider in the MILP. For 'VariableSizedBinPacking', how many bins
    // of each type to use is a decision, so this is only an upper bound,
    // estimated below via a tree search - which, per
    // 'compute_bin_instance_upper_bound', is itself capped by the instance's
    // own (finite) bin type copies whenever the caller gives one (e.g.
    // rectangle's Benders decomposition, which derives a bound valid for
    // every iteration from true 2D geometry, upfront): the tree search then
    // only ever tightens that cap further using this specific call's
    // (possibly cut-augmented) area-based view, never loosens it. For
    // 'Knapsack', 'Feasibility' and 'BinPacking', the bins (and, for
    // 'BinPacking', their order) are already fixed by the instance, so the
    // exact instance counts must be used rather than an estimated bound.
    std::vector<BinPos> bin_type_upper_bounds(instance.number_of_bin_types());
    if (instance.objective() == Objective::VariableSizedBinPacking) {
        for (BinTypeId bin_type_id = 0;
                bin_type_id < instance.number_of_bin_types();
                ++bin_type_id) {
            bin_type_upper_bounds[bin_type_id] = compute_bin_instance_upper_bound(
                    instance,
                    bin_type_id,
                    parameters,
                    algorithm_formatter);
            if (algorithm_formatter.end_boolean()) {
                algorithm_formatter.end();
                return output;
            }
        }
    } else {
        for (BinTypeId bin_type_id = 0;
                bin_type_id < instance.number_of_bin_types();
                ++bin_type_id) {
            bin_type_upper_bounds[bin_type_id] = instance.bin_type(bin_type_id).copies;
        }
        // 'BinPacking' already knows its exact bin counts above (unlike
        // 'VariableSizedBinPacking', it has nothing to bound), but with a
        // single bin type its own objective - minimize the number of bins -
        // is exactly what 'compute_bin_instance_upper_bound''s sub-solve
        // already optimizes, so it is still worth calling purely for the
        // bound/solution reporting it does internally (see
        // 'output.solution_pool.best()' below).
        if (instance.objective() == Objective::BinPacking
                && instance.number_of_bin_types() == 1) {
            compute_bin_instance_upper_bound(
                    instance,
                    0,
                    parameters,
                    algorithm_formatter);
            if (algorithm_formatter.end_boolean()) {
                algorithm_formatter.end();
                return output;
            }
        }
    }

    MilpModel milp_model = build_milp_model(instance, bin_type_upper_bounds);

    // Solve.
    std::vector<double> milp_solution;
    if (parameters.solver == mathoptsolverscmake::SolverName::Highs) {
#ifdef HIGHS_FOUND
        Highs highs;
        mathoptsolverscmake::reduce_printout(highs);
        mathoptsolverscmake::set_time_limit(highs, parameters.timer.remaining_time());
        mathoptsolverscmake::load(highs, milp_model.model);
        if (output.solution_pool.best().full()) {
            std::vector<double> initial_solution = build_initial_solution(
                    instance,
                    milp_model,
                    output.solution_pool.best());
            if (!initial_solution.empty())
                mathoptsolverscmake::set_solution(highs, initial_solution);
        }
        highs.setCallback([
                &instance,
                &milp_model,
                &parameters,
                &output,
                &algorithm_formatter](
                    const int,
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
                                "node " + std::to_string(highs_output->mip_node_count));
                    }
                    if (instance.objective() == Objective::VariableSizedBinPacking) {
                        if (output.solution_pool.best().full()
                                && !strictly_lesser(
                                    highs_output->mip_dual_bound,
                                    output.solution_pool.best().cost())) {
                            highs_input->user_interrupt = 1;
                        }
                    } else if (instance.objective() == Objective::Knapsack) {
                        if (!strictly_greater(
                                    highs_output->mip_dual_bound * milp_model.multiplier_profit,
                                    output.solution_pool.best().profit())) {
                            highs_input->user_interrupt = 1;
                        }
                    } else if (instance.objective() == Objective::BinPacking) {
                        if (output.solution_pool.best().full()
                                && !strictly_lesser(
                                    highs_output->mip_dual_bound,
                                    (double)output.solution_pool.best().number_of_bins())) {
                            highs_input->user_interrupt = 1;
                        }
                    } else {
                        // 'Feasibility': the demand constraints are
                        // equalities, so any incumbent is already a full
                        // solution; no need to search further.
                        if (output.solution_pool.best().full())
                            highs_input->user_interrupt = 1;
                    }

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
        if (!milp_solution.empty()) {
            double bound = mathoptsolverscmake::get_bound(highs);
            if (instance.objective() == Objective::VariableSizedBinPacking) {
                algorithm_formatter.update_variable_sized_bin_packing_bound(bound);
            } else if (instance.objective() == Objective::Knapsack) {
                algorithm_formatter.update_knapsack_bound(bound * milp_model.multiplier_profit);
            } else if (instance.objective() == Objective::BinPacking) {
                algorithm_formatter.update_bin_packing_bound((BinPos)std::ceil(bound - 1e-6));
            }
        } else if (instance.objective() == Objective::Feasibility
                && proven_infeasible) {
            // The MILP exactly encodes the instance's fixed set of bins (no
            // upper-bound approximation), so its infeasibility is a genuine
            // proof that the instance itself is infeasible.
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
        algorithm_formatter.update_solution(solution, "final");
    }

    algorithm_formatter.end();
    return output;
}
