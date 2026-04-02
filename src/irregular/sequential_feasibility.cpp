#include "irregular/sequential_feasibility.hpp"

#include "packingsolver/irregular/algorithm_formatter.hpp"
#include "packingsolver/irregular/instance_builder.hpp"
#include "packingsolver/irregular/optimize.hpp"

#include "shape/shape.hpp"

#include <cmath>
#include <sstream>

using namespace packingsolver;
using namespace packingsolver::irregular;

SequentialFeasibilityOutput packingsolver::irregular::sequential_feasibility(
        const Instance& instance,
        const SequentialFeasibilityParameters& parameters)
{
    SequentialFeasibilityOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    AreaDbl a = 0;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        AxisAlignedBoundingBox aabb = item_type.compute_min_max();
        LengthDbl dx = aabb.x_max - aabb.x_min;
        LengthDbl dy = aabb.y_max - aabb.y_min;
        a += dx * dy * item_type.copies;
    }
    LengthDbl x = std::sqrt(a / instance.parameters().open_dimension_xy_aspect_ratio);

    for (Counter it = 0;; ++it) {
        if (algorithm_formatter.end_boolean())
            break;
        if (parameters.timer.needs_to_end())
            break;

        LengthDbl y = x * instance.parameters().open_dimension_xy_aspect_ratio;

        // Build an instance with a single bin with dimensions (x, y).
        InstanceBuilder sub_instance_builder;
        sub_instance_builder.set_objective(Objective::BinPacking);
        sub_instance_builder.set_parameters(instance.parameters());
        sub_instance_builder.add_bin_type(shape::build_rectangle(x, y));
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = instance.item_type(item_type_id);
            sub_instance_builder.add_item_type(
                    item_type,
                    item_type.profit,
                    item_type.copies);
        }
        Instance sub_instance = sub_instance_builder.build();

        // Solve the instance.
        OptimizeParameters sub_parameters;
        sub_parameters.verbosity_level = 0;
        sub_parameters.timer = parameters.timer;
        sub_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
        sub_parameters.optimization_mode = parameters.optimization_mode;
        sub_parameters.not_anytime_maximum_approximation_ratio
            = parameters.not_anytime_maximum_approximation_ratio;
        sub_parameters.not_anytime_tree_search_queue_size
            = parameters.not_anytime_tree_search_queue_size;
        sub_parameters.linear_programming_solver_name = parameters.linear_programming_solver_name;
        auto sub_output = optimize(sub_instance, sub_parameters);

        // If no solution has been found, break.
        if (!sub_output.solution_pool.best().full())
            break;
        Solution solution(instance);
        solution.append(
                sub_output.solution_pool.best(),
                0,  // bin_pos
                1,  // copies
                {0});  // bin_types_ids
        std::stringstream ss;
        ss << "SF it " << it;
        algorithm_formatter.update_solution(solution, ss.str());

        x = 0.99 * (std::max)(
                solution.x_max() - solution.x_min(),
                solution.y_max() - solution.y_min());
        AreaDbl a_cur = (solution.x_max() - solution.x_min()) * (solution.y_max() - solution.y_min());
        LengthDbl x_cur = std::sqrt(a_cur / instance.parameters().open_dimension_xy_aspect_ratio);
        if (x > x_cur)
            x = x_cur;
    }

    algorithm_formatter.end();
    return output;
}
