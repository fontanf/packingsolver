#include "box/static_stability.hpp"

#include "mathoptsolverscmake/mathopt.hpp"
#ifdef HIGHS_FOUND
#include "mathoptsolverscmake/mathopt_highs.hpp"
#endif

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

}

bool packingsolver::box::is_statically_stable(
        const Instance& instance,
        const BinType& bin_type,
        const std::vector<SolutionItem>& items)
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
        if (!supported && weights[item_pos] > 0)
            return false;
    }

    // Build the linear program. The variables are the forces at the corners
    // of the contacts. For each item, the rows are the balance of the
    // vertical forces and the balances of the moments about the axes
    // parallel to x and y through its center. The forces are divided by the
    // largest weight and the lever arms by the largest dimension of the bin,
    // so that the coefficients are between -1 and 1.
    Weight weight_max = 0;
    for (Weight weight: weights)
        weight_max = (std::max)(weight_max, weight);
    if (weight_max == 0)
        return true;
    double length_max = (double)(std::max)({bin_type.box.x, bin_type.box.y, bin_type.box.z});

    std::vector<std::vector<int>> rows_variables(3 * number_of_items);
    std::vector<std::vector<double>> rows_coefficients(3 * number_of_items);
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
                }
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
        model.objective_coefficients.push_back(0.0);
    }
    for (int row = 0; row < 3 * number_of_items; ++row) {
        ItemPos item_pos = row / 3;
        double value = (row % 3 == 0)? weights[item_pos] / weight_max: 0.0;
        model.constraints_starts.push_back((int)model.elements_variables.size());
        for (std::size_t pos = 0; pos < rows_variables[row].size(); ++pos) {
            model.elements_variables.push_back(rows_variables[row][pos]);
            model.elements_coefficients.push_back(rows_coefficients[row][pos]);
        }
        model.constraints_lower_bounds.push_back(value);
        model.constraints_upper_bounds.push_back(value);
    }

#ifdef HIGHS_FOUND
    Highs highs;
    mathoptsolverscmake::reduce_printout(highs);
    mathoptsolverscmake::load(highs, model);
    mathoptsolverscmake::solve(highs);
    return highs.getModelStatus() == HighsModelStatus::kOptimal;
#else
    throw std::invalid_argument(
            FUNC_SIGNATURE + ": "
            "the static stability check requires HiGHS.");
#endif
}
