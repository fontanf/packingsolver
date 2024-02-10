#include "packingsolver/irregular/irregular_to_rectangle.hpp"

#include "packingsolver/irregular/algorithm_formatter.hpp"

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/convex_hull_2.h>
#include <CGAL/min_quadrilateral_2.h>

using namespace packingsolver;
using namespace packingsolver::irregular;

using Kernel = CGAL::Exact_predicates_inexact_constructions_kernel;

const IrregularToRectangleOutput irregular::irregular_to_rectangle(
        const Instance& instance,
        const IrregularToRectangleParameters& parameters)
{
    IrregularToRectangleOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    // Build rectangle instance.
    rectangle::InstanceBuilder rectangle_instance_builder;
    rectangle_instance_builder.set_objective(instance.objective());

    // Add bin types.
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);

        // Check if the shape of the bin type is of type 'Rectangle'.
        if (!bin_type.shape.is_rectangle()) {
            throw std::invalid_argument(
                    "IrregularToRectangle algorithm requires bin types to"
                    " have a shape of type 'Rectangle'.");
        }

        LengthDbl width = bin_type.shape.elements[0].length();
        LengthDbl height = bin_type.shape.elements[1].length();
        rectangle_instance_builder.add_bin_type(
                (Length)std::ceil(width * 1),
                (Length)std::ceil(height * 1),
                bin_type.cost,
                bin_type.copies,
                bin_type.copies_min);
    }

    // Add item types.
    std::vector<Angle> item_type_angles(instance.number_of_item_types(), 0.0);
    std::vector<LengthDbl> item_type_xs_min(instance.number_of_item_types(), 0.0);
    std::vector<LengthDbl> item_type_ys_min(instance.number_of_item_types(), 0.0);
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);

        LengthDbl width = -1;
        LengthDbl height = -1;
        bool oriented = true;

        if (item_type.has_only_discrete_rotations()) {
            oriented = true;
            for (const auto& angle: item_type.allowed_rotations) {
                auto points = item_type.compute_min_max(angle.first);
                LengthDbl width_cur = points.second.x - points.first.x;
                LengthDbl height_cur = points.second.y - points.first.y;
                if (width == -1
                        || width * height > width_cur * height_cur) {
                    width = width_cur;
                    height = height_cur;
                    item_type_angles[item_type_id] = angle.first;
                    item_type_xs_min[item_type_id] = points.first.x;
                    item_type_ys_min[item_type_id] = points.first.y;
                }
            }

        } else if (item_type.has_full_continuous_rotations()) {
            oriented = false;

            // Compute the convex hull.
            std::vector<Kernel::Point_2> cgal_points;
            for (const ItemShape& shape: item_type.shapes) {
                for (const ShapeElement& element: shape.shape.elements) {
                    if (element.type == ShapeElementType::CircularArc) {
                        throw std::runtime_error("");
                    }
                    cgal_points.push_back(Kernel::Point_2(
                                element.start.x,
                                element.start.y));
                }
            }
            std::vector<Kernel::Point_2> cgal_convex_hull;
            CGAL::convex_hull_2(
                    cgal_points.begin(),
                    cgal_points.end(),
                    std::back_inserter(cgal_convex_hull));

            // Get the minimum area enclosing rectangle.
            std::vector<Kernel::Point_2> cgal_rectangle;
            CGAL::min_rectangle_2(
                    cgal_convex_hull.begin(),
                    cgal_convex_hull.end(),
                    std::back_inserter(cgal_rectangle));

            // Compute the angle.
            Point p1 { cgal_rectangle[0].x(), cgal_rectangle[0].y() };
            Point p2 { cgal_rectangle[1].x(), cgal_rectangle[1].y() };
            Angle angle = irregular::angle(p2 - p1);

            auto points = item_type.compute_min_max(angle);
            width = points.second.x - points.first.x;
            height = points.second.y - points.first.y;
            item_type_angles[item_type_id] = angle;
            item_type_xs_min[item_type_id] = points.first.x;
            item_type_ys_min[item_type_id] = points.first.y;

        } else {
            throw std::invalid_argument(
                    "IrregularToRectangle algorithm requires item types to"
                    " have either only discrete rotations"
                    " or full continous rotations.");
        }

        rectangle_instance_builder.add_item_type(
                (Length)std::ceil(width * 1),
                (Length)std::ceil(height * 1),
                item_type.profit,
                item_type.copies,
                oriented);
    }

    rectangle::Instance rectangle_instance = rectangle_instance_builder.build();

    // Solve onedimensional instance.
    rectangle::OptimizeParameters rectangle_parameters = parameters.rectangle_parameters;

    rectangle_parameters.new_solution_callback = [
            &instance,
            &algorithm_formatter,
            &item_type_angles,
            &item_type_xs_min,
            &item_type_ys_min](
                const packingsolver::Output<rectangle::Instance, rectangle::Solution>& ps_output)
        {
            const rectangle::Output& rectangle_output
                = static_cast<const rectangle::Output&>(ps_output);
            const rectangle::Solution& rectangle_solution
                = rectangle_output.solution_pool.best();
            Solution solution(instance);
            for (BinPos bin_pos = 0;
                    bin_pos < rectangle_solution.number_of_different_bins();
                    ++bin_pos) {
                const rectangle::SolutionBin& rectangle_solution_bin
                    = rectangle_solution.bin(bin_pos);
                solution.add_bin(
                        rectangle_solution_bin.bin_type_id,
                        rectangle_solution_bin.copies);
                for (const rectangle::SolutionItem& rectangle_solution_item: rectangle_solution_bin.items) {
                    ItemTypeId item_type_id = rectangle_solution_item.item_type_id;
                    LengthDbl x = rectangle_solution_item.bl_corner.x / 1
                        - item_type_xs_min[item_type_id];
                    LengthDbl y = rectangle_solution_item.bl_corner.y / 1
                        - item_type_ys_min[item_type_id];
                    solution.add_item(
                            bin_pos,
                            item_type_id,
                            {x, y},
                            item_type_angles[item_type_id]);
                }
            }
            std::stringstream ss;
            algorithm_formatter.update_solution(solution, ss.str());
        };

    rectangle_parameters.verbosity_level = 0;
    rectangle_parameters.timer = parameters.timer;
    auto rectangle_output = rectangle::optimize(
            rectangle_instance,
            rectangle_parameters);

    return output;
}
