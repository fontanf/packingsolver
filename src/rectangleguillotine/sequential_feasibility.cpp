#include "rectangleguillotine/sequential_feasibility.hpp"

#include "packingsolver/rectangleguillotine/algorithm_formatter.hpp"
#include "packingsolver/rectangleguillotine/instance_builder.hpp"
#include "rectangleguillotine/solution_builder.hpp"

#include <algorithm>
#include <sstream>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

namespace
{

/**
 * Packing of the items in strips: a feasible solution, from which the
 * sequential feasibility starts.
 *
 * The first-stage cuts are vertical (horizontal for objective
 * OpenDimensionY): they cut the bins into strips, and the second-stage cuts
 * cut the strips into the items, with a third-stage cut when an item is
 * narrower than its strip. Each item which can be rotated is packed with its
 * smaller side across the strips, unless it is then longer than the bins.
 * The items are sorted by decreasing width; each one is packed in the first
 * strip of the current bin where it fits, else in a new strip, else in the
 * next bin. The cut thickness and the trims of the bins are taken into
 * account.
 *
 * Return 'false' if some items can't be packed, if the solution isn't
 * feasible, or if the instance has constraints which the packing doesn't
 * handle: a limited number of stages (lower than 3), the homogenous cut type,
 * a first stage orientation different from the one of the strips, defects,
 * fixed items, resources, stacks of several item types, a minimum waste
 * length, distances between cuts, or a maximum number of cuts.
 */
bool pack_strips(
        const Instance& instance,
        Solution& solution)
{
    const CutOrientation first_cut_orientation = (instance.objective() == Objective::OpenDimensionY)?
        CutOrientation::Horizontal:
        CutOrientation::Vertical;
    const rectangleguillotine::Parameters& instance_parameters = instance.parameters();
    if ((!instance.number_of_stages_unlimited() && instance_parameters.number_of_stages < 3)
            || instance_parameters.cut_type == CutType::Homogenous
            || (instance_parameters.first_stage_orientation != CutOrientation::Any
                && instance_parameters.first_stage_orientation != first_cut_orientation)
            || instance.number_of_defects() > 0
            || instance.number_of_non_empty_stacks() != instance.number_of_item_types()
            || instance_parameters.minimum_waste_length != 0
            || instance_parameters.minimum_distance_1_cuts != 0
            || instance_parameters.maximum_distance_1_cuts != -1
            || instance_parameters.minimum_distance_2_cuts != 0
            || instance_parameters.maximum_distance_2_cuts != -1
            || instance_parameters.maximum_number_1_cuts != -1
            || instance_parameters.maximum_number_2_cuts != -1) {
        return false;
    }
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        if (!bin_type.fixed_items.empty() || !bin_type.resources.empty())
            return false;
    }
    const Length cut_thickness = instance_parameters.cut_thickness;
    const bool rows = (first_cut_orientation == CutOrientation::Horizontal);

    // The part of a bin where the items can be packed, along 'u', across the
    // strips, and along 'v', along the strips: from the end of the left and
    // bottom trims, to the start of the right and top trims. 'u_node_start'
    // and 'v_node_start' are the starts of the root node of the bin, after
    // its hard trims only.
    struct BinSpace
    {
        Length u_node_start;
        Length u_start;
        Length u_end;
        Length v_node_start;
        Length v_start;
        Length v_end;
    };
    auto bin_space = [&instance, rows](const BinType& bin_type)
    {
        Length x_node_start = (bin_type.left_trim_type == TrimType::Hard)? bin_type.left_trim: 0;
        Length y_node_start = (bin_type.bottom_trim_type == TrimType::Hard)? bin_type.bottom_trim: 0;
        Length x_start = instance.x_start(bin_type);
        Length y_start = instance.y_start(bin_type);
        Length x_end = bin_type.rect.w - bin_type.right_trim;
        Length y_end = bin_type.rect.h - bin_type.top_trim;
        return (!rows)?
            BinSpace{x_node_start, x_start, x_end, y_node_start, y_start, y_end}:
            BinSpace{y_node_start, y_start, y_end, x_node_start, x_start, x_end};
    };

    // The length of the longest bin along the strips.
    Length v_length_max = 0;
    for (BinPos bin_pos = 0; bin_pos < instance.number_of_bins(); ++bin_pos) {
        BinSpace space = bin_space(instance.bin_type(instance.bin_type_id(bin_pos)));
        v_length_max = (std::max)(v_length_max, space.v_end - space.v_start);
    }

    // The items, with their orientation.
    struct PackedItem
    {
        ItemTypeId item_type_id;
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
        item.u_length = (!rows)? item_type.rect.w: item_type.rect.h;
        item.v_length = (!rows)? item_type.rect.h: item_type.rect.w;
        bool smaller_side_across = (item.u_length <= item.v_length);
        bool fits = (item.v_length <= v_length_max);
        if (!item_type.oriented
                && (!smaller_side_across || !fits)
                && item.u_length <= v_length_max
                && (!fits || item.v_length <= item.u_length)) {
            std::swap(item.u_length, item.v_length);
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

    // The strips of each bin.
    struct StripItem
    {
        ItemTypeId item_type_id;
        Length u_length;
        Length v_start;
        Length v_length;
    };
    struct Strip
    {
        Length u_start;
        Length u_length;
        std::vector<StripItem> items;
    };
    std::vector<std::vector<Strip>> bins;
    BinSpace space;
    auto add_bin = [&]()
    {
        if ((BinPos)bins.size() == instance.number_of_bins())
            return false;
        bins.push_back({});
        space = bin_space(instance.bin_type(instance.bin_type_id(bins.size() - 1)));
        return true;
    };
    // Return the start of the next item of a strip.
    auto strip_v_next = [&space, cut_thickness](const Strip& strip)
    {
        return (strip.items.empty())?
            space.v_start:
            strip.items.back().v_start + strip.items.back().v_length + cut_thickness;
    };

    if (!add_bin())
        return false;
    for (const PackedItem& item: items) {
        Strip* strip = nullptr;
        for (Strip& strip_cur: bins.back()) {
            if (item.u_length <= strip_cur.u_length
                    && strip_v_next(strip_cur) + item.v_length <= space.v_end) {
                strip = &strip_cur;
                break;
            }
        }
        if (strip == nullptr) {
            for (;;) {
                Length u_start = (bins.back().empty())?
                    space.u_start:
                    bins.back().back().u_start + bins.back().back().u_length + cut_thickness;
                if (u_start + item.u_length <= space.u_end
                        && space.v_start + item.v_length <= space.v_end) {
                    bins.back().push_back({u_start, item.u_length, {}});
                    strip = &bins.back().back();
                    break;
                }
                if (!add_bin())
                    return false;
            }
        }
        strip->items.push_back({item.item_type_id, item.u_length, strip_v_next(*strip), item.v_length});
    }

    // Build the solution.
    SolutionBuilder solution_builder(instance);
    for (BinPos bin_pos = 0; bin_pos < (BinPos)bins.size(); ++bin_pos) {
        BinTypeId bin_type_id = instance.bin_type_id(bin_pos);
        BinSpace bin_space_cur = bin_space(instance.bin_type(bin_type_id));
        solution_builder.add_bin(bin_type_id, 1, first_cut_orientation);
        for (Counter strip_pos = 0; strip_pos < (Counter)bins[bin_pos].size(); ++strip_pos) {
            const Strip& strip = bins[bin_pos][strip_pos];
            // The waste of a soft left (bottom) trim.
            if (strip_pos == 0 && strip.u_start > bin_space_cur.u_node_start)
                solution_builder.add_node(1, strip.u_start - cut_thickness);
            solution_builder.add_node(1, strip.u_start + strip.u_length);
            for (Counter item_pos = 0; item_pos < (Counter)strip.items.size(); ++item_pos) {
                const StripItem& item = strip.items[item_pos];
                if (item_pos == 0 && item.v_start > bin_space_cur.v_node_start)
                    solution_builder.add_node(2, item.v_start - cut_thickness);
                solution_builder.add_node(2, item.v_start + item.v_length);
                if (item.u_length < strip.u_length)
                    solution_builder.add_node(3, strip.u_start + item.u_length);
                solution_builder.set_last_node_item(item.item_type_id);
            }
        }
    }
    solution = solution_builder.build();
    return solution.full() && solution.feasible();
}

}

SequentialFeasibilityOutput packingsolver::rectangleguillotine::sequential_feasibility(
        const Instance& instance,
        const SequentialFeasibilitySolver& solver,
        const SequentialFeasibilityParameters& parameters)
{
    SequentialFeasibilityOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    BinTypeId bin_type_id = instance.bin_type_id(0);
    const BinType& bin_type = instance.bin_type(bin_type_id);

    // Without the strip packing, the first iteration uses all the bins, at
    // their full size.
    BinPos current_number_of_bins = instance.number_of_bins();
    Length current_x = instance.bin_type(instance.bin_type_id(current_number_of_bins - 1)).rect.w;
    Length current_y = bin_type.rect.h;
    if (instance.objective() == Objective::OpenDimensionX)
        current_x = bin_type.rect.w;

    // Update the bin count and/or the lengths from a solution found,
    // shrinking them. Return false if there is nothing left to try.
    auto update_targets = [&instance, &bin_type, &current_number_of_bins, &current_x, &current_y](
            const Solution& solution)
    {
        if (instance.objective() == Objective::BinPacking) {
            current_number_of_bins = solution.number_of_bins() - 1;
            if (current_number_of_bins == 0)
                return false;
        } else if (instance.objective() == Objective::BinPackingWithLeftovers) {
            current_x = std::min(
                    (Length)(0.99 * solution.width()),
                    solution.width() - 1);
            if (solution.number_of_bins() < current_number_of_bins) {
                current_number_of_bins = solution.number_of_bins();
                BinTypeId bin_type_id_new = instance.bin_type_id(current_number_of_bins - 1);
                const BinType& bin_type_new = instance.bin_type(bin_type_id_new);
                current_x = bin_type_new.rect.w;
            } else if (current_x <= instance.bin_type(
                        instance.bin_type_id(current_number_of_bins - 1)).left_trim) {
                current_number_of_bins--;
                if (current_number_of_bins == 0)
                    return false;
                BinTypeId bin_type_id_new = instance.bin_type_id(current_number_of_bins - 1);
                const BinType& bin_type_new = instance.bin_type(bin_type_id_new);
                current_x = bin_type_new.rect.w;
            }
        } else if (instance.objective() == Objective::OpenDimensionX) {
            current_x = std::min(
                    (Length)(0.99 * solution.width()),
                    solution.width() - 1);
            if (current_x <= bin_type.left_trim)
                return false;
        } else {  // OpenDimensionY
            current_y = std::min(
                    (Length)(0.99 * solution.height()),
                    solution.height() - 1);
            if (current_y <= bin_type.bottom_trim)
                return false;
        }
        return true;
    };

    // A packing of the items in strips: a first solution, from which the
    // targets of the first iteration are shrunk.
    bool stop = false;
    Solution strip_packing(instance);
    if (pack_strips(instance, strip_packing)) {
        algorithm_formatter.update_solution(strip_packing, "SF strips");
        current_number_of_bins = strip_packing.number_of_bins();
        stop = !update_targets(strip_packing);
    }

    for (Counter it = 0; !stop; ++it) {
        if (algorithm_formatter.end_boolean())
            break;
        if (parameters.timer.needs_to_end())
            break;

        // Build the Feasibility sub-instance.
        InstanceBuilder sub_instance_builder;
        sub_instance_builder.set_objective(Objective::Feasibility);
        sub_instance_builder.set_parameters(instance.parameters());
        sub_instance_builder.set_original_instance(instance);
        std::vector<BinTypeId> sub_to_orig_bin_type_ids;
        if (instance.objective() == Objective::BinPacking) {
            BinPos remaining_bins = current_number_of_bins;
            for (BinTypeId bin_type_id_iter = 0;
                    bin_type_id_iter < instance.number_of_bin_types() && remaining_bins > 0;
                    ++bin_type_id_iter) {
                const BinType& bin_type_iter = instance.bin_type(bin_type_id_iter);
                BinPos copies = std::min(bin_type_iter.copies, remaining_bins);
                BinTypeId sub_bin_type_id = sub_instance_builder.add_bin_type_from_original_instance(bin_type_id_iter);
                sub_instance_builder.set_bin_type_copies_min(sub_bin_type_id, 0);
                sub_instance_builder.set_bin_type_copies(sub_bin_type_id, copies);
                sub_to_orig_bin_type_ids.push_back(bin_type_id_iter);
                remaining_bins -= copies;
            }
        } else if (instance.objective() == Objective::BinPackingWithLeftovers) {
            BinPos remaining_bins = current_number_of_bins;
            for (BinTypeId bin_type_id_iter = 0;
                    bin_type_id_iter < instance.number_of_bin_types() && remaining_bins > 0;
                    ++bin_type_id_iter) {
                const BinType& bin_type_iter = instance.bin_type(bin_type_id_iter);
                BinPos copies = std::min(bin_type_iter.copies, remaining_bins);
                BinTypeId sub_bin_type_id = sub_instance_builder.add_bin_type_from_original_instance(bin_type_id_iter);
                sub_instance_builder.set_bin_type_copies_min(sub_bin_type_id, 0);
                sub_instance_builder.set_bin_type_copies(sub_bin_type_id, copies);
                sub_instance_builder.crop_bin_type(sub_bin_type_id, current_x, bin_type_iter.rect.h);
                sub_to_orig_bin_type_ids.push_back(bin_type_id_iter);
                remaining_bins -= copies;
            }
        } else if (instance.objective() == Objective::OpenDimensionX) {
            BinTypeId sub_bin_type_id = sub_instance_builder.add_bin_type_from_original_instance(bin_type_id);
            sub_instance_builder.set_bin_type_copies_min(sub_bin_type_id, 0);
            sub_instance_builder.set_bin_type_copies(sub_bin_type_id, 1);
            sub_instance_builder.crop_bin_type(sub_bin_type_id, current_x, bin_type.rect.h);
        } else {  // OpenDimensionY
            BinTypeId sub_bin_type_id = sub_instance_builder.add_bin_type_from_original_instance(bin_type_id);
            sub_instance_builder.set_bin_type_copies_min(sub_bin_type_id, 0);
            sub_instance_builder.set_bin_type_copies(sub_bin_type_id, 1);
            sub_instance_builder.crop_bin_type(sub_bin_type_id, bin_type.rect.w, current_y);
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
        if (!sub_solution_pool.best().feasible())
            break;

        // Transfer the sub-solution to the main instance.
        Solution solution(instance);
        if (instance.objective() == Objective::BinPacking
                || instance.objective() == Objective::BinPackingWithLeftovers) {
            solution.append_bins(
                    sub_solution_pool.best(),
                    sub_to_orig_bin_type_ids,
                    {});
        } else {
            solution.append_bin(
                    sub_solution_pool.best(),
                    0,  // bin_pos
                    1,  // copies
                    {0});  // bin_type_ids
        }
        // Check feasibility.
        if (!solution.item_copies_feasible()) {
            throw std::logic_error(
                    FUNC_SIGNATURE + ": solution doesn't satisfy item copies.");
        }

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
