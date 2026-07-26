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

    bool has_fitting_item = false;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        if (!instance.item_type_fits_bin_type(item_type_id, bin_type_id))
            continue;
        sub_instance_builder.add_item_type(instance, item_type_id);
        has_fitting_item = true;
    }
    if (!has_fitting_item)
        return 0;
    Instance sub_instance = sub_instance_builder.build();

    OptimizeParameters sub_parameters;
    sub_parameters.verbosity_level = 0;
    sub_parameters.timer = parameters.timer;
    sub_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    sub_parameters.optimization_mode = OptimizationMode::NotAnytimeDeterministic;
    sub_parameters.use_tree_search = true;
    sub_parameters.not_anytime_tree_search_queue_size = parameters.bin_count_subproblem_tree_search_queue_size;
    auto sub_output = optimize(sub_instance, sub_parameters);

    BinPos bin_instance_upper_bound = sub_output.solution_pool.best().number_of_bins();
    if (bin_type.copies != -1)
        bin_instance_upper_bound = std::min(bin_instance_upper_bound, bin_type.copies);
    return bin_instance_upper_bound;
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

    /** Number of bin instances of each bin type considered in the model. */
    std::vector<BinPos> bin_type_upper_bounds;

    /**
     * y[bin_type_id][bin_instance_pos]: bin instance is used, or -1 if the
     * bin instance has no 'y' variable, meaning it is always available (this
     * is the case for every bin instance for the 'Knapsack' and
     * 'Feasibility' objectives).
     */
    std::vector<std::vector<int>> y;

    /** x[item_type_id][bin_type_id][bin_instance_pos]: copies of the item type packed in the bin instance. */
    std::vector<std::vector<std::vector<int>>> x;
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

    // Variables: x_{i,t,k}.
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
            milp_model.x[item_type_id][bin_type_id] = std::vector<int>(number_of_bin_instances);
            for (BinPos bin_instance_pos = 0;
                    bin_instance_pos < number_of_bin_instances;
                    ++bin_instance_pos) {
                milp_model.x[item_type_id][bin_type_id][bin_instance_pos] = milp_model.model.variables_lower_bounds.size();
                milp_model.model.variables_lower_bounds.push_back(0.0);
                milp_model.model.variables_upper_bounds.push_back((double)item_type.copies);
                milp_model.model.variables_types.push_back(mathoptsolverscmake::VariableType::Integer);
                milp_model.model.objective_coefficients.push_back(is_knapsack? item_type.profit: 0.0);
            }
        }
    }

    // Constraints: demand.
    // 'VariableSizedBinPacking', 'BinPacking' and 'Feasibility': sum_{t,k} x_{i,t,k} = copies_i
    // 'Knapsack':                                                sum_{t,k} x_{i,t,k} <= copies_i
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
            for (int variable_id: milp_model.x[item_type_id][bin_type_id]) {
                milp_model.model.elements_variables.push_back(variable_id);
                milp_model.model.elements_coefficients.push_back(1.0);
            }
        }
        // Add row bounds.
        milp_model.model.constraints_lower_bounds.push_back(is_knapsack? 0.0: (double)item_type.copies);
        milp_model.model.constraints_upper_bounds.push_back((double)item_type.copies);
    }

    // Constraints: length capacity.
    // With a 'y' variable:    sum_i length_i * x_{i,t,k} <= length_t * y_{t,k}
    //                     <=> sum_i length_i * x_{i,t,k} - length_t * y_{t,k} <= 0
    // Without a 'y' variable: sum_i length_i * x_{i,t,k} <= length_t
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
                milp_model.model.elements_variables.push_back(milp_model.x[item_type_id][bin_type_id][bin_instance_pos]);
                milp_model.model.elements_coefficients.push_back((double)item_type.length);
            }
            int y_variable_id = milp_model.y[bin_type_id][bin_instance_pos];
            if (y_variable_id != -1) {
                milp_model.model.elements_variables.push_back(y_variable_id);
                milp_model.model.elements_coefficients.push_back(-(double)bin_type.length);
                // Add row bounds.
                milp_model.model.constraints_lower_bounds.push_back(-std::numeric_limits<double>::infinity());
                milp_model.model.constraints_upper_bounds.push_back(0.0);
            } else {
                // Add row bounds.
                milp_model.model.constraints_lower_bounds.push_back(-std::numeric_limits<double>::infinity());
                milp_model.model.constraints_upper_bounds.push_back((double)bin_type.length);
            }
        }
    }

    // Constraints: weight capacity.
    // With a 'y' variable:    sum_i weight_i * x_{i,t,k} <= maximum_weight_t * y_{t,k}
    //                     <=> sum_i weight_i * x_{i,t,k} - maximum_weight_t * y_{t,k} <= 0
    // Without a 'y' variable: sum_i weight_i * x_{i,t,k} <= maximum_weight_t
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
                milp_model.model.elements_variables.push_back(milp_model.x[item_type_id][bin_type_id][bin_instance_pos]);
                milp_model.model.elements_coefficients.push_back(item_type.weight);
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
                double value = milp_solution[milp_model.x[item_type_id][bin_type_id][bin_instance_pos]];
                ItemPos copies = (ItemPos)std::llround(value);
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
    // of each type to use is a decision, so this is only an upper bound. For
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
                                    highs_output->mip_dual_bound,
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
                algorithm_formatter.update_knapsack_bound(bound);
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
