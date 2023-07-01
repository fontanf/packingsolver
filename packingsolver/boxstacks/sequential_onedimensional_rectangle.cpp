#include "packingsolver/boxstacks/sequential_onedimensional_rectangle.hpp"
#include "packingsolver/rectangle/branching_scheme.hpp"

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

    /**
     * Boolean indicating if the solution found did not satisfy the axle weight
     * constraints before the repair step.
     */
    bool failed = false;
};

SequentialOneDimensionalRectangleSubproblemOutput sequential_onedimensional_rectangle_subproblem(
        const Instance& instance,
        SequentialOneDimensionalRectangleOptionalParameters& parameters,
        SequentialOneDimensionalRectangleOutput& sequential_onedimensional_rectangle_output,
        Counter iteration,
        const Solution& fixed_items,
        std::vector<StackabilityGroup> stackability_groups,
        const rectangle::Instance& rectangle_instance,
        const std::vector<std::tuple<StackabilityId, int, StackId>>& rectangle2boxstacks,
        const rectangle::BranchingScheme::Parameters rectangle_parameters)
{
    SequentialOneDimensionalRectangleSubproblemOutput output(instance);
    //std::cout << "it " << iteration << " guide " << rectangle_parameters.guide_id << std::endl;

    // Solve rectanlge instance.
    //rectangle_parameters.fixed_items = &rectangle_fixed_items;
    rectangle::BranchingScheme rectangle_branching_scheme(rectangle_instance, rectangle_parameters);
    treesearchsolver::IterativeBeamSearch2OptionalParameters<rectangle::BranchingScheme> ibs_parameters;
    ibs_parameters.minimum_size_of_the_queue = parameters.rectangle_queue_size;
    ibs_parameters.maximum_size_of_the_queue = parameters.rectangle_queue_size;
    ibs_parameters.info = Info(parameters.info, false, "");
    //ibs_parameters.info.set_verbosity_level(2);
    auto rectangle_begin = std::chrono::steady_clock::now();
    auto rectangle_output = treesearchsolver::iterative_beam_search_2<rectangle::BranchingScheme>(
            rectangle_branching_scheme,
            ibs_parameters);
    auto rectangle_end = std::chrono::steady_clock::now();
    std::chrono::duration<double> rectangle_time_span
        = std::chrono::duration_cast<std::chrono::duration<double>>(rectangle_end - rectangle_begin);
    sequential_onedimensional_rectangle_output.rectangle_time += rectangle_time_span.count();
    sequential_onedimensional_rectangle_output.number_of_rectangle_calls++;
    auto rectangle_solution = rectangle_branching_scheme.to_solution(rectangle_output.solution_pool.best());
    //std::cout << "rectangle_solution.number_of_items " << rectangle_solution.number_of_items()
    //    << " / " << rectangle_instance.number_of_items()
    //    << std::endl;

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
                    [&stacks](
                        StackId stack_pos_1,
                        StackId stack_pos_2) -> bool
                    {
                        return stacks[stack_pos_1].weight
                            < stacks[stack_pos_2].weight;
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
    if (iteration == 0) {
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
    sequential_onedimensional_rectangle_output.maximum_number_of_items = std::max(
            sequential_onedimensional_rectangle_output.maximum_number_of_items,
            number_of_items_before_repair);
    output.profit_before_repair = solution.profit();
    //solution.write("sol_" + std::to_string(iteration) + "_" + std::to_string(rectangle_parameters.guide_id) + "_before.csv");
    //std::cout << "number of items before repair " << output.number_of_items_before_repair
    //    << " / " << instance.number_of_items()
    //    << std::endl;
    //std::cout << "profit before repair " << solution.profit() << std::endl;

    // If the solution is not feasible, which would necessarily be because
    // of axle weight constraints, we try to repair it.
    if (solution.compute_weight_constraints_violation() > 0)
        output.failed = true;

    while (solution.compute_weight_constraints_violation() > 0) {
        Weight violation_cur = solution.compute_weight_constraints_violation();
        Weight violation_best = violation_cur;
        //std::cout << "violation_best " << violation_best << std::endl;

        // Try to shift an item.
        if (parameters.move_intra_shift) {
            BinPos bin_pos_best = -1;
            StackId stack_id_1_best = -1;
            ItemPos item_pos_1_best = -1;
            int rotation_1_best = -1;
            StackId stack_id_2_best = -1;
            ItemPos item_pos_2_best = -1;
            for (BinPos bin_pos = 0; bin_pos < solution.number_of_different_bins(); ++bin_pos) {
                const SolutionBin& solution_bin = solution.bin(bin_pos);
                for (StackId stack_pos_1 = 0; stack_pos_1 < (StackId)solution_bin.stacks.size(); ++stack_pos_1) {
                    const SolutionStack& solution_stack_1 = solution_bin.stacks[stack_pos_1];
                    // The stack is already empty.
                    if (solution_stack_1.items.size() == 0)
                        continue;
                    ItemTypeId j0_1 = solution_stack_1.items.front().item_type_id;
                    const ItemType& item_type_0_1 = instance.item_type(j0_1);
                    // Don't remove a fixed item.
                    if (bin_pos < fixed_items.number_of_different_bins()
                            && stack_pos_1 < (StackId)fixed_items.bin(bin_pos).stacks.size())
                        continue;
                    for (StackId stack_pos_2 = 0; stack_pos_2 < (StackId)solution_bin.stacks.size(); ++stack_pos_2) {
                        const SolutionStack& solution_stack_2 = solution_bin.stacks[stack_pos_2];
                        // The stack is already empty.
                        if (solution_stack_2.items.size() == 0)
                            continue;
                        ItemTypeId j0_2 = solution_stack_2.items.front().item_type_id;
                        const ItemType& item_type_0_2 = instance.item_type(j0_2);
                        // Don't remove a fixed item.
                        if (bin_pos < fixed_items.number_of_different_bins()
                                && stack_pos_2 < (StackId)fixed_items.bin(bin_pos).stacks.size())
                            continue;
                        if (stack_pos_2 == stack_pos_1)
                            continue;
                        if (item_type_0_1.stackability_id != item_type_0_2.stackability_id)
                            continue;
                        if (item_type_0_1.group_id != item_type_0_2.group_id)
                            continue;

                        for (ItemPos item_pos_1 = (ItemPos)solution_stack_1.items.size() - 1; item_pos_1 >= 0; item_pos_1--) {
                            const SolutionItem& solution_item_1 = solution_stack_1.items[item_pos_1];
                            const ItemType& item_type_1 = instance.item_type(solution_item_1.item_type_id);
                            // Check orientation.
                            int rotation_1 = -1;
                            Direction o = Direction::X;
                            for (int r = 0; r < 6; ++r) {
                                if (item_type_1.can_rotate(r)) {
                                    Length xj = instance.x(item_type_1, r, o);
                                    Length yj = instance.y(item_type_1, r, o);
                                    if (xj == solution_stack_2.x_end - solution_stack_2.x_start
                                            && yj == solution_stack_2.y_end - solution_stack_2.y_start) {
                                        rotation_1 = r;
                                        break;
                                    }
                                }
                            }
                            if (rotation_1 == -1)
                                continue;
                            // Compute violation.
                            std::vector<Weight> bin_weight = solution_bin.weight;
                            std::vector<Weight> bin_weight_weighted_sum = solution_bin.weight_weighted_sum;
                            for (GroupId group_id = 0; group_id <= item_type_1.group_id; ++group_id) {
                                bin_weight_weighted_sum[group_id]
                                    -= ((double)solution_stack_1.x_start
                                            + (double)(solution_stack_1.x_end - solution_stack_1.x_start) / 2)
                                    * item_type_1.weight;
                                bin_weight_weighted_sum[group_id]
                                    += ((double)solution_stack_2.x_start
                                            + (double)(solution_stack_2.x_end - solution_stack_2.x_start) / 2)
                                    * item_type_1.weight;
                            }
                            double violation = solution.compute_weight_constraints_violation(
                                    bin_pos, bin_weight, bin_weight_weighted_sum);
                            bool better = (violation_best > 0 && violation == 0)
                                || (striclty_greater(violation_best, violation));
                            if (!better)
                                continue;
                            for (ItemPos item_pos_2 = (ItemPos)solution_stack_2.items.size(); item_pos_2 >= 0; item_pos_2--) {
                                // Build new stack.
                                std::vector<std::pair<ItemTypeId, int>> new_stack_2;
                                for (ItemPos item_pos = 0; item_pos < item_pos_2; item_pos++)
                                    new_stack_2.push_back({solution_stack_2.items[item_pos].item_type_id, solution_stack_2.items[item_pos].rotation});
                                new_stack_2.push_back({solution_item_1.item_type_id, rotation_1});
                                for (ItemPos item_pos = item_pos_2; item_pos < (ItemPos)solution_stack_2.items.size(); item_pos++)
                                    new_stack_2.push_back({solution_stack_2.items[item_pos].item_type_id, solution_stack_2.items[item_pos].rotation});
                                // Check new stack.
                                if (!solution.check_stack(solution_bin.bin_type_id, new_stack_2))
                                    continue;
                                bin_pos_best = bin_pos;
                                stack_id_1_best = stack_pos_1;
                                item_pos_1_best = item_pos_1;
                                rotation_1_best = rotation_1;
                                stack_id_2_best = stack_pos_2;
                                item_pos_2_best = item_pos_2;
                                violation_best = violation;
                            }
                        }
                    }
                }
            }

            if (bin_pos_best != -1) {
                // Apply move.
                //std::cout << "apply intra-shift"
                //    << " violation " << solution.compute_weight_constraints_violation()
                //    << " violation_best " << violation_best
                //    << std::endl;
                Solution solution_tmp(instance);
                for (BinPos bin_pos = 0; bin_pos < solution.number_of_different_bins(); ++bin_pos) {
                    const SolutionBin& solution_bin = solution.bin(bin_pos);
                    BinPos new_bin_pos = solution_tmp.add_bin(solution_bin.bin_type_id, solution_bin.copies);
                    for (StackId stack_id = 0; stack_id < (StackId)solution_bin.stacks.size(); ++stack_id) {
                        const SolutionStack& solution_stack = solution_bin.stacks[stack_id];
                        StackId new_stack_id = solution_tmp.add_stack(
                                new_bin_pos,
                                solution_stack.x_start,
                                solution_stack.x_end,
                                solution_stack.y_start,
                                solution_stack.y_end);
                        if (bin_pos == bin_pos_best
                                && stack_id == stack_id_1_best) {
                            for (ItemPos item_pos = 0; item_pos < (ItemPos)solution_stack.items.size(); ++item_pos) {
                                if (item_pos == item_pos_1_best)
                                    continue;
                                const SolutionItem& solution_item = solution_stack.items[item_pos];
                                solution_tmp.add_item(
                                        new_bin_pos,
                                        new_stack_id,
                                        solution_item.item_type_id,
                                        solution_item.rotation);
                            }
                        } else if (bin_pos == bin_pos_best
                                && stack_id == stack_id_2_best) {
                            for (ItemPos item_pos = 0; item_pos < item_pos_2_best; ++item_pos) {
                                const SolutionItem& solution_item = solution_stack.items[item_pos];
                                solution_tmp.add_item(
                                        new_bin_pos,
                                        new_stack_id,
                                        solution_item.item_type_id,
                                        solution_item.rotation);
                            }
                            const SolutionItem& solution_item = solution_bin.stacks[stack_id_1_best].items[item_pos_1_best];
                            solution_tmp.add_item(
                                    new_bin_pos,
                                    new_stack_id,
                                    solution_item.item_type_id,
                                    rotation_1_best);
                            for (ItemPos item_pos = item_pos_2_best; item_pos < (ItemPos)solution_stack.items.size(); ++item_pos) {
                                const SolutionItem& solution_item = solution_stack.items[item_pos];
                                solution_tmp.add_item(
                                        new_bin_pos,
                                        new_stack_id,
                                        solution_item.item_type_id,
                                        solution_item.rotation);
                            }
                        } else {
                            for (ItemPos item_pos = 0; item_pos < (ItemPos)solution_stack.items.size(); ++item_pos) {
                                const SolutionItem& solution_item = solution_stack.items[item_pos];
                                solution_tmp.add_item(
                                        new_bin_pos,
                                        new_stack_id,
                                        solution_item.item_type_id,
                                        solution_item.rotation);
                            }
                        }
                    }
                }
                // Check.
                Weight v = solution.compute_weight_constraints_violation();
                Weight v_tmp = solution_tmp.compute_weight_constraints_violation();
                bool better = (v > 0 && v_tmp == 0)
                    || (striclty_greater(v, v_tmp));
                if (!better) {
                    throw std::runtime_error("intra-shift");
                }
                if (!equal(solution_tmp.profit(), solution.profit())) {
                    throw std::runtime_error(
                            "intra-shift."
                            "; bin_pos_best: " + std::to_string(bin_pos_best)
                            + "; stack_id_1_best: " + std::to_string(stack_id_1_best)
                            + "; item_pos_1_best: " + std::to_string(item_pos_1_best)
                            + "; stack_id_2_best: " + std::to_string(stack_id_2_best)
                            + "; item_pos_2_best: " + std::to_string(item_pos_2_best)
                            + "; solution.profit(): " + std::to_string(solution.profit())
                            + "; solution_tmp.profit(): " + std::to_string(solution_tmp.profit())
                            + ".");
                }
                solution = solution_tmp;
                if (solution.compute_weight_constraints_violation() == 0)
                    break;
                continue;
            }
        }

        // Try to exchange 2 items.
        //std::cout << "exchange start" << std::endl;
        if (parameters.move_intra_swap) {
            BinPos bin_pos_best = -1;
            StackId stack_id_1_best = -1;
            ItemPos item_pos_1_best = -1;
            int rotation_1_best = -1;
            StackId stack_id_2_best = -1;
            ItemPos item_pos_2_best = -1;
            int rotation_2_best = -1;
            for (BinPos bin_pos = 0; bin_pos < solution.number_of_different_bins(); ++bin_pos) {
                const SolutionBin& solution_bin = solution.bin(bin_pos);
                for (StackId stack_pos_1 = 0; stack_pos_1 < (StackId)solution_bin.stacks.size(); ++stack_pos_1) {
                    const SolutionStack& solution_stack_1 = solution_bin.stacks[stack_pos_1];
                    // The stack is already empty.
                    if (solution_stack_1.items.size() == 0)
                        continue;
                    ItemTypeId j0_1 = solution_stack_1.items.front().item_type_id;
                    const ItemType& item_type_0_1 = instance.item_type(j0_1);
                    // Don't remove a fixed item.
                    if (bin_pos < fixed_items.number_of_different_bins()
                            && stack_pos_1 < (StackId)fixed_items.bin(bin_pos).stacks.size())
                        continue;
                    for (StackId stack_pos_2 = 0; stack_pos_2 < (StackId)solution_bin.stacks.size(); ++stack_pos_2) {
                        const SolutionStack& solution_stack_2 = solution_bin.stacks[stack_pos_2];
                        // The stack is already empty.
                        if (solution_stack_2.items.size() == 0)
                            continue;
                        ItemTypeId j0_2 = solution_stack_2.items.front().item_type_id;
                        const ItemType& item_type_0_2 = instance.item_type(j0_2);
                        // Don't remove a fixed item.
                        if (bin_pos < fixed_items.number_of_different_bins()
                                && stack_pos_2 < (StackId)fixed_items.bin(bin_pos).stacks.size())
                            continue;
                        if (stack_pos_2 == stack_pos_1)
                            continue;
                        if (item_type_0_1.stackability_id != item_type_0_2.stackability_id)
                            continue;
                        if (item_type_0_1.group_id != item_type_0_2.group_id)
                            continue;

                        for (ItemPos item_pos_1 = (ItemPos)solution_stack_1.items.size() - 1; item_pos_1 >= 0; item_pos_1--) {
                            const SolutionItem& solution_item_1 = solution_stack_1.items[item_pos_1];
                            const ItemType& item_type_1 = instance.item_type(solution_item_1.item_type_id);
                            // Check orientation.
                            int rotation_1 = -1;
                            Direction o = Direction::X;
                            for (int r = 0; r < 6; ++r) {
                                if (item_type_1.can_rotate(r)) {
                                    Length xj = instance.x(item_type_1, r, o);
                                    Length yj = instance.y(item_type_1, r, o);
                                    if (xj == solution_stack_2.x_end - solution_stack_2.x_start
                                            && yj == solution_stack_2.y_end - solution_stack_2.y_start) {
                                        rotation_1 = r;
                                        break;
                                    }
                                }
                            }
                            if (rotation_1 == -1)
                                continue;
                            for (ItemPos item_pos_2 = (ItemPos)solution_stack_2.items.size() - 1; item_pos_2 >= 0; item_pos_2--) {
                                const SolutionItem& solution_item_2 = solution_stack_2.items[item_pos_2];
                                const ItemType& item_type_2 = instance.item_type(solution_item_2.item_type_id);
                                if (solution_item_2.item_type_id == solution_item_1.item_type_id)
                                    continue;
                                // Check orientation.
                                int rotation_2 = -1;
                                for (int r = 0; r < 6; ++r) {
                                    if (item_type_2.can_rotate(r)) {
                                        Length xj = instance.x(item_type_2, r, o);
                                        Length yj = instance.y(item_type_2, r, o);
                                        if (xj == solution_stack_1.x_end - solution_stack_1.x_start
                                                && yj == solution_stack_1.y_end - solution_stack_1.y_start) {
                                            rotation_2 = r;
                                            break;
                                        }
                                    }
                                }
                                if (rotation_2 == -1)
                                    continue;
                                // Compute violation.
                                std::vector<Weight> bin_weight = solution_bin.weight;
                                std::vector<Weight> bin_weight_weighted_sum = solution_bin.weight_weighted_sum;
                                for (GroupId group_id = 0; group_id <= item_type_1.group_id; ++group_id) {
                                    bin_weight_weighted_sum[group_id]
                                        -= ((double)solution_stack_1.x_start
                                                + (double)(solution_stack_1.x_end - solution_stack_1.x_start) / 2)
                                        * item_type_1.weight;
                                    bin_weight_weighted_sum[group_id]
                                        += ((double)solution_stack_2.x_start
                                                + (double)(solution_stack_2.x_end - solution_stack_2.x_start) / 2)
                                        * item_type_1.weight;
                                }
                                for (GroupId group_id = 0; group_id <= item_type_2.group_id; ++group_id) {
                                    bin_weight_weighted_sum[group_id]
                                        -= ((double)solution_stack_2.x_start
                                                + (double)(solution_stack_2.x_end - solution_stack_2.x_start) / 2)
                                        * item_type_2.weight;
                                    bin_weight_weighted_sum[group_id]
                                        += ((double)solution_stack_1.x_start
                                                + (double)(solution_stack_1.x_end - solution_stack_1.x_start) / 2)
                                        * item_type_2.weight;
                                }
                                double violation = solution.compute_weight_constraints_violation(
                                        bin_pos, bin_weight, bin_weight_weighted_sum);
                                bool better = (violation_best > 0 && violation == 0)
                                    || (striclty_greater(violation_best, violation));
                                if (!better)
                                    continue;
                                // Build new stack.
                                std::vector<std::pair<ItemTypeId, int>> new_stack_1;
                                for (ItemPos item_pos = 0; item_pos < item_pos_1; item_pos++)
                                    new_stack_1.push_back({solution_stack_1.items[item_pos].item_type_id, solution_stack_1.items[item_pos].rotation});
                                new_stack_1.push_back({solution_item_2.item_type_id, rotation_2});
                                for (ItemPos item_pos = item_pos_1 + 1; item_pos < (ItemPos)solution_stack_1.items.size(); item_pos++)
                                    new_stack_1.push_back({solution_stack_1.items[item_pos].item_type_id, solution_stack_1.items[item_pos].rotation});
                                // Check new stack.
                                if (!solution.check_stack(solution_bin.bin_type_id, new_stack_1))
                                    continue;
                                // Build new stack.
                                std::vector<std::pair<ItemTypeId, int>> new_stack_2;
                                for (ItemPos item_pos = 0; item_pos < item_pos_2; item_pos++)
                                    new_stack_2.push_back({solution_stack_2.items[item_pos].item_type_id, solution_stack_2.items[item_pos].rotation});
                                new_stack_2.push_back({solution_item_1.item_type_id, rotation_1});
                                for (ItemPos item_pos = item_pos_2 + 1; item_pos < (ItemPos)solution_stack_2.items.size(); item_pos++)
                                    new_stack_2.push_back({solution_stack_2.items[item_pos].item_type_id, solution_stack_2.items[item_pos].rotation});
                                // Check new stack.
                                if (!solution.check_stack(solution_bin.bin_type_id, new_stack_2))
                                    continue;
                                bin_pos_best = bin_pos;
                                stack_id_1_best = stack_pos_1;
                                item_pos_1_best = item_pos_1;
                                rotation_1_best = rotation_1;
                                stack_id_2_best = stack_pos_2;
                                item_pos_2_best = item_pos_2;
                                rotation_2_best = rotation_2;
                                violation_best = violation;
                            }
                        }
                    }
                }
            }

            if (bin_pos_best != -1) {
                // Apply move.
                //std::cout << "apply intra-swap"
                //    << " violation " << solution.compute_weight_constraints_violation()
                //    << " violation_best " << violation_best
                //    << std::endl;
                Solution solution_tmp(instance);
                for (BinPos bin_pos = 0; bin_pos < solution.number_of_different_bins(); ++bin_pos) {
                    const SolutionBin& solution_bin = solution.bin(bin_pos);
                    BinPos new_bin_pos = solution_tmp.add_bin(
                            solution_bin.bin_type_id,
                            solution_bin.copies);
                    for (StackId stack_id = 0; stack_id < (StackId)solution_bin.stacks.size(); ++stack_id) {
                        const SolutionStack& solution_stack = solution_bin.stacks[stack_id];
                        StackId new_stack_id = solution_tmp.add_stack(
                                new_bin_pos,
                                solution_stack.x_start,
                                solution_stack.x_end,
                                solution_stack.y_start,
                                solution_stack.y_end);
                        for (ItemPos item_pos = 0; item_pos < (ItemPos)solution_stack.items.size(); ++item_pos) {
                            if (bin_pos == bin_pos_best
                                    && stack_id == stack_id_1_best
                                    && item_pos == item_pos_1_best) {
                                const SolutionItem& solution_item = solution_bin.stacks[stack_id_2_best].items[item_pos_2_best];
                                solution_tmp.add_item(
                                        new_bin_pos,
                                        new_stack_id,
                                        solution_item.item_type_id,
                                        rotation_2_best);
                            } else if (bin_pos == bin_pos_best
                                    && stack_id == stack_id_2_best
                                    && item_pos == item_pos_2_best) {
                                const SolutionItem& solution_item = solution_bin.stacks[stack_id_1_best].items[item_pos_1_best];
                                solution_tmp.add_item(
                                        new_bin_pos,
                                        new_stack_id,
                                        solution_item.item_type_id,
                                        rotation_1_best);
                            } else {
                                const SolutionItem& solution_item = solution_stack.items[item_pos];
                                solution_tmp.add_item(
                                        new_bin_pos,
                                        new_stack_id,
                                        solution_item.item_type_id,
                                        solution_item.rotation);
                            }
                        }
                    }
                }
                // Check.
                Weight v = solution.compute_weight_constraints_violation();
                Weight v_tmp = solution_tmp.compute_weight_constraints_violation();
                bool better = (v > 0 && v_tmp == 0)
                    || (striclty_greater(v, v_tmp));
                if (!better) {
                    throw std::runtime_error("exchange");
                }
                if (!equal(solution_tmp.profit(), solution.profit())) {
                    throw std::runtime_error("exchange");
                }
                solution = solution_tmp;
                if (solution.compute_weight_constraints_violation() == 0)
                    break;
                continue;
            }
        }
        //std::cout << "exchange end" << std::endl;

        // Try to remove an item.
        //std::cout << "remove start" << std::endl;
        {
            BinPos bin_pos_best = -1;
            StackId stack_id_best = -1;
            ItemPos item_pos_best = -1;
            Length x_best = -1;
            double score_best = -1;
            Length xs_max = 0;
            for (BinPos bin_pos = 0; bin_pos < solution.number_of_different_bins(); ++bin_pos) {
                const SolutionBin& solution_bin = solution.bin(bin_pos);
                for (StackId stack_pos = 0; stack_pos < (StackId)solution_bin.stacks.size(); ++stack_pos) {
                    const SolutionStack& solution_stack = solution_bin.stacks[stack_pos];
                    if (solution_stack.items.size() == 0)
                        continue;
                    if (xs_max < solution_stack.x_start)
                        xs_max = solution_stack.x_start;
                }
                for (StackId stack_pos = 0; stack_pos < (StackId)solution_bin.stacks.size(); ++stack_pos) {
                    const SolutionStack& solution_stack = solution_bin.stacks[stack_pos];
                    // The stack is already empty.
                    if (solution_stack.items.size() == 0)
                        continue;
                    // The stack has a single item and is not the last one.
                    if (solution_stack.items.size() == 1 && solution_stack.x_start != xs_max)
                        continue;
                    // Don't remove a fixed item.
                    if (bin_pos < fixed_items.number_of_different_bins()
                            && stack_pos < (StackId)fixed_items.bin(bin_pos).stacks.size())
                        continue;
                    for (ItemPos item_pos = (ItemPos)solution_stack.items.size() - 1; item_pos >= 0; item_pos--) {
                    //for (ItemPos item_pos = (ItemPos)solution_stack.items.size() - 1; item_pos >= (ItemPos)solution_stack.items.size() - 1; item_pos--) {
                        const SolutionItem& solution_item = solution_stack.items[item_pos];
                        const ItemType& item_type = instance.item_type(solution_item.item_type_id);
                        std::vector<Weight> bin_weight = solution_bin.weight;
                        std::vector<Weight> bin_weight_weighted_sum = solution_bin.weight_weighted_sum;
                        for (GroupId group_id = 0; group_id <= item_type.group_id; ++group_id) {
                            bin_weight[group_id] -= item_type.weight;
                            bin_weight_weighted_sum[group_id]
                                -= ((double)solution_stack.x_start
                                        + (double)(solution_stack.x_end - solution_stack.x_start) / 2)
                                * item_type.weight;
                        }
                        double violation = solution.compute_weight_constraints_violation(
                                bin_pos, bin_weight, bin_weight_weighted_sum);
                        if (violation > 0 && !striclty_lesser(violation, violation_cur))
                            continue;
                        double score = (violation_cur - violation) / item_type.profit;
                        if (violation == 0)
                            score *= 1.1;
                        if (bin_pos_best == -1
                                || striclty_lesser(score_best, score)
                                || (equal(score_best, score)
                                    && x_best > solution_stack.x_start)) {
                            bin_pos_best = bin_pos;
                            stack_id_best = stack_pos;
                            item_pos_best = item_pos;
                            violation_best = violation;
                            x_best = solution_stack.x_start;
                            score_best = score;
                        }
                    }
                }
            }

            if (stack_id_best != -1) {
                // Apply move.
                //std::cout << "apply remove" << std::endl;
                Solution solution_tmp(instance);
                for (BinPos bin_pos = 0; bin_pos < solution.number_of_different_bins(); ++bin_pos) {
                    const SolutionBin& solution_bin = solution.bin(bin_pos);
                    BinPos new_bin_pos = solution_tmp.add_bin(
                            solution_bin.bin_type_id,
                            solution_bin.copies);
                    for (StackId stack_id = 0; stack_id < (StackId)solution_bin.stacks.size(); ++stack_id) {
                        const SolutionStack& solution_stack = solution_bin.stacks[stack_id];
                        StackId new_stack_id = solution_tmp.add_stack(
                                new_bin_pos,
                                solution_stack.x_start,
                                solution_stack.x_end,
                                solution_stack.y_start,
                                solution_stack.y_end);
                        for (ItemPos item_pos = 0; item_pos < (ItemPos)solution_stack.items.size(); ++item_pos) {
                            if (bin_pos == bin_pos_best
                                    && stack_id == stack_id_best
                                    && item_pos == item_pos_best)
                                continue;
                            const SolutionItem& solution_item = solution_stack.items[item_pos];
                            solution_tmp.add_item(
                                    new_bin_pos,
                                    new_stack_id,
                                    solution_item.item_type_id,
                                    solution_item.rotation);
                        }
                    }
                }
                // Check.
                //Weight v = solution.compute_weight_constraints_violation();
                //Weight v_tmp = solution_tmp.compute_weight_constraints_violation();
                //if (!equal(v_tmp, violation_best)) {
                //    throw std::runtime_error(
                //            "remove. v: "
                //            + std::to_string(v)
                //            + "; v_tmp: " + std::to_string(v_tmp)
                //            + "; violation_best: " + std::to_string(violation_best)
                //            + ".");
                //}
                solution = solution_tmp;
                if (solution.compute_weight_constraints_violation() == 0)
                    break;
                continue;
            }
        }
        //std::cout << "remove end" << std::endl;

        break;
    }

    //std::cout
    //    << "solution.compute_axle_weight_constraints_violation() "
    //    << solution.compute_axle_weight_constraints_violation() << std::endl;
    if (solution.compute_weight_constraints_violation() > 0)
        return output;

    for (;;) {

        // Now, we try to add some items on existing stacks, while maintaining
        // the feasibility of the solution.
        //std::cout << "add start" << std::endl;
        if (parameters.move_add) {
            BinPos bin_pos_best = -1;
            StackId stack_id_best = -1;
            ItemTypeId item_pos_best = -1;
            ItemTypeId item_type_id_best = -1;
            int rotation_best = -1;
            Profit profit_best = solution.profit();
            for (BinPos bin_pos = 0; bin_pos < solution.number_of_different_bins(); ++bin_pos) {
                const SolutionBin& solution_bin = solution.bin(bin_pos);
                for (StackId stack_pos = 0; stack_pos < (StackId)solution_bin.stacks.size(); ++stack_pos) {
                    const SolutionStack& solution_stack = solution_bin.stacks[stack_pos];
                    if (solution_stack.items.empty())
                        continue;
                    ItemTypeId j0 = solution_stack.items.front().item_type_id;
                    const ItemType& item_type_0 = instance.item_type(j0);
                    for (ItemTypeId item_type_id = 0;
                           item_type_id < instance.number_of_item_types();
                           ++item_type_id) {
                        const ItemType& item_type = instance.item_type(item_type_id);
                        // Check if there remain unpack copies of the item type.
                        if (solution.item_copies(item_type_id) == item_type.copies)
                            continue;
                        // Check stackability id and group.
                        if (item_type.stackability_id != item_type_0.stackability_id
                                || item_type.group_id != item_type_0.group_id)
                            continue;
                        // Check orientation.
                        int rotation = -1;
                        Direction o = Direction::X;
                        for (int r = 0; r < 6; ++r) {
                            if (item_type.can_rotate(r)) {
                                Length xj = instance.x(item_type, r, o);
                                Length yj = instance.y(item_type, r, o);
                                if (xj == solution_stack.x_end - solution_stack.x_start
                                        && yj == solution_stack.y_end - solution_stack.y_start) {
                                    rotation = r;
                                    break;
                                }
                            }
                        }
                        if (rotation == -1)
                            continue;
                        // Compute violation.
                        std::vector<Weight> bin_weight = solution_bin.weight;
                        std::vector<Weight> bin_weight_weighted_sum = solution_bin.weight_weighted_sum;
                        for (GroupId group_id = 0; group_id <= item_type.group_id; ++group_id) {
                            bin_weight[group_id] += item_type.weight;
                            bin_weight_weighted_sum[group_id]
                                += ((double)solution_stack.x_start
                                        + (double)(solution_stack.x_end - solution_stack.x_start) / 2)
                                * item_type.weight;
                        }
                        Profit profit = solution.profit() + item_type.profit;
                        if (solution.compute_weight_constraints_violation(
                                    bin_pos, bin_weight, bin_weight_weighted_sum) > 0)
                            continue;
                        // Check positions in the stack.
                        for (ItemPos item_pos = (ItemPos)solution_stack.items.size(); item_pos >= 0; item_pos--) {
                            // Build new stack.
                            std::vector<std::pair<ItemTypeId, int>> new_stack;
                            for (ItemPos item_pos_2 = 0; item_pos_2 < item_pos; item_pos_2++)
                                new_stack.push_back({solution_stack.items[item_pos_2].item_type_id, solution_stack.items[item_pos_2].rotation});
                            new_stack.push_back({item_type_id, rotation});
                            for (ItemPos item_pos_2 = item_pos; item_pos_2 < (ItemPos)solution_stack.items.size(); item_pos_2++)
                                new_stack.push_back({solution_stack.items[item_pos_2].item_type_id, solution_stack.items[item_pos_2].rotation});
                            // Check new stack.
                            if (!solution.check_stack(solution_bin.bin_type_id, new_stack))
                                continue;
                            if (striclty_lesser(profit_best, profit)) {
                                bin_pos_best = bin_pos;
                                stack_id_best = stack_pos;
                                item_pos_best = item_pos;
                                item_type_id_best = item_type_id;
                                rotation_best = rotation;
                                profit_best = profit;
                            }
                        }
                    }
                }
            }

            if (stack_id_best != -1) {
                // Apply move.
                //std::cout << "apply add"
                //    << " profit " << solution.profit()
                //    << " profit_best " << profit_best
                //    << std::endl;
                Solution solution_tmp(instance);
                for (BinPos bin_pos = 0; bin_pos < solution.number_of_different_bins(); ++bin_pos) {
                    const SolutionBin& solution_bin = solution.bin(bin_pos);
                    BinPos new_bin_pos = solution_tmp.add_bin(solution_bin.bin_type_id, solution_bin.copies);
                    for (StackId stack_id = 0; stack_id < (StackId)solution_bin.stacks.size(); ++stack_id) {
                        const SolutionStack& solution_stack = solution_bin.stacks[stack_id];
                        StackId new_stack_id = solution_tmp.add_stack(
                                new_bin_pos,
                                solution_stack.x_start,
                                solution_stack.x_end,
                                solution_stack.y_start,
                                solution_stack.y_end);
                        if (bin_pos == bin_pos_best && stack_id == stack_id_best) {
                            for (ItemPos item_pos = 0; item_pos < item_pos_best; ++item_pos) {
                                const SolutionItem& solution_item = solution_stack.items[item_pos];
                                solution_tmp.add_item(
                                        new_bin_pos,
                                        new_stack_id,
                                        solution_item.item_type_id,
                                        solution_item.rotation);
                            }
                            solution_tmp.add_item(
                                    new_bin_pos,
                                    new_stack_id,
                                    item_type_id_best,
                                    rotation_best);
                            for (ItemPos item_pos = item_pos_best; item_pos < (ItemPos)solution_stack.items.size(); ++item_pos) {
                                const SolutionItem& solution_item = solution_stack.items[item_pos];
                                solution_tmp.add_item(
                                        new_bin_pos,
                                        new_stack_id,
                                        solution_item.item_type_id,
                                        solution_item.rotation);
                            }
                        } else {
                            for (ItemPos item_pos = 0; item_pos < (ItemPos)solution_stack.items.size(); ++item_pos) {
                                const SolutionItem& solution_item = solution_stack.items[item_pos];
                                solution_tmp.add_item(
                                        new_bin_pos,
                                        new_stack_id,
                                        solution_item.item_type_id,
                                        solution_item.rotation);
                            }
                        }
                    }
                }
                // Check.
                if (!equal(solution_tmp.profit(), profit_best)) {
                    throw std::runtime_error("add");
                }
                if (!striclty_greater(solution_tmp.profit(), solution.profit())) {
                    throw std::runtime_error("add");
                }
                if (solution_tmp.compute_weight_constraints_violation() > 0) {
                    throw std::runtime_error("add");
                }
                solution = solution_tmp;
                continue;
            }
        }
        //std::cout << "add end" << std::endl;

        // Try to swap an item outisde of the solution with an item from the
        // solution.
        //std::cout << "inter-swap start" << std::endl;
        if (parameters.move_inter_swap) {
            BinPos bin_pos_best = -1;
            StackId stack_id_best = -1;
            ItemTypeId item_pos_best = -1;
            ItemTypeId item_type_id_best = -1;
            int rotation_best = -1;
            Profit profit_best = solution.profit();
            for (BinPos bin_pos = 0; bin_pos < solution.number_of_different_bins(); ++bin_pos) {
                const SolutionBin& solution_bin = solution.bin(bin_pos);
                for (StackId stack_pos = 0; stack_pos < (StackId)solution_bin.stacks.size(); ++stack_pos) {
                    const SolutionStack& solution_stack = solution_bin.stacks[stack_pos];
                    if (solution_stack.items.empty())
                        continue;
                    ItemTypeId j0 = solution_stack.items.front().item_type_id;
                    const ItemType& item_type_0 = instance.item_type(j0);
                    for (ItemTypeId item_type_id = 0;
                            item_type_id < instance.number_of_item_types();
                            ++item_type_id) {
                        const ItemType& item_type = instance.item_type(item_type_id);
                        // Check if there remain unpack copies of the item type.
                        if (solution.item_copies(item_type_id) == item_type.copies)
                            continue;
                        // Check stackability id and group.
                        if (item_type.stackability_id != item_type_0.stackability_id
                                || item_type.group_id != item_type_0.group_id)
                            continue;
                        // Check orientation.
                        int rotation = -1;
                        Direction o = Direction::X;
                        for (int r = 0; r < 6; ++r) {
                            if (item_type.can_rotate(r)) {
                                Length xj = instance.x(item_type, r, o);
                                Length yj = instance.y(item_type, r, o);
                                if (xj == solution_stack.x_end - solution_stack.x_start
                                        && yj == solution_stack.y_end - solution_stack.y_start) {
                                    rotation = r;
                                    break;
                                }
                            }
                        }
                        if (rotation == -1)
                            continue;
                        // Check item to replace.
                        for (ItemPos item_pos = (ItemPos)solution_stack.items.size() - 1; item_pos >= 0; item_pos--) {
                            ItemTypeId item_type_id_old = solution_stack.items[item_pos].item_type_id;
                            if (item_type_id_old == item_type_id)
                                continue;
                            const ItemType& item_type_old = instance.item_type(item_type_id_old);
                            // Compute violation.
                            std::vector<Weight> bin_weight = solution_bin.weight;
                            std::vector<Weight> bin_weight_weighted_sum = solution_bin.weight_weighted_sum;
                            for (GroupId group_id = 0; group_id <= item_type_old.group_id; ++group_id) {
                                bin_weight[group_id] -= item_type_old.weight;
                                bin_weight_weighted_sum[group_id]
                                    -= ((double)solution_stack.x_start
                                            + (double)(solution_stack.x_end - solution_stack.x_start) / 2)
                                    * item_type_old.weight;
                            }
                            for (GroupId group_id = 0; group_id <= item_type.group_id; ++group_id) {
                                bin_weight[group_id] += item_type.weight;
                                bin_weight_weighted_sum[group_id]
                                    += ((double)solution_stack.x_start
                                            + (double)(solution_stack.x_end - solution_stack.x_start) / 2)
                                    * item_type.weight;
                            }
                            Profit profit = solution.profit()
                                - item_type_old.profit + item_type.profit;
                            if (solution.compute_weight_constraints_violation(
                                        bin_pos, bin_weight, bin_weight_weighted_sum) > 0)
                                continue;
                            // Build new stack.
                            std::vector<std::pair<ItemTypeId, int>> new_stack;
                            for (ItemPos item_pos_2 = 0; item_pos_2 < item_pos; item_pos_2++)
                                new_stack.push_back({
                                        solution_stack.items[item_pos_2].item_type_id,
                                        solution_stack.items[item_pos_2].rotation});
                            new_stack.push_back({item_type_id, rotation});
                            for (ItemPos item_pos_2 = item_pos + 1; item_pos_2 < (ItemPos)solution_stack.items.size(); item_pos_2++)
                                new_stack.push_back({
                                        solution_stack.items[item_pos_2].item_type_id,
                                        solution_stack.items[item_pos_2].rotation});
                            // Check new stack.
                            if (!solution.check_stack(solution_bin.bin_type_id, new_stack))
                                continue;
                            if (striclty_lesser(profit_best, profit)) {
                                bin_pos_best = bin_pos;
                                stack_id_best = stack_pos;
                                item_pos_best = item_pos;
                                item_type_id_best = item_type_id;
                                rotation_best = rotation;
                                profit_best = profit;
                            }
                        }
                    }
                }
            }

            if (stack_id_best != -1) {
                // Apply move.
                //std::cout << "apply inter-swap"
                //    << " profit " << solution.profit()
                //    << " profit_best " << profit_best
                //    << std::endl;
                Solution solution_tmp(instance);
                for (BinPos bin_pos = 0; bin_pos < solution.number_of_different_bins(); ++bin_pos) {
                    const SolutionBin& solution_bin = solution.bin(bin_pos);
                    BinPos new_bin_pos = solution_tmp.add_bin(
                            solution_bin.bin_type_id,
                            solution_bin.copies);
                    for (StackId stack_id = 0; stack_id < (StackId)solution_bin.stacks.size(); ++stack_id) {
                        const SolutionStack& solution_stack = solution_bin.stacks[stack_id];
                        StackId new_stack_id = solution_tmp.add_stack(
                                new_bin_pos,
                                solution_stack.x_start,
                                solution_stack.x_end,
                                solution_stack.y_start,
                                solution_stack.y_end);
                        if (bin_pos == bin_pos_best && stack_id == stack_id_best) {
                            for (ItemPos item_pos = 0; item_pos < item_pos_best; ++item_pos) {
                                const SolutionItem& solution_item = solution_stack.items[item_pos];
                                solution_tmp.add_item(
                                        new_bin_pos,
                                        new_stack_id,
                                        solution_item.item_type_id,
                                        solution_item.rotation);
                            }
                            solution_tmp.add_item(
                                    new_bin_pos,
                                    new_stack_id,
                                    item_type_id_best,
                                    rotation_best);
                            for (ItemPos item_pos = item_pos_best + 1; item_pos < (ItemPos)solution_stack.items.size(); ++item_pos) {
                                const SolutionItem& solution_item = solution_stack.items[item_pos];
                                solution_tmp.add_item(
                                        new_bin_pos,
                                        new_stack_id,
                                        solution_item.item_type_id,
                                        solution_item.rotation);
                            }
                        } else {
                            for (ItemPos item_pos = 0; item_pos < (ItemPos)solution_stack.items.size(); ++item_pos) {
                                const SolutionItem& solution_item = solution_stack.items[item_pos];
                                solution_tmp.add_item(
                                        new_bin_pos,
                                        new_stack_id,
                                        solution_item.item_type_id,
                                        solution_item.rotation);
                            }
                        }
                    }
                }
                // Check.
                if (!equal(solution_tmp.profit(), profit_best)) {
                    throw std::runtime_error("inter-swap");
                }
                if (!striclty_greater(solution_tmp.profit(), solution.profit())) {
                    throw std::runtime_error("inter-swap");
                }
                if (solution_tmp.compute_weight_constraints_violation() > 0) {
                    throw std::runtime_error("inter-swap");
                }
                solution = solution_tmp;
            }
        }
        //std::cout << "inter-swap end" << std::endl;

        break;
    }

    //solution.write("sol_" + std::to_string(iteration) + "_" + std::to_string(rectangle_parameters.guide_id) + "_after.csv");
    //std::cout << "solution.profit() " << solution.profit() << std::endl;

    // Save the solution if feasible.
    if (solution.compute_weight_constraints_violation() == 0) {
        std::stringstream ss;
        ss << "iteration " << iteration;
        sequential_onedimensional_rectangle_output.solution_pool.add(solution, ss, parameters.info);
    }
    output.solution = solution;
    return output;
}

SequentialOneDimensionalRectangleOutput boxstacks::sequential_onedimensional_rectangle(
        const Instance& instance,
        SequentialOneDimensionalRectangleOptionalParameters parameters)
{
    //std::cout << "sequential_onedimensional_rectangle" << std::endl;
    SequentialOneDimensionalRectangleOutput output(instance);

    // Part of solution which is fixed.
    Solution fixed_items(instance);

    for (Counter iteration = 0;; ++iteration) {
        //std::cout << "iteration " << iteration << std::endl;
        //std::cout << "number_of_fixed_items " << fixed_items.number_of_items() << std::endl;

        // Compute the number of copies of each item type to pack, considering
        // the part of the solution which is already fixed.
        std::vector<ItemPos> copies(instance.number_of_item_types(), 0);
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            copies[item_type_id] = instance.item_type(item_type_id).copies;
            if (iteration > 0)
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
                } else if (item_type.can_rotate(0)) {
                    onedim_instance_builder.set_item_type_eligibility(
                            onedim_item_type_id,
                            0);
                } else if (item_type.can_rotate(1)) {
                    onedim_instance_builder.set_item_type_eligibility(
                            onedim_item_type_id,
                            1);
                }
            }

            onedimensional::Instance onedim_instance = onedim_instance_builder.build();

            // Solve onedimensional instance.
            onedimensional::OptimizeOptionalParameters onedim_parameters = parameters.onedimensional_parameters;
            onedim_parameters.column_generation_maximum_discrepancy = 0;
            onedim_parameters.info = Info(parameters.info, false, "");
            //onedim_parameters.info.set_verbosity_level(2);
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
            if (parameters.info.needs_to_end())
                return output;
            if (!onedim_solution.full()) {
                throw std::runtime_error("No solution to VBPP subproblem.");
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
        }

        // Add item types.
        std::vector<std::tuple<StackabilityId, int, StackId>> rectangle2boxstacks;
        for (StackId stackability_group_pos = 0; stackability_group_pos < (StackId)stackability_groups.size(); ++stackability_group_pos) {
            const StackabilityGroup& stackability_group = stackability_groups[stackability_group_pos];

            for (int rotation = 0; rotation < 3; ++rotation) {
                bool oriented = (rotation != 2);
                Length x = (rotation != 1)? stackability_group.x: stackability_group.y;
                Length y = (rotation != 1)? stackability_group.y: stackability_group.x;
                //std::cout << "stackability_group_pos " << stackability_group_pos
                //    << " rotation " << rotation
                //    << " copies " << copies
                //    << std::endl;
                for (StackId stack_pos = 0; stack_pos < (StackId)stackability_group.stacks[rotation].size(); ++stack_pos) {
                    const Stack& stack = stackability_group.stacks[rotation][stack_pos];
                    auto rectangle_item_type_id = rectangle_instance_builder.add_item_type(
                            x,
                            y,
                            stack.profit,
                            1,  // copies
                            oriented,
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
                        false, // oriented
                        item_type.group_id);
                rectangle_instance_builder.set_item_type_weight(rectangle_item_type_id, weight);
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
        rectangle::Solution rectangle_fixed_items(rectangle_instance);
        for (BinPos bin_pos = 0; bin_pos < fixed_items.number_of_different_bins(); ++bin_pos) {
            const SolutionBin& solution_bin = fixed_items.bin(bin_pos);
            rectangle_fixed_items.add_bin(bin_pos, 1);
            for (StackId stack_pos = 0; stack_pos < (StackId)solution_bin.stacks.size(); ++stack_pos) {
                const SolutionStack& solution_stack = solution_bin.stacks[stack_pos];
                ItemTypeId rectangle_item_type_id = solution_stack_2_rectangle_item_type_id[bin_pos][stack_pos];
                rectangle_fixed_items.add_item(
                        bin_pos,
                        rectangle_item_type_id,
                        {solution_stack.x_start, solution_stack.y_start},
                        false);
            }
        }

        //rectangle_instance.print(std::cout, 2);

        Profit best_possible_profit = 0;
        if (try_to_pack_all_items) {

            // First we try to pack all items with guides 0 and 1. This the
            // guides that will most likely lead to the best solution if axle
            // weight constraints are not critical.
            // If the solution is not full, it might be
            // - Because of axle weight constraints
            //   In this case, we try again to pack all items but with
            //   guide 8.
            //   If the solution is still not full, then we give up on trying
            //   to pack all items and try again with guides 4 and 5.
            // - Because of geometric constraints (area)
            //   In this case we give up on trying to pack all items and try
            //   again with guides 4 and 5.
            //   If axle weight constraints happen to be critical, we try
            //   again with guide 8.

            bool failed = false;

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
                        iteration,
                        fixed_items,
                        stackability_groups,
                        rectangle_instance,
                        rectangle2boxstacks,
                        rectangle_parameters);
                best_possible_profit = std::max(best_possible_profit, subproblem_output.profit_before_repair);
                failed |= subproblem_output.failed;
                if (output.solution_pool.best().full())
                    break;
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
                        iteration,
                        fixed_items,
                        stackability_groups,
                        rectangle_instance,
                        rectangle2boxstacks,
                        rectangle_parameters);
                best_possible_profit = std::max(
                        best_possible_profit,
                        subproblem_output.profit_before_repair);
                failed |= subproblem_output.failed;
                if (output.solution_pool.best().full())
                    break;
            }

            if (failed) {

                {
                    rectangle::BranchingScheme::Parameters rectangle_parameters;
                    rectangle_parameters.guide_id = 8;
                    rectangle_parameters.predecessor_strategy = 2;
                    rectangle_parameters.group_guiding_strategy = 1;
                    rectangle_parameters.staircase = false;
                    rectangle_parameters.fixed_items = &rectangle_fixed_items;
                    auto subproblem_output = sequential_onedimensional_rectangle_subproblem(
                            instance,
                            parameters,
                            output,
                            iteration,
                            fixed_items,
                            stackability_groups,
                            rectangle_instance,
                            rectangle2boxstacks,
                            rectangle_parameters);
                    best_possible_profit = std::max(
                            best_possible_profit,
                            subproblem_output.profit_before_repair);
                    if (output.solution_pool.best().full())
                        break;
                }

                bool failed = false;

                {
                    rectangle::BranchingScheme::Parameters rectangle_parameters;
                    rectangle_parameters.guide_id = 4;
                    rectangle_parameters.predecessor_strategy = 1;
                    rectangle_parameters.group_guiding_strategy = 1;
                    rectangle_parameters.staircase = false;
                    rectangle_parameters.fixed_items = &rectangle_fixed_items;
                    auto subproblem_output = sequential_onedimensional_rectangle_subproblem(
                            instance,
                            parameters,
                            output,
                            iteration,
                            fixed_items,
                            stackability_groups,
                            rectangle_instance,
                            rectangle2boxstacks,
                            rectangle_parameters);
                    best_possible_profit = std::max(
                            best_possible_profit,
                            subproblem_output.profit_before_repair);
                    failed |= subproblem_output.failed;
                    if (output.solution_pool.best().full())
                        break;
                }

                {
                    rectangle::BranchingScheme::Parameters rectangle_parameters;
                    rectangle_parameters.guide_id = 5;
                    rectangle_parameters.predecessor_strategy = 1;
                    rectangle_parameters.group_guiding_strategy = 1;
                    rectangle_parameters.staircase = false;
                    rectangle_parameters.fixed_items = &rectangle_fixed_items;
                    auto subproblem_output = sequential_onedimensional_rectangle_subproblem(
                            instance,
                            parameters,
                            output,
                            iteration,
                            fixed_items,
                            stackability_groups,
                            rectangle_instance,
                            rectangle2boxstacks,
                            rectangle_parameters);
                    best_possible_profit = std::max(
                            best_possible_profit,
                            subproblem_output.profit_before_repair);
                    failed |= subproblem_output.failed;
                    if (output.solution_pool.best().full())
                        break;
                }

                if (!failed)
                    break;

            } else {

                bool failed = false;

                {
                    rectangle::BranchingScheme::Parameters rectangle_parameters;
                    rectangle_parameters.guide_id = 4;
                    rectangle_parameters.predecessor_strategy = 1;
                    rectangle_parameters.group_guiding_strategy = 1;
                    rectangle_parameters.staircase = false;
                    rectangle_parameters.fixed_items = &rectangle_fixed_items;
                    auto subproblem_output = sequential_onedimensional_rectangle_subproblem(
                            instance,
                            parameters,
                            output,
                            iteration,
                            fixed_items,
                            stackability_groups,
                            rectangle_instance,
                            rectangle2boxstacks,
                            rectangle_parameters);
                    best_possible_profit = std::max(
                            best_possible_profit,
                            subproblem_output.profit_before_repair);
                    failed |= subproblem_output.failed;
                    if (output.solution_pool.best().full())
                        break;
                }

                {
                    rectangle::BranchingScheme::Parameters rectangle_parameters;
                    rectangle_parameters.guide_id = 5;
                    rectangle_parameters.predecessor_strategy = 1;
                    rectangle_parameters.group_guiding_strategy = 1;
                    rectangle_parameters.staircase = false;
                    rectangle_parameters.fixed_items = &rectangle_fixed_items;
                    auto subproblem_output = sequential_onedimensional_rectangle_subproblem(
                            instance,
                            parameters,
                            output,
                            iteration,
                            fixed_items,
                            stackability_groups,
                            rectangle_instance,
                            rectangle2boxstacks,
                            rectangle_parameters);
                    best_possible_profit = std::max(
                            best_possible_profit,
                            subproblem_output.profit_before_repair);
                    failed |= subproblem_output.failed;
                    if (output.solution_pool.best().full())
                        break;
                }

                if (!failed)
                    break;

                {
                    rectangle::BranchingScheme::Parameters rectangle_parameters;
                    rectangle_parameters.guide_id = 8;
                    rectangle_parameters.predecessor_strategy = 2;
                    rectangle_parameters.group_guiding_strategy = 1;
                    rectangle_parameters.staircase = false;
                    rectangle_parameters.fixed_items = &rectangle_fixed_items;
                    auto subproblem_output = sequential_onedimensional_rectangle_subproblem(
                            instance,
                            parameters,
                            output,
                            iteration,
                            fixed_items,
                            stackability_groups,
                            rectangle_instance,
                            rectangle2boxstacks,
                            rectangle_parameters);
                    best_possible_profit = std::max(
                            best_possible_profit,
                            subproblem_output.profit_before_repair);
                    if (output.solution_pool.best().full())
                        break;
                }

            }

        } else {

            // All items do not fit in the bin. We try to maximize the profit
            // of the packed items.
            // First we try to pack with guides 4 and 5.
            // If the axle weight constraints happened to be critical, then we
            // try again with guide 8.

            bool failed = false;

            {
                rectangle::BranchingScheme::Parameters rectangle_parameters;
                rectangle_parameters.guide_id = 4;
                rectangle_parameters.predecessor_strategy = 1;
                rectangle_parameters.group_guiding_strategy = 1;
                rectangle_parameters.staircase = false;
                rectangle_parameters.fixed_items = &rectangle_fixed_items;
                auto subproblem_output = sequential_onedimensional_rectangle_subproblem(
                        instance,
                        parameters,
                        output,
                        iteration,
                        fixed_items,
                        stackability_groups,
                        rectangle_instance,
                        rectangle2boxstacks,
                        rectangle_parameters);
                best_possible_profit = std::max(
                        best_possible_profit,
                        subproblem_output.profit_before_repair);
                failed |= subproblem_output.failed;
            }

            {
                rectangle::BranchingScheme::Parameters rectangle_parameters;
                rectangle_parameters.guide_id = 5;
                rectangle_parameters.predecessor_strategy = 1;
                rectangle_parameters.group_guiding_strategy = 1;
                rectangle_parameters.staircase = false;
                rectangle_parameters.fixed_items = &rectangle_fixed_items;
                auto subproblem_output = sequential_onedimensional_rectangle_subproblem(
                        instance,
                        parameters,
                        output,
                        iteration,
                        fixed_items,
                        stackability_groups,
                        rectangle_instance,
                        rectangle2boxstacks,
                        rectangle_parameters);
                best_possible_profit = std::max(
                        best_possible_profit,
                        subproblem_output.profit_before_repair);
                failed |= subproblem_output.failed;
            }

            if (!failed)
                break;

            {
                rectangle::BranchingScheme::Parameters rectangle_parameters;
                rectangle_parameters.guide_id = 8;
                rectangle_parameters.predecessor_strategy = 2;
                rectangle_parameters.group_guiding_strategy = 1;
                rectangle_parameters.staircase = false;
                rectangle_parameters.fixed_items = &rectangle_fixed_items;
                auto subproblem_output = sequential_onedimensional_rectangle_subproblem(
                        instance,
                        parameters,
                        output,
                        iteration,
                        fixed_items,
                        stackability_groups,
                        rectangle_instance,
                        rectangle2boxstacks,
                        rectangle_parameters);
                best_possible_profit = std::max(
                        best_possible_profit,
                        subproblem_output.profit_before_repair);
            }

        }

        Solution solution = output.solution_pool.best();
        output.failed = true;

        // If the profit of the solution before repair is lower than the profit
        // of the previous feasible solution, stop.
        //std::cout << "best_possible_profit " << best_possible_profit
        //    << " output.solution_pool.best().profit() "
        //    << output.solution_pool.best().profit()
        //    << std::endl;
        if (best_possible_profit <= output.solution_pool.best().profit())
            break;

        //std::cout << "solution.number_of_items " << solution.number_of_items() << std::endl;
        if (solution.number_of_items() == 0)
            break;

        bool has_empty_stack = false;
        for (BinPos bin_pos = 0; bin_pos < solution.number_of_different_bins(); ++bin_pos) {
            const SolutionBin& solution_bin = solution.bin(bin_pos);
            for (StackId stack_pos = 0; stack_pos < (StackId)solution_bin.stacks.size(); ++stack_pos) {
                if (solution_bin.stacks[stack_pos].items.empty())
                    has_empty_stack = true;
            }
        }
        //std::cout << "has_empty_stack " << has_empty_stack << std::endl;
        if (has_empty_stack)
            break;

        // Build fixed items.
        // The proportion of fixed items is not more than:
        // (iteration + 1) / (iteration + 3)
        // - iteration 0: 0.33
        // - iteration 1: 0.5
        // - iteration 2: 0.6
        // - iteration 3: 0.66
        // - iteration 4: 0.71
        // ...
        // In addition, we do not allow fixing an item of group g if there is
        // an unpacked item of group g' > g..

        // Compute greatest group id in unpacked items.
        GroupId unpacked_group_id_max = -1;
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            if (solution.item_copies(item_type_id) < instance.item_type(item_type_id).copies) {
                unpacked_group_id_max = std::max(
                        unpacked_group_id_max,
                        instance.item_type(item_type_id).group_id);
            }
        }
        //std::cout << "unpacked_group_id_max " << unpacked_group_id_max << std::endl;

        // Get the stacks and sort them by x-coordinate.
        ItemPos n_prev = fixed_items.number_of_items();
        fixed_items = Solution(instance);
        std::vector<std::pair<BinPos, StackId>> sorted_stacks;
        for (BinPos bin_pos = 0; bin_pos < solution.number_of_different_bins(); ++bin_pos) {
            const SolutionBin& solution_bin = solution.bin(bin_pos);
            fixed_items.add_bin(
                    solution_bin.bin_type_id,
                    solution_bin.copies);
            for (StackId stack_pos = 0; stack_pos < (StackId)solution_bin.stacks.size(); ++stack_pos) {
                if (!solution_bin.stacks[stack_pos].items.empty())
                    sorted_stacks.push_back({bin_pos, stack_pos});
            }
        }
        std::sort(
                sorted_stacks.begin(),
                sorted_stacks.end(),
                [&solution](
                    const std::pair<BinPos, StackId>& p1,
                    const std::pair<BinPos, StackId>& p2) -> bool
                {
                    if (p1.first != p2.first)
                        return p1.first < p2.first;
                    Length x1 = solution.bin(p1.first).stacks[p1.second].x_start;
                    Length x2 = solution.bin(p2.first).stacks[p2.second].x_start;
                    return x1 < x2;
                });

        double coef = (double)(iteration + 1) / (iteration + 3);
        for (StackId pos = 0; pos < (StackId)sorted_stacks.size() * coef; ++pos) {
            BinPos bin_pos = sorted_stacks[pos].first;
            StackId stack_pos = sorted_stacks[pos].second;
            const SolutionStack& solution_stack = solution.bin(bin_pos).stacks[stack_pos];
            if (try_to_pack_all_items) {
                if (instance.item_type(solution_stack.items.front().item_type_id).group_id
                        < unpacked_group_id_max)
                    break;
            } else {
                if (instance.item_type(solution_stack.items.front().item_type_id).group_id
                        < instance.number_of_groups() - iteration - 1)
                    break;
            }
            StackId fixed_item_stack_pos = fixed_items.add_stack(
                    bin_pos,
                    solution_stack.x_start,
                    solution_stack.x_end,
                    solution_stack.y_start,
                    solution_stack.y_end);
            for (const SolutionItem& solution_item: solution_stack.items) {
                fixed_items.add_item(
                        bin_pos,
                        fixed_item_stack_pos,
                        solution_item.item_type_id,
                        solution_item.rotation);
            }
        }
        // Avoid infinite loop.
        // This might happen if previously unpacked item with the greatest
        // group id has been packed by the rectangle algorithm.
        if (fixed_items.number_of_items() <= n_prev)
            break;
    }

    //std::cout << "sequential_onedimensional_rectangle end" << std::endl;
    return output;
}
