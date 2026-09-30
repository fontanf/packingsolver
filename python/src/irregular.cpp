#include "common.hpp"

#include "packingsolver/irregular/instance_builder.hpp"
#include "packingsolver/irregular/optimize.hpp"

using namespace packingsolver;
using namespace packingsolver::python;
using namespace packingsolver::irregular;

void bind_irregular(nb::module_& m)
{
    using Handle = OutputHandle<Instance, irregular::Output>;

    /*
     * Shapes (from the 'shape' library)
     *
     * No other problem type uses 'shape::' types, so they are bound here.
     */

    nb::enum_<shape::ShapeElementType>(m, "ShapeElementType")
        .value("LineSegment", shape::ShapeElementType::LineSegment)
        .value("CircularArc", shape::ShapeElementType::CircularArc);

    nb::enum_<shape::ShapeElementOrientation>(m, "ShapeElementOrientation")
        .value("Anticlockwise", shape::ShapeElementOrientation::Anticlockwise)
        .value("Clockwise", shape::ShapeElementOrientation::Clockwise)
        .value("Full", shape::ShapeElementOrientation::Full);

    nb::enum_<shape::ShapeType>(m, "ShapeType")
        .value("Circle", shape::ShapeType::Circle)
        .value("Square", shape::ShapeType::Square)
        .value("Rectangle", shape::ShapeType::Rectangle)
        .value("Polygon", shape::ShapeType::Polygon)
        .value("MultiPolygon", shape::ShapeType::MultiPolygon)
        .value("PolygonWithHoles", shape::ShapeType::PolygonWithHoles)
        .value("MultiPolygonWithHoles", shape::ShapeType::MultiPolygonWithHoles)
        .value("GeneralShape", shape::ShapeType::GeneralShape);

    nb::class_<shape::Point>(m, "Point")
        .def("__init__",
                [](shape::Point* point, double x, double y) { new (point) shape::Point{x, y}; },
                nb::arg("x") = 0.0, nb::arg("y") = 0.0)
        .def_rw("x", &shape::Point::x)
        .def_rw("y", &shape::Point::y)
        .def("__eq__", [](const shape::Point& point_1, const shape::Point& point_2) { return point_1 == point_2; })
        .def("__repr__", [](const shape::Point& point) { return "Point" + point.to_string(); });

    nb::class_<shape::AxisAlignedBoundingBox>(m, "AxisAlignedBoundingBox")
        .def(nb::init<>())
        .def_rw("x_min", &shape::AxisAlignedBoundingBox::x_min)
        .def_rw("x_max", &shape::AxisAlignedBoundingBox::x_max)
        .def_rw("y_min", &shape::AxisAlignedBoundingBox::y_min)
        .def_rw("y_max", &shape::AxisAlignedBoundingBox::y_max);

    nb::class_<shape::ShapeElement>(m, "ShapeElement")
        .def("__init__",
                [](shape::ShapeElement* element,
                    shape::ShapeElementType type,
                    const shape::Point& start,
                    const shape::Point& end,
                    const shape::Point& center,
                    shape::ShapeElementOrientation orientation)
                {
                    new (element) shape::ShapeElement();
                    element->type = type;
                    element->start = start;
                    element->end = end;
                    element->center = center;
                    element->orientation = orientation;
                },
                nb::arg("type") = shape::ShapeElementType::LineSegment,
                nb::arg("start") = shape::Point{0, 0},
                nb::arg("end") = shape::Point{0, 0},
                nb::arg("center") = shape::Point{0, 0},
                nb::arg("orientation") = shape::ShapeElementOrientation::Anticlockwise)
        .def_rw("type", &shape::ShapeElement::type)
        .def_rw("start", &shape::ShapeElement::start)
        .def_rw("end", &shape::ShapeElement::end)
        .def_rw("center", &shape::ShapeElement::center)
        .def_rw("orientation", &shape::ShapeElement::orientation)
        .def("radius", &shape::ShapeElement::radius)
        .def("length", nb::overload_cast<>(&shape::ShapeElement::length, nb::const_))
        .def("min_max", &shape::ShapeElement::min_max)
        .def("reverse", &shape::ShapeElement::reverse)
        .def("to_json", [](const shape::ShapeElement& element) { return json_to_python(element.to_json()); })
        .def("__eq__", [](const shape::ShapeElement& element_1, const shape::ShapeElement& element_2) { return element_1 == element_2; })
        .def("__repr__", &shape::ShapeElement::to_string);

    nb::class_<shape::Shape>(m, "Shape")
        .def("__init__",
                [](shape::Shape* self, const std::vector<shape::ShapeElement>& elements, bool is_path) {
                    new (self) shape::Shape();
                    self->elements = elements;
                    self->is_path = is_path;
                },
                nb::arg("elements") = std::vector<shape::ShapeElement>(),
                nb::arg("is_path") = false)
        .def_rw("elements", &shape::Shape::elements)
        .def_rw("is_path", &shape::Shape::is_path)
        .def("is_circle", &shape::Shape::is_circle)
        .def("is_square", &shape::Shape::is_square)
        .def("is_rectangle", &shape::Shape::is_rectangle)
        .def("is_polygon", &shape::Shape::is_polygon)
        .def("is_polyline", &shape::Shape::is_polyline)
        .def("is_convex", &shape::Shape::is_convex)
        .def("compute_area", &shape::Shape::compute_area)
        .def("compute_length", &shape::Shape::compute_length)
        .def("compute_min_max",
                nb::overload_cast<shape::Angle, bool>(&shape::Shape::compute_min_max, nb::const_),
                nb::arg("angle") = 0.0, nb::arg("mirror") = false)
        .def("contains", &shape::Shape::contains,
                nb::arg("point"), nb::arg("strict") = false)
        .def("check", &shape::Shape::check)
        .def("shift",
                nb::overload_cast<shape::LengthDbl, shape::LengthDbl>(&shape::Shape::shift),
                nb::arg("x"), nb::arg("y"),
                nb::rv_policy::reference_internal)
        .def("rotate", &shape::Shape::rotate, nb::arg("angle"))
        .def("axial_symmetry_identity_line", &shape::Shape::axial_symmetry_identity_line)
        .def("axial_symmetry_y_axis", &shape::Shape::axial_symmetry_y_axis)
        .def("axial_symmetry_x_axis", &shape::Shape::axial_symmetry_x_axis)
        .def("reverse", &shape::Shape::reverse)
        .def_static("read_json", &shape::Shape::read_json, nb::arg("file_path"))
        .def("write_json", &shape::Shape::write_json, nb::arg("file_path"))
        .def("write_svg", &shape::Shape::write_svg, nb::arg("file_path"))
        .def("to_json", [](const shape::Shape& self) { return json_to_python(self.to_json()); })
        .def("__eq__", [](const shape::Shape& shape_1, const shape::Shape& shape_2) { return shape_1 == shape_2; })
        .def("__repr__", [](const shape::Shape& self) { return self.to_string(0); });

    nb::class_<shape::ShapeWithHoles>(m, "ShapeWithHoles")
        .def("__init__",
                [](shape::ShapeWithHoles* self, const shape::Shape& outer, const std::vector<shape::Shape>& holes) {
                    new (self) shape::ShapeWithHoles();
                    self->shape = outer;
                    self->holes = holes;
                },
                nb::arg("shape") = shape::Shape(),
                nb::arg("holes") = std::vector<shape::Shape>())
        .def_rw("shape", &shape::ShapeWithHoles::shape)
        .def_rw("holes", &shape::ShapeWithHoles::holes)
        .def("check", &shape::ShapeWithHoles::check)
        .def("is_polygon", &shape::ShapeWithHoles::is_polygon)
        .def("compute_area", &shape::ShapeWithHoles::compute_area)
        .def("compute_min_max", &shape::ShapeWithHoles::compute_min_max,
                nb::arg("angle") = 0.0, nb::arg("mirror") = false)
        .def("contains", &shape::ShapeWithHoles::contains,
                nb::arg("point"), nb::arg("strict") = false)
        .def("shift",
                nb::overload_cast<shape::LengthDbl, shape::LengthDbl>(&shape::ShapeWithHoles::shift),
                nb::arg("x"), nb::arg("y"),
                nb::rv_policy::reference_internal)
        .def("rotate", &shape::ShapeWithHoles::rotate, nb::arg("angle"))
        .def("axial_symmetry_identity_line", &shape::ShapeWithHoles::axial_symmetry_identity_line)
        .def("axial_symmetry_y_axis", &shape::ShapeWithHoles::axial_symmetry_y_axis)
        .def("axial_symmetry_x_axis", &shape::ShapeWithHoles::axial_symmetry_x_axis)
        .def_static("read_json", &shape::ShapeWithHoles::read_json, nb::arg("file_path"))
        .def("write_json", &shape::ShapeWithHoles::write_json, nb::arg("file_path"))
        .def("write_svg", &shape::ShapeWithHoles::write_svg, nb::arg("file_path"))
        .def("to_json", [](const shape::ShapeWithHoles& self) { return json_to_python(self.to_json()); })
        .def("__eq__", [](const shape::ShapeWithHoles& shape_1, const shape::ShapeWithHoles& shape_2) { return shape_1 == shape_2; })
        .def("__repr__", [](const shape::ShapeWithHoles& self) { return self.to_string(0); });

    nb::class_<shape::BuildShapeElement>(m, "BuildShapeElement")
        .def("__init__",
                [](shape::BuildShapeElement* element, double x, double y, int type) {
                    new (element) shape::BuildShapeElement{x, y, type};
                },
                nb::arg("x") = 0.0, nb::arg("y") = 0.0, nb::arg("type") = 0)
        .def_rw("x", &shape::BuildShapeElement::x)
        .def_rw("y", &shape::BuildShapeElement::y)
        .def_rw("type", &shape::BuildShapeElement::type);

    m.def("build_line_segment", &shape::build_line_segment,
            nb::arg("start"), nb::arg("end"));
    m.def("build_circular_arc", &shape::build_circular_arc,
            nb::arg("start"), nb::arg("end"), nb::arg("center"), nb::arg("orientation"));
    m.def("build_triangle", &shape::build_triangle,
            nb::arg("p1"), nb::arg("p2"), nb::arg("p3"));
    m.def("build_square", &shape::build_square, nb::arg("size_length"));
    m.def("build_rectangle",
            nb::overload_cast<shape::LengthDbl, shape::LengthDbl, shape::LengthDbl, shape::LengthDbl>(&shape::build_rectangle),
            nb::arg("x_min"), nb::arg("x_max"), nb::arg("y_min"), nb::arg("y_max"));
    m.def("build_rectangle",
            nb::overload_cast<shape::LengthDbl, shape::LengthDbl>(&shape::build_rectangle),
            nb::arg("x"), nb::arg("y"));
    m.def("build_rectangle",
            nb::overload_cast<const shape::Point&, const shape::Point&>(&shape::build_rectangle),
            nb::arg("bottom_left"), nb::arg("top_right"));
    m.def("build_circle", &shape::build_circle, nb::arg("radius"));
    // 'shape::build_shape' reads 'points.front()' unconditionally.
    auto build_shape = [](const std::vector<shape::BuildShapeElement>& points, bool is_path) {
        if (points.empty())
            throw nb::value_error("build_shape: 'points' must not be empty.");
        return shape::build_shape(points, is_path);
    };
    m.def("build_shape", build_shape,
            nb::arg("points"), nb::arg("is_path") = false);
    m.def("build_shape",
            nb::overload_cast<const std::vector<shape::ShapeElement>&>(&shape::build_shape),
            nb::arg("elements"));
    // Python convenience: 'points' given as '(x, y)' / '(x, y, type)' tuples.
    m.def("build_shape",
            [build_shape](const std::vector<std::vector<double>>& points, bool is_path) {
                std::vector<shape::BuildShapeElement> elements;
                for (const std::vector<double>& point: points) {
                    if (point.size() != 2 && point.size() != 3)
                        throw nb::value_error("build_shape: each point must be (x, y) or (x, y, type).");
                    shape::BuildShapeElement element;
                    element.x = point[0];
                    element.y = point[1];
                    if (point.size() == 3)
                        element.type = (int)point[2];
                    elements.push_back(element);
                }
                return build_shape(elements, is_path);
            },
            nb::arg("points"), nb::arg("is_path") = false,
            "Build a shape from '(x, y)' points and '(x, y, 1)' / '(x, y, -1)' "
            "anticlockwise / clockwise circular arc centers.");
    m.def("build_path",
            [build_shape](const std::vector<shape::BuildShapeElement>& points) {
                return build_shape(points, true);
            },
            nb::arg("points"));
    m.def("build_path",
            nb::overload_cast<const std::vector<shape::ShapeElement>&>(&shape::build_path),
            nb::arg("elements"));

    /*
     * Enums
     */

    nb::enum_<LeftoverMode>(m, "LeftoverMode")
        .value("BottomLeft", LeftoverMode::BottomLeft)
        .value("BottomRight", LeftoverMode::BottomRight)
        .value("TopLeft", LeftoverMode::TopLeft)
        .value("TopRight", LeftoverMode::TopRight)
        .value("Left", LeftoverMode::Left)
        .value("Right", LeftoverMode::Right)
        .value("Bottom", LeftoverMode::Bottom)
        .value("Top", LeftoverMode::Top);

    /*
     * Instance
     */

    nb::class_<irregular::Parameters>(m, "Parameters")
        .def_ro("quality_rules", &irregular::Parameters::quality_rules)
        .def_ro("item_item_minimum_spacing", &irregular::Parameters::item_item_minimum_spacing)
        .def_ro("leftover_mode", &irregular::Parameters::leftover_mode)
        .def_ro("scale_value", &irregular::Parameters::scale_value)
        .def_ro("open_dimension_xy_aspect_ratio", &irregular::Parameters::open_dimension_xy_aspect_ratio);

    nb::class_<Defect>(m, "Defect")
        .def_ro("shape_orig", &Defect::shape_orig)
        .def_ro("type", &Defect::type)
        .def_ro("item_defect_minimum_spacing", &Defect::item_defect_minimum_spacing);

    nb::class_<FixedItem>(m, "FixedItem")
        .def_ro("item_type_id", &FixedItem::item_type_id)
        .def_ro("bl_corner", &FixedItem::bl_corner)
        .def_ro("angle", &FixedItem::angle)
        .def_ro("mirror", &FixedItem::mirror);

    nb::class_<BinType>(m, "BinType")
        .def_ro("cost", &BinType::cost)
        .def_ro("copies", &BinType::copies)
        .def_ro("copies_min", &BinType::copies_min)
        .def_ro("shape_orig", &BinType::shape_orig)
        .def_ro("defects", &BinType::defects)
        .def_ro("fixed_items", &BinType::fixed_items)
        .def_ro("item_bin_minimum_spacing", &BinType::item_bin_minimum_spacing)
        .def_ro("area_orig", &BinType::area_orig)
        .def_ro("aabb_orig", &BinType::aabb_orig)
        .def("space", &BinType::space)
        .def("number_of_resources", &BinType::number_of_resources);

    nb::class_<ItemShape>(m, "ItemShape")
        .def("__init__",
                [](ItemShape* item_shape, const shape::ShapeWithHoles& shape_orig, QualityRule quality_rule) {
                    new (item_shape) ItemShape();
                    item_shape->shape_orig = shape_orig;
                    item_shape->quality_rule = quality_rule;
                },
                nb::arg("shape_orig") = shape::ShapeWithHoles(),
                nb::arg("quality_rule") = -1)
        .def_rw("shape_orig", &ItemShape::shape_orig)
        .def_rw("quality_rule", &ItemShape::quality_rule)
        .def("check", &ItemShape::check);

    nb::class_<AllowedRotation>(m, "AllowedRotation")
        .def_ro("start_angle", &AllowedRotation::start_angle)
        .def_ro("end_angle", &AllowedRotation::end_angle)
        .def_ro("mirror", &AllowedRotation::mirror);

    nb::class_<ItemType>(m, "ItemType")
        .def_ro("profit", &ItemType::profit)
        .def_ro("copies", &ItemType::copies)
        .def_ro("copies_min", &ItemType::copies_min)
        .def_ro("shapes", &ItemType::shapes)
        .def_ro("allowed_rotations", &ItemType::allowed_rotations)
        .def_ro("copies_fixed", &ItemType::copies_fixed)
        .def_ro("area_orig", &ItemType::area_orig)
        .def("space", &ItemType::space)
        .def("shape_type", &ItemType::shape_type)
        .def("compute_min_max", &ItemType::compute_min_max,
                nb::arg("angle") = 0.0, nb::arg("mirror") = false, nb::arg("type") = 0)
        .def("has_full_continuous_rotations", &ItemType::has_full_continuous_rotations)
        .def("has_only_discrete_rotations", &ItemType::has_only_discrete_rotations)
        .def("is_rotation_allowed", &ItemType::is_rotation_allowed,
                nb::arg("angle"), nb::arg("mirror"));

    nb::class_<Instance>(m, "Instance")
        .def("objective", &Instance::objective)
        .def("parameters", &Instance::parameters, nb::rv_policy::reference_internal)
        .def("number_of_bin_types", &Instance::number_of_bin_types)
        .def("bin_type",
                [](const Instance& instance, BinTypeId bin_type_id) -> const BinType& {
                    check_index(bin_type_id, instance.number_of_bin_types(), "bin_type_id");
                    return instance.bin_type(bin_type_id);
                },
                nb::arg("bin_type_id"),
                nb::rv_policy::reference_internal)
        .def("number_of_bins", &Instance::number_of_bins)
        .def("bin_type_id",
                [](const Instance& instance, BinPos bin_pos) {
                    check_index(bin_pos, instance.number_of_bins(), "bin_pos");
                    return instance.bin_type_id(bin_pos);
                },
                nb::arg("bin_pos"))
        .def("previous_bin_area",
                [](const Instance& instance, BinPos bin_pos) {
                    check_index(bin_pos, instance.number_of_bins(), "bin_pos");
                    return instance.previous_bin_area(bin_pos);
                },
                nb::arg("bin_pos"))
        .def("bin_area", &Instance::bin_area)
        .def("largest_bin_cost", &Instance::largest_bin_cost)
        .def("number_of_defects", &Instance::number_of_defects)
        .def("last_bin_with_fixed_items", &Instance::last_bin_with_fixed_items)
        .def("number_of_item_types", &Instance::number_of_item_types)
        .def("item_type",
                [](const Instance& instance, ItemTypeId item_type_id) -> const ItemType& {
                    check_index(item_type_id, instance.number_of_item_types(), "item_type_id");
                    return instance.item_type(item_type_id);
                },
                nb::arg("item_type_id"),
                nb::rv_policy::reference_internal)
        .def("item_shape_orig",
                [](const Instance& instance,
                    ItemTypeId item_type_id,
                    ItemPos item_shape_pos,
                    shape::Angle angle,
                    bool mirror)
                {
                    check_index(item_type_id, instance.number_of_item_types(), "item_type_id");
                    check_index(item_shape_pos, instance.item_type(item_type_id).shapes.size(), "item_shape_pos");
                    return instance.item_shape_orig(item_type_id, item_shape_pos, angle, mirror);
                },
                nb::arg("item_type_id"), nb::arg("item_shape_pos"), nb::arg("angle"), nb::arg("mirror"))
        .def("item_shape_scaled",
                [](const Instance& instance,
                    ItemTypeId item_type_id,
                    ItemPos item_shape_pos,
                    shape::Angle angle,
                    bool mirror)
                {
                    check_index(item_type_id, instance.number_of_item_types(), "item_type_id");
                    check_index(item_shape_pos, instance.item_type(item_type_id).shapes.size(), "item_shape_pos");
                    return instance.item_shape_scaled(item_type_id, item_shape_pos, angle, mirror);
                },
                nb::arg("item_type_id"), nb::arg("item_shape_pos"), nb::arg("angle"), nb::arg("mirror"))
        .def("number_of_items", &Instance::number_of_items)
        .def("number_of_fixed_items", &Instance::number_of_fixed_items)
        .def("number_of_rectangular_items", &Instance::number_of_rectangular_items)
        .def("number_of_circular_items", &Instance::number_of_circular_items)
        .def("item_area", &Instance::item_area)
        .def("mean_area", &Instance::mean_area)
        .def("smallest_item_area", &Instance::smallest_item_area)
        .def("largest_item_area", &Instance::largest_item_area)
        .def("item_profit", &Instance::item_profit)
        .def("largest_item_profit", &Instance::largest_item_profit)
        .def("largest_efficiency_item_type_id", &Instance::largest_efficiency_item_type_id)
        .def("largest_item_copies", &Instance::largest_item_copies)
        .def("unbounded_knapsack", &Instance::unbounded_knapsack)
        .def("fits_some_bin",
                [](const Instance& instance, ItemTypeId item_type_id) {
                    check_index(item_type_id, instance.number_of_item_types(), "item_type_id");
                    return instance.fits_some_bin(item_type_id);
                },
                nb::arg("item_type_id"))
        .def("resources_matter", &Instance::resources_matter)
        .def("write", &Instance::write, nb::arg("instance_path"))
        .def("format",
                [](const Instance& instance, int verbosity_level) {
                    std::stringstream ss;
                    instance.format(ss, verbosity_level);
                    return ss.str();
                },
                nb::arg("verbosity_level") = 1);

    nb::class_<InstanceBuilder>(m, "InstanceBuilder")
        .def(nb::init<>())
        .def("set_objective", &InstanceBuilder::set_objective, nb::arg("objective"))
        .def("read", &InstanceBuilder::read, nb::arg("instance_path"))
        .def("set_item_item_minimum_spacing", &InstanceBuilder::set_item_item_minimum_spacing,
                nb::arg("item_item_minimum_spacing"))
        .def("set_open_dimension_xy_aspect_ratio", &InstanceBuilder::set_open_dimension_xy_aspect_ratio,
                nb::arg("open_dimension_xy_aspect_ratio"))
        .def("set_leftover_mode", &InstanceBuilder::set_leftover_mode, nb::arg("leftover_mode"))
        // Bin types.
        .def("add_bin_type",
                nb::overload_cast<const shape::Shape&>(&InstanceBuilder::add_bin_type),
                nb::arg("shape"))
        .def("set_bin_type_cost", &InstanceBuilder::set_bin_type_cost,
                nb::arg("bin_type_id"), nb::arg("cost"))
        .def("add_defect", &InstanceBuilder::add_defect,
                nb::arg("bin_type_id"), nb::arg("type"), nb::arg("shape"))
        .def("set_item_bin_minimum_spacing", &InstanceBuilder::set_item_bin_minimum_spacing,
                nb::arg("bin_type_id"), nb::arg("item_bin_minimum_spacing"))
        .def("set_item_defect_minimum_spacing", &InstanceBuilder::set_item_defect_minimum_spacing,
                nb::arg("bin_type_id"), nb::arg("defect_id"), nb::arg("item_defect_minimum_spacing"))
        .def("add_bin_type_resource", &InstanceBuilder::add_bin_type_resource,
                nb::arg("bin_type_id"), nb::arg("capacity"), nb::arg("penalize") = false, nb::arg("penalty") = 0.0)
        .def("add_resource_consumption", &InstanceBuilder::add_resource_consumption,
                nb::arg("bin_type_id"), nb::arg("resource_id"), nb::arg("item_type_id"), nb::arg("schedule"))
        .def("set_bin_type_copies", &InstanceBuilder::set_bin_type_copies,
                nb::arg("bin_type_id"), nb::arg("copies"))
        .def("set_bin_type_copies_min", &InstanceBuilder::set_bin_type_copies_min,
                nb::arg("bin_type_id"), nb::arg("copies_min"))
        .def("set_bin_types_infinite_copies", &InstanceBuilder::set_bin_types_infinite_copies)
        .def("set_bin_types_unweighted", &InstanceBuilder::set_bin_types_unweighted)
        // Item types.
        .def("add_item_type",
                nb::overload_cast<const std::vector<ItemShape>&>(&InstanceBuilder::add_item_type),
                nb::arg("shapes"))
        .def("add_item_type_allowed_rotation", &InstanceBuilder::add_item_type_allowed_rotation,
                nb::arg("item_type_id"), nb::arg("start_angle"), nb::arg("end_angle"), nb::arg("mirror"))
        .def("set_item_type_profit", &InstanceBuilder::set_item_type_profit,
                nb::arg("item_type_id"), nb::arg("profit"))
        .def("set_item_type_copies", &InstanceBuilder::set_item_type_copies,
                nb::arg("item_type_id"), nb::arg("copies"))
        .def("set_item_type_copies_min", &InstanceBuilder::set_item_type_copies_min,
                nb::arg("item_type_id"), nb::arg("copies_min"))
        .def("set_item_types_unweighted", &InstanceBuilder::set_item_types_unweighted)
        .def("set_item_types_continuous_rotations", &InstanceBuilder::set_item_types_continuous_rotations)
        .def("build",
                [](InstanceBuilder& instance_builder) {
                    std::shared_ptr<Instance> instance = std::make_shared<Instance>(instance_builder.build());
                    // 'build' moves the instance out of the builder.
                    instance_builder = InstanceBuilder();
                    return instance;
                },
                "Build the instance; the builder is reset afterwards.");

    /*
     * Solution
     */

    nb::class_<SolutionItem>(m, "SolutionItem")
        .def_ro("item_type_id", &SolutionItem::item_type_id)
        .def_ro("bl_corner", &SolutionItem::bl_corner)
        .def_ro("angle", &SolutionItem::angle)
        .def_ro("mirror", &SolutionItem::mirror)
        .def_ro("is_fixed", &SolutionItem::is_fixed);

    nb::class_<SolutionBin>(m, "SolutionBin")
        .def_ro("bin_type_id", &SolutionBin::bin_type_id)
        .def_ro("copies", &SolutionBin::copies)
        .def_ro("items", &SolutionBin::items)
        .def_ro("item_area", &SolutionBin::item_area)
        .def_ro("x_min", &SolutionBin::x_min)
        .def_ro("x_max", &SolutionBin::x_max)
        .def_ro("y_min", &SolutionBin::y_min)
        .def_ro("y_max", &SolutionBin::y_max)
        .def_ro("resource_consumption", &SolutionBin::resource_consumption);

    nb::class_<Solution>(m, "Solution")
        .def("feasible", &Solution::feasible)
        .def("resource_feasible", &Solution::resource_feasible)
        .def("full", &Solution::full)
        .def("number_of_bins", &Solution::number_of_bins)
        .def("number_of_different_bins", &Solution::number_of_different_bins)
        .def("bin",
                [](const Solution& solution, BinPos bin_pos) -> const SolutionBin& {
                    check_index(bin_pos, solution.number_of_different_bins(), "bin_pos");
                    return solution.bin(bin_pos);
                },
                nb::arg("bin_pos"),
                nb::rv_policy::reference_internal)
        .def("bin_copies",
                [](const Solution& solution, BinTypeId bin_type_id) {
                    check_index(bin_type_id, solution.instance().number_of_bin_types(), "bin_type_id");
                    return solution.bin_copies(bin_type_id);
                },
                nb::arg("bin_type_id"))
        .def("cost", &Solution::cost)
        .def("bin_area", &Solution::bin_area)
        .def("number_of_items", &Solution::number_of_items)
        .def("item_area", &Solution::item_area)
        .def("profit", &Solution::profit)
        .def("item_copies",
                [](const Solution& solution, ItemTypeId item_type_id) {
                    check_index(item_type_id, solution.instance().number_of_item_types(), "item_type_id");
                    return solution.item_copies(item_type_id);
                },
                nb::arg("item_type_id"))
        .def("number_of_infeasible_item_copies_min", &Solution::number_of_infeasible_item_copies_min)
        .def("item_copies_feasible", &Solution::item_copies_feasible)
        .def("x_min", &Solution::x_min)
        .def("y_min", &Solution::y_min)
        .def("x_max", &Solution::x_max)
        .def("y_max", &Solution::y_max)
        .def("open_dimension_xy_area", &Solution::open_dimension_xy_area)
        .def("leftover_value_orig", &Solution::leftover_value_orig)
        .def("full_waste", &Solution::full_waste)
        .def("full_waste_percentage", &Solution::full_waste_percentage)
        .def("density_x", &Solution::density_x)
        .def("density_y", &Solution::density_y)
        .def("shape_scaled",
                [](const Solution& solution,
                    BinPos bin_pos,
                    ItemPos item_pos,
                    shape::ItemShapePos item_shape_pos,
                    double scale_value)
                {
                    check_index(bin_pos, solution.number_of_different_bins(), "bin_pos");
                    const SolutionBin& solution_bin = solution.bin(bin_pos);
                    check_index(item_pos, solution_bin.items.size(), "item_pos");
                    const ItemType& item_type = solution.instance().item_type(
                            solution_bin.items[item_pos].item_type_id);
                    check_index(item_shape_pos, item_type.shapes.size(), "item_shape_pos");
                    return solution.shape_scaled(bin_pos, item_pos, item_shape_pos, scale_value);
                },
                nb::arg("bin_pos"), nb::arg("item_pos"), nb::arg("item_shape_pos"), nb::arg("scale_value") = 1.0)
        .def("write", &solution_write_path<Solution>, nb::arg("certificate_path"),
                "Write the solution certificate to a file.")
        .def("write", &solution_write_stream<Solution>, nb::arg("stream"),
                "Write the solution certificate to a text stream (e.g. 'io.StringIO').")
        .def("write_svg",
                [](const Solution& solution, const std::string& file_path, BinPos bin_pos, bool scaled) {
                    check_index(bin_pos, solution.number_of_different_bins(), "bin_pos");
                    solution.write_svg(file_path, bin_pos, scaled);
                },
                nb::arg("file_path"), nb::arg("bin_pos"), nb::arg("scaled") = false)
        .def("to_json", [](const Solution& solution) { return json_to_python(solution.to_json()); });

    /*
     * Optimize
     */

    nb::class_<ReductionParameters>(m, "ReductionParameters")
        .def(nb::init<>())
        .def_rw("reduce", &ReductionParameters::reduce)
        .def_rw("remove_negative_profit_items", &ReductionParameters::remove_negative_profit_items)
        .def_rw("merge_identical_items", &ReductionParameters::merge_identical_items);

    nb::class_<OptimizeParameters> parameters(m, "OptimizeParameters");
    bind_parameters_base<irregular::Output>(parameters);
    parameters
        .def_rw("optimization_mode", &OptimizeParameters::optimization_mode)
        .def_rw("memory_limit_megabytes", &OptimizeParameters::memory_limit_megabytes)
        .def_rw("linear_programming_solver_name", &OptimizeParameters::linear_programming_solver_name)
        .def_rw("use_tree_search", &OptimizeParameters::use_tree_search)
        .def_rw("use_tree_search_periodic_packing", &OptimizeParameters::use_tree_search_periodic_packing)
        .def_rw("use_local_search", &OptimizeParameters::use_local_search)
        .def_rw("use_milp_raster", &OptimizeParameters::use_milp_raster)
        .def_rw("use_sequential_single_knapsack", &OptimizeParameters::use_sequential_single_knapsack)
        .def_rw("use_sequential_value_correction", &OptimizeParameters::use_sequential_value_correction)
        .def_rw("use_dichotomic_search", &OptimizeParameters::use_dichotomic_search)
        .def_rw("use_column_generation", &OptimizeParameters::use_column_generation)
        .def_rw("initial_maximum_approximation_ratio", &OptimizeParameters::initial_maximum_approximation_ratio)
        .def_rw("maximum_approximation_ratio_factor", &OptimizeParameters::maximum_approximation_ratio_factor)
        .def_rw("many_items_in_bins_threshold", &OptimizeParameters::many_items_in_bins_threshold)
        .def_rw("many_item_type_copies_factor", &OptimizeParameters::many_item_type_copies_factor)
        .def_rw("periodic_packing_copies_threshold", &OptimizeParameters::periodic_packing_copies_threshold)
        .def_rw("periodic_packing_max_items_threshold", &OptimizeParameters::periodic_packing_max_items_threshold)
        .def_rw("tree_search_guides", &OptimizeParameters::tree_search_guides)
        .def_rw("sequential_value_correction_subproblem_tree_search_queue_size", &OptimizeParameters::sequential_value_correction_subproblem_tree_search_queue_size)
        .def_rw("column_generation_subproblem_tree_search_queue_size", &OptimizeParameters::column_generation_subproblem_tree_search_queue_size)
        .def_rw("not_anytime_maximum_approximation_ratio", &OptimizeParameters::not_anytime_maximum_approximation_ratio)
        .def_rw("not_anytime_tree_search_queue_size", &OptimizeParameters::not_anytime_tree_search_queue_size)
        .def_rw("not_anytime_tree_search_periodic_packing_queue_size", &OptimizeParameters::not_anytime_tree_search_periodic_packing_queue_size)
        .def_rw("not_anytime_sequential_single_knapsack_subproblem_tree_search_queue_size", &OptimizeParameters::not_anytime_sequential_single_knapsack_subproblem_tree_search_queue_size)
        .def_rw("not_anytime_sequential_value_correction_number_of_iterations", &OptimizeParameters::not_anytime_sequential_value_correction_number_of_iterations)
        .def_rw("not_anytime_dichotomic_search_subproblem_tree_search_queue_size", &OptimizeParameters::not_anytime_dichotomic_search_subproblem_tree_search_queue_size)
        .def_rw("reduction_parameters", &OptimizeParameters::reduction_parameters);

    nb::class_<Handle> output = bind_output<Instance, Solution, irregular::Output>(m);
    def_output_field(output, "knapsack_bound", &irregular::Output::knapsack_bound);
    def_output_field(output, "bin_packing_bound", &irregular::Output::bin_packing_bound);
    def_output_field(output, "variable_sized_bin_packing_bound", &irregular::Output::variable_sized_bin_packing_bound);
    def_output_field(output, "open_dimension_x_bound", &irregular::Output::open_dimension_x_bound);
    def_output_field(output, "open_dimension_y_bound", &irregular::Output::open_dimension_y_bound);
    def_output_field(output, "is_proven_infeasible", &irregular::Output::is_proven_infeasible);

    m.def("optimize",
            [](std::shared_ptr<Instance> instance, const OptimizeParameters& parameters) {
                return run_optimize<Instance, irregular::Output>(
                        instance,
                        parameters,
                        [](const Instance& instance, const OptimizeParameters& parameters) {
                            return irregular::optimize(instance, parameters);
                        });
            },
            nb::arg("instance"),
            nb::arg("parameters") = default_optimize_parameters<OptimizeParameters>());
}
