#include "rectangle/sequential_feasibility.hpp"

#include "packingsolver/rectangle/algorithm_formatter.hpp"
#include "packingsolver/rectangle/instance_builder.hpp"
#include "rectangle/solution_builder.hpp"

#include <algorithm>
#include <sstream>

using namespace packingsolver;
using namespace packingsolver::rectangle;

namespace
{

/**
 * Return the length of a bin along the shrunk direction, for objective
 * BinPackingWithLeftovers.
 */
Length leftover_length(
        const Instance& instance,
        const Rectangle& rect)
{
    return (instance.parameters().leftover_mode == LeftoverMode::Y)? rect.y: rect.x;
}

/**
 * Return the length used by the last bin of a solution along the shrunk
 * direction, for objective BinPackingWithLeftovers.
 */
Length leftover_length(
        const Instance& instance,
        const Solution& solution)
{
    return (instance.parameters().leftover_mode == LeftoverMode::Y)?
        solution.y_max():
        solution.x_max();
}

/**
 * Box in the coordinates of the column packing: the columns follow each other
 * along 'u', and the items of a column along 'v'.
 */
struct UvBox
{
    Length u_start;
    Length u_end;
    Length v_start;
    Length v_end;
};

/** Return 'true' iff the interiors of two boxes intersect. */
bool intersect(
        const UvBox& box_1,
        const UvBox& box_2)
{
    return box_1.u_end > box_2.u_start
        && box_2.u_end > box_1.u_start
        && box_1.v_end > box_2.v_start
        && box_2.v_end > box_1.v_start;
}

/**
 * Packing of the items in columns: a feasible solution, from which the
 * sequential feasibility starts.
 *
 * Each item which can be rotated is packed with its smaller side across the
 * columns, unless it is then longer than the bins. The items are sorted by
 * decreasing width; each one is packed in the first column of the current bin
 * where it fits, else in a new column, else in the next bin. In a column, an
 * item is packed as low as possible: while it intersects a defect, it is
 * moved above it; a new column is moved right the same way. A bin is also
 * full when the item would exceed its maximum weight.
 *
 * For objective OpenDimensionY, and for objective BinPackingWithLeftovers
 * with leftover mode Y, x and y are swapped: the items are packed in rows.
 *
 * Return 'false' if some items can't be packed, if the solution isn't
 * feasible, or if the instance has eligibilities, resources or fixed items,
 * which the column packing doesn't handle.
 */
bool pack_columns(
        const Instance& instance,
        Solution& solution)
{
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        if (!bin_type.eligibility_ids.empty()
                || bin_type.number_of_resources() > 0
                || !bin_type.fixed_items.empty()) {
            return false;
        }
    }
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        if (instance.item_type(item_type_id).eligibility_id != -1)
            return false;
    }

    const bool rows = (instance.objective() == Objective::OpenDimensionY
            || (instance.objective() == Objective::BinPackingWithLeftovers
                && instance.parameters().leftover_mode == LeftoverMode::Y));
    auto to_uv = [rows](Length x, Length y, Length lx, Length ly)
    {
        return (!rows)?
            UvBox{x, x + lx, y, y + ly}:
            UvBox{y, y + ly, x, x + lx};
    };

    // The length of the longest bin along v.
    Length bin_v_length_max = 0;
    for (BinPos bin_pos = 0; bin_pos < instance.number_of_bins(); ++bin_pos) {
        const BinType& bin_type = instance.bin_type(instance.bin_type_id(bin_pos));
        bin_v_length_max = (std::max)(bin_v_length_max, (!rows)? bin_type.rect.y: bin_type.rect.x);
    }

    // The items, with their orientation.
    struct PackedItem
    {
        ItemTypeId item_type_id;
        bool rotate;
        Length u_length;
        Length v_length;
    };
    std::vector<PackedItem> items;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        PackedItem item;
        item.item_type_id = item_type_id;
        item.rotate = false;
        UvBox box = to_uv(0, 0, item_type.x(false), item_type.y(false));
        item.u_length = box.u_end;
        item.v_length = box.v_end;
        bool smaller_side_across = (item.u_length <= item.v_length);
        bool fits = (item.v_length <= bin_v_length_max);
        if (!item_type.oriented && (!smaller_side_across || !fits)) {
            UvBox box_rotated = to_uv(0, 0, item_type.x(true), item_type.y(true));
            if (box_rotated.v_end <= bin_v_length_max
                    && (!fits || box_rotated.u_end <= box_rotated.v_end)) {
                item.rotate = true;
                item.u_length = box_rotated.u_end;
                item.v_length = box_rotated.v_end;
            }
        }
        for (ItemPos copy = item_type.copies_fixed; copy < item_type.copies; ++copy)
            items.push_back(item);
    }
    std::stable_sort(
            items.begin(),
            items.end(),
            [](const PackedItem& item_1, const PackedItem& item_2)
            {
                return item_1.u_length > item_2.u_length;
            });

    // Current bin: its box, its defects, its weight and its columns.
    BinPos number_of_bins = 0;
    BinPos bin_pos = -1;
    UvBox bin_box = {0, 0, 0, 0};
    Weight bin_maximum_weight = 0;
    Weight bin_weight = 0;
    std::vector<UvBox> obstacles;
    struct Column
    {
        Length u_start;
        Length u_length;
        Length v_end;
    };
    std::vector<Column> columns;
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
        bin_box = to_uv(0, 0, bin_type.rect.x, bin_type.rect.y);
        bin_maximum_weight = bin_type.maximum_weight;
        bin_weight = 0;
        columns.clear();
        obstacles.clear();
        for (const Defect& defect: bin_type.defects)
            obstacles.push_back(to_uv(defect.pos.x, defect.pos.y, defect.rect.x, defect.rect.y));
        return true;
    };

    // Return the lowest position, not lower than 'v', of the box from
    // 'u_start' of lengths 'u_length' and 'v_length' which doesn't intersect
    // an obstacle and ends before the end of the bin; -1 if there is none.
    auto lowest_position = [&obstacles, &bin_box](
            Length u_start,
            Length u_length,
            Length v_length,
            Length v)
    {
        for (;;) {
            if (v + v_length > bin_box.v_end)
                return (Length)-1;
            UvBox box = {u_start, u_start + u_length, v, v + v_length};
            Length v_next = v;
            for (const UvBox& obstacle: obstacles)
                if (intersect(obstacle, box))
                    v_next = (std::max)(v_next, obstacle.v_end);
            if (v_next == v)
                return v;
            v = v_next;
        }
    };

    if (!add_bin())
        return false;
    for (const PackedItem& item: items) {
        const ItemType& item_type = instance.item_type(item.item_type_id);
        Counter column_id = -1;
        Length v = -1;
        if (bin_weight + item_type.weight <= bin_maximum_weight) {
            // The first column where the item fits (it fits in width, since
            // the items are sorted by decreasing width).
            for (Counter c = 0; c < (Counter)columns.size(); ++c) {
                v = lowest_position(columns[c].u_start, item.u_length, item.v_length, columns[c].v_end);
                if (v != -1) {
                    column_id = c;
                    break;
                }
            }
        }
        if (column_id == -1) {
            // A new column, after the last one. If the defects prevent
            // packing the item in it, it is moved to the right of the first
            // one; if the bin is full, the item is packed in a new bin.
            Length u_start = (columns.empty())?
                bin_box.u_start:
                columns.back().u_start + columns.back().u_length;
            for (;;) {
                if (bin_weight + item_type.weight <= bin_maximum_weight
                        && u_start + item.u_length <= bin_box.u_end) {
                    v = lowest_position(u_start, item.u_length, item.v_length, bin_box.v_start);
                    if (v != -1)
                        break;
                    UvBox column_box = {u_start, u_start + item.u_length, bin_box.v_start, bin_box.v_end};
                    Length u_next = -1;
                    for (const UvBox& obstacle: obstacles) {
                        if (intersect(obstacle, column_box)
                                && (u_next == -1 || obstacle.u_end < u_next)) {
                            u_next = obstacle.u_end;
                        }
                    }
                    if (u_next != -1) {
                        u_start = u_next;
                        continue;
                    }
                }
                if (!add_bin())
                    return false;
                u_start = bin_box.u_start;
            }
            columns.push_back({u_start, item.u_length, 0});
            column_id = columns.size() - 1;
        }
        columns[column_id].v_end = v + item.v_length;
        bin_weight += item_type.weight;
        Point bl_corner = (!rows)?
            Point{columns[column_id].u_start, v}:
            Point{v, columns[column_id].u_start};
        solution_builder.add_item(bin_pos, item.item_type_id, bl_corner, item.rotate);
    }
    solution = solution_builder.build();
    return solution.full() && solution.feasible();
}
}

SequentialFeasibilityOutput packingsolver::rectangle::sequential_feasibility(
        const Instance& instance,
        const SequentialFeasibilitySolver& solver,
        const SequentialFeasibilityParameters& parameters)
{
    SequentialFeasibilityOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    const BinType& bin_type = instance.bin_type(instance.bin_type_id(0));

    // Without the column packing, the first iteration uses all the bins, at
    // their full size.
    BinPos current_number_of_bins = instance.number_of_bins();
    // Length of the last bin along the shrunk direction.
    Length current_length = 0;
    if (instance.objective() == Objective::BinPackingWithLeftovers) {
        const BinType& last_bin_type = instance.bin_type(
                instance.bin_type_id(current_number_of_bins - 1));
        current_length = leftover_length(instance, last_bin_type.rect);
    } else if (instance.objective() == Objective::OpenDimensionX) {
        current_length = bin_type.rect.x;
    } else if (instance.objective() == Objective::OpenDimensionY) {
        current_length = bin_type.rect.y;
    }

    // Update the bin count and/or the length from a solution found, shrinking
    // them. Return false if there is nothing left to try.
    auto update_targets = [&instance, &current_number_of_bins, &current_length](
            const Solution& solution)
    {
        if (instance.objective() == Objective::BinPacking) {
            current_number_of_bins = solution.number_of_bins() - 1;
            if (current_number_of_bins == 0)
                return false;
        } else if (instance.objective() == Objective::BinPackingWithLeftovers) {
            Length length = leftover_length(instance, solution);
            current_length = std::min((Length)(0.99 * length), length - 1);
            if (solution.number_of_bins() < current_number_of_bins) {
                current_number_of_bins = solution.number_of_bins();
                const BinType& bin_type_new = instance.bin_type(
                        instance.bin_type_id(current_number_of_bins - 1));
                current_length = leftover_length(instance, bin_type_new.rect);
            } else if (current_length <= 0) {
                current_number_of_bins--;
                if (current_number_of_bins == 0)
                    return false;
                const BinType& bin_type_new = instance.bin_type(
                        instance.bin_type_id(current_number_of_bins - 1));
                current_length = leftover_length(instance, bin_type_new.rect);
            }
        } else if (instance.objective() == Objective::OpenDimensionX) {
            current_length = std::min(
                    (Length)(0.99 * solution.x_max()),
                    solution.x_max() - 1);
            if (current_length <= 0)
                return false;
        } else {  // OpenDimensionY
            current_length = std::min(
                    (Length)(0.99 * solution.y_max()),
                    solution.y_max() - 1);
            if (current_length <= 0)
                return false;
        }
        return true;
    };

    // A packing of the items in columns: a first solution, from which the
    // targets of the first iteration are shrunk.
    bool stop = false;
    Solution column_packing(instance);
    if (pack_columns(instance, column_packing)) {
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
                if (instance.parameters().leftover_mode == LeftoverMode::Y) {
                    sub_instance_builder.crop_bin_type(
                            sub_bin_type_id,
                            bin_type_cur.rect.x,
                            current_length);
                } else {
                    sub_instance_builder.crop_bin_type(
                            sub_bin_type_id,
                            current_length,
                            bin_type_cur.rect.y);
                }
                sub_to_orig_bin_type_ids.push_back(bin_type_id);
            }
        } else {
            BinTypeId sub_bin_type_id = sub_instance_builder.add_bin_type_from_original_instance(instance.bin_type_id(0));
            sub_instance_builder.set_bin_type_copies(sub_bin_type_id, 1);
            sub_instance_builder.set_bin_type_copies_min(sub_bin_type_id, 0);
            if (instance.objective() == Objective::OpenDimensionX) {
                sub_instance_builder.crop_bin_type(
                        sub_bin_type_id,
                        current_length,
                        bin_type.rect.y);
            } else {  // OpenDimensionY
                sub_instance_builder.crop_bin_type(
                        sub_bin_type_id,
                        bin_type.rect.x,
                        current_length);
            }
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
