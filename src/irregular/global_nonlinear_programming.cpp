#include "irregular/global_nonlinear_programming.hpp"

#include "irregular/solution_builder.hpp"
#include "packingsolver/irregular/algorithm_formatter.hpp"

#include "shape/convex_hull.hpp"
#include "shape/convex_partition.hpp"

#if defined(SCIP_FOUND)
#include "mathoptsolverscmake/mathopt_scip.hpp"
#endif

#include <cmath>
#include <map>
#include <set>

using namespace packingsolver;
using namespace packingsolver::irregular;

#if defined(SCIP_FOUND)

namespace
{

using mathoptsolverscmake::MathOptModel;

const double infinity = std::numeric_limits<double>::infinity();

/** Data of an item, in the normalized coordinates. */
struct ItemData
{
    ItemTypeId item_type_id;

    /** Center of the item, in the original coordinates. */
    Point center;

    /** Convex parts, relatively to the center. */
    std::vector<std::vector<Point>> parts;

    /** Vertices of the convex hulls of the shapes, relatively to the center. */
    std::vector<Point> hull_vertices;
};

/** Data shared by the subsets. */
struct GlobalNonlinearProgrammingData
{
    const Instance* instance = nullptr;

    BinTypeId bin_type_id = 0;

    /** Bottom-left corner of the bounding box of the bin. */
    Point origin;

    /** Scale from the original coordinates to the normalized ones. */
    double scale = 1;

    /**
     * Half-planes of the bin, in the normalized coordinates, shrunk by the
     * item-bin spacing: n_x x + n_y y <= h.
     */
    std::vector<std::pair<Point, double>> bin_half_planes;

    /** Center of the bin, in the normalized coordinates. */
    Point bin_center;

    /** 'true' iff the bin is centrally symmetric. */
    bool bin_centrally_symmetric = false;

    /** Item-item spacing, in the normalized coordinates. */
    double spacing = 0;

    /** Items, the largest ones first, the identical ones consecutive. */
    std::vector<ItemData> items;
};

struct ModelBuilder
{
    MathOptModel model;

    int add_variable(
            double lower_bound,
            double upper_bound,
            mathoptsolverscmake::VariableType type
                = mathoptsolverscmake::VariableType::Continuous)
    {
        model.variables_lower_bounds.push_back(lower_bound);
        model.variables_upper_bounds.push_back(upper_bound);
        model.variables_types.push_back(type);
        model.objective_coefficients.push_back(0);
        return model.variables_lower_bounds.size() - 1;
    }

    /** Start a constraint; its terms are added with 'linear' and 'quadratic'. */
    void add_constraint(
            double lower_bound,
            double upper_bound)
    {
        model.constraints_lower_bounds.push_back(lower_bound);
        model.constraints_upper_bounds.push_back(upper_bound);
        model.constraints_starts.push_back(model.elements_variables.size());
        model.quadratic_elements_constraints_starts.push_back(
                model.quadratic_elements_variables_1.size());
        model.constraints_functions.push_back(nullptr);
    }

    void linear(
            int variable_id,
            double coefficient)
    {
        if (coefficient == 0)
            return;
        model.elements_variables.push_back(variable_id);
        model.elements_coefficients.push_back(coefficient);
    }

    void quadratic(
            int variable_id_1,
            int variable_id_2,
            double coefficient)
    {
        if (coefficient == 0)
            return;
        model.quadratic_elements_variables_1.push_back(variable_id_1);
        model.quadratic_elements_variables_2.push_back(variable_id_2);
        model.quadratic_elements_coefficients.push_back(coefficient);
    }
};

std::vector<Point> vertices(
        const Shape& shape)
{
    std::vector<Point> points;
    for (const ShapeElement& element: shape.elements)
        points.push_back(element.start);
    return points;
}

/** Return 'true' iff the item type can be rotated by any angle. */
bool full_rotations(
        const ItemType& item_type)
{
    for (const AllowedRotation& rotation: item_type.allowed_rotations)
        if (rotation.end_angle - rotation.start_angle >= 360)
            return true;
    return false;
}

GlobalNonlinearProgrammingData build_data(
        const Instance& instance)
{
    GlobalNonlinearProgrammingData data;
    data.instance = &instance;
    while (instance.bin_type(data.bin_type_id).copies == 0)
        data.bin_type_id++;
    const BinType& bin_type = instance.bin_type(data.bin_type_id);
    if (!bin_type.defects.empty() || !bin_type.fixed_items.empty()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": defects and fixed items are not supported.");
    }
    if (!bin_type.shape_orig.is_polygon() || !bin_type.shape_orig.is_convex()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": the bin must be a convex polygon.");
    }

    // Normalization.
    std::vector<Point> bin_vertices = vertices(bin_type.shape_orig);
    AxisAlignedBoundingBox aabb;
    for (const Point& point: bin_vertices) {
        aabb.x_min = (std::min)(aabb.x_min, point.x);
        aabb.x_max = (std::max)(aabb.x_max, point.x);
        aabb.y_min = (std::min)(aabb.y_min, point.y);
        aabb.y_max = (std::max)(aabb.y_max, point.y);
    }
    data.origin = {aabb.x_min, aabb.y_min};
    data.scale = 1.0 / (std::max)(aabb.x_max - aabb.x_min, aabb.y_max - aabb.y_min);
    auto normalize = [&data](const Point& point)
    {
        return Point{
            (point.x - data.origin.x) * data.scale,
            (point.y - data.origin.y) * data.scale};
    };

    // Bin.
    double bin_spacing = bin_type.item_bin_minimum_spacing * data.scale;
    for (size_t pos = 0; pos < bin_vertices.size(); ++pos) {
        Point u = normalize(bin_vertices[pos]);
        Point v = normalize(bin_vertices[(pos + 1) % bin_vertices.size()]);
        double length = std::sqrt((v.x - u.x) * (v.x - u.x) + (v.y - u.y) * (v.y - u.y));
        // Outward normal of an anticlockwise polygon.
        Point normal = {(v.y - u.y) / length, -(v.x - u.x) / length};
        data.bin_half_planes.push_back({
                normal,
                normal.x * u.x + normal.y * u.y - bin_spacing});
    }
    data.bin_center = normalize({
            (aabb.x_min + aabb.x_max) / 2,
            (aabb.y_min + aabb.y_max) / 2});
    data.bin_centrally_symmetric = true;
    for (const Point& point: bin_vertices) {
        Point point_normalized = normalize(point);
        Point opposite = {
            2 * data.bin_center.x - point_normalized.x,
            2 * data.bin_center.y - point_normalized.y};
        bool found = false;
        for (const Point& point_2: bin_vertices)
            if (distance(normalize(point_2), opposite) < 1e-9)
                found = true;
        if (!found)
            data.bin_centrally_symmetric = false;
    }
    data.spacing = instance.parameters().item_item_minimum_spacing * data.scale;

    // Items.
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        for (const AllowedRotation& rotation: item_type.allowed_rotations) {
            if (rotation.mirror) {
                throw std::invalid_argument(
                        FUNC_SIGNATURE + ": mirrored rotations are not supported.");
            }
        }
        ItemData item;
        item.item_type_id = item_type_id;
        Point center = {0, 0};
        Counter number_of_vertices = 0;
        for (const ItemShape& item_shape: item_type.shapes) {
            if (!item_shape.shape_orig.is_polygon()) {
                throw std::invalid_argument(
                        FUNC_SIGNATURE + ": the item shapes must be polygons.");
            }
            for (const ShapeElement& element: item_shape.shape_orig.shape.elements) {
                center.x += element.start.x;
                center.y += element.start.y;
                number_of_vertices++;
            }
        }
        // The average of the vertices is in the convex hull of the item, so
        // in the bin.
        item.center = {center.x / number_of_vertices, center.y / number_of_vertices};
        auto relative = [&data, &item](const Point& point)
        {
            return Point{
                (point.x - item.center.x) * data.scale,
                (point.y - item.center.y) * data.scale};
        };
        for (const ItemShape& item_shape: item_type.shapes) {
            for (const Shape& part: compute_convex_partition(item_shape.shape_orig)) {
                std::vector<Point> part_vertices;
                for (const Point& point: vertices(part))
                    part_vertices.push_back(relative(point));
                item.parts.push_back(part_vertices);
            }
            for (const Point& point: vertices(convex_hull(item_shape.shape_orig.shape)))
                item.hull_vertices.push_back(relative(point));
        }
        for (ItemPos copy = item_type.copies_fixed; copy < item_type.copies; ++copy)
            data.items.push_back(item);
    }
    std::stable_sort(
            data.items.begin(),
            data.items.end(),
            [&instance](const ItemData& item_1, const ItemData& item_2)
            {
                AreaDbl area_1 = instance.item_type(item_1.item_type_id).space();
                AreaDbl area_2 = instance.item_type(item_2.item_type_id).space();
                if (area_1 != area_2)
                    return area_1 > area_2;
                return item_1.item_type_id < item_2.item_type_id;
            });
    return data;
}

enum class SubsetStatus { Feasible, Infeasible, Unknown };

struct SubsetOutput
{
    SubsetStatus status = SubsetStatus::Unknown;

    Counter number_of_nodes = 0;

    /** For each item of the subset, its position and its angle. */
    std::vector<std::pair<Point, Angle>> placements;
};

/** Return the allowed angle of an item type closest to 'angle'. */
Angle allowed_angle(
        const ItemType& item_type,
        Angle angle)
{
    Angle angle_best = angle;
    Angle distance_best = infinity;
    for (const AllowedRotation& rotation: item_type.allowed_rotations) {
        for (int k = -2; k <= 2; ++k) {
            Angle a = angle + 360 * k;
            Angle a_clamped = (std::max)(rotation.start_angle, (std::min)(rotation.end_angle, a));
            if (std::abs(a_clamped - a) < distance_best) {
                distance_best = std::abs(a_clamped - a);
                angle_best = a_clamped;
            }
        }
    }
    return angle_best;
}

SubsetOutput solve_subset(
        const GlobalNonlinearProgrammingData& data,
        const std::vector<ItemPos>& subset,
        double time_limit)
{
    const Instance& instance = *data.instance;
    ItemPos number_of_items = subset.size();
    ModelBuilder model_builder;
    model_builder.model.objective_direction = mathoptsolverscmake::ObjectiveDirection::Minimize;

    std::vector<int> x(number_of_items);
    std::vector<int> y(number_of_items);
    std::vector<int> c(number_of_items);
    std::vector<int> s(number_of_items);
    bool all_full_rotations = true;
    for (ItemPos pos = 0; pos < number_of_items; ++pos) {
        const ItemData& item = data.items[subset[pos]];
        const ItemType& item_type = instance.item_type(item.item_type_id);
        x[pos] = model_builder.add_variable(0, 1);
        y[pos] = model_builder.add_variable(0, 1);
        c[pos] = model_builder.add_variable(-1, 1);
        s[pos] = model_builder.add_variable(-1, 1);

        // c^2 + s^2 = 1.
        model_builder.add_constraint(1, 1);
        model_builder.quadratic(c[pos], c[pos], 1);
        model_builder.quadratic(s[pos], s[pos], 1);

        // Allowed rotations.
        if (!full_rotations(item_type)) {
            all_full_rotations = false;
            std::vector<int> range_variables;
            for (const AllowedRotation& rotation: item_type.allowed_rotations) {
                double width = (rotation.end_angle - rotation.start_angle) * M_PI / 180;
                double middle = (rotation.start_angle + rotation.end_angle) / 2 * M_PI / 180;
                double cos_half_width = std::cos(width / 2);
                if (item_type.allowed_rotations.size() == 1) {
                    model_builder.add_constraint(cos_half_width, infinity);
                    model_builder.linear(c[pos], std::cos(middle));
                    model_builder.linear(s[pos], std::sin(middle));
                } else {
                    // c cos(m) + s sin(m) >= cos(w / 2) - (1 + cos(w / 2)) (1 - z).
                    int z = model_builder.add_variable(
                            0, 1, mathoptsolverscmake::VariableType::Binary);
                    range_variables.push_back(z);
                    model_builder.add_constraint(-1, infinity);
                    model_builder.linear(c[pos], std::cos(middle));
                    model_builder.linear(s[pos], std::sin(middle));
                    model_builder.linear(z, -(1 + cos_half_width));
                }
            }
            if (!range_variables.empty()) {
                model_builder.add_constraint(1, 1);
                for (int z: range_variables)
                    model_builder.linear(z, 1);
            }
        }

        // The item is in the bin.
        for (const Point& v: item.hull_vertices) {
            for (const auto& half_plane: data.bin_half_planes) {
                const Point& normal = half_plane.first;
                // normal . (x + c v_x - s v_y, y + s v_x + c v_y) <= h.
                model_builder.add_constraint(-infinity, half_plane.second);
                model_builder.linear(x[pos], normal.x);
                model_builder.linear(y[pos], normal.y);
                model_builder.linear(c[pos], normal.x * v.x + normal.y * v.y);
                model_builder.linear(s[pos], -normal.x * v.y + normal.y * v.x);
            }
        }
    }

    // Symmetries.
    for (ItemPos pos = 0; pos + 1 < number_of_items; ++pos) {
        if (data.items[subset[pos]].item_type_id
                != data.items[subset[pos + 1]].item_type_id) {
            continue;
        }
        model_builder.add_constraint(-infinity, 0);
        model_builder.linear(x[pos], 1);
        model_builder.linear(x[pos + 1], -1);
    }
    if (data.bin_centrally_symmetric && all_full_rotations) {
        // The first item is the first one of its identical items, and the
        // one with the smallest x-coordinate among them.
        model_builder.add_constraint(-infinity, data.bin_center.x);
        model_builder.linear(x[0], 1);
    }

    // Separating lines.
    for (ItemPos pos_1 = 0; pos_1 < number_of_items; ++pos_1) {
        for (ItemPos pos_2 = pos_1 + 1; pos_2 < number_of_items; ++pos_2) {
            for (const std::vector<Point>& part_1: data.items[subset[pos_1]].parts) {
                for (const std::vector<Point>& part_2: data.items[subset[pos_2]].parts) {
                    int a = model_builder.add_variable(-1, 1);
                    int b = model_builder.add_variable(-1, 1);
                    int d = model_builder.add_variable(-2, 2);
                    if (data.spacing > 0) {
                        model_builder.add_constraint(1, 1);
                    } else {
                        model_builder.add_constraint(1, infinity);
                    }
                    model_builder.quadratic(a, a, 1);
                    model_builder.quadratic(b, b, 1);
                    for (int side = 0; side < 2; ++side) {
                        ItemPos pos = (side == 0)? pos_1: pos_2;
                        const std::vector<Point>& part = (side == 0)? part_1: part_2;
                        for (const Point& v: part) {
                            // a (x + c v_x - s v_y) + b (y + s v_x + c v_y) - d.
                            if (side == 0) {
                                model_builder.add_constraint(-infinity, -data.spacing / 2);
                            } else {
                                model_builder.add_constraint(data.spacing / 2, infinity);
                            }
                            model_builder.quadratic(a, x[pos], 1);
                            model_builder.quadratic(a, c[pos], v.x);
                            model_builder.quadratic(a, s[pos], -v.y);
                            model_builder.quadratic(b, y[pos], 1);
                            model_builder.quadratic(b, s[pos], v.x);
                            model_builder.quadratic(b, c[pos], v.y);
                            model_builder.linear(d, -1);
                        }
                    }
                }
            }
        }
    }

    mathoptsolverscmake::ScipModel scip_model;
    mathoptsolverscmake::reduce_printout(scip_model);
    mathoptsolverscmake::set_time_limit(scip_model, (std::min)(time_limit, 1e20));
    mathoptsolverscmake::load(scip_model, model_builder.model);
    mathoptsolverscmake::solve(scip_model);

    SubsetOutput output;
    output.number_of_nodes = mathoptsolverscmake::get_number_of_nodes(scip_model);
    if (mathoptsolverscmake::is_infeasible(scip_model)) {
        output.status = SubsetStatus::Infeasible;
        return output;
    }
    std::vector<double> solution = mathoptsolverscmake::get_solution(scip_model);
    if (solution.empty())
        return output;
    output.status = SubsetStatus::Feasible;
    for (ItemPos pos = 0; pos < number_of_items; ++pos) {
        Point position = {
            data.origin.x + solution[x[pos]] / data.scale,
            data.origin.y + solution[y[pos]] / data.scale};
        Angle angle = std::atan2(solution[s[pos]], solution[c[pos]]) * 180 / M_PI;
        output.placements.push_back({position, angle});
    }
    return output;
}

/** Build the solution of the subset with all the items; return 'false' if the items overlap. */
bool build_solution(
        const GlobalNonlinearProgrammingData& data,
        const SubsetOutput& subset_output,
        Solution& solution)
{
    const Instance& instance = *data.instance;
    SolutionBuilder solution_builder(instance);
    BinPos bin_pos = solution_builder.add_bin(data.bin_type_id, 1);
    for (ItemPos pos = 0; pos < (ItemPos)data.items.size(); ++pos) {
        const ItemData& item = data.items[pos];
        const ItemType& item_type = instance.item_type(item.item_type_id);
        Angle angle = allowed_angle(item_type, subset_output.placements[pos].second);
        // The center of the item is at the position; the shapes are rotated
        // around the origin, and then translated.
        Point center_rotated = item.center.rotate(angle);
        Point position = subset_output.placements[pos].first;
        solution_builder.add_item(
                bin_pos,
                item.item_type_id,
                {position.x - center_rotated.x, position.y - center_rotated.y},
                angle,
                false);
    }
    Solution new_solution = solution_builder.build();
    Solution::OverlappingItems overlapping_items = new_solution.compute_overlapping_items(bin_pos);
    if (!overlapping_items.item_border_pairs.empty()
            || !overlapping_items.item_defect_pairs.empty()
            || !overlapping_items.item_item_pairs.empty()
            || !overlapping_items.items_outside_bin.empty()) {
        return false;
    }
    solution = new_solution;
    return true;
}

}

#endif

bool packingsolver::irregular::global_nonlinear_programming_supports(
        const Instance& instance)
{
#if defined(SCIP_FOUND)
    if (instance.objective() != Objective::Feasibility
            || instance.number_of_bins() != 1) {
        return false;
    }
    BinTypeId bin_type_id = 0;
    while (instance.bin_type(bin_type_id).copies == 0)
        bin_type_id++;
    const BinType& bin_type = instance.bin_type(bin_type_id);
    if (!bin_type.defects.empty()
            || !bin_type.fixed_items.empty()
            || !bin_type.shape_orig.is_polygon()
            || !bin_type.shape_orig.is_convex()) {
        return false;
    }
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        for (const AllowedRotation& rotation: item_type.allowed_rotations)
            if (rotation.mirror)
                return false;
        for (const ItemShape& item_shape: item_type.shapes)
            if (!item_shape.shape_orig.is_polygon())
                return false;
    }
    return true;
#else
    (void)instance;
    return false;
#endif
}

GlobalNonlinearProgrammingOutput packingsolver::irregular::global_nonlinear_programming(
        const Instance& instance,
        const GlobalNonlinearProgrammingParameters& parameters)
{
    GlobalNonlinearProgrammingOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    if (instance.objective() != Objective::Feasibility
            || instance.number_of_bins() != 1) {
        throw std::invalid_argument(FUNC_SIGNATURE + ": unsupported objective.");
    }

#if defined(SCIP_FOUND)
    GlobalNonlinearProgrammingData data = build_data(instance);
    ItemPos number_of_items = data.items.size();

    // Subsets: the pairs, by decreasing area, each pair of item types once,
    // then the increasing subsets of the largest items.
    std::vector<std::vector<ItemPos>> subsets;
    std::vector<std::vector<ItemPos>> pairs;
    std::set<std::pair<ItemTypeId, ItemTypeId>> item_type_pairs;
    for (ItemPos pos_1 = 0; pos_1 < number_of_items; ++pos_1) {
        for (ItemPos pos_2 = pos_1 + 1; pos_2 < number_of_items; ++pos_2) {
            std::pair<ItemTypeId, ItemTypeId> key = {
                data.items[pos_1].item_type_id,
                data.items[pos_2].item_type_id};
            if (!item_type_pairs.insert(key).second)
                continue;
            pairs.push_back({pos_1, pos_2});
        }
    }
    auto subset_area = [&instance, &data](const std::vector<ItemPos>& subset)
    {
        AreaDbl area = 0;
        for (ItemPos pos: subset)
            area += instance.item_type(data.items[pos].item_type_id).space();
        return area;
    };
    std::stable_sort(
            pairs.begin(),
            pairs.end(),
            [&subset_area](const std::vector<ItemPos>& pair_1, const std::vector<ItemPos>& pair_2)
            {
                return subset_area(pair_1) > subset_area(pair_2);
            });
    if (number_of_items == 1)
        subsets.push_back({0});
    subsets.insert(subsets.end(), pairs.begin(), pairs.end());
    for (ItemPos number_of_items_subset = 3;
            number_of_items_subset <= number_of_items;
            ++number_of_items_subset) {
        std::vector<ItemPos> subset;
        for (ItemPos pos = 0; pos < number_of_items_subset; ++pos)
            subset.push_back(pos);
        subsets.push_back(subset);
    }

    for (const std::vector<ItemPos>& subset: subsets) {
        if (parameters.timer.needs_to_end())
            break;
        SubsetOutput subset_output = solve_subset(
                data,
                subset,
                parameters.timer.remaining_time());
        output.number_of_subsets++;
        output.number_of_nodes += subset_output.number_of_nodes;
        if (subset_output.status == SubsetStatus::Unknown)
            break;
        if (subset_output.status == SubsetStatus::Infeasible) {
            std::map<ItemTypeId, ItemPos> copies;
            for (ItemPos pos: subset)
                copies[data.items[pos].item_type_id]++;
            std::vector<std::pair<ItemTypeId, ItemPos>> infeasible_subset(
                    copies.begin(),
                    copies.end());
            algorithm_formatter.update_is_proven_infeasible(infeasible_subset);
            break;
        }
        if ((ItemPos)subset.size() == number_of_items) {
            Solution solution(instance);
            if (build_solution(data, subset_output, solution))
                algorithm_formatter.update_solution(solution, "");
        }
    }
#else
    throw std::invalid_argument(
            FUNC_SIGNATURE + ": requires SCIP (CMake option 'PACKINGSOLVER_USE_SCIP').");
#endif

    algorithm_formatter.end();
    return output;
}
