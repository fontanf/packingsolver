#include "box/sequential_feasibility.hpp"

#include "packingsolver/box/algorithm_formatter.hpp"
#include "packingsolver/box/instance_builder.hpp"
#include "box/solution_builder.hpp"

#include <algorithm>
#include <sstream>

using namespace packingsolver;
using namespace packingsolver::box;

namespace
{

/**
 * Return the dimension along which a bin is shrunk: 0 for x, 1 for y, 2 for
 * z.
 */
int shrunk_dimension(
        const Instance& instance)
{
    switch (instance.objective()) {
    case Objective::OpenDimensionX: {
        return 0;
    } case Objective::OpenDimensionY: {
        return 1;
    } case Objective::OpenDimensionZ: {
        return 2;
    } default: {
        switch (instance.parameters().leftover_mode) {
        case LeftoverMode::Y: case LeftoverMode::YZ: {
            return 1;
        } case LeftoverMode::Z: {
            return 2;
        } default: {
            return 0;
        }
        }
    }
    }
}

/** Return the length of a box along a dimension. */
Length length(
        const Box& box,
        int dimension)
{
    return (dimension == 0)? box.x: (dimension == 1)? box.y: box.z;
}

/**
 * Return the length used by the last bin of a solution along a dimension.
 */
Length length(
        const Solution& solution,
        int dimension)
{
    return (dimension == 0)? solution.x_max():
        (dimension == 1)? solution.y_max():
        solution.z_max();
}

/** Return a box shrunk to 'length' along a dimension. */
Box shrink(
        Box box,
        int dimension,
        Length length)
{
    if (dimension == 0) {
        box.x = length;
    } else if (dimension == 1) {
        box.y = length;
    } else {
        box.z = length;
    }
    return box;
}

/**
 * Packing of the items in columns: a feasible solution, from which the
 * sequential feasibility starts.
 *
 * The columns are stacks of items along z (along y if the bins are shrunk
 * along z). They stand in rows across the shrunk dimension, and the rows
 * follow each other along the shrunk dimension. Each item is packed with the
 * allowed rotation which minimizes its length along the shrunk dimension,
 * then across the rows, among the ones which fit in the largest bin. The
 * items are sorted by decreasing length along the shrunk dimension, then
 * across the rows; each one is packed on top of the first column of the
 * current bin whose footprint contains its own, else in a new column at the
 * end of the last row, else in a new row, else in the next bin. A bin is also
 * full when the item would exceed its maximum weight.
 *
 * Return 'false' if some items can't be packed, if the solution isn't
 * feasible, or if the instance has resources or fixed items, which the
 * column packing doesn't handle.
 */
bool pack_columns(
        const Instance& instance,
        int dimension,
        Solution& solution)
{
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        if (bin_type.number_of_resources() > 0 || !bin_type.fixed_items.empty())
            return false;
    }

    // The dimensions: 'u' along which the rows follow each other, 'w' along
    // which the columns of a row follow each other, 'v' along which the items
    // of a column follow each other.
    const int u = dimension;
    const int v = (dimension == 2)? 1: 2;
    const int w = 3 - u - v;

    // The largest lengths of the bins.
    Box bin_max = {0, 0, 0};
    for (BinPos bin_pos = 0; bin_pos < instance.number_of_bins(); ++bin_pos) {
        const Box& box = instance.bin_type(instance.bin_type_id(bin_pos)).box;
        bin_max = {
            (std::max)(bin_max.x, box.x),
            (std::max)(bin_max.y, box.y),
            (std::max)(bin_max.z, box.z)};
    }

    // The items, with their rotation.
    struct PackedItem
    {
        ItemTypeId item_type_id;
        Rotation rotation;
        Box box;
    };
    std::vector<PackedItem> items;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        bool found = false;
        PackedItem best;
        best.item_type_id = item_type_id;
        for (Rotation rotation: item_type.rotations) {
            Box box = item_type.box.rotate(rotation);
            if (box.x > bin_max.x || box.y > bin_max.y || box.z > bin_max.z)
                continue;
            if (!found
                    || length(box, u) < length(best.box, u)
                    || (length(box, u) == length(best.box, u)
                        && length(box, w) < length(best.box, w))) {
                found = true;
                best.rotation = rotation;
                best.box = box;
            }
        }
        if (!found)
            return false;
        for (ItemPos copy = item_type.copies_fixed; copy < item_type.copies; ++copy)
            items.push_back(best);
    }
    std::stable_sort(
            items.begin(),
            items.end(),
            [u, w](const PackedItem& item_1, const PackedItem& item_2)
            {
                if (length(item_1.box, u) != length(item_2.box, u))
                    return length(item_1.box, u) > length(item_2.box, u);
                return length(item_1.box, w) > length(item_2.box, w);
            });

    // Current bin: its box, its weight, its columns and its last row.
    BinPos number_of_bins = 0;
    BinPos bin_pos = -1;
    Box bin_box = {0, 0, 0};
    Weight bin_maximum_weight = 0;
    Weight bin_weight = 0;
    struct Column
    {
        Length u_start;
        Length u_length;
        Length w_start;
        Length w_length;
        Length v_end;
    };
    std::vector<Column> columns;
    bool row_open = false;
    Length row_u_start = 0;
    Length row_u_length = 0;
    Length row_w_end = 0;
    SolutionBuilder solution_builder(instance);

    // Add the next bin. Return 'false' if all the bins are already used.
    auto add_bin = [&]()
    {
        if (number_of_bins == instance.number_of_bins())
            return false;
        BinTypeId bin_type_id = instance.bin_type_id(number_of_bins);
        const BinType& bin_type = instance.bin_type(bin_type_id);
        bin_pos = solution_builder.add_bin(bin_type_id, 1);
        number_of_bins++;
        bin_box = bin_type.box;
        bin_maximum_weight = bin_type.maximum_weight;
        bin_weight = 0;
        columns.clear();
        row_open = false;
        return true;
    };

    if (!add_bin())
        return false;
    for (const PackedItem& item: items) {
        const ItemType& item_type = instance.item_type(item.item_type_id);
        Length item_u = length(item.box, u);
        Length item_v = length(item.box, v);
        Length item_w = length(item.box, w);
        Counter column_id = -1;
        if (bin_weight + item_type.weight <= bin_maximum_weight) {
            for (Counter c = 0; c < (Counter)columns.size(); ++c) {
                const Column& column = columns[c];
                if (item_u <= column.u_length
                        && item_w <= column.w_length
                        && column.v_end + item_v <= length(bin_box, v)) {
                    column_id = c;
                    break;
                }
            }
        }
        if (column_id == -1) {
            for (;;) {
                if (bin_weight + item_type.weight <= bin_maximum_weight
                        && item_v <= length(bin_box, v)) {
                    // A new column at the end of the last row.
                    if (row_open
                            && item_u <= row_u_length
                            && row_w_end + item_w <= length(bin_box, w)) {
                        break;
                    }
                    // A new row.
                    Length u_start = (row_open)? row_u_start + row_u_length: 0;
                    if (u_start + item_u <= length(bin_box, u)
                            && item_w <= length(bin_box, w)) {
                        row_open = true;
                        row_u_start = u_start;
                        row_u_length = item_u;
                        row_w_end = 0;
                        break;
                    }
                }
                if (!add_bin())
                    return false;
            }
            columns.push_back({row_u_start, item_u, row_w_end, item_w, 0});
            row_w_end += item_w;
            column_id = columns.size() - 1;
        }
        Column& column = columns[column_id];
        Length coordinates[3];
        coordinates[u] = column.u_start;
        coordinates[w] = column.w_start;
        coordinates[v] = column.v_end;
        column.v_end += item_v;
        bin_weight += item_type.weight;
        solution_builder.add_item(
                bin_pos,
                item.item_type_id,
                {coordinates[0], coordinates[1], coordinates[2]},
                item.rotation);
    }
    solution = solution_builder.build();
    return solution.full() && solution.feasible();
}
}

SequentialFeasibilityOutput packingsolver::box::sequential_feasibility(
        const Instance& instance,
        const SequentialFeasibilitySolver& solver,
        const SequentialFeasibilityParameters& parameters)
{
    SequentialFeasibilityOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    const BinType& bin_type = instance.bin_type(instance.bin_type_id(0));
    const int dimension = shrunk_dimension(instance);

    // Without the column packing, the first iteration uses all the bins, at
    // their full size.
    BinPos current_number_of_bins = instance.number_of_bins();
    // Length of the last bin along the shrunk dimension.
    Length current_length = length(
            instance.bin_type(instance.bin_type_id(current_number_of_bins - 1)).box,
            dimension);

    // Update the bin count and/or the length from a solution found, shrinking
    // them. Return false if there is nothing left to try.
    auto update_targets = [&instance, dimension, &current_number_of_bins, &current_length](
            const Solution& solution)
    {
        if (instance.objective() == Objective::BinPacking) {
            current_number_of_bins = solution.number_of_bins() - 1;
            if (current_number_of_bins == 0)
                return false;
        } else if (instance.objective() == Objective::BinPackingWithLeftovers) {
            Length length_used = length(solution, dimension);
            current_length = std::min((Length)(0.99 * length_used), length_used - 1);
            if (solution.number_of_bins() < current_number_of_bins) {
                current_number_of_bins = solution.number_of_bins();
                const BinType& bin_type_new = instance.bin_type(
                        instance.bin_type_id(current_number_of_bins - 1));
                current_length = length(bin_type_new.box, dimension);
            } else if (current_length <= 0) {
                current_number_of_bins--;
                if (current_number_of_bins == 0)
                    return false;
                const BinType& bin_type_new = instance.bin_type(
                        instance.bin_type_id(current_number_of_bins - 1));
                current_length = length(bin_type_new.box, dimension);
            }
        } else {  // OpenDimensionX, OpenDimensionY, OpenDimensionZ
            Length length_used = length(solution, dimension);
            current_length = std::min((Length)(0.99 * length_used), length_used - 1);
            if (current_length <= 0)
                return false;
        }
        return true;
    };

    // A packing of the items in columns: a first solution, from which the
    // targets of the first iteration are shrunk.
    bool stop = false;
    Solution column_packing(instance);
    if (pack_columns(instance, dimension, column_packing)) {
        algorithm_formatter.update_solution(column_packing, "SF columns");
        current_number_of_bins = column_packing.number_of_bins();
        stop = !update_targets(column_packing);
    }

    for (Counter it = 0; !stop; ++it) {
        if (algorithm_formatter.end_boolean())
            break;
        if (parameters.timer.needs_to_end())
            break;

        // Build the Feasibility sub-instance. Its bins are the first
        // 'current_number_of_bins' bins of the instance; for objective
        // BinPackingWithLeftovers, the last one is shrunk and is a bin type of
        // its own.
        InstanceBuilder sub_instance_builder;
        sub_instance_builder.set_objective(Objective::Feasibility);
        sub_instance_builder.set_parameters(instance.parameters());
        sub_instance_builder.set_original_instance(instance);
        std::vector<BinTypeId> sub_to_orig_bin_type_ids;
        if (instance.objective() == Objective::BinPacking
                || instance.objective() == Objective::BinPackingWithLeftovers) {
            bool restrict_last_bin = (instance.objective() == Objective::BinPackingWithLeftovers);
            BinPos remaining_bins = current_number_of_bins;
            for (BinTypeId bin_type_id = 0;
                    bin_type_id < instance.number_of_bin_types() && remaining_bins > 0;
                    ++bin_type_id) {
                const BinType& bin_type_cur = instance.bin_type(bin_type_id);
                BinPos copies = std::min(bin_type_cur.copies, remaining_bins);
                remaining_bins -= copies;
                BinPos copies_full = (restrict_last_bin && remaining_bins == 0)?
                    copies - 1:
                    copies;
                if (copies_full > 0) {
                    BinTypeId sub_bin_type_id = sub_instance_builder.add_bin_type_from_original_instance(bin_type_id);
                    sub_instance_builder.set_bin_type_copies(sub_bin_type_id, copies_full);
                    sub_instance_builder.set_bin_type_copies_min(sub_bin_type_id, 0);
                    sub_to_orig_bin_type_ids.push_back(bin_type_id);
                }
                if (copies_full == copies)
                    continue;

                // Last bin.
                BinTypeId sub_bin_type_id = sub_instance_builder.add_bin_type_from_original_instance(bin_type_id);
                sub_instance_builder.set_bin_type_copies(sub_bin_type_id, 1);
                sub_instance_builder.set_bin_type_copies_min(sub_bin_type_id, 0);
                Box box = shrink(bin_type_cur.box, dimension, current_length);
                sub_instance_builder.crop_bin_type(sub_bin_type_id, box.x, box.y, box.z);
                sub_to_orig_bin_type_ids.push_back(bin_type_id);
            }
        } else {
            BinTypeId sub_bin_type_id = sub_instance_builder.add_bin_type_from_original_instance(instance.bin_type_id(0));
            sub_instance_builder.set_bin_type_copies(sub_bin_type_id, 1);
            sub_instance_builder.set_bin_type_copies_min(sub_bin_type_id, 0);
            Box box = shrink(bin_type.box, dimension, current_length);
            sub_instance_builder.crop_bin_type(sub_bin_type_id, box.x, box.y, box.z);
            sub_to_orig_bin_type_ids.push_back(instance.bin_type_id(0));
        }
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            sub_instance_builder.add_item_type_from_original_instance(item_type_id);
        }
        Instance sub_instance = sub_instance_builder.build();

        // Solve the sub-instance.
        SolutionPool<Instance, Solution> sub_solution_pool = solver(sub_instance);

        // If no feasible solution found, stop.
        if (!sub_solution_pool.best().feasible()
                || !sub_solution_pool.best().full()) {
            break;
        }

        // Transfer the sub-solution to the main instance.
        Solution solution(instance);
        solution.append_bins(
                sub_solution_pool.best(),
                sub_to_orig_bin_type_ids,
                {});
        std::stringstream ss;
        ss << "SF it " << it << " " << sub_solution_pool.best_label();
        algorithm_formatter.update_solution(solution, ss.str());

        // Update for the next iteration.
        if (!update_targets(solution))
            break;
    }

    algorithm_formatter.end();
    return output;
}
