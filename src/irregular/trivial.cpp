#include "irregular/trivial.hpp"

#include "packingsolver/irregular/algorithm_formatter.hpp"

#include "shape/intersection_tree.hpp"

using namespace packingsolver;
using namespace packingsolver::irregular;

TrivialSingleItemOutput packingsolver::irregular::trivial_single_item(
        const Instance& instance,
        const TrivialSingleItemParameters& parameters)
{
    TrivialSingleItemOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    if (instance.number_of_bins() != 1 || instance.number_of_items() != 1) {
        algorithm_formatter.end();
        return output;
    }

    const LengthDbl scale = instance.parameters().scale_value;
    BinTypeId bin_type_id = instance.bin_type_id(0);
    const BinType& bin_type = instance.bin_type(bin_type_id);
    const ItemType& item_type = instance.item_type(0);

    // Use the first allowed rotation and no mirroring.
    Angle angle = item_type.allowed_rotations[0].start_angle;
    bool mirror = false;

    // Build IntersectionTree from bin borders and defects (inflated shapes).
    std::vector<ShapeWithHoles> tree_shapes;
    for (const Defect& border : bin_type.borders)
        tree_shapes.push_back(border.shape_inflated);
    for (const Defect& defect : bin_type.defects)
        tree_shapes.push_back(defect.shape_inflated);
    shape::IntersectionTree tree(tree_shapes, {}, {});

    // Bin center in scaled coordinates.
    LengthDbl bin_cx_scaled = (bin_type.aabb_scaled.x_min + bin_type.aabb_scaled.x_max) / 2.0;
    LengthDbl bin_cy_scaled = (bin_type.aabb_scaled.y_min + bin_type.aabb_scaled.y_max) / 2.0;

    // Compute item AABB in scaled coords after rotate.
    AxisAlignedBoundingBox aabb = item_type.compute_min_max(angle, mirror, 1);
    LengthDbl item_cx_scaled = (aabb.x_min + aabb.x_max) / 2.0;
    LengthDbl item_cy_scaled = (aabb.y_min + aabb.y_max) / 2.0;

    // Bottom-left corner shift in original coords.
    Point bl_corner;
    bl_corner.x = (bin_cx_scaled - item_cx_scaled) / scale;
    bl_corner.y = (bin_cy_scaled - item_cy_scaled) / scale;

    LengthDbl dx = bl_corner.x * scale;
    LengthDbl dy = bl_corner.y * scale;

    // Check that the item AABB fits within the bin AABB.
    if (shape::strictly_lesser(dx + aabb.x_min, bin_type.aabb_scaled.x_min)
            || shape::strictly_greater(dx + aabb.x_max, bin_type.aabb_scaled.x_max)
            || shape::strictly_lesser(dy + aabb.y_min, bin_type.aabb_scaled.y_min)
            || shape::strictly_greater(dy + aabb.y_max, bin_type.aabb_scaled.y_max)) {
        algorithm_formatter.end();
        return output;
    }

    // Check intersection for each shape part.
    bool fits = true;
    for (const ItemShape& item_shape : item_type.shapes) {
        ShapeWithHoles transformed = item_shape.shape_scaled;
        transformed = transformed.rotate(angle);
        transformed = transformed.shift(dx, dy);
        auto result = tree.intersect(transformed, false);
        if (!result.shape_ids.empty()) {
            fits = false;
            break;
        }
    }

    if (fits) {
        Solution solution(instance);
        BinPos bin_pos = solution.add_bin(bin_type_id, 1);
        solution.add_item(bin_pos, 0, bl_corner, angle, mirror);
        algorithm_formatter.update_solution(solution, "trivial");
    }

    algorithm_formatter.end();
    return output;
}
