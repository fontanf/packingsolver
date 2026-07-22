#include "irregular/sequential_feasibility.hpp"

#include "packingsolver/irregular/algorithm_formatter.hpp"
#include "packingsolver/irregular/instance_builder.hpp"

#include "shape/shape.hpp"
#include "shape/boolean_operations.hpp"

#include <cmath>
#include <sstream>

using namespace packingsolver;
using namespace packingsolver::irregular;

SequentialFeasibilityOutput packingsolver::irregular::sequential_feasibility(
        const Instance& instance,
        const SequentialFeasibilitySolver& solver,
        const SequentialFeasibilityParameters& parameters)
{
    SequentialFeasibilityOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    // Compute total item AABB area to derive the initial bin size.
    AreaDbl total_item_aabb_area = 0;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        AxisAlignedBoundingBox aabb = item_type.compute_min_max();
        LengthDbl dx = aabb.x_max - aabb.x_min;
        LengthDbl dy = aabb.y_max - aabb.y_min;
        total_item_aabb_area += dx * dy * item_type.copies;
    }

    // Initialize the open dimension variable(s) and/or bin count.
    BinPos current_number_of_bins = 0;
    LengthDbl x = 0;
    LengthDbl y = 0;
    if (instance.objective() == Objective::BinPacking) {
        AreaDbl total_bin_aabb_area = 0;
        for (BinTypeId bin_type_id = 0;
                bin_type_id < instance.number_of_bin_types();
                ++bin_type_id) {
            const BinType& bin_type = instance.bin_type(bin_type_id);
            LengthDbl bx = bin_type.aabb_scaled.x_max - bin_type.aabb_scaled.x_min;
            LengthDbl by = bin_type.aabb_scaled.y_max - bin_type.aabb_scaled.y_min;
            AreaDbl bin_aabb_area = bx * by;
            AreaDbl remaining = 2.0 * total_item_aabb_area - total_bin_aabb_area;
            BinPos copies_needed = (BinPos)std::ceil(remaining / bin_aabb_area);
            BinPos copies_used = std::min(copies_needed, bin_type.copies);
            total_bin_aabb_area += copies_used * bin_aabb_area;
            current_number_of_bins += copies_used;
            if (total_bin_aabb_area >= 2.0 * total_item_aabb_area)
                break;
        }
    } else if (instance.objective() == Objective::BinPackingWithLeftovers) {
        AreaDbl total_bin_aabb_area = 0;
        for (BinTypeId bin_type_id = 0;
                bin_type_id < instance.number_of_bin_types();
                ++bin_type_id) {
            const BinType& bin_type = instance.bin_type(bin_type_id);
            LengthDbl bx = bin_type.aabb_scaled.x_max - bin_type.aabb_scaled.x_min;
            LengthDbl by = bin_type.aabb_scaled.y_max - bin_type.aabb_scaled.y_min;
            AreaDbl bin_aabb_area = bx * by;
            AreaDbl remaining = 2.0 * total_item_aabb_area - total_bin_aabb_area;
            BinPos copies_needed = (BinPos)std::ceil(remaining / bin_aabb_area);
            BinPos copies_used = std::min(copies_needed, bin_type.copies);
            total_bin_aabb_area += copies_used * bin_aabb_area;
            current_number_of_bins += copies_used;
            if (total_bin_aabb_area >= 2.0 * total_item_aabb_area)
                break;
        }
        BinTypeId last_bin_type_id = instance.bin_type_id(current_number_of_bins - 1);
        const BinType& last_bin_type = instance.bin_type(last_bin_type_id);
        x = last_bin_type.aabb_scaled.x_max - last_bin_type.aabb_scaled.x_min;
    } else if (instance.objective() == Objective::OpenDimensionX) {
        const BinType& bin_type = instance.bin_type(instance.bin_type_id(0));
        y = bin_type.aabb_scaled.y_max - bin_type.aabb_scaled.y_min;
        x = 2 * total_item_aabb_area / y;
    } else if (instance.objective() == Objective::OpenDimensionY) {
        const BinType& bin_type = instance.bin_type(instance.bin_type_id(0));
        x = bin_type.aabb_scaled.x_max - bin_type.aabb_scaled.x_min;
        y = 2 * total_item_aabb_area / x;
    } else {  // OpenDimensionXY
        x = std::sqrt(total_item_aabb_area / instance.parameters().open_dimension_xy_aspect_ratio);
        y = x * instance.parameters().open_dimension_xy_aspect_ratio;
    }

    for (Counter it = 0;; ++it) {
        if (algorithm_formatter.end_boolean())
            break;
        if (parameters.timer.needs_to_end())
            break;

        // Build the Feasibility sub-instance.
        InstanceBuilder sub_instance_builder;
        sub_instance_builder.set_objective(Objective::Feasibility);
        sub_instance_builder.set_parameters(instance.parameters());
        std::vector<BinTypeId> sub_to_orig_bin_type_ids;
        if (instance.objective() == Objective::BinPacking) {
            // Add bin types from the original instance up to current_number_of_bins.
            BinPos remaining_bins = current_number_of_bins;
            for (BinTypeId bin_type_id = 0;
                    bin_type_id < instance.number_of_bin_types() && remaining_bins > 0;
                    ++bin_type_id) {
                const BinType& bin_type = instance.bin_type(bin_type_id);
                BinPos copies = std::min(bin_type.copies, remaining_bins);
                BinTypeId sub_bin_type_id = sub_instance_builder.add_bin_type(instance, bin_type_id);
                sub_instance_builder.set_bin_type_copies(sub_bin_type_id, copies);
                sub_instance_builder.set_bin_type_copies_min(sub_bin_type_id, 0);
                sub_to_orig_bin_type_ids.push_back(bin_type_id);
                remaining_bins -= copies;
            }
        } else if (instance.objective() == Objective::BinPackingWithLeftovers) {
            // Like BinPacking but each bin type is x-restricted (like OpenDimensionX).
            BinPos remaining_bins = current_number_of_bins;
            for (BinTypeId bin_type_id = 0;
                    bin_type_id < instance.number_of_bin_types() && remaining_bins > 0;
                    ++bin_type_id) {
                const BinType& bin_type = instance.bin_type(bin_type_id);
                BinPos copies = std::min(bin_type.copies, remaining_bins);
                AxisAlignedBoundingBox restricting_aabb = bin_type.aabb_scaled;
                restricting_aabb.x_max = restricting_aabb.x_min + x;
                const Shape restricting_rect = shape::build_rectangle(restricting_aabb);
                const shape::MultiShapeWithHoles intersection = shape::compute_intersection(
                        {{bin_type.shape_orig},
                        {restricting_rect}});
                BinTypeId sub_bin_type_id = sub_instance_builder.add_bin_type(
                        intersection.shapes_with_holes[0].shape);
                sub_instance_builder.set_bin_type_cost(sub_bin_type_id, bin_type.cost);
                sub_instance_builder.set_bin_type_copies(sub_bin_type_id, copies);
                sub_instance_builder.set_item_bin_minimum_spacing(
                        sub_bin_type_id, bin_type.item_bin_minimum_spacing);
                sub_to_orig_bin_type_ids.push_back(bin_type_id);
                remaining_bins -= copies;
            }
        } else {
            // Build a single bin as the intersection of the original bin with a
            // rectangle restricted to the current open dimension estimate.
            const BinType& original_bin_type = instance.bin_type(instance.bin_type_id(0));
            AxisAlignedBoundingBox restricting_aabb = original_bin_type.aabb_scaled;
            restricting_aabb.x_max = restricting_aabb.x_min + x;
            restricting_aabb.y_max = restricting_aabb.y_min + y;
            const Shape restricting_rect = shape::build_rectangle(restricting_aabb);
            const shape::MultiShapeWithHoles intersection = shape::compute_intersection(
                    {{original_bin_type.shape_orig},
                    {restricting_rect}});
            sub_instance_builder.add_bin_type(intersection.shapes_with_holes[0].shape);
            sub_instance_builder.set_item_bin_minimum_spacing(
                    0,
                    instance.bin_type(0).item_bin_minimum_spacing);
        }
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            sub_instance_builder.add_item_type(instance, item_type_id);
        }
        Instance sub_instance = sub_instance_builder.build();

        // Solve the sub-instance.
        SolutionPool<Instance, Solution> sub_solution_pool = solver(sub_instance);

        // If no feasible solution found, stop.
        if (!sub_solution_pool.best().full())
            break;

        // Transfer the sub-solution to the main instance.
        Solution solution(instance);
        if (instance.objective() == Objective::BinPacking
                || instance.objective() == Objective::BinPackingWithLeftovers) {
            solution.append(
                    sub_solution_pool.best(),
                    sub_to_orig_bin_type_ids,
                    {});
        } else {
            solution.append(
                    sub_solution_pool.best(),
                    0,  // bin_pos
                    1,  // copies
                    {0});  // bin_type_ids
        }
        std::stringstream ss;
        ss << "SF it " << it << " " << sub_solution_pool.best_label();
        algorithm_formatter.update_solution(solution, ss.str());

        // Update for the next iteration.
        if (instance.objective() == Objective::BinPacking) {
            current_number_of_bins = solution.number_of_bins() - 1;
            if (current_number_of_bins == 0)
                break;
        } else if (instance.objective() == Objective::BinPackingWithLeftovers) {
            BinTypeId bin_type_id_last = instance.bin_type_id(solution.number_of_bins() - 1);
            const BinType& bin_type_last = instance.bin_type(bin_type_id_last);
            x = 0.99 * (solution.x_max() - bin_type_last.aabb_scaled.x_min)
                + bin_type_last.item_bin_minimum_spacing;
            if (solution.number_of_bins() < current_number_of_bins) {
                current_number_of_bins = solution.number_of_bins();
                BinTypeId bin_type_id_new = instance.bin_type_id(current_number_of_bins - 1);
                const BinType& bin_type_new = instance.bin_type(bin_type_id_new);
                x = bin_type_new.aabb_scaled.x_max;
            } else if (!strictly_greater(x, bin_type_last.aabb_scaled.x_min + bin_type_last.item_bin_minimum_spacing)) {
                current_number_of_bins = solution.number_of_bins() - 1;
                if (current_number_of_bins == 0)
                    break;
                BinTypeId bin_type_id_new = instance.bin_type_id(current_number_of_bins - 1);
                const BinType& bin_type_new = instance.bin_type(bin_type_id_new);
                x = bin_type_new.aabb_scaled.x_max;
            }
        } else if (instance.objective() == Objective::OpenDimensionX) {
            const BinType& bin_type = instance.bin_type(instance.bin_type_id(0));
            x = 0.99 * (solution.x_max() - bin_type.aabb_scaled.x_min)
                + bin_type.item_bin_minimum_spacing;
        } else if (instance.objective() == Objective::OpenDimensionY) {
            const BinType& bin_type = instance.bin_type(instance.bin_type_id(0));
            y = 0.99 * (solution.y_max() - bin_type.aabb_scaled.y_min)
                + bin_type.item_bin_minimum_spacing;
        } else {  // OpenDimensionXY
            const BinType& bin_type = instance.bin_type(instance.bin_type_id(0));
            x = 0.99 * (std::max)(
                    solution.x_max() - bin_type.aabb_scaled.x_min,
                    solution.y_max() - bin_type.aabb_scaled.y_min)
                + bin_type.item_bin_minimum_spacing;
            AreaDbl a_cur = (solution.x_max() - solution.x_min()) * (solution.y_max() - solution.y_min());
            LengthDbl x_cur = std::sqrt(a_cur / instance.parameters().open_dimension_xy_aspect_ratio);
            if (x > x_cur)
                x = x_cur;
            y = x * instance.parameters().open_dimension_xy_aspect_ratio;
        }
    }

    algorithm_formatter.end();
    return output;
}
