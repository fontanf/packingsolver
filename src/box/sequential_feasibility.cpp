#include "box/sequential_feasibility.hpp"

#include "packingsolver/box/algorithm_formatter.hpp"
#include "packingsolver/box/instance_builder.hpp"

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
 * Return the number of bins of the first bin types whose volume is at least
 * twice the item volume.
 */
BinPos initial_number_of_bins(
        const Instance& instance)
{
    Volume total_item_volume = instance.item_volume();
    Volume total_bin_volume = 0;
    BinPos number_of_bins = 0;
    for (BinPos bin_pos = 0; bin_pos < instance.number_of_bins(); ++bin_pos) {
        if (total_bin_volume >= 2 * total_item_volume)
            break;
        const BinType& bin_type = instance.bin_type(instance.bin_type_id(bin_pos));
        total_bin_volume += bin_type.volume();
        number_of_bins++;
    }
    return (std::max)(number_of_bins, (BinPos)1);
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

    BinPos current_number_of_bins = 0;
    // Length of the last bin along the shrunk dimension.
    Length current_length = 0;
    if (instance.objective() == Objective::BinPacking) {
        current_number_of_bins = initial_number_of_bins(instance);
    } else if (instance.objective() == Objective::BinPackingWithLeftovers) {
        current_number_of_bins = initial_number_of_bins(instance);
        const BinType& last_bin_type = instance.bin_type(
                instance.bin_type_id(current_number_of_bins - 1));
        current_length = length(last_bin_type.box, dimension);
    } else {  // OpenDimensionX, OpenDimensionY, OpenDimensionZ
        Area section_area = bin_type.volume() / length(bin_type.box, dimension);
        current_length = std::min(
                (2 * instance.item_volume() + section_area - 1) / section_area,
                length(bin_type.box, dimension));
    }

    for (Counter it = 0;; ++it) {
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
                sub_instance_builder.set_bin_type_box(sub_bin_type_id, box.x, box.y, box.z);
                sub_to_orig_bin_type_ids.push_back(bin_type_id);
            }
        } else {
            BinTypeId sub_bin_type_id = sub_instance_builder.add_bin_type_from_original_instance(instance.bin_type_id(0));
            sub_instance_builder.set_bin_type_copies(sub_bin_type_id, 1);
            sub_instance_builder.set_bin_type_copies_min(sub_bin_type_id, 0);
            Box box = shrink(bin_type.box, dimension, current_length);
            sub_instance_builder.set_bin_type_box(sub_bin_type_id, box.x, box.y, box.z);
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
        if (instance.objective() == Objective::BinPacking) {
            current_number_of_bins = solution.number_of_bins() - 1;
            if (current_number_of_bins == 0)
                break;
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
                    break;
                const BinType& bin_type_new = instance.bin_type(
                        instance.bin_type_id(current_number_of_bins - 1));
                current_length = length(bin_type_new.box, dimension);
            }
        } else {  // OpenDimensionX, OpenDimensionY, OpenDimensionZ
            Length length_used = length(solution, dimension);
            current_length = std::min((Length)(0.99 * length_used), length_used - 1);
            if (current_length <= 0)
                break;
        }
    }

    algorithm_formatter.end();
    return output;
}
