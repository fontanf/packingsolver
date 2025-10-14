#include "rectangle/benders_decomposition.hpp"

#include "packingsolver/rectangle/algorithm_formatter.hpp"
#include "packingsolver/rectangle/instance_builder.hpp"
#include "packingsolver/rectangle/optimize.hpp"

using namespace packingsolver;
using namespace packingsolver::rectangle;

BendersDecompositionOutput packingsolver::rectangle::benders_decomposition(
        const Instance& instance,
        const BendersDecompositionParameters& parameters)
{
    //std::cout << "benders_decomposition..." << std::endl;
    const BinType& bin_type = instance.bin_type(0);
    double multiplier_area = largest_power_of_two_lesser_or_equal(bin_type.area());
    double multiplier_profit = largest_power_of_two_lesser_or_equal(instance.largest_item_profit());

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
        mathoptsolverscmake::MilpModel milp_model;

        // Variables.
        // x[item_type_id][copy]
        std::vector<std::pair<ItemTypeId, ItemPos>> milp_to_orig;
        std::vector<std::vector<int>> x(instance.number_of_item_types());

        // Set objective sense.
        milp_model.objective_direction = mathoptsolverscmake::ObjectiveDirection::Maximize;

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

        Profit highest_profit = 0.0;
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = instance.item_type(item_type_id);
            highest_profit = (std::max)(highest_profit, item_type.profit);
        }

        // Objective: maximize the overall profit.
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = instance.item_type(item_type_id);
            x[item_type_id] = std::vector<int>(item_copies[item_type_id]);
            for (ItemPos copy = 0;
                    copy < item_copies[item_type_id];
                    ++copy) {
                x[item_type_id][copy] = milp_model.variables_lower_bounds.size();
                milp_model.variables_lower_bounds.push_back(0);
                milp_model.variables_upper_bounds.push_back(1);
                milp_model.variables_types.push_back(mathoptsolverscmake::VariableType::Binary);
                milp_model.objective_coefficients.push_back(item_type.profit / multiplier_profit);
            }
        }

        // Constraints.

        // Constraint: area.
        {
            // Initialize new row.
            milp_model.constraints_starts.push_back(milp_model.elements_variables.size());
            // Add row elements.
            for (ItemTypeId item_type_id = 0;
                    item_type_id < instance.number_of_item_types();
                    ++item_type_id) {
                const ItemType& item_type = instance.item_type(item_type_id);
                for (ItemPos copy = 0;
                        copy < item_copies[item_type_id];
                        ++copy) {
                    milp_model.elements_variables.push_back(x[item_type_id][copy]);
                    milp_model.elements_coefficients.push_back((double)item_type.area() / multiplier_area);
                }
            }
            // Add row bounds.
            milp_model.constraints_lower_bounds.push_back(-std::numeric_limits<double>::infinity());
            milp_model.constraints_upper_bounds.push_back((double)bin_type.area() / multiplier_area);
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
                // Initialize new row.
                milp_model.constraints_starts.push_back(milp_model.elements_variables.size());
                // Add row elements.
                milp_model.elements_variables.push_back(x[item_type_id][copy]);
                milp_model.elements_coefficients.push_back(1.0);
                milp_model.elements_variables.push_back(x[item_type_id][copy + 1]);
                milp_model.elements_coefficients.push_back(-1.0);
                // Add row bounds.
                milp_model.constraints_lower_bounds.push_back(0);
                milp_model.constraints_upper_bounds.push_back(std::numeric_limits<double>::infinity());
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
                    // Initialize new row.
                    milp_model.constraints_starts.push_back(milp_model.elements_variables.size());
                    // Add row elements.
                    ItemPos copy_2 = item_copies[item_type_id_2] - 1;
                    milp_model.elements_variables.push_back(x[item_type_id_2][copy_2]);
                    milp_model.elements_coefficients.push_back(1.0);
                    for (ItemPos copy = 0;
                            copy < item_copies[item_type_id];
                            ++copy) {
                        milp_model.elements_variables.push_back(x[item_type_id][copy]);
                        milp_model.elements_coefficients.push_back(-1.0);
                    }
                    // Add row bounds.
                    milp_model.constraints_lower_bounds.push_back(0);
                    milp_model.constraints_upper_bounds.push_back(std::numeric_limits<double>::infinity());
                }
            }
        }

        // Constraints: incompatible items.
        // sum_j x_{j, 0} <= 1
        // Initialize new row.
        milp_model.constraints_starts.push_back(milp_model.elements_variables.size());
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
                milp_model.elements_variables.push_back(x[item_type_id][0]);
                milp_model.elements_coefficients.push_back(1.0);
            }
        }
        // Add row bounds
        milp_model.constraints_lower_bounds.push_back(-std::numeric_limits<double>::infinity());
        milp_model.constraints_upper_bounds.push_back(1);

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
                    // Initialize new row.
                    milp_model.constraints_starts.push_back(milp_model.elements_variables.size());
                    // Add row elements.
                    milp_model.elements_variables.push_back(x[item_type_id][0]);
                    milp_model.elements_coefficients.push_back(1.0);
                    milp_model.elements_variables.push_back(x[item_type_id_2][0]);
                    milp_model.elements_coefficients.push_back(1.0);
                    // Add row bounds.
                    milp_model.constraints_lower_bounds.push_back(-std::numeric_limits<double>::infinity());
                    milp_model.constraints_upper_bounds.push_back(1);
                }
            }
        }

        // Constraints: cuts.
        // sum_{j in C} x_{j, c} <= |C| - 1
        for (const std::vector<std::pair<ItemTypeId, ItemPos>>& cut: milp_cuts) {
            //std::cout << "Add cut" << std::endl;
            // Initialize new row.
            milp_model.constraints_starts.push_back(milp_model.elements_variables.size());
            // Add row elements.
            ItemPos cut_size = 0;
            for (const std::pair<ItemTypeId, ItemPos>& p: cut) {
                ItemTypeId item_type_id = p.first;
                ItemPos copies = p.second;
                cut_size += copies;
                for (ItemPos copy = 0;
                        copy < copies;
                        ++copy) {
                    milp_model.elements_variables.push_back(x[item_type_id][copy]);
                    milp_model.elements_coefficients.push_back(1.0);
                }
            }
            // Add row bounds.
            milp_model.constraints_lower_bounds.push_back(-std::numeric_limits<double>::infinity());
            milp_model.constraints_upper_bounds.push_back(cut_size - 1);
        }

        std::vector<double> milp_solution;
        if (parameters.solver == mathoptsolverscmake::SolverName::Highs) {
#ifdef HIGHS_FOUND
            Highs highs;
            mathoptsolverscmake::reduce_printout(highs);
            mathoptsolverscmake::set_time_limit(highs, parameters.timer.remaining_time());
            highs.setOptionValue("parallel", "off");
            highs.setOptionValue("mip_allow_restart", false);
            //mathoptsolverscmake::set_log_file(highs, "highs.log");
            mathoptsolverscmake::load(highs, milp_model);
            mathoptsolverscmake::solve(highs);
            milp_solution = mathoptsolverscmake::get_solution(highs);
#else
            throw std::invalid_argument("");
#endif

        } else {
            throw std::invalid_argument("");
        }

        // Check end.
        if (parameters.timer.needs_to_end())
            break;

        // Retrieve solution.
        std::vector<std::pair<ItemTypeId, ItemPos>> selected_items;
        if (milp_solution.empty()) {
            throw std::runtime_error(
                    FUNC_SIGNATURE + ": "
                    "the MILP model should not be infeasible.");
        }
        Solution solution(instance);
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = instance.item_type(item_type_id);
            for (ItemPos copy = item_copies[item_type_id] - 1;
                    copy >= 0;
                    --copy) {
                double value = milp_solution[x[item_type_id][copy]];
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
        if (sub_solution.number_of_bins() > 0) {
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
            //std::cout << "it " << output.number_of_iterations
            //    << " val " << solution.profit()
            //    << std::endl;
        }
        //sub_solution.format(std::cout, 1);

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
