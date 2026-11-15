#include "box/static_stability.hpp"

#include "box/solution_builder.hpp"

#include "mathoptsolverscmake/mathopt.hpp"
#ifdef HIGHS_FOUND
#include "mathoptsolverscmake/mathopt_highs.hpp"
#endif

#include <algorithm>
#include <unordered_map>

using namespace packingsolver;
using namespace packingsolver::box;

namespace
{

/** Contact between the bottom face of an item and a supporting face. */
struct Contact
{
    /** Position of the supported item in 'items'. */
    ItemPos item_pos;

    /**
     * Position of the supporting item in 'items', or -1 if the item is
     * supported by the floor of the bin.
     */
    ItemPos supporting_item_pos;

    /** Contact rectangle. */
    Length x_start;
    Length x_end;
    Length y_start;
    Length y_end;
};

/**
 * Solve the equilibrium linear program of the items of a bin (see
 * 'is_statically_stable').
 *
 * Without 'with_moments', only the balances of the vertical forces are
 * required. With 'with_maximum_weights_above', the total force on the top
 * face of each item is at most the maximum weight above of its item type.
 *
 * If 'unbalanced_items' is not null, the balance rows of each item may be
 * violated at a cost equal to the violation, and the items whose rows are
 * violated in an optimal solution are added to 'unbalanced_items'.
 *
 * Return true iff the items are in equilibrium.
 */
bool solve_equilibrium(
        const Instance& instance,
        const BinType& bin_type,
        const std::vector<SolutionItem>& items,
        bool with_moments,
        bool with_maximum_weights_above,
        std::vector<ItemPos>* unbalanced_items)
{
    ItemPos number_of_items = items.size();

    // Positions of the items.
    std::vector<Point> starts(number_of_items);
    std::vector<Point> ends(number_of_items);
    std::vector<Weight> weights(number_of_items);
    bool all_items_on_the_floor = true;
    for (ItemPos item_pos = 0; item_pos < number_of_items; ++item_pos) {
        const SolutionItem& item = items[item_pos];
        const ItemType& item_type = instance.item_type(item.item_type_id);
        Box box = item_type.box.rotate(item.rotation);
        starts[item_pos] = item.bl_corner;
        ends[item_pos] = {
            item.bl_corner.x + box.x,
            item.bl_corner.y + box.y,
            item.bl_corner.z + box.z};
        weights[item_pos] = item_type.weight;
        if (item.bl_corner.z != 0)
            all_items_on_the_floor = false;
    }
    if (all_items_on_the_floor)
        return true;

    // Compute the contacts.
    std::unordered_map<Length, std::vector<ItemPos>> items_by_z_end;
    for (ItemPos item_pos = 0; item_pos < number_of_items; ++item_pos)
        items_by_z_end[ends[item_pos].z].push_back(item_pos);
    std::vector<Contact> contacts;
    for (ItemPos item_pos = 0; item_pos < number_of_items; ++item_pos) {
        if (starts[item_pos].z == 0) {
            contacts.push_back({
                    item_pos,
                    -1,
                    starts[item_pos].x,
                    ends[item_pos].x,
                    starts[item_pos].y,
                    ends[item_pos].y});
            continue;
        }
        bool supported = false;
        auto it = items_by_z_end.find(starts[item_pos].z);
        if (it != items_by_z_end.end()) {
            for (ItemPos supporting_item_pos: it->second) {
                Length x_start = (std::max)(starts[item_pos].x, starts[supporting_item_pos].x);
                Length x_end = (std::min)(ends[item_pos].x, ends[supporting_item_pos].x);
                Length y_start = (std::max)(starts[item_pos].y, starts[supporting_item_pos].y);
                Length y_end = (std::min)(ends[item_pos].y, ends[supporting_item_pos].y);
                if (x_start >= x_end || y_start >= y_end)
                    continue;
                contacts.push_back({
                        item_pos,
                        supporting_item_pos,
                        x_start,
                        x_end,
                        y_start,
                        y_end});
                supported = true;
            }
        }
        // A floating item with a weight falls.
        if (!supported && weights[item_pos] > 0 && unbalanced_items == nullptr)
            return false;
    }

    // Build the linear program. The variables are the forces at the corners
    // of the contacts. For each item, the rows are the balance of the
    // vertical forces and the balances of the moments about the axes
    // parallel to x and y through its center, and the total force on its top
    // face. The forces are divided by the largest weight and the lever arms
    // by the largest dimension of the bin, so that the coefficients are
    // between -1 and 1.
    Weight weight_max = 0;
    for (Weight weight: weights)
        weight_max = (std::max)(weight_max, weight);
    if (weight_max == 0)
        return true;
    double length_max = (double)(std::max)({bin_type.box.x, bin_type.box.y, bin_type.box.z});

    std::vector<std::vector<int>> rows_variables(3 * number_of_items);
    std::vector<std::vector<double>> rows_coefficients(3 * number_of_items);
    // Forces on the top face of each item.
    std::vector<std::vector<int>> above_variables(number_of_items);
    int number_of_variables = 0;
    for (const Contact& contact: contacts) {
        for (Length x: {contact.x_start, contact.x_end}) {
            for (Length y: {contact.y_start, contact.y_end}) {
                // The force pushes the supported item up and the supporting
                // item down.
                for (int side = 0; side < 2; ++side) {
                    ItemPos item_pos = (side == 0)?
                        contact.item_pos:
                        contact.supporting_item_pos;
                    if (item_pos == -1)
                        continue;
                    double sign = (side == 0)? 1: -1;
                    double x_center = (double)(starts[item_pos].x + ends[item_pos].x) / 2;
                    double y_center = (double)(starts[item_pos].y + ends[item_pos].y) / 2;
                    rows_variables[3 * item_pos].push_back(number_of_variables);
                    rows_coefficients[3 * item_pos].push_back(sign);
                    rows_variables[3 * item_pos + 1].push_back(number_of_variables);
                    rows_coefficients[3 * item_pos + 1].push_back(sign * (x - x_center) / length_max);
                    rows_variables[3 * item_pos + 2].push_back(number_of_variables);
                    rows_coefficients[3 * item_pos + 2].push_back(sign * (y - y_center) / length_max);
                    if (side == 1)
                        above_variables[item_pos].push_back(number_of_variables);
                }
                number_of_variables++;
            }
        }
    }

    // Violations of the rows, two per row, in both directions.
    int number_of_forces = number_of_variables;
    if (unbalanced_items != nullptr) {
        for (int row = 0; row < 3 * number_of_items; ++row) {
            for (double sign: {1.0, -1.0}) {
                rows_variables[row].push_back(number_of_variables);
                rows_coefficients[row].push_back(sign);
                number_of_variables++;
            }
        }
    }

    mathoptsolverscmake::MathOptModel model;
    model.objective_direction = mathoptsolverscmake::ObjectiveDirection::Minimize;
    for (int variable_id = 0; variable_id < number_of_variables; ++variable_id) {
        model.variables_lower_bounds.push_back(0.0);
        model.variables_upper_bounds.push_back(std::numeric_limits<double>::infinity());
        model.variables_types.push_back(mathoptsolverscmake::VariableType::Continuous);
        model.objective_coefficients.push_back(
                (variable_id < number_of_forces)? 0.0: 1.0);
    }
    for (int row = 0; row < 3 * number_of_items; ++row) {
        ItemPos item_pos = row / 3;
        model.constraints_starts.push_back((int)model.elements_variables.size());
        for (std::size_t pos = 0; pos < rows_variables[row].size(); ++pos) {
            model.elements_variables.push_back(rows_variables[row][pos]);
            model.elements_coefficients.push_back(rows_coefficients[row][pos]);
        }
        if (row % 3 == 0) {
            model.constraints_lower_bounds.push_back(weights[item_pos] / weight_max);
            model.constraints_upper_bounds.push_back(weights[item_pos] / weight_max);
        } else if (with_moments) {
            model.constraints_lower_bounds.push_back(0.0);
            model.constraints_upper_bounds.push_back(0.0);
        } else {
            model.constraints_lower_bounds.push_back(-std::numeric_limits<double>::infinity());
            model.constraints_upper_bounds.push_back(std::numeric_limits<double>::infinity());
        }
    }
    if (with_maximum_weights_above) {
        for (ItemPos item_pos = 0; item_pos < number_of_items; ++item_pos) {
            Weight maximum_weight_above = instance.item_type(
                    items[item_pos].item_type_id).maximum_weight_above;
            if (maximum_weight_above == std::numeric_limits<Weight>::infinity()
                    || above_variables[item_pos].empty()) {
                continue;
            }
            model.constraints_starts.push_back((int)model.elements_variables.size());
            for (int variable_id: above_variables[item_pos]) {
                model.elements_variables.push_back(variable_id);
                model.elements_coefficients.push_back(1.0);
            }
            model.constraints_lower_bounds.push_back(-std::numeric_limits<double>::infinity());
            model.constraints_upper_bounds.push_back(maximum_weight_above / weight_max);
        }
    }

#ifdef HIGHS_FOUND
    Highs highs;
    mathoptsolverscmake::reduce_printout(highs);
    mathoptsolverscmake::load(highs, model);
    mathoptsolverscmake::solve(highs);
    if (highs.getModelStatus() != HighsModelStatus::kOptimal)
        return false;
    if (unbalanced_items == nullptr)
        return true;
    std::vector<double> solution = mathoptsolverscmake::get_solution(highs);
    bool balanced = true;
    for (ItemPos item_pos = 0; item_pos < number_of_items; ++item_pos) {
        double violation = 0;
        for (int k = 0; k < 6; ++k)
            violation += solution[number_of_forces + 6 * item_pos + k];
        if (violation > 1e-7) {
            unbalanced_items->push_back(item_pos);
            balanced = false;
        }
    }
    return balanced;
#else
    throw std::invalid_argument(
            FUNC_SIGNATURE + ": "
            "the static stability and maximum weight above checks require HiGHS.");
#endif
}

/** Let the items of a bin fall (see 'drop_items'). */
std::vector<SolutionItem> drop_bin_items(
        const Instance& instance,
        const std::vector<SolutionItem>& items)
{
    ItemPos number_of_items = items.size();
    std::vector<SolutionItem> new_items = items;
    std::vector<Box> boxes(number_of_items);
    for (ItemPos item_pos = 0; item_pos < number_of_items; ++item_pos) {
        const ItemType& item_type = instance.item_type(items[item_pos].item_type_id);
        boxes[item_pos] = item_type.box.rotate(items[item_pos].rotation);
    }
    std::vector<ItemPos> sorted_items(number_of_items);
    for (ItemPos item_pos = 0; item_pos < number_of_items; ++item_pos)
        sorted_items[item_pos] = item_pos;
    std::sort(
            sorted_items.begin(),
            sorted_items.end(),
            [&items](ItemPos item_pos_1, ItemPos item_pos_2)
            {
                return items[item_pos_1].bl_corner.z
                    < items[item_pos_2].bl_corner.z;
            });

    // Move the items down, from the lowest to the highest one. The items
    // which may stop an item are below it, so they have already moved.
    for (ItemPos sorted_pos = 0; sorted_pos < number_of_items; ++sorted_pos) {
        ItemPos item_pos = sorted_items[sorted_pos];
        SolutionItem& item = new_items[item_pos];
        Length z = 0;
        for (ItemPos sorted_pos_2 = 0; sorted_pos_2 < sorted_pos; ++sorted_pos_2) {
            ItemPos item_pos_2 = sorted_items[sorted_pos_2];
            const SolutionItem& item_2 = new_items[item_pos_2];
            if (item_2.bl_corner.x >= item.bl_corner.x + boxes[item_pos].x
                    || item.bl_corner.x >= item_2.bl_corner.x + boxes[item_pos_2].x
                    || item_2.bl_corner.y >= item.bl_corner.y + boxes[item_pos].y
                    || item.bl_corner.y >= item_2.bl_corner.y + boxes[item_pos_2].y) {
                continue;
            }
            z = (std::max)(z, item_2.bl_corner.z + boxes[item_pos_2].z);
        }
        item.bl_corner.z = z;
    }
    return new_items;
}

/**
 * Return true iff the items of a bin satisfy the static stability and the
 * maximum weights above, when the instance requires them.
 */
bool balanced(
        const Instance& instance,
        const BinType& bin_type,
        const std::vector<SolutionItem>& items)
{
    if (instance.parameters().static_stability
            && !is_statically_stable(instance, bin_type, items)) {
        return false;
    }
    if (instance.has_maximum_weight_above()
            && !satisfies_maximum_weights_above(instance, bin_type, items)) {
        return false;
    }
    return true;
}

/** Return true iff a bin of a solution must be repaired. */
bool unbalanced(const SolutionBin& bin)
{
    return !bin.stable || !bin.maximum_weight_above_feasible;
}

/** Remove the unbalanced items of a bin (see 'remove_unbalanced_items'). */
std::vector<SolutionItem> remove_bin_unbalanced_items(
        const Instance& instance,
        const BinType& bin_type,
        const std::vector<SolutionItem>& items)
{
    std::vector<SolutionItem> new_items = items;
    while (!balanced(instance, bin_type, new_items)) {
        ItemPos number_of_items = new_items.size();
        std::vector<Point> ends(number_of_items);
        for (ItemPos item_pos = 0; item_pos < number_of_items; ++item_pos) {
            const SolutionItem& item = new_items[item_pos];
            Box box = instance.item_type(item.item_type_id).box.rotate(item.rotation);
            ends[item_pos] = {
                item.bl_corner.x + box.x,
                item.bl_corner.y + box.y,
                item.bl_corner.z + box.z};
        }

        // Find the highest unbalanced item. If the linear program doesn't
        // find any, because of the tolerances, take the highest item, so
        // that an item is removed at each iteration.
        std::vector<ItemPos> unbalanced_items;
        solve_equilibrium(
                instance,
                bin_type,
                new_items,
                instance.parameters().static_stability,
                instance.has_maximum_weight_above(),
                &unbalanced_items);
        if (unbalanced_items.empty()) {
            for (ItemPos item_pos = 0; item_pos < number_of_items; ++item_pos)
                unbalanced_items.push_back(item_pos);
        }
        ItemPos item_pos_highest = unbalanced_items.front();
        for (ItemPos item_pos: unbalanced_items)
            if (ends[item_pos].z > ends[item_pos_highest].z)
                item_pos_highest = item_pos;

        // Remove it with the items which rest on it, directly or not.
        std::vector<uint8_t> removed(number_of_items, 0);
        removed[item_pos_highest] = 1;
        std::vector<ItemPos> stack = {item_pos_highest};
        while (!stack.empty()) {
            ItemPos item_pos = stack.back();
            stack.pop_back();
            for (ItemPos item_pos_2 = 0; item_pos_2 < number_of_items; ++item_pos_2) {
                const SolutionItem& item_2 = new_items[item_pos_2];
                if (removed[item_pos_2]
                        || item_2.bl_corner.z != ends[item_pos].z
                        || item_2.bl_corner.x >= ends[item_pos].x
                        || new_items[item_pos].bl_corner.x >= ends[item_pos_2].x
                        || item_2.bl_corner.y >= ends[item_pos].y
                        || new_items[item_pos].bl_corner.y >= ends[item_pos_2].y) {
                    continue;
                }
                removed[item_pos_2] = 1;
                stack.push_back(item_pos_2);
            }
        }
        std::vector<SolutionItem> remaining_items;
        for (ItemPos item_pos = 0; item_pos < number_of_items; ++item_pos)
            if (!removed[item_pos])
                remaining_items.push_back(new_items[item_pos]);
        new_items = remaining_items;
    }
    return new_items;
}

}

bool packingsolver::box::is_statically_stable(
        const Instance& instance,
        const BinType& bin_type,
        const std::vector<SolutionItem>& items)
{
    return solve_equilibrium(instance, bin_type, items, true, false, nullptr);
}

bool packingsolver::box::satisfies_maximum_weights_above(
        const Instance& instance,
        const BinType& bin_type,
        const std::vector<SolutionItem>& items)
{
    return solve_equilibrium(
            instance,
            bin_type,
            items,
            instance.parameters().static_stability,
            true,
            nullptr);
}

Solution packingsolver::box::drop_items(const Solution& solution)
{
    if (solution.stable() && solution.maximum_weight_above_feasible())
        return solution;

    const Instance& instance = solution.instance();
    SolutionBuilder solution_builder(instance);
    for (BinPos bin_pos = 0;
            bin_pos < solution.number_of_different_bins();
            ++bin_pos) {
        const SolutionBin& bin = solution.bin(bin_pos);
        BinPos new_bin_pos = solution_builder.add_bin(bin.bin_type_id, bin.copies);
        std::vector<SolutionItem> items = (!unbalanced(bin))?
            bin.items:
            drop_bin_items(instance, bin.items);
        for (const SolutionItem& item: items) {
            solution_builder.add_item(
                    new_bin_pos,
                    item.item_type_id,
                    item.bl_corner,
                    item.rotation);
        }
    }
    return solution_builder.build();
}

Solution packingsolver::box::remove_unbalanced_items(const Solution& solution)
{
    if (solution.stable() && solution.maximum_weight_above_feasible())
        return solution;

    const Instance& instance = solution.instance();
    SolutionBuilder solution_builder(instance);
    for (BinPos bin_pos = 0;
            bin_pos < solution.number_of_different_bins();
            ++bin_pos) {
        const SolutionBin& bin = solution.bin(bin_pos);
        std::vector<SolutionItem> items = (!unbalanced(bin))?
            bin.items:
            remove_bin_unbalanced_items(
                    instance,
                    instance.bin_type(bin.bin_type_id),
                    bin.items);
        BinPos new_bin_pos = solution_builder.add_bin(bin.bin_type_id, bin.copies);
        for (const SolutionItem& item: items) {
            solution_builder.add_item(
                    new_bin_pos,
                    item.item_type_id,
                    item.bl_corner,
                    item.rotation);
        }
    }
    return solution_builder.build();
}
