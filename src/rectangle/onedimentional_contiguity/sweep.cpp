#include "rectangle/onedimentional_contiguity/sweep.hpp"

#include <algorithm>
#include <numeric>

using namespace packingsolver;
using namespace packingsolver::rectangle;
using namespace packingsolver::rectangle::onedimentional_contiguity;

namespace
{

struct SweepData
{
    const SweepParameters* parameters = nullptr;

    /** Width of the bin. */
    Length bin_width = 0;

    /** Height of the bin: capacity of a column. */
    Length bin_height = 0;

    /** Width of each unit. */
    std::vector<Length> widths;

    /** Height of each unit. */
    std::vector<Length> heights;

    /** Units, the largest ones first. */
    std::vector<ItemPos> order;

    /**
     * Previous unit in the order with the same width and height; -1 if none.
     * A unit is only placed after this one.
     */
    std::vector<ItemPos> previous_identical;

    /** Height used in each column. */
    std::vector<Length> loads;

    /** Start of each unit; -1 if not placed. */
    std::vector<Length> positions;

    /** 'is_start[x]' is 'true' iff 'x == 0' or a unit ends at 'x'. */
    std::vector<char> is_start;

    /** Area of the units not placed. */
    Area remaining_area = 0;

    /** Number of units not placed. */
    ItemPos number_of_remaining_units = 0;

    /** Number of operations. */
    Counter number_of_operations = 0;

    /** Number of nodes. */
    Counter number_of_nodes = 0;

    /** 'true' iff the search reached its limits. */
    bool aborted = false;
};

/**
 * Search from a start, the units starting there being chosen from position
 * 'minimum_order_pos' in the order; return 'true' iff a solution is found.
 */
bool search(
        SweepData& data,
        Length start,
        ItemPos minimum_order_pos)
{
    ItemPos number_of_units = data.widths.size();
    if (data.number_of_remaining_units == 0)
        return true;
    if (data.parameters->maximum_number_of_operations >= 0
            && data.number_of_operations >= data.parameters->maximum_number_of_operations) {
        data.aborted = true;
        return false;
    }
    data.number_of_nodes++;
    if (data.number_of_nodes % 1024 == 0
            && data.parameters->timer.needs_to_end()) {
        data.aborted = true;
        return false;
    }

    // Area bound: the remaining units start at or after 'start', and the
    // free space of a column where the lowest remaining unit doesn't fit is
    // lost.
    Length minimum_height = data.bin_height + 1;
    for (ItemPos unit_pos = 0; unit_pos < number_of_units; ++unit_pos)
        if (data.positions[unit_pos] == -1)
            minimum_height = (std::min)(minimum_height, data.heights[unit_pos]);
    data.number_of_operations += data.bin_width - start;
    Area usable_area = 0;
    for (Length column = start; column < data.bin_width; ++column)
        if (data.bin_height - data.loads[column] >= minimum_height)
            usable_area += data.bin_height - data.loads[column];
    if (usable_area < data.remaining_area)
        return false;

    // Units starting at 'start'.
    for (ItemPos order_pos = minimum_order_pos; order_pos < number_of_units; ++order_pos) {
        ItemPos unit_pos = data.order[order_pos];
        if (data.positions[unit_pos] != -1)
            continue;
        ItemPos previous_unit_pos = data.previous_identical[unit_pos];
        if (previous_unit_pos != -1 && data.positions[previous_unit_pos] == -1)
            continue;
        Length width = data.widths[unit_pos];
        Length height = data.heights[unit_pos];
        if (start + width > data.bin_width)
            continue;
        data.number_of_operations += width;
        bool fits = true;
        for (Length column = start; column < start + width; ++column) {
            if (data.loads[column] + height > data.bin_height) {
                fits = false;
                break;
            }
        }
        if (!fits)
            continue;

        data.positions[unit_pos] = start;
        for (Length column = start; column < start + width; ++column)
            data.loads[column] += height;
        data.remaining_area -= (Area)width * height;
        data.number_of_remaining_units--;
        char is_start_old = data.is_start[start + width];
        data.is_start[start + width] = 1;

        bool found = search(data, start, order_pos + 1);

        data.is_start[start + width] = is_start_old;
        data.number_of_remaining_units++;
        data.remaining_area += (Area)width * height;
        for (Length column = start; column < start + width; ++column)
            data.loads[column] -= height;
        if (found)
            return true;
        data.positions[unit_pos] = -1;
        if (data.aborted)
            return false;
    }

    // Close 'start'.
    Length next_start = start + 1;
    while (next_start < data.bin_width && !data.is_start[next_start])
        next_start++;
    if (next_start >= data.bin_width)
        return false;
    return search(data, next_start, 0);
}

}

SweepOutput packingsolver::rectangle::onedimentional_contiguity::sweep(
        const Instance& instance,
        const SweepParameters& parameters)
{
    if (instance.objective() != Objective::Feasibility)
        throw std::invalid_argument(FUNC_SIGNATURE + ": unsupported objective.");
    if (instance.number_of_bin_types() != 1
            || instance.bin_type(0).copies != 1) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": the instance must have a single bin.");
    }

    SweepOutput output;
    const BinType& bin_type = instance.bin_type(0);
    SweepData data;
    data.parameters = &parameters;
    data.bin_width = bin_type.rect.x;
    data.bin_height = bin_type.rect.y;

    // Units.
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        Length width = item_type.rect.x;
        Length height = item_type.rect.y;
        if (!item_type.oriented) {
            Length smallest = (std::min)(item_type.rect.x, item_type.rect.y);
            width = smallest;
            height = smallest;
        }
        if (width > data.bin_width || height > data.bin_height) {
            output.status = OnedimensionalContiguityResult::Status::Infeasible;
            return output;
        }
        for (ItemPos copy = 0; copy < item_type.copies; ++copy) {
            data.widths.push_back(width);
            data.heights.push_back(height);
            data.remaining_area += (Area)width * height;
        }
    }
    ItemPos number_of_units = data.widths.size();
    data.number_of_remaining_units = number_of_units;
    if (data.remaining_area > (Area)data.bin_width * data.bin_height) {
        output.status = OnedimensionalContiguityResult::Status::Infeasible;
        return output;
    }

    data.order.resize(number_of_units);
    std::iota(data.order.begin(), data.order.end(), 0);
    std::sort(
            data.order.begin(),
            data.order.end(),
            [&data](ItemPos unit_pos_1, ItemPos unit_pos_2)
            {
                Area area_1 = (Area)data.widths[unit_pos_1] * data.heights[unit_pos_1];
                Area area_2 = (Area)data.widths[unit_pos_2] * data.heights[unit_pos_2];
                if (area_1 != area_2)
                    return area_1 > area_2;
                if (data.widths[unit_pos_1] != data.widths[unit_pos_2])
                    return data.widths[unit_pos_1] > data.widths[unit_pos_2];
                return unit_pos_1 < unit_pos_2;
            });
    data.previous_identical.assign(number_of_units, -1);
    for (ItemPos order_pos = 1; order_pos < number_of_units; ++order_pos) {
        ItemPos unit_pos = data.order[order_pos];
        ItemPos previous_unit_pos = data.order[order_pos - 1];
        if (data.widths[unit_pos] == data.widths[previous_unit_pos]
                && data.heights[unit_pos] == data.heights[previous_unit_pos]) {
            data.previous_identical[unit_pos] = previous_unit_pos;
        }
    }

    data.loads.assign(data.bin_width, 0);
    data.positions.assign(number_of_units, -1);
    data.is_start.assign(data.bin_width + 1, 0);
    data.is_start[0] = 1;

    bool found = search(data, 0, 0);
    output.number_of_operations = data.number_of_operations;
    if (found) {
        output.status = OnedimensionalContiguityResult::Status::Feasible;
    } else if (!data.aborted) {
        output.status = OnedimensionalContiguityResult::Status::Infeasible;
    }
    return output;
}
