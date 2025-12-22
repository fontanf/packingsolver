#include "boxstacks/sequential_onedimensional_rectangle.hpp"

#include "packingsolver/boxstacks/algorithm_formatter.hpp"

#include "packingsolver/onedimensional/instance_builder.hpp"

#include "packingsolver/rectangle/instance_builder.hpp"
#include "rectangle/branching_scheme.hpp"
#include "rectangle/solution_builder.hpp"

#include "treesearchsolver/iterative_beam_search_2.hpp"

using namespace packingsolver;
using namespace packingsolver::boxstacks;

struct Stack
{
    /** Items; */
    std::vector<ItemTypeId> items;

    /** Profit. */
    Profit profit = 0.0;

    /** Profit for the rectangle subproblem. */
    Profit rectanlge_profit = 0.0;

    /** Weight. */
    Weight weight = 0;

    /** 0: lengthwise; 1: widthwise; 2: any. */
    int rotation;
};

struct StackabilityGroup
{
    /** Stackability. */
    StackabilityId stackability_id;

    /** Group. */
    GroupId group_id;

    /** x-length of the stack. */
    Length x;

    /** y-length of the stack. */
    Length y;

    /**
     * Item types added to the onedimensional subproblem.
     *
     * This array is used to retrieve the indices of the item types in the
     * original problem from their ids in the subproblem.
     */
    std::vector<ItemTypeId> item_types;

    /**
     * For each rotation, the stacks found by the onedimensional subproblem.
     */
    std::vector<std::vector<Stack>> stacks = std::vector<std::vector<Stack>>(3);

    /**
     * For each rotation, the list of locations where the corresponding items
     * have been packed in the rectangle solution.
     */
    std::vector<std::vector<StackId>> location_ids = std::vector<std::vector<StackId>>(3);
};

struct Location
{
    /** x-coordinate. */
    Length x;

    /** y-coordinate. */
    Length y;

    /** x-length. */
    Length lx;

    /** y-length. */
    Length ly;

    /** Stackability group. */
    StackId stackability_group_pos;

    /**
     * Rotation.
     *
     * Related to the corresponding item type in the rectangle subproblem
     * instance. It can have value '0', '1', or '2'.
     */
    int rotation;

    /**
     * Boolean indicating if the location is rotated in the rectangle solution.
     */
    bool rotate;

    /** Position of the stack assigned to the location. */
    StackId stack_position = -1;
};

struct SequentialOneDimensionalRectangleSubproblemOutput
{
    SequentialOneDimensionalRectangleSubproblemOutput(const Instance& instance):
        solution(instance) { }

    /** Solution. */
    Solution solution;

    /** Profit of the solution before the repair step. */
    Profit profit_before_repair = 0.0;
};

SequentialOneDimensionalRectangleSubproblemOutput sequential_onedimensional_rectangle_subproblem(
        const Instance& instance,
        const SequentialOneDimensionalRectangleParameters& parameters,
        SequentialOneDimensionalRectangleOutput& sor_output,
        AlgorithmFormatter& sor_algorithm_formatter,
        const Solution& fixed_items,
        std::vector<StackabilityGroup> stackability_groups,
        const rectangle::Instance& rectangle_instance,
        const std::vector<std::tuple<StackabilityId, int, StackId>>& rectangle2boxstacks,
        const rectangle::BranchingScheme::Parameters rectangle_parameters)
{
    SequentialOneDimensionalRectangleSubproblemOutput output(instance);
    FFOT_LOG_FOLD_START(
            parameters.logger,
            "it " << sor_output.number_of_iterations
            << " guide " << rectangle_parameters.guide_id
            << std::endl);

    // Solve rectanlge instance.
    //rectangle_parameters.fixed_items = &rectangle_fixed_items;
    //rectangle_parameters.fixed_items->format(std::cout, 3);
    rectangle::BranchingScheme rectangle_branching_scheme(rectangle_instance, rectangle_parameters);
    treesearchsolver::IterativeBeamSearch2Parameters<rectangle::BranchingScheme> ibs_parameters;
    ibs_parameters.verbosity_level = 0;
    ibs_parameters.timer = parameters.timer;
    ibs_parameters.minimum_size_of_the_queue = parameters.rectangle_queue_size;
    ibs_parameters.maximum_size_of_the_queue = parameters.rectangle_queue_size;
    auto rectangle_begin = std::chrono::steady_clock::now();
    auto rectangle_output = treesearchsolver::iterative_beam_search_2<rectangle::BranchingScheme>(
            rectangle_branching_scheme,
            ibs_parameters);
    auto rectangle_end = std::chrono::steady_clock::now();
    std::chrono::duration<double> rectangle_time_span
        = std::chrono::duration_cast<std::chrono::duration<double>>(rectangle_end - rectangle_begin);
    sor_output.rectangle_time += rectangle_time_span.count();
    sor_output.number_of_rectangle_calls++;
    auto rectangle_solution = rectangle_branching_scheme.to_solution(rectangle_output.solution_pool.best());
    FFOT_LOG(
            parameters.logger,
            "rectangle_solution.number_of_items " << rectangle_solution.number_of_items()
            << " / " << rectangle_instance.number_of_items()
            << std::endl);

    // For each stackability code x group, count the number of locations.
    std::vector<Location> locations;
    for (BinPos bin_pos = 0; bin_pos < rectangle_solution.number_of_different_bins(); ++bin_pos) {
        const auto& bin = rectangle_solution.bin(bin_pos);
        for (const auto& item: bin.items) {

            // If the item corresponds to a stack from the fixed part of the
            // solution, skip it.
            bool stop = false;
            for (BinPos bin_pos = 0; bin_pos < fixed_items.number_of_different_bins(); ++bin_pos) {
                const SolutionBin& solution_bin = fixed_items.bin(bin_pos);
                for (StackId stack_pos = 0; stack_pos < (StackId)solution_bin.stacks.size(); ++stack_pos) {
                    const SolutionStack& solution_stack = solution_bin.stacks[stack_pos];
                    if (solution_stack.x_start == item.bl_corner.x
                            && solution_stack.y_start == item.bl_corner.y) {
                        stop = true;
                    }
                }
            }
            if (stop)
                continue;

            StackabilityId stackability_group_pos = std::get<0>(rectangle2boxstacks[item.item_type_id]);
            int rotation = std::get<1>(rectangle2boxstacks[item.item_type_id]);
            StackId stack_pos = std::get<2>(rectangle2boxstacks[item.item_type_id]);

            Location location;
            StackId location_id = locations.size();
            location.x = item.bl_corner.x;
            location.y = item.bl_corner.y;
            //std::cout << "x " << location.x << " y " << location.y << std::endl;
            location.stackability_group_pos = stackability_group_pos;
            location.rotation = rotation;
            location.stack_position = stack_pos;
            if (rotation == 0) {
                location.rotate = false;
            } else if (rotation == 1) {
                location.rotate = true;
            } else if (rotation == 2) {
                location.rotate = item.rotate;
            }
            const StackabilityGroup& stackability_group = stackability_groups[stackability_group_pos];
            location.lx = (!location.rotate)? stackability_group.x: stackability_group.y;
            location.ly = (!location.rotate)? stackability_group.y: stackability_group.x;
            location.stack_position = stack_pos;
            locations.push_back(location);
            stackability_groups[stackability_group_pos].location_ids[rotation].push_back(location_id);
        }
    }

    // Re-order the stacks packed to put the lightest ones first.
    for (StackId stackability_group_pos = 0; stackability_group_pos < (StackId)stackability_groups.size(); ++stackability_group_pos) {
        StackabilityGroup& stackability_group = stackability_groups[stackability_group_pos];
        for (int rotation = 0; rotation < 3; ++rotation) {
            const auto& stacks = stackability_group.stacks[rotation];
            //std::vector<StackId> selected_stacks(stacks.size());
            //std::iota(selected_stacks.begin(), selected_stacks.end(), 0);
            std::vector<StackId> selected_stacks;
            for (StackId location_id: stackability_group.location_ids[rotation])
                selected_stacks.push_back(locations[location_id].stack_position);
            // Sort by profit.
            std::sort(
                    selected_stacks.begin(),
                    selected_stacks.end(),
                    [&stacks](
                        StackId stack_pos_1,
                        StackId stack_pos_2) -> bool
                    {
                        return stacks[stack_pos_1].profit
                            > stacks[stack_pos_2].profit;
                    });
            // Unselect less profitable stacks.
            while (selected_stacks.size()
                    > stackability_group.location_ids[rotation].size()) {
                selected_stacks.pop_back();
            }
            // Sort the stacks selected by decreasing order of weight.
            sort(
                    selected_stacks.begin(),
                    selected_stacks.end(),
                    [&stacks, &rectangle_parameters](
                        StackId stack_pos_1,
                        StackId stack_pos_2) -> bool
                    {
                        if (rectangle_parameters.guide_id != 9) {
                            return stacks[stack_pos_1].weight
                                < stacks[stack_pos_2].weight;
                        } else {
                            return stacks[stack_pos_1].weight
                                > stacks[stack_pos_2].weight;
                        }
                    });
            // Sort locations in increasing order of x.
            sort(
                    stackability_group.location_ids[rotation].begin(),
                    stackability_group.location_ids[rotation].end(),
                    [&locations](StackId location_id_1, StackId location_id_2) -> bool
                    {
                        return locations[location_id_1].x
                            < locations[location_id_2].x;
                    });
            // Compute location positions.
            for (StackId pos = 0; pos < (StackId)selected_stacks.size(); ++pos) {
                StackId location_id = stackability_group.location_ids[rotation][pos];
                locations[location_id].stack_position = selected_stacks[pos];
            }
        }
    }

    // Build boxstacks solution.
    Solution solution(instance);
    BinPos bin_pos = 0;
    if (sor_output.number_of_iterations == 0) {
        bin_pos = solution.add_bin(0, 1);
    } else {
        solution = fixed_items;
    }
    for (auto it = locations.begin(); it != locations.end(); ++it) {
        Location& location = *it;
        StackabilityGroup& stackability_group = stackability_groups[location.stackability_group_pos];
        // Get the stack to add.
        Stack& stack = stackability_group.stacks[location.rotation][location.stack_position];
        StackId stack_id = solution.add_stack(
                bin_pos,
                location.x,
                location.x + location.lx,
                location.y,
                location.y + location.ly);
        // Add the items from the stack.
        for (auto it2 = stack.items.rbegin(); it2 != stack.items.rend(); ++it2) {
            solution.add_item(
                    bin_pos,
                    stack_id,
                    *it2,
                    location.rotate);
        }
    }
    ItemPos number_of_items_before_repair = solution.number_of_items();
    sor_output.maximum_number_of_items = std::max(
            sor_output.maximum_number_of_items,
            number_of_items_before_repair);
    output.profit_before_repair = solution.profit();
    FFOT_LOG(
            parameters.logger,
            "number of items " << solution.number_of_items() << std::endl
            << "profit " << solution.profit() << std::endl
            << "middle axle weight constraints violation " << solution.compute_middle_axle_weight_constraints_violation() << std::endl
            << "rear axle weight constraints violation " << solution.compute_rear_axle_weight_constraints_violation() << std::endl);
    //solution.write("sol_" + std::to_string(sor_output.number_of_iterations)
    //        + "_" + std::to_string(rectangle_parameters.guide_id)
    //        + "_" + std::to_string(rectangle_instance.number_of_items())
    //        + ".csv");

    // Save the solution if feasible.
    if (solution.compute_weight_constraints_violation() == 0) {
        std::stringstream ss;
        ss << "iteration " << sor_output.number_of_iterations;
        sor_algorithm_formatter.update_solution(solution, ss.str());
        parameters.new_solution_callback(sor_output);
    }
    output.solution = solution;
    FFOT_LOG_FOLD_END(parameters.logger, "");
    return output;
}

const SequentialOneDimensionalRectangleOutput boxstacks::sequential_onedimensional_rectangle(
        const Instance& instance,
        const SequentialOneDimensionalRectangleParameters& parameters)
{
    FFOT_LOG_FOLD_START(
            parameters.logger,
            "sequential_onedimensional_rectangle" << std::endl);

    SequentialOneDimensionalRectangleOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    const BinType& bin_type = instance.bin_type(0);
    Length yi = bin_type.box.y;
    Length xi = bin_type.box.x;
    BinPos bin_pos = 0;

    std::vector<Solution> fixed_items_solutions;
    auto cmp = [](const Solution& solution_1, const Solution& solution_2)
    {
        return solution_1.x_max() < solution_2.x_max();
    };
    std::set<Solution, decltype(cmp)> queue(cmp);
    Solution solution_empty(instance);
    solution_empty.add_bin(0, 1);
    queue.insert(solution_empty);
    while (!queue.empty()) {
        Solution solution = *queue.begin();
        fixed_items_solutions.push_back(solution);
        //std::cout << "x " << solution.x_max()
        //    << " w " << bin_type.semi_trailer_truck_data.compute_axle_weights(
        //                        solution.bin(0).weight_weighted_sum.front(), solution.bin(0).weight.front()).first
        //    << " n " << solution.number_of_items()
        //    << std::endl;
        queue.erase(queue.begin());
        GroupId highest_group_id = 0;
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = instance.item_type(item_type_id);
            if (solution.item_copies(item_type_id) == item_type.copies)
                continue;
            highest_group_id = (std::max)(highest_group_id, item_type.group_id);
        }
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = instance.item_type(item_type_id);
            if (solution.item_copies(item_type_id) == item_type.copies)
                continue;
            if (item_type.group_id != highest_group_id)
                continue;
            for (int rotation = 0; rotation < 6; ++rotation) {
                if (instance.item_type(item_type_id).can_rotate(rotation)) {
                    Solution solution_child = solution;
                    Length xj = item_type.x(rotation);
                    Length yj = item_type.y(rotation);
                    if (yj > yi)
                        continue;
                    Length x_start = solution.x_max();
                    Length x_end = x_start + xj;
                    Length y_start = std::max(Length(0), yi / 2 - yj / 2);
                    Length y_end = y_start + yj;
                    if (x_end > xi)
                        continue;
                    StackId stack_pos = solution_child.add_stack(
                            bin_pos,
                            x_start,
                            x_end,
                            y_start,
                            y_end);
                    solution_child.add_item(
                            bin_pos,
                            stack_pos,
                            item_type_id,
                            rotation);
                    // Check if the solution is dominated.
                    bool dominated = false;
                    for (auto it = queue.begin(); it != queue.end();) {
                        std::pair<double, double> axle_weights = bin_type.semi_trailer_truck_data.compute_axle_weights(
                                it->bin(0).weight_weighted_sum.front(), it->bin(0).weight.front());
                        std::pair<double, double> axle_weights_child = bin_type.semi_trailer_truck_data.compute_axle_weights(
                                solution_child.bin(0).weight_weighted_sum.front(), solution_child.bin(0).weight.front());
                        if (it->x_max() <= solution_child.x_max()
                                && it->item_weight() >= solution_child.item_weight()
                                && axle_weights.first <= axle_weights_child.first) {
                            dominated = true;
                            break;
                        }
                        if (it->x_max() >= solution_child.x_max()
                                && it->item_weight() <= solution_child.item_weight()
                                && axle_weights.first >= axle_weights_child.first) {
                            queue.erase(it++);
                        } else {
                            ++it;
                        }
                    }
                    if (dominated)
                        continue;
                    queue.insert(solution_child);
                }
            }
        }
    }
    FFOT_LOG(
            parameters.logger,
            "fixed_items_solutions.size() " << fixed_items_solutions.size()
            << std::endl);

    ItemPos fixed_items_solutions_pos_lower_bound = 0;
    ItemPos fixed_items_solutions_pos_upper_bound = fixed_items_solutions.size() - 1;
    ItemPos fixed_items_solutions_pos = 0;

    for (output.number_of_iterations = 0;; ++output.number_of_iterations) {

        // Part of solution which is fixed.
        Solution fixed_items = fixed_items_solutions[fixed_items_solutions_pos];
        FFOT_LOG_FOLD_START(
                parameters.logger,
                "iteration " << output.number_of_iterations << std::endl
                << "fixed_items.number_of_items() " << fixed_items.number_of_items() << std::endl
                << "fixed_items.x_max() " << fixed_items.x_max() << std::endl);

        // Compute the number of copies of each item type to pack, considering
        // the part of the solution which is already fixed.
        std::vector<ItemPos> copies(instance.number_of_item_types(), 0);
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            copies[item_type_id] = instance.item_type(item_type_id).copies;
            if (output.number_of_iterations > 0)
                copies[item_type_id] -= fixed_items.item_copies(item_type_id);
        }

        // Build stacks by solving a one-dimensional variable-sized bin packing
        // for each stackability group.
        // Get all pairs stackability id x group.
        std::vector<std::pair<StackabilityId, GroupId>> stackability_ids;
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = instance.item_type(item_type_id);
            stackability_ids.push_back({item_type.stackability_id, item_type.group_id});
        }
        std::sort(stackability_ids.begin(), stackability_ids.end());
        stackability_ids.erase(unique(stackability_ids.begin(), stackability_ids.end()), stackability_ids.end());
        // For each stackability group.
        std::vector<StackabilityGroup> stackability_groups;
        for (const auto& p: stackability_ids) {
            StackabilityGroup stackability_group;
            stackability_group.stackability_id = p.first;
            stackability_group.group_id = p.second;
            for (ItemTypeId item_type_id = 0;
                    item_type_id < instance.number_of_item_types();
                    ++item_type_id) {
                const ItemType& item_type = instance.item_type(item_type_id);
                if (item_type.stackability_id == stackability_group.stackability_id
                        && item_type.group_id == stackability_group.group_id) {
                    stackability_group.x = instance.item_type(item_type_id).box.x;
                    stackability_group.y = instance.item_type(item_type_id).box.y;
                    break;
                }
            }
            //std::cout << "stackability_id " << stackability_group.stackability_id
            //    << " group_id " << stackability_group.group_id
            //    << " x " << stackability_group.x
            //    << " y " << stackability_group.y
            //    << std::endl;

            // Build onedimensional instance.
            onedimensional::InstanceBuilder onedim_instance_builder;
            onedim_instance_builder.set_objective(Objective::VariableSizedBinPacking);

            // Add bin types.
            BinTypeId onedim_bin_type_id_0 = onedim_instance_builder.add_bin_type(
                    instance.bin_type(0).box.z,
                    10,
                    instance.number_of_items());
            onedim_instance_builder.set_bin_type_maximum_weight(
                    onedim_bin_type_id_0,
                    instance.bin_type(0).maximum_stack_density
                    * stackability_group.x
                    * stackability_group.y);
            onedim_instance_builder.add_bin_type_eligibility(
                    onedim_bin_type_id_0,
                    0);
            onedim_instance_builder.add_bin_type_eligibility(
                    onedim_bin_type_id_0,
                    2);

            BinTypeId onedim_bin_type_id_1 = onedim_instance_builder.add_bin_type(
                    instance.bin_type(0).box.z,
                    10,
                    instance.number_of_items());
            onedim_instance_builder.set_bin_type_maximum_weight(
                    onedim_bin_type_id_1,
                    instance.bin_type(0).maximum_stack_density
                    * stackability_group.x
                    * stackability_group.y);
            onedim_instance_builder.add_bin_type_eligibility(
                    onedim_bin_type_id_1,
                    1);
            onedim_instance_builder.add_bin_type_eligibility(
                    onedim_bin_type_id_1,
                    2);

            BinTypeId onedim_bin_type_id_01 = onedim_instance_builder.add_bin_type(
                    instance.bin_type(0).box.z,
                    8,
                    instance.number_of_items());
            onedim_instance_builder.set_bin_type_maximum_weight(
                    onedim_bin_type_id_01,
                    instance.bin_type(0).maximum_stack_density
                    * stackability_group.x
                    * stackability_group.y);
            onedim_instance_builder.add_bin_type_eligibility(
                    onedim_bin_type_id_01,
                    2);

            // Add item types.
            std::vector<ItemTypeId> onedimensional2boxstacks;
            for (ItemTypeId item_type_id = 0;
                    item_type_id < instance.number_of_item_types();
                    ++item_type_id) {
                const ItemType& item_type = instance.item_type(item_type_id);
                if (item_type.stackability_id != stackability_group.stackability_id
                        || item_type.group_id != stackability_group.group_id)
                    continue;
                if (copies[item_type_id] == 0)
                    continue;
                stackability_group.item_types.push_back(item_type_id);
                ItemTypeId onedim_item_type_id = onedim_instance_builder.add_item_type(
                        item_type.box.z,
                        -1,
                        copies[item_type_id]);
                onedim_instance_builder.set_item_type_weight(
                        onedim_item_type_id,
                        item_type.weight);
                onedim_instance_builder.set_item_type_maximum_stackability(
                        onedim_item_type_id,
                        item_type.maximum_stackability);
                onedim_instance_builder.set_item_type_maximum_weight_after(
                        onedim_item_type_id,
                        item_type.maximum_weight_above);
                onedim_instance_builder.set_item_type_nesting_length(
                        onedim_item_type_id,
                        item_type.nesting_height);
                if (item_type.can_rotate(0) && item_type.can_rotate(1)) {
                    onedim_instance_builder.set_item_type_eligibility(
                            onedim_item_type_id,
                            2);
                    //std::cout << "item_type_id " << item_type_id
                    //    << " onedim_item_type_id " << onedim_item_type_id
                    //    << " eligibility " << 1 << std::endl;
                } else if (item_type.can_rotate(0)) {
                    onedim_instance_builder.set_item_type_eligibility(
                            onedim_item_type_id,
                            0);
                    //std::cout << "item_type_id " << item_type_id
                    //    << " onedim_item_type_id " << onedim_item_type_id
                    //    << " eligibility " << 0 << std::endl;
                } else if (item_type.can_rotate(1)) {
                    onedim_instance_builder.set_item_type_eligibility(
                            onedim_item_type_id,
                            1);
                    //std::cout << "item_type_id " << item_type_id
                    //    << " onedim_item_type_id " << onedim_item_type_id
                    //    << " eligibility " << 1 << std::endl;
                }
            }

            onedimensional::Instance onedim_instance = onedim_instance_builder.build();

            // Solve onedimensional instance.
            onedimensional::OptimizeParameters onedim_parameters = parameters.onedimensional_parameters;
            onedim_parameters.verbosity_level = 0;
            onedim_parameters.timer = parameters.timer;
            onedim_parameters.optimization_mode
                = (parameters.sequential)?
                OptimizationMode::NotAnytimeSequential:
                OptimizationMode::NotAnytime;
            onedim_parameters.use_column_generation = 1;
            auto onedim_begin = std::chrono::steady_clock::now();
            auto onedim_output = onedimensional::optimize(
                    onedim_instance,
                    onedim_parameters);
            auto onedim_end = std::chrono::steady_clock::now();
            std::chrono::duration<double> onedim_time_span
                = std::chrono::duration_cast<std::chrono::duration<double>>(onedim_end - onedim_begin);
            output.onedimensional_time += onedim_time_span.count();
            output.number_of_onedimensional_calls++;

            auto onedim_solution = onedim_output.solution_pool.best();
            if (parameters.timer.needs_to_end()) {
                FFOT_LOG_FOLD_END(parameters.logger, "");
                algorithm_formatter.end();
                return output;
            }
            if (!onedim_solution.full()) {
                throw std::runtime_error(
                        FUNC_SIGNATURE + ": "
                        "no solution to VBPP subproblem.");
            }

            // Convert solution to stacks.
            for (BinPos bin_pos = 0;
                    bin_pos < onedim_solution.number_of_different_bins();
                    ++bin_pos) {
                const onedimensional::SolutionBin& onedim_solution_bin
                    = onedim_solution.bin(bin_pos);
                Stack stack;
                stack.rotation = onedim_solution_bin.bin_type_id;
                for (const auto& onedim_solution_item: onedim_solution_bin.items) {
                    ItemTypeId item_type_id = stackability_group.item_types[onedim_solution_item.item_type_id];
                    const ItemType& item_type = instance.item_type(item_type_id);
                    stack.items.push_back(item_type_id);
                    stack.profit += item_type.profit;
                    stack.weight += item_type.weight;
                }
                std::reverse(stack.items.begin(), stack.items.end());
                for (BinPos copies = 0; copies < onedim_solution_bin.copies; ++copies)
                    stackability_group.stacks[stack.rotation].push_back(stack);
            }

            stackability_groups.push_back(stackability_group);
        }

        Length x_max = xi;
        bool failed_middle_axle_weight_constraint = false;
        bool failed_rear_axle_weight_constraint = false;
        for (output.number_of_stack_splits = 0;
                output.number_of_stack_splits < 7;
                ++output.number_of_stack_splits) {
            FFOT_LOG(parameters.logger, "number of splitted stacks " << output.number_of_stack_splits << std::endl);

            Area stack_area = 0;
            for (BinPos bin_pos = 0; bin_pos < fixed_items.number_of_different_bins(); ++bin_pos) {
                const SolutionBin& solution_bin = fixed_items.bin(bin_pos);
                for (StackId stack_pos = 0; stack_pos < (StackId)solution_bin.stacks.size(); ++stack_pos) {
                    const SolutionStack& solution_stack = solution_bin.stacks[stack_pos];
                    stack_area += (solution_stack.x_end - solution_stack.x_start)
                        * (solution_stack.y_end - solution_stack.y_start);
                }
            }
            for (StackId stackability_group_pos = 0; stackability_group_pos < (StackId)stackability_groups.size(); ++stackability_group_pos) {
                const StackabilityGroup& stackability_group = stackability_groups[stackability_group_pos];
                for (int rotation = 0; rotation < 3; ++rotation)
                    stack_area += stackability_group.x * stackability_group.y * stackability_group.stacks[rotation].size();
            }
            bool try_to_pack_all_items
                = stack_area <= instance.bin_type(0).area()
                && instance.item_weight() <= instance.bin_weight();

            // Build rectangle instance.
            rectangle::InstanceBuilder rectangle_instance_builder;
            rectangle_instance_builder.set_objective(Objective::SequentialOneDimensionalRectangleSubproblem);
            rectangle_instance_builder.set_unloading_constraint(instance.unloading_constraint());

            // Add bin types.
            for (BinTypeId bin_type_id = 0; bin_type_id < instance.number_of_bin_types(); ++bin_type_id) {
                const BinType& bin_type = instance.bin_type(bin_type_id);
                auto rectangle_bin_type_id = rectangle_instance_builder.add_bin_type(
                        bin_type.box.x,
                        bin_type.box.y,
                        bin_type.cost,
                        bin_type.copies,
                        bin_type.copies_min);
                rectangle_instance_builder.set_bin_type_maximum_weight(
                        rectangle_bin_type_id,
                        bin_type.maximum_weight);
                rectangle_instance_builder.set_bin_type_semi_trailer_truck_parameters(
                        rectangle_bin_type_id,
                        bin_type.semi_trailer_truck_data);
            }

            // Add item types.
            std::vector<std::tuple<StackabilityId, int, StackId>> rectangle2boxstacks;
            for (StackId stackability_group_pos = 0; stackability_group_pos < (StackId)stackability_groups.size(); ++stackability_group_pos) {
                const StackabilityGroup& stackability_group = stackability_groups[stackability_group_pos];
                //std::cout << "stackability_group_pos " << stackability_group_pos
                //    << " x " << stackability_group.x
                //    << " y " << stackability_group.y
                //    << std::endl;

                for (int rotation = 0; rotation < 3; ++rotation) {
                    bool oriented = (rotation != 2);
                    Length x = (rotation != 1)? stackability_group.x: stackability_group.y;
                    Length y = (rotation != 1)? stackability_group.y: stackability_group.x;
                    for (StackId stack_pos = 0; stack_pos < (StackId)stackability_group.stacks[rotation].size(); ++stack_pos) {
                        const Stack& stack = stackability_group.stacks[rotation][stack_pos];
                        //std::cout << "stackability_group_pos " << stackability_group_pos
                        //    << " rotation " << rotation
                        //    << " stack_pos " << stack_pos
                        //    << " x " << x
                        //    << " y " << y
                        //    << " items";
                        //for (ItemTypeId item: stack.items)
                        //    std::cout << " " << item;
                        //std::cout << std::endl;
                        auto rectangle_item_type_id = rectangle_instance_builder.add_item_type(
                                x,
                                y,
                                stack.profit,
                                1,  // copies
                                oriented);
                        rectangle_instance_builder.set_item_type_group(
                                rectangle_item_type_id,
                                stackability_group.group_id);
                        rectangle_instance_builder.set_item_type_weight(
                                rectangle_item_type_id,
                                stack.weight);
                        rectangle_instance_builder.set_group_weight_constraints(
                                stackability_group.group_id,
                                instance.check_weight_constraints(stackability_group.group_id));
                        rectangle2boxstacks.push_back({stackability_group_pos, rotation, stack_pos});
                    }
                }
            }

            // Add fixed items to instance.
            std::vector<std::vector<ItemTypeId>> solution_stack_2_rectangle_item_type_id;
            //std::cout << "fixed_weight " << fixed_items.item_weight() << std::endl;
            for (BinPos bin_pos = 0; bin_pos < fixed_items.number_of_different_bins(); ++bin_pos) {
                const SolutionBin& solution_bin = fixed_items.bin(bin_pos);
                solution_stack_2_rectangle_item_type_id.push_back({});
                for (StackId stack_pos = 0; stack_pos < (StackId)solution_bin.stacks.size(); ++stack_pos) {
                    const SolutionStack& solution_stack = solution_bin.stacks[stack_pos];
                    const ItemType& item_type = instance.item_type(solution_stack.items.front().item_type_id);
                    Profit profit = 0;
                    Weight weight = 0;
                    for (const SolutionItem& solution_item: solution_stack.items) {
                        const ItemType& item_type = instance.item_type(solution_item.item_type_id);
                        profit += item_type.profit;
                        weight += item_type.weight;
                    }
                    ItemTypeId rectangle_item_type_id = rectangle_instance_builder.add_item_type(
                            solution_stack.x_end - solution_stack.x_start,
                            solution_stack.y_end - solution_stack.y_start,
                            profit,
                            1, // copies
                            false); // oriented
                    rectangle_instance_builder.set_item_type_group(
                            rectangle_item_type_id,
                            item_type.group_id);
                    rectangle_instance_builder.set_item_type_weight(
                            rectangle_item_type_id,
                            weight);
                    rectangle_instance_builder.set_group_weight_constraints(
                            item_type.group_id,
                            instance.check_weight_constraints(item_type.group_id));
                    //std::cout << "rectangle_item_type_id " << rectangle_item_type_id
                    //    << " w " << weight
                    //    << std::endl;
                    solution_stack_2_rectangle_item_type_id[bin_pos].push_back(rectangle_item_type_id);
                }
            }

            rectangle::Instance rectangle_instance = rectangle_instance_builder.build();

            // Create rectangle solution with fixed items.
            rectangle::SolutionBuilder rectangle_fixed_items_builder(rectangle_instance);
            for (BinPos bin_pos = 0; bin_pos < fixed_items.number_of_different_bins(); ++bin_pos) {
                const SolutionBin& solution_bin = fixed_items.bin(bin_pos);
                rectangle_fixed_items_builder.add_bin(bin_pos, 1);
                for (StackId stack_pos = 0; stack_pos < (StackId)solution_bin.stacks.size(); ++stack_pos) {
                    const SolutionStack& solution_stack = solution_bin.stacks[stack_pos];
                    ItemTypeId rectangle_item_type_id = solution_stack_2_rectangle_item_type_id[bin_pos][stack_pos];
                    rectangle_fixed_items_builder.add_item(
                            bin_pos,
                            rectangle_item_type_id,
                            {solution_stack.x_start, solution_stack.y_start},
                            false);
                }
            }
            rectangle::Solution rectangle_fixed_items = rectangle_fixed_items_builder.build();

            //rectangle_instance.format(std::cout, 2);

            bool failed_middle_axle_weight_constraint_cur = false;
            bool failed_rear_axle_weight_constraint_cur = false;

            if (try_to_pack_all_items) {

                // First we try to pack all items with guides 0 and 1. This the
                // guides that will most likely lead to the best solution if axle
                // weight constraints are not critical.
                // If the solution is not full, it might be
                // - Because of axle weight constraints
                //   In this case, we try again to pack all items but with
                //   guide 8.
                // - Because of geometric constraints (area)
                //   In this case we give up on trying to pack all items and try
                //   again with guides 4 and 5.
                //   If axle weight constraints happen to be critical, we try
                //   again with guide 8.

                {
                    rectangle::BranchingScheme::Parameters rectangle_parameters;
                    rectangle_parameters.guide_id = 0;
                    rectangle_parameters.predecessor_strategy = 0;
                    rectangle_parameters.group_guiding_strategy = 1;
                    rectangle_parameters.staircase = false;
                    rectangle_parameters.fixed_items = &rectangle_fixed_items;
                    auto subproblem_output = sequential_onedimensional_rectangle_subproblem(
                            instance,
                            parameters,
                            output,
                            algorithm_formatter,
                            fixed_items,
                            stackability_groups,
                            rectangle_instance,
                            rectangle2boxstacks,
                            rectangle_parameters);
                    if (subproblem_output.solution.full())
                        x_max = (std::min)(x_max, subproblem_output.solution.x_max());
                    failed_middle_axle_weight_constraint_cur |= (subproblem_output.solution.compute_middle_axle_weight_constraints_violation() > 0);
                    failed_rear_axle_weight_constraint_cur |= (subproblem_output.solution.compute_rear_axle_weight_constraints_violation() > 0);
                    if (output.solution_pool.best().full()) {
                        FFOT_LOG_FOLD_END(parameters.logger, "");
                        FFOT_LOG_FOLD_END(parameters.logger, "");
                        algorithm_formatter.end();
                        return output;
                    }
                }

                {
                    rectangle::BranchingScheme::Parameters rectangle_parameters;
                    rectangle_parameters.guide_id = 1;
                    rectangle_parameters.predecessor_strategy = 0;
                    rectangle_parameters.group_guiding_strategy = 1;
                    rectangle_parameters.staircase = false;
                    rectangle_parameters.fixed_items = &rectangle_fixed_items;
                    auto subproblem_output = sequential_onedimensional_rectangle_subproblem(
                            instance,
                            parameters,
                            output,
                            algorithm_formatter,
                            fixed_items,
                            stackability_groups,
                            rectangle_instance,
                            rectangle2boxstacks,
                            rectangle_parameters);
                    if (subproblem_output.solution.full())
                        x_max = (std::min)(x_max, subproblem_output.solution.x_max());
                    failed_middle_axle_weight_constraint_cur |= (subproblem_output.solution.compute_middle_axle_weight_constraints_violation() > 0);
                    failed_rear_axle_weight_constraint_cur |= (subproblem_output.solution.compute_rear_axle_weight_constraints_violation() > 0);
                    if (output.solution_pool.best().full()) {
                        FFOT_LOG_FOLD_END(parameters.logger, "");
                        FFOT_LOG_FOLD_END(parameters.logger, "");
                        algorithm_formatter.end();
                        return output;
                    }
                }

                if (failed_middle_axle_weight_constraint_cur) {

                    rectangle::BranchingScheme::Parameters rectangle_parameters;
                    rectangle_parameters.guide_id = 8;
                    rectangle_parameters.predecessor_strategy = 1;
                    rectangle_parameters.group_guiding_strategy = 1;
                    rectangle_parameters.staircase = false;
                    rectangle_parameters.fixed_items = &rectangle_fixed_items;
                    auto subproblem_output = sequential_onedimensional_rectangle_subproblem(
                            instance,
                            parameters,
                            output,
                            algorithm_formatter,
                            fixed_items,
                            stackability_groups,
                            rectangle_instance,
                            rectangle2boxstacks,
                            rectangle_parameters);
                    if (output.solution_pool.best().full()) {
                        FFOT_LOG_FOLD_END(parameters.logger, "");
                        FFOT_LOG_FOLD_END(parameters.logger, "");
                        algorithm_formatter.end();
                        return output;
                    }

                } else if (failed_rear_axle_weight_constraint_cur) {

                    rectangle::BranchingScheme::Parameters rectangle_parameters;
                    rectangle_parameters.guide_id = 9;
                    rectangle_parameters.predecessor_strategy = 2;
                    rectangle_parameters.group_guiding_strategy = 1;
                    rectangle_parameters.staircase = false;
                    rectangle_parameters.fixed_items = &rectangle_fixed_items;
                    auto subproblem_output = sequential_onedimensional_rectangle_subproblem(
                            instance,
                            parameters,
                            output,
                            algorithm_formatter,
                            fixed_items,
                            stackability_groups,
                            rectangle_instance,
                            rectangle2boxstacks,
                            rectangle_parameters);
                    if (output.solution_pool.best().full()) {
                        FFOT_LOG_FOLD_END(parameters.logger, "");
                        FFOT_LOG_FOLD_END(parameters.logger, "");
                        algorithm_formatter.end();
                        return output;
                    }

                } else {
                    try_to_pack_all_items = false;
                }
            }

            if (!try_to_pack_all_items) {

                // All items do not fit in the bin. We try to maximize the profit
                // of the packed items.
                // First we try to pack with guides 4 and 5.
                // If the axle weight constraints happened to be critical, then we
                // try again with guide 8.

                {
                    rectangle::BranchingScheme::Parameters rectangle_parameters;
                    rectangle_parameters.guide_id = 4;
                    rectangle_parameters.predecessor_strategy = 0;
                    rectangle_parameters.group_guiding_strategy = 1;
                    rectangle_parameters.staircase = false;
                    rectangle_parameters.fixed_items = &rectangle_fixed_items;
                    auto subproblem_output = sequential_onedimensional_rectangle_subproblem(
                            instance,
                            parameters,
                            output,
                            algorithm_formatter,
                            fixed_items,
                            stackability_groups,
                            rectangle_instance,
                            rectangle2boxstacks,
                            rectangle_parameters);
                    failed_middle_axle_weight_constraint_cur |= (subproblem_output.solution.compute_middle_axle_weight_constraints_violation() > 0);
                    failed_rear_axle_weight_constraint_cur |= (subproblem_output.solution.compute_rear_axle_weight_constraints_violation() > 0);
                    if (output.solution_pool.best().full()) {
                        FFOT_LOG_FOLD_END(parameters.logger, "");
                        FFOT_LOG_FOLD_END(parameters.logger, "");
                        algorithm_formatter.end();
                        return output;
                    }
                }

                {
                    rectangle::BranchingScheme::Parameters rectangle_parameters;
                    rectangle_parameters.guide_id = 5;
                    rectangle_parameters.predecessor_strategy = 0;
                    rectangle_parameters.group_guiding_strategy = 1;
                    rectangle_parameters.staircase = false;
                    rectangle_parameters.fixed_items = &rectangle_fixed_items;
                    auto subproblem_output = sequential_onedimensional_rectangle_subproblem(
                            instance,
                            parameters,
                            output,
                            algorithm_formatter,
                            fixed_items,
                            stackability_groups,
                            rectangle_instance,
                            rectangle2boxstacks,
                            rectangle_parameters);
                    if (output.solution_pool.best().full()) {
                        FFOT_LOG_FOLD_END(parameters.logger, "");
                        FFOT_LOG_FOLD_END(parameters.logger, "");
                        algorithm_formatter.end();
                        return output;
                    }
                    failed_middle_axle_weight_constraint_cur |= (subproblem_output.solution.compute_middle_axle_weight_constraints_violation() > 0);
                    failed_rear_axle_weight_constraint_cur |= (subproblem_output.solution.compute_rear_axle_weight_constraints_violation() > 0);
                }

                if (!failed_middle_axle_weight_constraint_cur
                        && !failed_rear_axle_weight_constraint_cur
                        && fixed_items_solutions_pos == 0
                        && output.number_of_stack_splits == 0) {
                    FFOT_LOG_FOLD_END(parameters.logger, "");
                    FFOT_LOG_FOLD_END(parameters.logger, "");
                    algorithm_formatter.end();
                    return output;
                }

                if (failed_middle_axle_weight_constraint_cur) {

                    rectangle::BranchingScheme::Parameters rectangle_parameters;
                    rectangle_parameters.guide_id = 8;
                    rectangle_parameters.predecessor_strategy = 1;
                    rectangle_parameters.group_guiding_strategy = 1;
                    rectangle_parameters.staircase = false;
                    rectangle_parameters.fixed_items = &rectangle_fixed_items;
                    auto subproblem_output = sequential_onedimensional_rectangle_subproblem(
                            instance,
                            parameters,
                            output,
                            algorithm_formatter,
                            fixed_items,
                            stackability_groups,
                            rectangle_instance,
                            rectangle2boxstacks,
                            rectangle_parameters);
                    if (output.solution_pool.best().full()) {
                        FFOT_LOG_FOLD_END(parameters.logger, "");
                        FFOT_LOG_FOLD_END(parameters.logger, "");
                        algorithm_formatter.end();
                        return output;
                    }

                } else if (failed_rear_axle_weight_constraint) {

                    rectangle::BranchingScheme::Parameters rectangle_parameters;
                    rectangle_parameters.guide_id = 9;
                    rectangle_parameters.predecessor_strategy = 2;
                    rectangle_parameters.group_guiding_strategy = 1;
                    rectangle_parameters.staircase = false;
                    rectangle_parameters.fixed_items = &rectangle_fixed_items;
                    auto subproblem_output = sequential_onedimensional_rectangle_subproblem(
                            instance,
                            parameters,
                            output,
                            algorithm_formatter,
                            fixed_items,
                            stackability_groups,
                            rectangle_instance,
                            rectangle2boxstacks,
                            rectangle_parameters);
                    if (output.solution_pool.best().full()) {
                        FFOT_LOG_FOLD_END(parameters.logger, "");
                        algorithm_formatter.end();
                        return output;
                    }

                }

            }

            failed_middle_axle_weight_constraint |= failed_middle_axle_weight_constraint_cur;
            failed_rear_axle_weight_constraint |= failed_rear_axle_weight_constraint_cur;

            if (output.number_of_iterations > 0)
                break;

            // If not infeasible because of waste constraints, break.
            if (!failed_middle_axle_weight_constraint_cur)
                break;

            // Find a stack to split.
            StackId stackability_group_pos_best = -1;
            int rotation_best = -1;
            StackId stack_pos_best = -1;
            GroupId group_id_best = -1;
            Weight weight_best = -1;
            for (StackId stackability_group_pos = 0; stackability_group_pos < (StackId)stackability_groups.size(); ++stackability_group_pos) {
                const StackabilityGroup& stackability_group = stackability_groups[stackability_group_pos];
                for (int rotation = 0; rotation < 3; ++rotation) {
                    for (StackId stack_pos = 0;
                            stack_pos < (StackId)stackability_group.stacks[rotation].size();
                            ++stack_pos) {
                        const Stack& stack = stackability_group.stacks[rotation][stack_pos];
                        // At least one item.
                        if (stack.items.size() <= 1)
                            continue;
                        if (stack_pos_best == -1
                                || (group_id_best < stackability_group.group_id)
                                || (group_id_best == stackability_group.group_id
                                    && weight_best > stack.weight)) {
                            stackability_group_pos_best = stackability_group_pos;
                            rotation_best = rotation;
                            stack_pos_best = stack_pos;
                            group_id_best = stackability_group.group_id;
                            weight_best = stack.weight;
                        }
                    }
                }
            }
            if (stackability_group_pos_best == -1)
                break;

            StackabilityGroup& stackability_group_best = stackability_groups[stackability_group_pos_best];
            Stack& stack_best = stackability_group_best.stacks[rotation_best][stack_pos_best];
            Stack stack_1;
            Stack stack_2;
            stack_1.rotation = stack_best.rotation;
            stack_2.rotation = stack_best.rotation;
            auto items = stack_best.items;
            bool first = true;
            for (ItemTypeId item_type_id: items) {
                Stack& stack = (first)? stack_1: stack_2;
                const ItemType& item_type = instance.item_type(item_type_id);
                stack.items.push_back(item_type_id);
                stack.profit += item_type.profit;
                stack.weight += item_type.weight;
                first = false;
            }
            stack_best = stack_1;
            stackability_group_best.stacks[stack_2.rotation].push_back(stack_2);
        }

        output.failed = true;

        fixed_items = Solution(instance);
        BinPos bin_pos = fixed_items.add_bin(0, 1);
        FFOT_LOG(
                parameters.logger,
                "failed_middle_axle_weight_constraint " << failed_middle_axle_weight_constraint << std::endl
                << "failed_rear_axle_weight_constraint " << failed_rear_axle_weight_constraint << std::endl
                << "x_max " << x_max << " / " << xi << std::endl);
        if (failed_middle_axle_weight_constraint) {
            // If the solution is infeasible.
            fixed_items_solutions_pos_lower_bound = fixed_items_solutions_pos + 1;
            fixed_items_solutions_pos = 0;
            for (ItemPos pos = 0; pos < (ItemPos)fixed_items_solutions.size(); ++pos) {
                if (pos + 1 < (ItemPos)fixed_items_solutions.size()
                        && fixed_items_solutions[pos + 1].x_max() > xi - x_max)
                    break;
                //std::cout << "pos " << pos << " x " << fixed_items_x[pos] << " " << fixed_items_x[pos + 1] << std::endl;
                fixed_items_solutions_pos++;
            }
            if (fixed_items_solutions_pos < fixed_items_solutions_pos_lower_bound)
                fixed_items_solutions_pos = fixed_items_solutions_pos_lower_bound;
        } else if (failed_rear_axle_weight_constraint) {
            break;
            //throw std::runtime_error("failed_rear_axle_weight_constraint");
        } else {
            fixed_items_solutions_pos_upper_bound = fixed_items_solutions_pos - 1;
            fixed_items_solutions_pos = fixed_items_solutions_pos_upper_bound;
        }

        //fixed_items.write("fixed_items.csv");

        FFOT_LOG(
                parameters.logger,
                "fixed_items_solutions_pos " << fixed_items_solutions_pos << std::endl
                << "fixed_items_solutions_pos_lower_bound " << fixed_items_solutions_pos_lower_bound << std::endl
                << "fixed_items_solutions_pos_upper_bound " << fixed_items_solutions_pos_upper_bound << std::endl);
        FFOT_LOG_FOLD_END(parameters.logger, "");
        if (fixed_items_solutions_pos_lower_bound > fixed_items_solutions_pos_upper_bound)
            break;

    }

    FFOT_LOG_FOLD_END(parameters.logger, "");
    algorithm_formatter.end();
    return output;
}
