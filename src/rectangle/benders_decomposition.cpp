#include "rectangle/benders_decomposition.hpp"

#include "packingsolver/rectangle/algorithm_formatter.hpp"
#include "packingsolver/rectangle/instance_builder.hpp"
#include "packingsolver/rectangle/optimize.hpp"

#include "interfaces/highs_c_api.h"

using namespace packingsolver;
using namespace packingsolver::rectangle;

BendersDecompositionOutput packingsolver::rectangle::benders_decomposition(
        const Instance& instance,
        const BendersDecompositionParameters& parameters)
{
    //std::cout << "benders_decomposition..." << std::endl;
    const BinType& bin_type = instance.bin_type(0);

    BendersDecompositionOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    // MILP cuts.
    std::vector<std::vector<std::pair<ItemTypeId, ItemPos>>> milp_cuts;

    for (output.number_of_iterations = 0;
            ;
            ++output.number_of_iterations) {
        //std::cout << "iteration " << output.number_of_iterations << std::endl;

        // Check maximum number of iterations.
        if (parameters.maximum_number_of_iterations >= 0
                && output.number_of_iterations >= parameters.maximum_number_of_iterations) {
            break;
        }

        // Build MILP model.
        std::vector<double> column_lower_bounds;
        std::vector<double> column_upper_bounds;
        std::vector<HighsInt> column_types;
        std::vector<double> objective;

        std::vector<double> row_lower_bounds;
        std::vector<double> row_upper_bounds;

        // Define the constraint matrix row-wise
        const HighsInt a_format = kHighsMatrixFormatRowwise;
        std::vector<HighsInt> a_start;
        std::vector<HighsInt> a_index;
        std::vector<double> a_value;

        // Variables.
        // x[item_type_id][copy]
        std::vector<std::pair<ItemTypeId, ItemPos>> highs_to_orig;
        std::vector<std::vector<HighsInt>> x(instance.number_of_item_types());

        std::vector<ItemPos> item_copies(instance.number_of_item_types(), 0);
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = instance.item_type(item_type_id);
            item_copies[item_type_id] = std::min(
                    item_type.copies,
                    ItemPos(bin_type.area() / item_type.area()));
            if ((item_type.oriented
                    && (item_type.rect.x > bin_type.rect.x
                        || item_type.rect.y > bin_type.rect.y))
                    || (!item_type.oriented
                        && (item_type.rect.x > bin_type.rect.x
                            || item_type.rect.y > bin_type.rect.y)
                        && (item_type.rect.y > bin_type.rect.x
                            || item_type.rect.x > bin_type.rect.y))) {
               item_copies[item_type_id] = 0;
            }
        }

        // Objective: maximize the overall profit.
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = instance.item_type(item_type_id);
            for (ItemPos copy = 0;
                    copy < item_copies[item_type_id];
                    ++copy) {
                x[item_type_id].push_back(column_lower_bounds.size());
                highs_to_orig.push_back({item_type_id, copy});
                column_lower_bounds.push_back(0);
                column_upper_bounds.push_back(1);
                column_types.push_back(1);
                objective.push_back(item_type.profit);
            }
        }
        // Set objective sense.
        HighsInt sense = kHighsObjSenseMaximize;

        // Constraints.
        // will be increased each time a constraint is added.
        int number_of_rows = 0;
        std::vector<int> number_of_elements_in_rows;

        // Constraint: area.
        {
            // Initialize new row
            a_start.push_back(a_value.size());
            number_of_elements_in_rows.push_back(0);
            number_of_rows++;
            // Add row elements.
            for (ItemTypeId item_type_id = 0;
                    item_type_id < instance.number_of_item_types();
                    ++item_type_id) {
                const ItemType& item_type = instance.item_type(item_type_id);
                for (ItemPos copy = 0;
                        copy < item_copies[item_type_id];
                        ++copy) {
                    a_value.push_back(item_type.area());
                    a_index.push_back(x[item_type_id][copy]);
                    number_of_elements_in_rows.back()++;
                }
            }
            // Add row bounds
            row_lower_bounds.push_back(0);
            row_upper_bounds.push_back(bin_type.area());
        }

        // Constraints: dominated copies.
        // x_{j, c + 1} \le x_{j, c}
        // <=> 0 \le x_{j, c} - x_{j, c + 1} \le inf
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = instance.item_type(item_type_id);
            for (ItemPos copy = 0;
                    copy < item_copies[item_type_id] - 1;
                    ++copy) {
                // Initialize new row
                a_start.push_back(a_value.size());
                number_of_elements_in_rows.push_back(0);
                number_of_rows++;
                // Add row elements.
                a_value.push_back(1);
                a_index.push_back(x[item_type_id][copy]);
                number_of_elements_in_rows.back()++;
                a_value.push_back(-1);
                a_index.push_back(x[item_type_id][copy + 1]);
                number_of_elements_in_rows.back()++;
                // Add row bounds
                row_lower_bounds.push_back(0);
                row_upper_bounds.push_back(1.0e30);
            }
        }

        // Constraints: dominated items.
        // If j2 dominates j:
        // sum_c x_{j, c} <= x_{j2, copies[j2] - 1}
        // <=> 0 <= x_{j2, copies[j2] - 1} - sum_c x_{j, c} <= inf
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            if (item_copies[item_type_id] == 0)
                continue;
            const ItemType& item_type = instance.item_type(item_type_id);
            for (ItemTypeId item_type_id_2 = 0;
                    item_type_id_2 < instance.number_of_item_types();
                    ++item_type_id_2) {
                if (item_type_id_2 == item_type_id)
                    continue;
                if (item_copies[item_type_id_2] == 0)
                    continue;
                const ItemType& item_type_2 = instance.item_type(item_type_id_2);
                if (item_type_2.profit >= item_type.profit
                        && item_type_2.rect.x <= item_type.rect.x
                        && item_type_2.rect.y <= item_type.rect.y) {
                    // Initialize new row
                    a_start.push_back(a_value.size());
                    number_of_elements_in_rows.push_back(0);
                    number_of_rows++;
                    // Add row elements.
                    ItemPos copy_2 = item_copies[item_type_id_2] - 1;
                    a_value.push_back(1);
                    a_index.push_back(x[item_type_id_2][copy_2]);
                    number_of_elements_in_rows.back()++;
                    for (ItemPos copy = 0;
                            copy < item_copies[item_type_id];
                            ++copy) {
                        a_value.push_back(-1);
                        a_index.push_back(x[item_type_id][copy]);
                        number_of_elements_in_rows.back()++;
                    }
                    // Add row bounds
                    row_lower_bounds.push_back(0);
                    row_upper_bounds.push_back(1.0e30);
                }
            }
        }

        // Constraints: incompatible items.
        // sum_j x_{j, 0} <= 1
        {
            // Initialize new row
            a_start.push_back(a_value.size());
            number_of_elements_in_rows.push_back(0);
            number_of_rows++;
            // Add row elements.
            for (ItemTypeId item_type_id = 0;
                    item_type_id < instance.number_of_item_types();
                    ++item_type_id) {
                if (item_copies[item_type_id] == 0)
                    continue;
                const ItemType& item_type = instance.item_type(item_type_id);
                if ((item_type.oriented
                            && item_type.rect.x > bin_type.rect.x / 2
                            && item_type.rect.y > bin_type.rect.y / 2)
                        || (!item_type.oriented
                            && item_type.rect.min() > bin_type.rect.max() / 2)) {
                    a_value.push_back(1);
                    a_index.push_back(x[item_type_id][0]);
                    number_of_elements_in_rows.back()++;
                }
            }
            // Add row bounds
            row_lower_bounds.push_back(0);
            row_upper_bounds.push_back(1);
        }

        // Constraints: incompatible item pairs.
        // x_{j, 0} + x_j{j2, 0} <= 1
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            if (item_copies[item_type_id] == 0)
                continue;
            const ItemType& item_type = instance.item_type(item_type_id);
            for (ItemTypeId item_type_id_2 = item_type_id + 1;
                    item_type_id_2 < instance.number_of_item_types();
                    ++item_type_id_2) {
                if (item_copies[item_type_id_2] == 0)
                    continue;
                const ItemType& item_type_2 = instance.item_type(item_type_id_2);
                if ((!item_type.oriented
                            && !item_type_2.oriented
                            && item_type.rect.min() + item_type_2.rect.min()
                            > bin_type.rect.max())
                        || (item_type.oriented
                            && item_type_2.oriented
                            && item_type.rect.x + item_type_2.rect.x > bin_type.rect.x
                            && item_type.rect.y + item_type_2.rect.y > bin_type.rect.y)
                        || (!item_type.oriented
                            && item_type_2.oriented
                            && item_type.rect.max() + item_type_2.rect.x > bin_type.rect.x
                            && item_type.rect.max() + item_type_2.rect.y > bin_type.rect.y)
                        || (item_type.oriented
                            && !item_type_2.oriented
                            && item_type.rect.x + item_type_2.rect.max() > bin_type.rect.x
                            && item_type.rect.y + item_type_2.rect.max() > bin_type.rect.y)) {
                    //std::cout << "Add pair" << std::endl;
                    // Initialize new row
                    a_start.push_back(a_value.size());
                    number_of_elements_in_rows.push_back(0);
                    number_of_rows++;
                    // Add row elements.
                    a_value.push_back(1);
                    a_index.push_back(x[item_type_id][0]);
                    number_of_elements_in_rows.back()++;
                    a_value.push_back(1);
                    a_index.push_back(x[item_type_id_2][0]);
                    number_of_elements_in_rows.back()++;
                    // Add row bounds
                    row_lower_bounds.push_back(0);
                    row_upper_bounds.push_back(1);
                }
            }
        }

        // Constraints: cuts.
        // sum_{j in C} x_{j, c} <= |C| - 1
        for (const std::vector<std::pair<ItemTypeId, ItemPos>>& cut: milp_cuts) {
            //std::cout << "Add cut" << std::endl;
            // Initialize new row
            a_start.push_back(a_value.size());
            number_of_elements_in_rows.push_back(0);
            number_of_rows++;
            // Add row elements.
            ItemPos cut_size = 0;
            for (const std::pair<ItemTypeId, ItemPos>& p: cut) {
                ItemTypeId item_type_id = p.first;
                ItemPos copies = p.second;
                cut_size += copies;
                for (ItemPos copy = 0;
                        copy < copies;
                        ++copy) {
                    a_value.push_back(1);
                    a_index.push_back(x[item_type_id][copy]);
                    number_of_elements_in_rows.back()++;
                }
            }
            // Add row bounds
            row_lower_bounds.push_back(0);
            row_upper_bounds.push_back(cut_size - 1);
        }

        // Solve MILP model.
        double offset = 0.0;

        // Create a Highs instance
        void* highs = Highs_create();

        // Reduce printout.
        Highs_setBoolOptionValue(
                highs,
                "log_to_console",
                false);

        // Pass the MIP to HIGHS.
        Highs_passMip(
                highs,
                column_lower_bounds.size(),
                row_lower_bounds.size(),
                a_value.size(),
                a_format,
                sense,
                offset,
                objective.data(),
                column_lower_bounds.data(),
                column_upper_bounds.data(),
                row_lower_bounds.data(),
                row_upper_bounds.data(),
                a_start.data(),
                a_index.data(),
                a_value.data(),
                column_types.data());

        // Set time limit.
        if (parameters.timer.remaining_time() < std::numeric_limits<double>::infinity()) {
            Highs_setDoubleOptionValue(
                    highs,
                    "time_limit",
                    parameters.timer.remaining_time());
        }

        // Reduce printout.
        Highs_setBoolOptionValue(
                highs,
                "log_to_console",
                false);

        // Solve the incumbent model
        //std::cout << "Run Highs..." << std::endl;
        //Highs_writeModel(highs, "model.mps");
        HighsInt run_status = Highs_run(highs);

        // Check end.
        if (parameters.timer.needs_to_end())
            break;

        HighsInt model_status = Highs_getModelStatus(highs);
        HighsInt primal_solution_status = -1;
        Highs_getIntInfoValue(highs, "primal_solution_status", &primal_solution_status);

        // Retrieve solution.
        std::vector<std::pair<ItemTypeId, ItemPos>> selected_items;
        if (model_status == kHighsModelStatusInfeasible) {
            // Infeasible.
            // Something's wrong.
            throw std::runtime_error(
                    "packingsolver::rectangle::benders_decomposition: "
                    "the model shouldn't be infeasible.");

        } else if (model_status == kHighsModelStatusOptimal
                || primal_solution_status == kHighsSolutionStatusFeasible) {
            // Optimal or feasible solution found.

            // Retrieve solution.
            std::vector<double> column_values(column_lower_bounds.size(), 0.0);
            std::vector<double> column_duals(column_lower_bounds.size(), 0.0);
            std::vector<double> row_values(row_lower_bounds.size(), 0.0);
            std::vector<double> row_duals(row_lower_bounds.size(), 0.0);
            HighsInt highs_return_code = Highs_getSolution(
                    highs,
                    column_values.data(),
                    column_duals.data(),
                    row_values.data(),
                    row_duals.data());

            Solution solution(instance);
            for (ItemTypeId item_type_id = 0;
                    item_type_id < instance.number_of_item_types();
                    ++item_type_id) {
                const ItemType& item_type = instance.item_type(item_type_id);
                for (ItemPos copy = item_copies[item_type_id] - 1;
                        copy >= 0;
                        --copy) {
                    double value = column_values[x[item_type_id][copy]];
                    //std::cout << "item_type_id " << item_type_id
                    //    << " copy " << copy + 1
                    //    << " val " << value
                    //    << std::endl;
                    if (value > 0.5) {
                        //std::cout << "item_type_id " << item_type_id
                        //    << " copy " << copy + 1
                        //    << std::endl;
                        selected_items.push_back({item_type_id, copy + 1});
                        break;
                    }
                }
            }
        } else {
            // No feasible solution found.
            break;
        }

        // Build subproblem instance.
        InstanceBuilder sub_instance_builder;
        sub_instance_builder.set_objective(Objective::Knapsack);
        sub_instance_builder.set_parameters(instance.parameters());
        sub_instance_builder.add_bin_type(bin_type, 1);
        std::vector<ItemTypeId> sub_to_orig;
        for (const auto& p: selected_items) {
            const ItemType& item_type = instance.item_type(p.first);
            ItemPos copies = p.second;
            sub_instance_builder.add_item_type(
                    item_type,
                    item_type.profit,
                    copies);
            sub_to_orig.push_back(p.first);
        }
        Instance sub_instance = sub_instance_builder.build();

        // Solve.
        //std::cout << "Run subproblem..." << std::endl;
        OptimizeParameters sub_parameters;
        sub_parameters.verbosity_level = 0;
        sub_parameters.timer = parameters.timer;
        sub_parameters.optimization_mode = OptimizationMode::NotAnytime;
        sub_parameters.not_anytime_tree_search_queue_size = parameters.subproblem_queue_size;
        sub_parameters.use_tree_search = true;
        sub_parameters.tree_search_guides = {0, 1};
        auto sub_output = optimize(sub_instance, sub_parameters);
        const Solution& sub_solution = sub_output.solution_pool.best();

        // Update solution.
        Solution solution(instance);
        solution.append(
                sub_solution,
                0,  // bin_pos
                1,  // copies
                {0},  // bin_type_ids
                sub_to_orig);
        std::stringstream ss;
        ss << "BD it " << output.number_of_iterations;
        algorithm_formatter.update_solution(solution, ss.str());

        // Check end.
        if (parameters.timer.needs_to_end())
            break;

        if (sub_solution.full()) {
            // If all items have been packed, stop.
            break;
        } else {
            // Otherwise, add the corresponding cut.
            milp_cuts.push_back(selected_items);
        }
    }

    algorithm_formatter.end();
    return output;
}
