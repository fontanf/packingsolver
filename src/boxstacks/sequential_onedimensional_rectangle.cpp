#include "boxstacks/sequential_onedimensional_rectangle.hpp"

#include "packingsolver/boxstacks/algorithm_formatter.hpp"
#include "boxstacks/solution_builder.hpp"
#include "boxstacks/instance_flipper.hpp"
#include "boxstacks/tree_search.hpp"
#include "algorithms/thread_pool.hpp"

#include "packingsolver/onedimensional/instance_builder.hpp"

#include "packingsolver/rectangle/instance_builder.hpp"
#include "rectangle/tree_search.hpp"
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

/**
 * Lightweight, single-bin solution representation used only by the frontier
 * search in sequential_onedimensional_rectangle's main loop below.
 *
 * That loop copies a solution and extends it with one stack+item per
 * candidate, many times per outer iteration, so add_stack/add_item here stay
 * O(1) (mirroring what boxstacks::Solution::add_stack/add_item used to do
 * inline before they moved to SolutionBuilder) instead of paying the cost of
 * a full SolutionBuilder(const Solution&) resume+recompute on every
 * candidate. It is converted to a real boxstacks::Solution (via
 * SolutionBuilder) only once a candidate is accepted into the frontier.
 */
struct SorSolution
{
    SorSolution(const Instance& instance):
        item_copies(instance.number_of_item_types(), 0),
        weight(instance.number_of_groups(), 0.0),
        weight_weighted_sum(instance.number_of_groups(), 0.0)
    { }

    /** Stacks of the (single, implicit) bin. */
    std::vector<SolutionStack> stacks;

    /** Number of copies of each item type. */
    std::vector<ItemPos> item_copies;

    /** Maximum x. */
    Length x_max = 0;

    /** Total item weight. */
    Weight item_weight = 0.0;

    /** For each group, weight. */
    std::vector<Weight> weight;

    /** For each group, sum of x times weight of all items. */
    std::vector<Weight> weight_weighted_sum;

    /** Add a stack. */
    StackId add_stack(
            Length x_start,
            Length x_end,
            Length y_start,
            Length y_end)
    {
        SolutionStack stack;
        stack.x_start = x_start;
        stack.x_end = x_end;
        stack.y_start = y_start;
        stack.y_end = y_end;
        stack.weight = std::vector<Weight>(weight.size(), 0.0);
        stack.weight_weighted_sum = std::vector<Weight>(weight.size(), 0.0);
        stacks.push_back(stack);
        if (x_max < x_end)
            x_max = x_end;
        return stacks.size() - 1;
    }

    /** Add an item. */
    void add_item(
            const Instance& instance,
            StackId stack_id,
            ItemTypeId item_type_id,
            Rotation rotation)
    {
        SolutionStack& stack = stacks[stack_id];
        const ItemType& item_type = instance.item_type(item_type_id);

        SolutionItem item;
        item.item_type_id = item_type_id;
        item.z_start = stack.z_end;
        if (!stack.items.empty())
            item.z_start -= item_type.nesting_height;
        item.rotation = rotation;

        stack.z_end = item.z_start + item_type.z(rotation);
        stack.items.push_back(item);
        for (GroupId group_id = 0; group_id <= item_type.group_id; ++group_id) {
            stack.weight[group_id] += item_type.weight;
            stack.weight_weighted_sum[group_id]
                += ((double)stack.x_start + (double)(stack.x_end - stack.x_start) / 2) * item_type.weight;
            weight[group_id] += item_type.weight;
            weight_weighted_sum[group_id]
                += ((double)stack.x_start + (double)(stack.x_end - stack.x_start) / 2) * item_type.weight;
        }

        item_copies[item_type_id]++;
        item_weight += item_type.weight;
    }
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
        const rectangle::BranchingScheme::Parameters rectangle_parameters,
        NodeId rectangle_queue_size)
{
    auto logger = parameters.get_logger();
    SequentialOneDimensionalRectangleSubproblemOutput output(instance);
    FFOT_LOG_FOLD_START(
            logger,
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
    ibs_parameters.minimum_size_of_the_queue = rectangle_queue_size;
    ibs_parameters.maximum_size_of_the_queue = rectangle_queue_size;
    auto rectangle_begin = std::chrono::steady_clock::now();
    auto rectangle_output = treesearchsolver::iterative_beam_search_2<rectangle::BranchingScheme>(
            rectangle_branching_scheme,
            ibs_parameters);
    if (sor_output.number_of_iterations == 0 && rectangle_output.optimal)
        sor_output.rectangle_subproblem_explored_exhaustively = true;
    auto rectangle_end = std::chrono::steady_clock::now();
    std::chrono::duration<double> rectangle_time_span
        = std::chrono::duration_cast<std::chrono::duration<double>>(rectangle_end - rectangle_begin);
    sor_output.rectangle_time += rectangle_time_span.count();
    sor_output.number_of_rectangle_calls++;
    auto rectangle_solution = rectangle_branching_scheme.to_solution(rectangle_output.solution_pool.best());
    FFOT_LOG(
            logger,
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
    SolutionBuilder solution_builder(instance);
    BinPos bin_pos = solution_builder.add_bin(0, 1);
    if (sor_output.number_of_iterations > 0) {
        // Re-add the stacks/items already fixed from the previous iteration.
        const SolutionBin& fixed_bin = fixed_items.bin(0);
        for (const SolutionStack& fixed_stack: fixed_bin.stacks) {
            StackId fixed_stack_id = solution_builder.add_stack(
                    bin_pos,
                    fixed_stack.x_start,
                    fixed_stack.x_end,
                    fixed_stack.y_start,
                    fixed_stack.y_end);
            for (const SolutionItem& fixed_item: fixed_stack.items) {
                solution_builder.add_item(
                        bin_pos,
                        fixed_stack_id,
                        fixed_item.item_type_id,
                        fixed_item.rotation);
            }
        }
    }
    for (auto it = locations.begin(); it != locations.end(); ++it) {
        Location& location = *it;
        StackabilityGroup& stackability_group = stackability_groups[location.stackability_group_pos];
        // Get the stack to add.
        Stack& stack = stackability_group.stacks[location.rotation][location.stack_position];
        StackId stack_id = solution_builder.add_stack(
                bin_pos,
                location.x,
                location.x + location.lx,
                location.y,
                location.y + location.ly);
        // Add the items from the stack.
        for (auto it2 = stack.items.rbegin(); it2 != stack.items.rend(); ++it2) {
            solution_builder.add_item(
                    bin_pos,
                    stack_id,
                    *it2,
                    location.rotate ? Rotation::YXZ : Rotation::XYZ);
        }
    }
    Solution solution = solution_builder.build();
    ItemPos number_of_items_before_repair = solution.number_of_items();
    sor_output.maximum_number_of_items = std::max(
            sor_output.maximum_number_of_items,
            number_of_items_before_repair);
    output.profit_before_repair = solution.profit();
    FFOT_LOG(
            logger,
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
        ss << "it " << sor_output.number_of_iterations
            << " q " << rectangle_queue_size;
        sor_algorithm_formatter.update_solution(solution, ss.str());
        parameters.new_solution_callback(sor_output);
    }
    output.solution = solution;
    FFOT_LOG_FOLD_END(logger, "");
    return output;
}

namespace
{

/**
 * Sequential onedimensional rectangle algorithm filling the bin along X,
 * without the tree search step.
 */
void sequential_onedimensional_rectangle_sor(
        const Instance& instance,
        const SequentialOneDimensionalRectangleParameters& parameters,
        NodeId rectangle_queue_size,
        SequentialOneDimensionalRectangleOutput& output,
        AlgorithmFormatter& algorithm_formatter)
{
    auto logger = parameters.get_logger();
    FFOT_LOG_FOLD_START(
            logger,
            "sequential_onedimensional_rectangle" << std::endl);

    const BinType& bin_type = instance.bin_type(0);
    Length yi = bin_type.box.y;
    Length xi = bin_type.box.x;

    std::vector<Solution> fixed_items_solutions;
    auto cmp = [](const SorSolution& solution_1, const SorSolution& solution_2)
    {
        return solution_1.x_max < solution_2.x_max;
    };
    std::set<SorSolution, decltype(cmp)> queue(cmp);
    queue.insert(SorSolution(instance));
    while (!queue.empty()) {
        SorSolution solution = *queue.begin();

        // Convert to a real boxstacks solution and save it.
        {
            SolutionBuilder solution_builder(instance);
            BinPos sol_bin_pos = solution_builder.add_bin(0, 1);
            for (const SolutionStack& stack: solution.stacks) {
                StackId stack_id = solution_builder.add_stack(
                        sol_bin_pos,
                        stack.x_start,
                        stack.x_end,
                        stack.y_start,
                        stack.y_end);
                for (const SolutionItem& item: stack.items) {
                    solution_builder.add_item(
                            sol_bin_pos,
                            stack_id,
                            item.item_type_id,
                            item.rotation);
                }
            }
            fixed_items_solutions.push_back(solution_builder.build());
        }
        //std::cout << "x " << solution.x_max
        //    << " w " << bin_type.semi_trailer_truck_data.compute_axle_weights(
        //                        solution.weight_weighted_sum.front(), solution.weight.front()).first
        //    << std::endl;
        queue.erase(queue.begin());
        GroupId highest_group_id = 0;
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = instance.item_type(item_type_id);
            if (solution.item_copies[item_type_id] == item_type.copies)
                continue;
            highest_group_id = (std::max)(highest_group_id, item_type.group_id);
        }
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = instance.item_type(item_type_id);
            if (solution.item_copies[item_type_id] == item_type.copies)
                continue;
            if (item_type.group_id != highest_group_id)
                continue;
            for (Rotation rotation: item_type.rotations) {
                {
                    SorSolution solution_child = solution;
                    Length xj = item_type.x(rotation);
                    Length yj = item_type.y(rotation);
                    if (yj > yi)
                        continue;
                    Length x_start = solution.x_max;
                    Length x_end = x_start + xj;
                    Length y_start = std::max(Length(0), yi / 2 - yj / 2);
                    Length y_end = y_start + yj;
                    if (x_end > xi)
                        continue;
                    StackId stack_pos = solution_child.add_stack(
                            x_start,
                            x_end,
                            y_start,
                            y_end);
                    solution_child.add_item(
                            instance,
                            stack_pos,
                            item_type_id,
                            rotation);
                    // Check if the solution is dominated.
                    bool dominated = false;
                    for (auto it = queue.begin(); it != queue.end();) {
                        std::pair<double, double> axle_weights = bin_type.semi_trailer_truck_data.compute_axle_weights(
                                it->weight_weighted_sum.front(), it->weight.front());
                        std::pair<double, double> axle_weights_child = bin_type.semi_trailer_truck_data.compute_axle_weights(
                                solution_child.weight_weighted_sum.front(), solution_child.weight.front());
                        if (it->x_max <= solution_child.x_max
                                && it->item_weight >= solution_child.item_weight
                                && axle_weights.first <= axle_weights_child.first) {
                            dominated = true;
                            break;
                        }
                        if (it->x_max >= solution_child.x_max
                                && it->item_weight <= solution_child.item_weight
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
            logger,
            "fixed_items_solutions.size() " << fixed_items_solutions.size()
            << std::endl);

    ItemPos fixed_items_solutions_pos_lower_bound = 0;
    ItemPos fixed_items_solutions_pos_upper_bound = fixed_items_solutions.size() - 1;
    ItemPos fixed_items_solutions_pos = 0;

    for (output.number_of_iterations = 0;; ++output.number_of_iterations) {

        // Part of solution which is fixed.
        Solution fixed_items = fixed_items_solutions[fixed_items_solutions_pos];
        FFOT_LOG_FOLD_START(
                logger,
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

            // A stack can't be heavier than the bin itself.
            Weight stack_maximum_weight = (std::min)(
                    instance.bin_type(0).maximum_weight,
                    instance.bin_type(0).maximum_stack_density
                    * stackability_group.x
                    * stackability_group.y);

            // Add bin types.
            BinTypeId onedim_bin_type_id_0 = onedim_instance_builder.add_bin_type(
                    instance.bin_type(0).box.z);
            onedim_instance_builder.set_bin_type_cost(
                    onedim_bin_type_id_0,
                    10);
            onedim_instance_builder.set_bin_type_copies(
                    onedim_bin_type_id_0,
                    instance.number_of_items());
            onedim_instance_builder.set_bin_type_maximum_weight(
                    onedim_bin_type_id_0,
                    stack_maximum_weight);
            onedim_instance_builder.add_bin_type_eligibility(
                    onedim_bin_type_id_0,
                    0);
            onedim_instance_builder.add_bin_type_eligibility(
                    onedim_bin_type_id_0,
                    2);

            BinTypeId onedim_bin_type_id_1 = onedim_instance_builder.add_bin_type(
                    instance.bin_type(0).box.z);
            onedim_instance_builder.set_bin_type_cost(
                    onedim_bin_type_id_1,
                    10);
            onedim_instance_builder.set_bin_type_copies(
                    onedim_bin_type_id_1,
                    instance.number_of_items());
            onedim_instance_builder.set_bin_type_maximum_weight(
                    onedim_bin_type_id_1,
                    stack_maximum_weight);
            onedim_instance_builder.add_bin_type_eligibility(
                    onedim_bin_type_id_1,
                    1);
            onedim_instance_builder.add_bin_type_eligibility(
                    onedim_bin_type_id_1,
                    2);

            BinTypeId onedim_bin_type_id_01 = onedim_instance_builder.add_bin_type(
                    instance.bin_type(0).box.z);
            onedim_instance_builder.set_bin_type_cost(
                    onedim_bin_type_id_01,
                    8);
            onedim_instance_builder.set_bin_type_copies(
                    onedim_bin_type_id_01,
                    instance.number_of_items());
            onedim_instance_builder.set_bin_type_maximum_weight(
                    onedim_bin_type_id_01,
                    stack_maximum_weight);
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
                        item_type.box.z);
                onedim_instance_builder.set_item_type_copies(
                        onedim_item_type_id,
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
                if (item_type.can_rotate(Rotation::XYZ) && item_type.can_rotate(Rotation::YXZ)) {
                    onedim_instance_builder.set_item_type_eligibility(
                            onedim_item_type_id,
                            2);
                    //std::cout << "item_type_id " << item_type_id
                    //    << " onedim_item_type_id " << onedim_item_type_id
                    //    << " eligibility " << 1 << std::endl;
                } else if (item_type.can_rotate(Rotation::XYZ)) {
                    onedim_instance_builder.set_item_type_eligibility(
                            onedim_item_type_id,
                            0);
                    //std::cout << "item_type_id " << item_type_id
                    //    << " onedim_item_type_id " << onedim_item_type_id
                    //    << " eligibility " << 0 << std::endl;
                } else if (item_type.can_rotate(Rotation::YXZ)) {
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
            onedim_parameters.use_column_generation = true;
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
                FFOT_LOG_FOLD_END(logger, "");
                return;
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
            FFOT_LOG(logger, "number of splitted stacks " << output.number_of_stack_splits << std::endl);

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
            // For the OpenDimensionX objective, the rectangle subproblem has the
            // same objective.
            rectangle_instance_builder.set_objective(
                    (instance.objective() == Objective::OpenDimensionX)?
                    Objective::OpenDimensionX:
                    Objective::SequentialOneDimensionalRectangleSubproblem);
            rectangle_instance_builder.set_unloading_constraint(instance.unloading_constraint());

            // Add bin types.
            for (BinTypeId bin_type_id = 0; bin_type_id < instance.number_of_bin_types(); ++bin_type_id) {
                const BinType& bin_type = instance.bin_type(bin_type_id);
                auto rectangle_bin_type_id = rectangle_instance_builder.add_bin_type(
                        bin_type.box.x,
                        bin_type.box.y);
                rectangle_instance_builder.set_bin_type_cost(
                        rectangle_bin_type_id,
                        bin_type.cost);
                rectangle_instance_builder.set_bin_type_copies(
                        rectangle_bin_type_id,
                        bin_type.copies);
                rectangle_instance_builder.set_bin_type_copies_min(
                        rectangle_bin_type_id,
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
                                oriented);
                        rectangle_instance_builder.set_item_type_profit(
                                rectangle_item_type_id,
                                stack.profit);
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
                            false); // oriented
                    rectangle_instance_builder.set_item_type_profit(
                            rectangle_item_type_id,
                            profit);
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
                            rectangle_parameters,
                            rectangle_queue_size);
                    if (subproblem_output.solution.full())
                        x_max = (std::min)(x_max, subproblem_output.solution.x_max());
                    failed_middle_axle_weight_constraint_cur |= (subproblem_output.solution.compute_middle_axle_weight_constraints_violation() > 0);
                    failed_rear_axle_weight_constraint_cur |= (subproblem_output.solution.compute_rear_axle_weight_constraints_violation() > 0);
                    if (output.is_proven_optimal()) {
                        FFOT_LOG_FOLD_END(logger, "");
                        FFOT_LOG_FOLD_END(logger, "");
                        return;
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
                            rectangle_parameters,
                            rectangle_queue_size);
                    if (subproblem_output.solution.full())
                        x_max = (std::min)(x_max, subproblem_output.solution.x_max());
                    failed_middle_axle_weight_constraint_cur |= (subproblem_output.solution.compute_middle_axle_weight_constraints_violation() > 0);
                    failed_rear_axle_weight_constraint_cur |= (subproblem_output.solution.compute_rear_axle_weight_constraints_violation() > 0);
                    if (output.is_proven_optimal()) {
                        FFOT_LOG_FOLD_END(logger, "");
                        FFOT_LOG_FOLD_END(logger, "");
                        return;
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
                            rectangle_parameters,
                            rectangle_queue_size);
                    if (output.is_proven_optimal()) {
                        FFOT_LOG_FOLD_END(logger, "");
                        FFOT_LOG_FOLD_END(logger, "");
                        return;
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
                            rectangle_parameters,
                            rectangle_queue_size);
                    if (output.is_proven_optimal()) {
                        FFOT_LOG_FOLD_END(logger, "");
                        FFOT_LOG_FOLD_END(logger, "");
                        return;
                    }

                } else if (!output.solution_pool.best().full()) {
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
                            rectangle_parameters,
                            rectangle_queue_size);
                    failed_middle_axle_weight_constraint_cur |= (subproblem_output.solution.compute_middle_axle_weight_constraints_violation() > 0);
                    failed_rear_axle_weight_constraint_cur |= (subproblem_output.solution.compute_rear_axle_weight_constraints_violation() > 0);
                    if (output.is_proven_optimal()) {
                        FFOT_LOG_FOLD_END(logger, "");
                        FFOT_LOG_FOLD_END(logger, "");
                        return;
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
                            rectangle_parameters,
                            rectangle_queue_size);
                    if (output.is_proven_optimal()) {
                        FFOT_LOG_FOLD_END(logger, "");
                        FFOT_LOG_FOLD_END(logger, "");
                        return;
                    }
                    failed_middle_axle_weight_constraint_cur |= (subproblem_output.solution.compute_middle_axle_weight_constraints_violation() > 0);
                    failed_rear_axle_weight_constraint_cur |= (subproblem_output.solution.compute_rear_axle_weight_constraints_violation() > 0);
                }

                if (!failed_middle_axle_weight_constraint_cur
                        && !failed_rear_axle_weight_constraint_cur
                        && fixed_items_solutions_pos == 0
                        && output.number_of_stack_splits == 0) {
                    FFOT_LOG_FOLD_END(logger, "");
                    FFOT_LOG_FOLD_END(logger, "");
                    return;
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
                            rectangle_parameters,
                            rectangle_queue_size);
                    if (output.is_proven_optimal()) {
                        FFOT_LOG_FOLD_END(logger, "");
                        FFOT_LOG_FOLD_END(logger, "");
                        return;
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
                            rectangle_parameters,
                            rectangle_queue_size);
                    if (output.is_proven_optimal()) {
                        FFOT_LOG_FOLD_END(logger, "");
                        return;
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

        {
            SolutionBuilder fixed_items_builder(instance);
            fixed_items_builder.add_bin(0, 1);
            fixed_items = fixed_items_builder.build();
        }
        FFOT_LOG(
                logger,
                "failed_middle_axle_weight_constraint " << failed_middle_axle_weight_constraint << std::endl
                << "failed_rear_axle_weight_constraint " << failed_rear_axle_weight_constraint << std::endl
                << "x_max " << x_max << " / " << xi << std::endl);
        if (failed_middle_axle_weight_constraint) {
            // If the solution is infeasible.
            fixed_items_solutions_pos_lower_bound = fixed_items_solutions_pos + 1;
            fixed_items_solutions_pos = 0;
            for (ItemPos pos = 0; pos < (ItemPos)fixed_items_solutions.size(); ++pos) {
                if (pos + 1 >= (ItemPos)fixed_items_solutions.size()
                        || fixed_items_solutions[pos + 1].x_max() > xi - x_max)
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
                logger,
                "fixed_items_solutions_pos " << fixed_items_solutions_pos << std::endl
                << "fixed_items_solutions_pos_lower_bound " << fixed_items_solutions_pos_lower_bound << std::endl
                << "fixed_items_solutions_pos_upper_bound " << fixed_items_solutions_pos_upper_bound << std::endl);
        FFOT_LOG_FOLD_END(logger, "");
        if (fixed_items_solutions_pos_lower_bound > fixed_items_solutions_pos_upper_bound)
            break;

    }

    FFOT_LOG_FOLD_END(logger, "");
}

/**
 * Copy the parameters of the algorithm for a call in a single direction.
 */
SequentialOneDimensionalRectangleParameters direction_parameters(
        const SequentialOneDimensionalRectangleParameters& parameters,
        AlgorithmFormatter& algorithm_formatter)
{
    SequentialOneDimensionalRectangleParameters direction_parameters;
    direction_parameters.verbosity_level = 0;
    direction_parameters.logger = parameters.get_logger();
    direction_parameters.timer = parameters.timer;
    direction_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    direction_parameters.sequential = parameters.sequential;
    direction_parameters.directions = {Direction::X};
    direction_parameters.optimization_mode = parameters.optimization_mode;
    direction_parameters.onedimensional_parameters = parameters.onedimensional_parameters;
    direction_parameters.anytime_rectangle_initial_queue_size = parameters.anytime_rectangle_initial_queue_size;
    direction_parameters.anytime_tree_search_initial_queue_size = parameters.anytime_tree_search_initial_queue_size;
    direction_parameters.not_anytime_rectangle_queue_size = parameters.not_anytime_rectangle_queue_size;
    direction_parameters.not_anytime_tree_search_queue_size = parameters.not_anytime_tree_search_queue_size;
    direction_parameters.tree_search_guides = parameters.tree_search_guides;
    direction_parameters.move_intra_shift = parameters.move_intra_shift;
    direction_parameters.move_intra_swap = parameters.move_intra_swap;
    direction_parameters.move_add = parameters.move_add;
    direction_parameters.move_inter_swap = parameters.move_inter_swap;
    return direction_parameters;
}

/**
 * Add the statistics of 'direction_output' to 'output'.
 */
void add_statistics(
        SequentialOneDimensionalRectangleOutput& output,
        const SequentialOneDimensionalRectangleOutput& direction_output)
{
    output.number_of_iterations += direction_output.number_of_iterations;
    output.number_of_stack_splits += direction_output.number_of_stack_splits;
    output.maximum_number_of_items = (std::max)(
            output.maximum_number_of_items,
            direction_output.maximum_number_of_items);
    output.failed = output.failed || direction_output.failed;
    output.number_of_onedimensional_calls += direction_output.number_of_onedimensional_calls;
    output.onedimensional_time += direction_output.onedimensional_time;
    output.number_of_rectangle_calls += direction_output.number_of_rectangle_calls;
    output.rectangle_time += direction_output.rectangle_time;
    output.rectangle_subproblem_explored_exhaustively
        = output.rectangle_subproblem_explored_exhaustively
        || direction_output.rectangle_subproblem_explored_exhaustively;
    output.number_of_passes += direction_output.number_of_passes;
    output.first_pass_failed = output.first_pass_failed || direction_output.first_pass_failed;
    output.first_pass_full = output.first_pass_full || direction_output.first_pass_full;
    output.number_of_tree_search_calls += direction_output.number_of_tree_search_calls;
    output.number_of_tree_search_perfect += direction_output.number_of_tree_search_perfect;
    output.number_of_tree_search_better += direction_output.number_of_tree_search_better;
    output.tree_search_time += direction_output.tree_search_time;
}

/**
 * Sequential onedimensional rectangle algorithm filling the bin along X.
 *
 * Each pass runs the sequential onedimensional rectangle steps, followed by
 * the tree search step. In 'Anytime' mode, successive passes are performed
 * with growing queue sizes, so that a time limit or a manual stop still
 * leaves the tree search a chance to run, instead of the growth of the
 * rectangle queue size consuming the whole budget by itself before the tree
 * search is ever considered.
 *
 * The solutions are reported to 'local_output' if it is not 'nullptr', to
 * 'algorithm_formatter' otherwise. The returned output contains the
 * statistics of the run.
 */
SequentialOneDimensionalRectangleOutput sequential_onedimensional_rectangle_x(
        const Instance& instance,
        const SequentialOneDimensionalRectangleParameters& parameters,
        AlgorithmFormatter& algorithm_formatter,
        packingsolver::Output<Instance, Solution>* local_output)
{
    if (algorithm_formatter.end_boolean())
        return SequentialOneDimensionalRectangleOutput(instance);

    // The passes work on their own output, so that their decisions don't
    // depend on solutions found in parallel in the other direction.
    SequentialOneDimensionalRectangleParameters x_parameters
        = direction_parameters(parameters, algorithm_formatter);
    x_parameters.new_solution_callback = [&algorithm_formatter, local_output](
            const boxstacks::Output& sor_output)
        {
            std::string label = "X " + sor_output.solution_pool.best_label();
            if (local_output != nullptr) {
                local_output->solution_pool.add(sor_output.solution_pool.best(), label);
            } else {
                algorithm_formatter.update_solution(sor_output.solution_pool.best(), label);
            }
            algorithm_formatter.update_bounds(sor_output);
        };
    SequentialOneDimensionalRectangleOutput x_output(instance);
    AlgorithmFormatter x_algorithm_formatter(instance, x_parameters, x_output);
    x_algorithm_formatter.start();
    x_algorithm_formatter.print_header();

    // Trivial bounds, so that a solution packing all items is detected as
    // optimal when it is.
    switch (instance.objective()) {
    case Objective::Knapsack: {
        Profit profit = 0;
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = instance.item_type(item_type_id);
            if (item_type.profit > 0)
                profit += item_type.profit * item_type.copies;
        }
        x_algorithm_formatter.update_knapsack_bound(profit);
        break;
    } case Objective::BinPacking: {
        if (instance.number_of_items() > 0)
            x_algorithm_formatter.update_bin_packing_bound(1);
        break;
    } case Objective::VariableSizedBinPacking: {
        if (instance.number_of_items() > 0) {
            Profit cost = std::numeric_limits<Profit>::infinity();
            for (BinTypeId bin_type_id = 0;
                    bin_type_id < instance.number_of_bin_types();
                    ++bin_type_id) {
                const BinType& bin_type = instance.bin_type(bin_type_id);
                if (bin_type.copies > 0)
                    cost = (std::min)(cost, bin_type.cost);
            }
            if (cost < std::numeric_limits<Profit>::infinity())
                x_algorithm_formatter.update_variable_sized_bin_packing_bound(cost);
        }
        break;
    } default: {
        break;
    }
    }

    bool failed = false;
    for (Counter growth_factor = 1;;) {
        NodeId rectangle_queue_size
            = x_parameters.anytime_rectangle_initial_queue_size
            * growth_factor;
        NodeId tree_search_queue_size
            = x_parameters.anytime_tree_search_initial_queue_size
            * growth_factor;
        if (x_parameters.optimization_mode != OptimizationMode::Anytime) {
            rectangle_queue_size = x_parameters.not_anytime_rectangle_queue_size;
            tree_search_queue_size = x_parameters.not_anytime_tree_search_queue_size;
        }

        x_output.failed = false;
        x_output.rectangle_subproblem_explored_exhaustively = false;
        sequential_onedimensional_rectangle_sor(
                instance,
                x_parameters,
                rectangle_queue_size,
                x_output,
                x_algorithm_formatter);
        bool pass_failed = x_output.failed;
        failed = failed || pass_failed;
        x_output.number_of_passes++;

        // The boxstacks tree search is significantly more expensive than the
        // steps above. Run it only if it looks promising enough:
        // - The solution above violated the axle weight constraints (no
        //   reason to expect the tree search would do better otherwise).
        // - The weight of every unpacked item is greater than the remaining
        //   capacity (no way to pack more regardless of geometry).
        // A full pack already sets 'end_boolean()' on its own (every
        // 'update_solution()' call checks 'is_proven_optimal()'), so no
        // separate check for it is needed here.
        bool run_tree_search = false;
        if (!x_algorithm_formatter.end_boolean()
                && !x_parameters.timer.needs_to_end()) {
            run_tree_search = pass_failed;
            bool no_lighter_item = true;
            for (ItemTypeId item_type_id = 0;
                    item_type_id < instance.number_of_item_types();
                    ++item_type_id) {
                const ItemType& item_type = instance.item_type(item_type_id);
                if (x_output.solution_pool.best().item_copies(item_type_id) == item_type.copies)
                    continue;
                if (x_output.solution_pool.best().item_weight() + item_type.weight <= instance.bin_weight())
                    no_lighter_item = false;
            }
            if (no_lighter_item)
                run_tree_search = false;
        }

        if (run_tree_search) {
            auto tree_search_begin = std::chrono::steady_clock::now();
            Profit sor_profit = x_output.solution_pool.best().profit();

            TreeSearchParameters tree_search_parameters;
            tree_search_parameters.verbosity_level = 0;
            tree_search_parameters.timer = x_parameters.timer;
            tree_search_parameters.timer.add_end_boolean(&x_algorithm_formatter.end_boolean());
            tree_search_parameters.optimization_mode
                = (x_parameters.optimization_mode == OptimizationMode::NotAnytimeSequential)?
                OptimizationMode::NotAnytimeSequential:
                OptimizationMode::NotAnytime;
            tree_search_parameters.not_anytime_tree_search_queue_size = tree_search_queue_size;
            tree_search_parameters.guides = x_parameters.tree_search_guides;
            // The bin is filled along X here; filling it along Y is done by
            // running this function on the flipped instance.
            tree_search_parameters.directions = {Direction::X};
            tree_search_parameters.maximum_number_of_selected_items = x_output.maximum_number_of_items;
            tree_search_parameters.new_solution_callback = [&x_algorithm_formatter, tree_search_queue_size](
                    const boxstacks::Output& tree_search_output)
                {
                    std::stringstream ss;
                    ss << "TS " << tree_search_output.solution_pool.best_label()
                        << " q " << tree_search_queue_size;
                    x_algorithm_formatter.update_solution(
                            tree_search_output.solution_pool.best(),
                            ss.str());
                    // Forward any bound the tree search proved: once it
                    // matches the best profit/cost found, this sets
                    // 'end_boolean()' the same way a full pack already does.
                    x_algorithm_formatter.update_bounds(tree_search_output);
                };
            tree_search(instance, tree_search_parameters);

            auto tree_search_end = std::chrono::steady_clock::now();
            x_output.tree_search_time += std::chrono::duration_cast<
                std::chrono::duration<double>>(tree_search_end - tree_search_begin).count();
            x_output.number_of_tree_search_calls++;
            if (x_output.solution_pool.best().full()) {
                x_output.number_of_tree_search_perfect++;
            } else if (x_output.solution_pool.best().profit() > sor_profit) {
                x_output.number_of_tree_search_better++;
            }
        }

        if (growth_factor == 1) {
            x_output.first_pass_failed = pass_failed;
            x_output.first_pass_full = x_output.solution_pool.best().full();
        }

        // Check end.
        if (x_algorithm_formatter.end_boolean())
            break;
        if (x_parameters.timer.needs_to_end())
            break;
        if (x_parameters.optimization_mode != OptimizationMode::Anytime)
            break;
        // A larger rectangle queue size is guaranteed not to change the
        // result of the rectangle subproblem (see
        // 'rectangle_subproblem_explored_exhaustively''s own doc comment).
        // That alone isn't enough to stop, though: the tree search (a
        // distinct search space) may still benefit from growing further, so
        // only stop here when it isn't even being tried - otherwise growing
        // further would be pointless on both fronts, and on a small/easy
        // instance could otherwise keep doubling long after every pass above
        // returns in microseconds, until 'growth_factor' overflows.
        if (!run_tree_search
                && x_output.rectangle_subproblem_explored_exhaustively)
            break;

        growth_factor = std::max(
                growth_factor + 1,
                (Counter)(growth_factor * 2));
    }
    x_output.failed = failed;

    x_algorithm_formatter.end();
    return x_output;
}

/**
 * Sequential onedimensional rectangle algorithm filling the bin along Y.
 *
 * The algorithm is run along X on the flipped instance, and its solutions
 * are unflipped.
 *
 * The solutions are reported to 'local_output' if it is not 'nullptr', to
 * 'algorithm_formatter' otherwise. The returned output contains the
 * statistics of the run.
 */
SequentialOneDimensionalRectangleOutput sequential_onedimensional_rectangle_y(
        const Instance& instance,
        const SequentialOneDimensionalRectangleParameters& parameters,
        AlgorithmFormatter& algorithm_formatter,
        packingsolver::Output<Instance, Solution>* local_output)
{
    SequentialOneDimensionalRectangleOutput output(instance);
    if (algorithm_formatter.end_boolean())
        return output;

    // Build flipped instance.
    InstanceFlipper instance_flipper(instance);
    const Instance& flipped_instance = instance_flipper.flipped_instance();

    SequentialOneDimensionalRectangleParameters flipped_parameters
        = direction_parameters(parameters, algorithm_formatter);
    flipped_parameters.new_solution_callback = [
        &algorithm_formatter, local_output, &instance_flipper](
                const boxstacks::Output& flipped_output)
        {
            std::string label = flipped_output.solution_pool.best_label();
            if (!label.empty() && label[0] == 'X')
                label[0] = 'Y';
            Solution solution = instance_flipper.unflip_solution(
                    flipped_output.solution_pool.best());
            if (local_output != nullptr) {
                local_output->solution_pool.add(solution, label);
            } else {
                algorithm_formatter.update_solution(solution, label);
            }
            algorithm_formatter.update_bounds(flipped_output);
        };
    SequentialOneDimensionalRectangleOutput flipped_output = sequential_onedimensional_rectangle(
            flipped_instance,
            flipped_parameters);
    // 'flipped_output' refers to 'flipped_instance', which is destroyed at
    // the end of this function, so only its statistics are returned.
    add_statistics(output, flipped_output);
    return output;
}

}

const SequentialOneDimensionalRectangleOutput boxstacks::sequential_onedimensional_rectangle(
        const Instance& instance,
        const SequentialOneDimensionalRectangleParameters& parameters)
{
    SequentialOneDimensionalRectangleOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    // Directions in which the bin is filled. Same rules as in
    // 'tree_search()': the axle weight constraints of a semi-trailer truck
    // are only defined along X.
    std::vector<Direction> directions = parameters.directions;
    if (directions.empty()) {
        if (instance.objective() == Objective::OpenDimensionX) {
            directions = {Direction::X};
        } else if (instance.objective() == Objective::OpenDimensionY) {
            directions = {Direction::Y};
        } else if (instance.unloading_constraint() == rectangle::UnloadingConstraint::IncreasingX
                || instance.unloading_constraint() == rectangle::UnloadingConstraint::OnlyXMovements) {
            directions = {Direction::X};
        } else if (instance.unloading_constraint() == rectangle::UnloadingConstraint::IncreasingY
                || instance.unloading_constraint() == rectangle::UnloadingConstraint::OnlyYMovements) {
            directions = {Direction::Y};
        } else if (instance.bin_type(0).semi_trailer_truck_data.is) {
            directions = {Direction::X};
        } else {
            directions = {Direction::X, Direction::Y};
        }
    }

    if (directions == std::vector<Direction>{Direction::X}) {
        add_statistics(output, sequential_onedimensional_rectangle_x(
                    instance,
                    parameters,
                    algorithm_formatter,
                    nullptr));
    } else if (directions == std::vector<Direction>{Direction::Y}) {
        add_statistics(output, sequential_onedimensional_rectangle_y(
                    instance,
                    parameters,
                    algorithm_formatter,
                    nullptr));
    } else {
        // 'sequential_onedimensional_rectangle_x' and
        // 'sequential_onedimensional_rectangle_y' run in parallel; in
        // 'NotAnytimeDeterministic' mode, each writes its solutions to its
        // own local output instead of the shared 'algorithm_formatter', so
        // that they can be replayed into it in a fixed, deterministic order
        // (X then Y) once both have terminated, instead of the
        // (non-deterministic) order in which they actually finish. Their
        // statistics are merged once both have terminated.
        bool deterministic = (parameters.optimization_mode == OptimizationMode::NotAnytimeDeterministic);
        packingsolver::Output<Instance, Solution> local_output_x(instance);
        packingsolver::Output<Instance, Solution> local_output_y(instance);
        std::unique_ptr<SequentialOneDimensionalRectangleOutput> output_x;
        std::unique_ptr<SequentialOneDimensionalRectangleOutput> output_y;

        std::vector<std::function<void()>> tasks;
        std::exception_ptr exception_ptr_x;
        std::exception_ptr exception_ptr_y;
        tasks.push_back([&exception_ptr_x, &instance, &parameters, &algorithm_formatter, &local_output_x, &output_x, deterministic]() {
            try {
                output_x = std::make_unique<SequentialOneDimensionalRectangleOutput>(
                        sequential_onedimensional_rectangle_x(
                            instance,
                            parameters,
                            algorithm_formatter,
                            deterministic ? &local_output_x : nullptr));
            } catch (...) {
                exception_ptr_x = std::current_exception();
            }
        });
        tasks.push_back([&exception_ptr_y, &instance, &parameters, &algorithm_formatter, &local_output_y, &output_y, deterministic]() {
            try {
                output_y = std::make_unique<SequentialOneDimensionalRectangleOutput>(
                        sequential_onedimensional_rectangle_y(
                            instance,
                            parameters,
                            algorithm_formatter,
                            deterministic ? &local_output_y : nullptr));
            } catch (...) {
                exception_ptr_y = std::current_exception();
            }
        });
        run(tasks, parameters.optimization_mode != OptimizationMode::NotAnytimeSequential);
        if (exception_ptr_x)
            std::rethrow_exception(exception_ptr_x);
        if (exception_ptr_y)
            std::rethrow_exception(exception_ptr_y);

        add_statistics(output, *output_x);
        add_statistics(output, *output_y);
        // Growing the queue size further is only pointless if it is in both
        // directions.
        output.rectangle_subproblem_explored_exhaustively
            = output_x->rectangle_subproblem_explored_exhaustively
            && output_y->rectangle_subproblem_explored_exhaustively;

        if (deterministic) {
            algorithm_formatter.update_solution(
                    local_output_x.solution_pool.best(),
                    local_output_x.solution_pool.best_label());
            algorithm_formatter.update_solution(
                    local_output_y.solution_pool.best(),
                    local_output_y.solution_pool.best_label());
        }
    }

    algorithm_formatter.end();
    return output;
}
