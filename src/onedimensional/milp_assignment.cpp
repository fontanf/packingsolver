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
 * Variable-sized Bin Packing Problem, together with the bindings between its
 * variables and the instance's bin/item types.
 */
struct MilpModel
{
    /** Underlying generic MILP model. */
    mathoptsolverscmake::MathOptModel model;

    /** Number of bin instances of each bin type considered in the model. */
    std::vector<BinPos> bin_type_upper_bounds;

    /** y[bin_type_id][bin_instance_pos]: bin instance is used. */
    std::vector<std::vector<int>> y;

    /** x[item_type_id][bin_type_id][bin_instance_pos]: copies of the item type packed in the bin instance. */
    std::vector<std::vector<std::vector<int>>> x;
};

/** Build the classical assignment MILP model of the instance. */
MilpModel build_milp_model(
        const Instance& instance,
        const std::vector<BinPos>& bin_type_upper_bounds)
{
    MilpModel milp_model;
    milp_model.bin_type_upper_bounds = bin_type_upper_bounds;
    milp_model.model.objective_direction = mathoptsolverscmake::ObjectiveDirection::Minimize;
    milp_model.y.resize(instance.number_of_bin_types());
    milp_model.x.resize(instance.number_of_item_types());
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        milp_model.x[item_type_id].resize(instance.number_of_bin_types());
    }

    // Variables: y_{t,k}.
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        BinPos number_of_bin_instances = bin_type_upper_bounds[bin_type_id];
        milp_model.y[bin_type_id] = std::vector<int>(number_of_bin_instances);
        for (BinPos bin_instance_pos = 0;
                bin_instance_pos < number_of_bin_instances;
                ++bin_instance_pos) {
            milp_model.y[bin_type_id][bin_instance_pos] = milp_model.model.variables_lower_bounds.size();
            // Force the first 'copies_min' instances of the type to be used.
            milp_model.model.variables_lower_bounds.push_back(
                    (bin_instance_pos < bin_type.copies_min)? 1.0: 0.0);
            milp_model.model.variables_upper_bounds.push_back(1.0);
            milp_model.model.variables_types.push_back(mathoptsolverscmake::VariableType::Binary);
            milp_model.model.objective_coefficients.push_back(bin_type.cost);
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
                milp_model.model.objective_coefficients.push_back(0.0);
            }
        }
    }

    // Constraints: demand.
    // sum_{t,k} x_{i,t,k} = copies_i
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
        milp_model.model.constraints_lower_bounds.push_back((double)item_type.copies);
        milp_model.model.constraints_upper_bounds.push_back((double)item_type.copies);
    }

    // Constraints: capacity.
    // sum_i length_i * x_{i,t,k} <= length_t * y_{t,k}
    // <=> sum_i length_i * x_{i,t,k} - length_t * y_{t,k} <= 0
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
            milp_model.model.elements_variables.push_back(milp_model.y[bin_type_id][bin_instance_pos]);
            milp_model.model.elements_coefficients.push_back(-(double)bin_type.length);
            // Add row bounds.
            milp_model.model.constraints_lower_bounds.push_back(-std::numeric_limits<double>::infinity());
            milp_model.model.constraints_upper_bounds.push_back(0.0);
        }
    }

    // Constraints: symmetry breaking.
    // y_{t,k} >= y_{t,k+1}
    // <=> 0 <= y_{t,k} - y_{t,k+1} <= inf
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
            milp_model.model.elements_variables.push_back(milp_model.y[bin_type_id][bin_instance_pos]);
            milp_model.model.elements_coefficients.push_back(1.0);
            milp_model.model.elements_variables.push_back(milp_model.y[bin_type_id][bin_instance_pos + 1]);
            milp_model.model.elements_coefficients.push_back(-1.0);
            // Add row bounds.
            milp_model.model.constraints_lower_bounds.push_back(0.0);
            milp_model.model.constraints_upper_bounds.push_back(std::numeric_limits<double>::infinity());
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
            if (milp_solution[milp_model.y[bin_type_id][bin_instance_pos]] < 0.5)
                continue;
            BinPos solution_bin_pos = solution_builder.add_bin(bin_type_id, 1);
            for (ItemTypeId item_type_id = 0;
                    item_type_id < instance.number_of_item_types();
                    ++item_type_id) {
                if (milp_model.x[item_type_id][bin_type_id].empty())
                    continue;
                double value = milp_solution[milp_model.x[item_type_id][bin_type_id][bin_instance_pos]];
                ItemPos copies = (ItemPos)std::llround(value);
                for (ItemPos copy = 0; copy < copies; ++copy)
                    solution_builder.add_item(solution_bin_pos, item_type_id);
            }
        }
    }
    return solution_builder.build();
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

    if (instance.objective() != Objective::VariableSizedBinPacking) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": only the 'VariableSizedBinPacking' "
                "objective is currently supported.");
    }

    // Bound, for each bin type, the number of bin instances of that type to
    // consider in the MILP.
    std::vector<BinPos> bin_type_upper_bounds(instance.number_of_bin_types());
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
                    if (output.solution_pool.best().full()
                            && !strictly_lesser(
                                highs_output->mip_dual_bound,
                                output.solution_pool.best().cost())) {
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
        milp_solution = mathoptsolverscmake::get_solution(highs);
        if (!milp_solution.empty())
            algorithm_formatter.update_variable_sized_bin_packing_bound(mathoptsolverscmake::get_bound(highs));
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
