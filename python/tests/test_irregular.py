import gc
import math
import os
import re

import pytest

import packingsolver.irregular as psi


DATA_DIR = os.path.join(
        os.path.dirname(__file__), "..", "..", "data", "irregular", "tests")


def quiet_parameters(**kwargs):
    parameters = psi.OptimizeParameters()
    parameters.verbosity_level = 0
    parameters.optimization_mode = psi.OptimizationMode.NotAnytimeSequential
    parameters.time_limit = 10.0
    for name, value in kwargs.items():
        setattr(parameters, name, value)
    return parameters


def square(size, x=0, y=0):
    return psi.build_rectangle(x, x + size, y, y + size)


def item_shapes(shape, holes=()):
    return [psi.ItemShape(psi.ShapeWithHoles(shape, list(holes)))]


def bin_packing_instance(item_copies):
    """'item_copies' 5x5 squares in 10x10 bins."""
    instance_builder = psi.InstanceBuilder()
    instance_builder.set_objective(psi.Objective.BinPacking)
    instance_builder.add_bin_type(square(10), copies=10)
    instance_builder.add_item_type(item_shapes(square(5)), copies=item_copies)
    return instance_builder.build()


def test_point():
    point = psi.Point(1, 2)
    assert point.x == 1 and point.y == 2
    point.x = 3
    assert point == psi.Point(3, 2)
    assert psi.Point().x == 0


def test_shape_element():
    element = psi.ShapeElement(
            type=psi.ShapeElementType.LineSegment,
            start=psi.Point(0, 0),
            end=psi.Point(3, 4))
    assert element.length() == pytest.approx(5)
    element.end = psi.Point(0, 1)
    assert element.length() == pytest.approx(1)
    assert element == psi.build_line_segment(psi.Point(0, 0), psi.Point(0, 1))
    arc = psi.build_circular_arc(
            psi.Point(1, 0), psi.Point(0, 1), psi.Point(0, 0),
            psi.ShapeElementOrientation.Anticlockwise)
    assert arc.type == psi.ShapeElementType.CircularArc
    assert arc.radius() == pytest.approx(1)
    assert isinstance(arc.to_json(), dict)


def test_shape_from_elements():
    points = [(0, 0), (4, 0), (4, 3), (0, 3)]
    elements = [
        psi.build_line_segment(psi.Point(*points[i]), psi.Point(*points[(i + 1) % 4]))
        for i in range(4)]
    shape = psi.Shape(elements)
    assert len(shape.elements) == 4
    assert shape.check()
    assert shape.is_rectangle()
    assert shape.compute_area() == pytest.approx(12)
    assert shape == psi.build_rectangle(4, 3)
    assert shape == psi.build_shape(elements)
    shape.elements = shape.elements[:3]
    assert len(shape.elements) == 3


def test_build_shape():
    # Polygon from points, given as tuples or as 'BuildShapeElement'.
    triangle = psi.build_shape([(0, 0), (2, 0), (0, 2)])
    assert triangle.is_polygon()
    assert triangle.compute_area() == pytest.approx(2)
    assert triangle == psi.build_triangle(psi.Point(0, 0), psi.Point(2, 0), psi.Point(0, 2))
    assert triangle == psi.build_shape([
        psi.BuildShapeElement(0, 0),
        psi.BuildShapeElement(2, 0),
        psi.BuildShapeElement(0, 2)])
    # Quarter disk: the third point is the center of an anticlockwise arc.
    quarter_disk = psi.build_shape([(0, 0), (1, 0), (0, 0, 1), (0, 1)])
    assert quarter_disk.compute_area() == pytest.approx(math.pi / 4)
    with pytest.raises(ValueError):
        psi.build_shape([(0, 0, 0, 0)])
    with pytest.raises(ValueError):
        psi.build_shape([])


def test_basic_shapes():
    circle = psi.build_circle(2)
    assert circle.is_circle()
    assert circle.compute_area() == pytest.approx(4 * math.pi)
    assert psi.build_square(3).is_square()
    rectangle = psi.build_rectangle(psi.Point(1, 1), psi.Point(3, 2))
    aabb = rectangle.compute_min_max()
    assert (aabb.x_min, aabb.x_max, aabb.y_min, aabb.y_max) == (1, 3, 1, 2)
    assert rectangle.contains(psi.Point(2, 1.5))
    assert not rectangle.contains(psi.Point(0, 0))
    shifted = psi.build_square(1)
    shifted.shift(5, 5)
    assert shifted.compute_min_max().x_min == pytest.approx(5)


def test_shape_with_holes():
    shape = psi.ShapeWithHoles(square(30), [square(10, 10, 10)])
    assert shape.check()
    assert len(shape.holes) == 1
    assert shape.compute_area() == pytest.approx(800)
    assert shape.contains(psi.Point(5, 5))
    assert not shape.contains(psi.Point(15, 15))
    shape.holes = []
    assert shape.compute_area() == pytest.approx(900)
    assert isinstance(shape.to_json(), dict)


def test_instance_builder():
    instance = bin_packing_instance(5)
    assert instance.objective() == psi.Objective.BinPacking
    assert instance.number_of_item_types() == 1
    assert instance.number_of_items() == 5
    assert instance.number_of_bin_types() == 1
    assert instance.item_type(0).copies == 5
    assert instance.item_type(0).area_orig == pytest.approx(25)
    assert instance.item_type(0).shape_type() == psi.ShapeType.Square
    assert len(instance.item_type(0).shapes) == 1
    assert instance.bin_type(0).copies == 10
    assert instance.bin_type(0).area_orig == pytest.approx(100)
    assert instance.bin_area() == pytest.approx(1000)
    assert instance.item_area() == pytest.approx(125)
    assert instance.parameters().leftover_mode == psi.LeftoverMode.BottomLeft
    rotated = instance.item_shape_orig(0, 0, 0.0, False)
    assert rotated.compute_area() == pytest.approx(25)
    assert "Number of item types" in instance.format()
    with pytest.raises(IndexError):
        instance.item_type(1)
    with pytest.raises(IndexError):
        instance.bin_type(-1)
    with pytest.raises(IndexError):
        instance.item_shape_orig(0, 1, 0.0, False)


def test_instance_builder_invalid_argument():
    instance_builder = psi.InstanceBuilder()
    with pytest.raises(ValueError):
        instance_builder.add_bin_type(square(10), cost=0)
    with pytest.raises(ValueError):
        instance_builder.add_item_type(item_shapes(square(5)), copies=0)
    # Attributes are keyword-only.
    with pytest.raises(TypeError):
        instance_builder.add_item_type(item_shapes(square(5)), 2)
    with pytest.raises(TypeError):
        instance_builder.add_bin_type(square(10), 2)


def test_add_item_type_keywords():
    instance_builder = psi.InstanceBuilder()
    instance_builder.set_objective(psi.Objective.Knapsack)
    instance_builder.add_bin_type(square(10))
    item_type_id = instance_builder.add_item_type(
            item_shapes(square(5)),
            profit=7,
            copies=3,
            copies_min=1)
    assert item_type_id == 0
    item_type_id = instance_builder.add_item_type(
            item_shapes(square(4)),
            allowed_rotations=[(0, 0, False), (90, 90, True)])
    assert item_type_id == 1
    instance = instance_builder.build()
    item_type = instance.item_type(0)
    assert item_type.profit == pytest.approx(7)
    assert item_type.copies == 3
    assert item_type.copies_min == 1
    allowed_rotations = [
        (rotation.start_angle, rotation.end_angle, rotation.mirror)
        for rotation in instance.item_type(1).allowed_rotations]
    assert allowed_rotations == [(0, 0, False), (90, 90, True)]
    assert instance.item_type(1).is_rotation_allowed(90, True)


def test_add_item_type_defaults():
    instance_builder = psi.InstanceBuilder()
    instance_builder.set_objective(psi.Objective.BinPacking)
    instance_builder.add_bin_type(square(10))
    instance_builder.add_item_type(item_shapes(psi.build_rectangle(5, 3)))
    instance_builder.add_item_type(
            item_shapes(psi.build_rectangle(5, 3)),
            allowed_rotations=[(90, 90, False)])
    instance = instance_builder.build()
    item_type = instance.item_type(0)
    # The profit defaults to the area.
    assert item_type.profit == pytest.approx(15)
    assert item_type.copies == 1
    # 'copies_min' is resolved in 'build()': equal to 'copies' for bin packing.
    assert item_type.copies_min == 1
    allowed_rotations = [
        (rotation.start_angle, rotation.end_angle, rotation.mirror)
        for rotation in item_type.allowed_rotations]
    assert allowed_rotations == [(0, 0, False)]
    # The first given rotation replaces the default one.
    allowed_rotations = [
        (rotation.start_angle, rotation.end_angle, rotation.mirror)
        for rotation in instance.item_type(1).allowed_rotations]
    assert allowed_rotations == [(90, 90, False)]


def test_add_item_type_copies_min_knapsack_default():
    instance_builder = psi.InstanceBuilder()
    instance_builder.set_objective(psi.Objective.Knapsack)
    instance_builder.add_bin_type(square(10))
    instance_builder.add_item_type(item_shapes(square(5)), copies=2)
    # 'copies_min' is resolved in 'build()': 0 for knapsack.
    assert instance_builder.build().item_type(0).copies_min == 0


def test_add_bin_type_keywords():
    instance_builder = psi.InstanceBuilder()
    instance_builder.set_objective(psi.Objective.VariableSizedBinPacking)
    bin_type_id = instance_builder.add_bin_type(
            square(10),
            cost=42,
            item_bin_minimum_spacing=0.5,
            copies=4,
            copies_min=2)
    assert bin_type_id == 0
    instance_builder.add_item_type(item_shapes(square(5)))
    bin_type = instance_builder.build().bin_type(0)
    assert bin_type.cost == pytest.approx(42)
    assert bin_type.item_bin_minimum_spacing == pytest.approx(0.5)
    assert bin_type.copies == 4
    assert bin_type.copies_min == 2


def test_add_bin_type_defaults():
    instance_builder = psi.InstanceBuilder()
    instance_builder.set_objective(psi.Objective.BinPacking)
    instance_builder.add_bin_type(psi.build_rectangle(10, 8))
    instance_builder.add_bin_type(psi.build_rectangle(10, 8), cost=-1)
    instance_builder.add_item_type(item_shapes(square(5)))
    instance = instance_builder.build()
    bin_type = instance.bin_type(0)
    # The cost defaults to the area.
    assert bin_type.cost == pytest.approx(80)
    assert bin_type.copies == 1
    assert bin_type.copies_min == 0
    assert bin_type.item_bin_minimum_spacing == pytest.approx(0)
    # A cost of -1 means the area.
    assert instance.bin_type(1).cost == pytest.approx(80)


def test_removed_setters():
    for name in [
            "set_item_type_profit",
            "set_item_type_copies",
            "set_item_type_copies_min",
            "add_item_type_allowed_rotation",
            "set_bin_type_cost",
            "set_bin_type_copies",
            "set_bin_type_copies_min",
            "set_item_bin_minimum_spacing"]:
        assert not hasattr(psi.InstanceBuilder, name), name
    # Kept methods.
    for name in [
            "set_objective",
            "read",
            "set_item_item_minimum_spacing",
            "set_open_dimension_xy_aspect_ratio",
            "set_leftover_mode",
            "add_defect",
            "set_item_defect_minimum_spacing",
            "add_bin_type_resource",
            "add_resource_consumption",
            "set_bin_types_infinite_copies",
            "set_bin_types_unweighted",
            "set_item_types_unweighted",
            "set_item_types_continuous_rotations",
            "build"]:
        assert hasattr(psi.InstanceBuilder, name), name


INSTANCE_BUILDER_HEADER = os.path.join(
        os.path.dirname(__file__), "..", "..", "include", "packingsolver",
        "irregular", "instance_builder.hpp")

# Per-type C++ methods whose keyword name isn't the setter/adder suffix.
RENAMED_KEYWORDS = {
    "add_item_type_allowed_rotation": "allowed_rotations",
    "set_item_bin_minimum_spacing": "item_bin_minimum_spacing",
}

# Per-type C++ methods that are intentionally not keywords of
# 'add_item_type'/'add_bin_type'.
NOT_BOUND = {
    "add_defect": "returns a defect id; kept as a separate method",
    "set_item_defect_minimum_spacing": "needs a defect id returned by 'add_defect'; kept as a separate method",
    "add_bin_type_resource": "returns a resource id; kept as a separate method",
    "add_resource_consumption": "needs a resource id and an item type id; kept as a separate method",
    "add_fixed_item": "intentionally not bound in Python",
}


def cpp_per_type_methods():
    """Builder methods whose first parameter is a bin/item type id."""
    with open(INSTANCE_BUILDER_HEADER) as header_file:
        header = header_file.read()
    methods = {}
    for match in re.finditer(
            r"\b((?:set|add)_\w+)\(\s*(BinTypeId\s+bin_type_id|ItemTypeId\s+item_type_id)\b",
            header):
        methods[match.group(1)] = (
                "add_bin_type" if match.group(2).startswith("BinTypeId")
                else "add_item_type")
    return methods


def python_keywords(method):
    """Names of the keyword-only arguments of a nanobind method."""
    signatures = getattr(method, "__nb_signature__", None)
    if signatures:
        texts = [signature[0] for signature in signatures]
    else:
        texts = [method.__doc__ or ""]
    keywords = set()
    for text in texts:
        match = re.search(r"\*\s*,(.*?)\)\s*->", text, re.DOTALL)
        if match is None:
            continue
        keywords |= set(re.findall(r"(\w+)\s*:", match.group(1)))
    return keywords


def test_keywords_in_sync_with_cpp_builder():
    methods = cpp_per_type_methods()
    assert "set_item_type_copies" in methods
    assert "set_bin_type_copies" in methods
    keywords = {
        "add_item_type": python_keywords(psi.InstanceBuilder.add_item_type),
        "add_bin_type": python_keywords(psi.InstanceBuilder.add_bin_type),
    }
    assert "copies" in keywords["add_item_type"]
    assert "copies" in keywords["add_bin_type"]
    for name, python_method in methods.items():
        if name in NOT_BOUND:
            continue
        if name in RENAMED_KEYWORDS:
            keyword = RENAMED_KEYWORDS[name]
        else:
            keyword = re.sub(r"^(set|add)_(item|bin)_type_", "", name)
        assert keyword in keywords[python_method], (
                "'" + name + "' is neither a keyword of '" + python_method
                + "' nor in NOT_BOUND")
    for name in NOT_BOUND:
        assert name in methods, "stale NOT_BOUND entry: " + name


def test_build_resets_builder():
    instance_builder = psi.InstanceBuilder()
    instance_builder.add_bin_type(square(10))
    instance_builder.add_item_type(item_shapes(square(5)))
    instance = instance_builder.build()
    assert instance.number_of_item_types() == 1
    assert instance_builder.build().number_of_item_types() == 0


def test_bin_packing():
    output = psi.optimize(bin_packing_instance(5), quiet_parameters())
    solution = output.solution
    assert solution.feasible()
    assert solution.full()
    assert solution.number_of_items() == 5
    assert solution.number_of_bins() == 2
    assert output.bin_packing_bound == 2
    assert output.is_proven_optimal()

    items = [
        (bin_pos, item_pos, item)
        for bin_pos in range(solution.number_of_different_bins())
        for item_pos, item in enumerate(solution.bin(bin_pos).items)]
    assert len(items) == 5
    for bin_pos, item_pos, item in items:
        assert item.item_type_id == 0
        assert -1e-6 <= item.bl_corner.x <= 5 + 1e-6
        assert -1e-6 <= item.bl_corner.y <= 5 + 1e-6
        assert not item.mirror
        shape = solution.shape_scaled(bin_pos, item_pos, 0)
        assert shape.compute_area() > 0
    with pytest.raises(IndexError):
        solution.bin(solution.number_of_different_bins())
    with pytest.raises(IndexError):
        solution.shape_scaled(0, len(solution.bin(0).items), 0)
    with pytest.raises(IndexError):
        solution.item_copies(1)
    with pytest.raises(IndexError):
        solution.bin_copies(1)


def test_triangles_rotation():
    """Two right triangles, one rotated by 180 degrees, tile a square bin."""
    instance_builder = psi.InstanceBuilder()
    instance_builder.set_objective(psi.Objective.BinPacking)
    instance_builder.add_bin_type(square(10), copies=2)
    instance_builder.add_item_type(
            item_shapes(psi.build_shape([(0, 0), (10, 0), (0, 10)])),
            copies=2,
            allowed_rotations=[(0, 0, False), (180, 180, False)])
    instance = instance_builder.build()
    assert len(instance.item_type(0).allowed_rotations) == 2
    assert instance.item_type(0).is_rotation_allowed(180, False)
    output = psi.optimize(instance, quiet_parameters())
    assert output.solution.number_of_bins() == 1
    angles = sorted(item.angle for item in output.solution.bin(0).items)
    assert angles == pytest.approx([0, 180])


def test_knapsack():
    instance_builder = psi.InstanceBuilder()
    instance_builder.set_objective(psi.Objective.Knapsack)
    instance_builder.add_bin_type(square(10))
    # Only one of the two 10x6 items fits; the one with the largest profit
    # must be selected, together with the 10x4 item.
    for x, y, profit in [(10, 6, 5), (10, 6, 7), (10, 4, 3)]:
        instance_builder.add_item_type(item_shapes(psi.build_rectangle(x, y)), profit=profit)
    output = psi.optimize(instance_builder.build(), quiet_parameters())
    assert output.solution.profit() == pytest.approx(10)
    assert output.solution.item_copies(1) == 1
    assert output.solution.item_copies(0) == 0
    assert output.knapsack_bound >= 10 - 1e-6


@pytest.mark.parametrize("reduce", [True, False])
def test_knapsack_hole(reduce):
    """A small square fits only inside the hole of a large one (the holes
    used to be removed from the reduced instance)."""
    instance_builder = psi.InstanceBuilder()
    instance_builder.set_objective(psi.Objective.Knapsack)
    instance_builder.add_bin_type(square(30))
    instance_builder.add_item_type(
            item_shapes(square(30), [square(10, 10, 10)]), profit=10)
    instance_builder.add_item_type(item_shapes(square(10)), profit=1)
    parameters = quiet_parameters()
    parameters.reduction_parameters.reduce = reduce
    output = psi.optimize(instance_builder.build(), parameters)
    assert output.solution.profit() == pytest.approx(11)


def test_knapsack_items_in_hole():
    """Four small squares fit inside the hole of a large one."""
    instance_builder = psi.InstanceBuilder()
    instance_builder.set_objective(psi.Objective.Knapsack)
    instance_builder.add_bin_type(square(30))
    instance_builder.add_item_type(
            item_shapes(square(30), [square(20, 5, 5)]), profit=10)
    instance_builder.add_item_type(item_shapes(square(10)), profit=1, copies=4)
    output = psi.optimize(instance_builder.build(), quiet_parameters())
    assert output.solution.profit() == pytest.approx(14)


def test_defect():
    instance_builder = psi.InstanceBuilder()
    instance_builder.set_objective(psi.Objective.Knapsack)
    bin_type_id = instance_builder.add_bin_type(psi.build_rectangle(20, 10))
    defect_id = instance_builder.add_defect(
            bin_type_id, 0, psi.ShapeWithHoles(square(2, 4, 4)))
    instance_builder.set_item_defect_minimum_spacing(bin_type_id, defect_id, 0.0)
    for _ in range(2):
        instance_builder.add_item_type(item_shapes(square(10)), profit=1)
    instance = instance_builder.build()
    assert instance.number_of_defects() == 1
    assert len(instance.bin_type(0).defects) == 1
    assert instance.bin_type(0).defects[0].type == 0
    # The defect is in the left half of the bin: only one item fits.
    output = psi.optimize(instance, quiet_parameters())
    assert output.solution.profit() == pytest.approx(1)


def test_read_json():
    instance_builder = psi.InstanceBuilder()
    instance_builder.read(os.path.join(DATA_DIR, "polygon_with_hole.json"))
    instance = instance_builder.build()
    assert instance.objective() == psi.Objective.OpenDimensionX
    assert len(instance.item_type(0).shapes[0].shape_orig.holes) == 1
    output = psi.optimize(instance, quiet_parameters(use_tree_search=True))
    assert output.solution.full()
    assert output.solution.x_max() == pytest.approx(400)


def test_read_missing_file():
    instance_builder = psi.InstanceBuilder()
    with pytest.raises(Exception):
        instance_builder.read("this/file/does/not/exist.json")


def test_write(tmp_path):
    instance = bin_packing_instance(5)
    instance_path = str(tmp_path / "instance.json")
    instance.write(instance_path)
    instance_builder = psi.InstanceBuilder()
    instance_builder.read(instance_path)
    assert instance_builder.build().number_of_items() == 5

    output = psi.optimize(instance, quiet_parameters())
    certificate_path = str(tmp_path / "solution.json")
    output.solution.write(certificate_path)
    assert os.path.getsize(certificate_path) > 0
    svg_path = str(tmp_path / "solution.svg")
    output.solution.write_svg(svg_path, 0)
    assert os.path.getsize(svg_path) > 0
    with pytest.raises(IndexError):
        output.solution.write_svg(svg_path, 5)


def test_to_json():
    output = psi.optimize(bin_packing_instance(5), quiet_parameters())
    json = output.to_json()
    assert isinstance(json, dict)
    assert json["BinPackingBound"] == 2
    assert json["Solution"] == output.solution.to_json()
    assert json["Solution"]["NumberOfBins"] == 2


def test_parameters():
    parameters = psi.OptimizeParameters()
    assert parameters.time_limit == math.inf
    parameters.time_limit = 1.5
    assert parameters.time_limit == 1.5
    parameters.optimization_mode = psi.OptimizationMode.NotAnytimeSequential
    parameters.tree_search_guides = [0, 1]
    assert parameters.tree_search_guides == [0, 1]
    parameters.use_tree_search = True
    parameters.not_anytime_tree_search_queue_size = 64
    assert parameters.not_anytime_tree_search_queue_size == 64
    parameters.initial_maximum_approximation_ratio = 0.1
    assert parameters.initial_maximum_approximation_ratio == pytest.approx(0.1)
    parameters.reduction_parameters.reduce = False
    assert not parameters.reduction_parameters.reduce
    parameters.verbosity_level = 0
    output = psi.optimize(bin_packing_instance(5), parameters)
    assert output.solution.number_of_bins() == 2


def test_lifetimes():
    """Outputs and solutions stay valid after their instance is dropped."""
    output = psi.optimize(bin_packing_instance(5), quiet_parameters())
    gc.collect()
    solution = output.solution
    del output
    gc.collect()
    assert solution.number_of_bins() == 2
    assert solution.bin(0).bin_type_id == 0


def test_new_solution_callback():
    outputs = []
    parameters = quiet_parameters()
    parameters.new_solution_callback = outputs.append
    psi.optimize(bin_packing_instance(5), parameters)
    assert len(outputs) >= 1
    gc.collect()
    assert outputs[-1].solution.number_of_bins() == 2


@pytest.mark.parametrize("shapes", [
    lambda shape: shape,
    lambda shape: psi.ShapeWithHoles(shape),
    lambda shape: psi.ItemShape(psi.ShapeWithHoles(shape)),
], ids=["Shape", "ShapeWithHoles", "ItemShape"])
def test_add_item_type_single_shape(shapes):
    """A single shape gives the same item type as the list of item shapes."""
    def instance(item_type_shapes):
        instance_builder = psi.InstanceBuilder()
        instance_builder.set_objective(psi.Objective.BinPacking)
        instance_builder.add_bin_type(square(10), copies=2)
        instance_builder.add_item_type(item_type_shapes, copies=3)
        return instance_builder.build()

    reference = instance(item_shapes(square(5)))
    assert (instance(shapes(square(5))).format(verbosity_level=2)
            == reference.format(verbosity_level=2))


def test_add_item_type_invalid_shapes():
    instance_builder = psi.InstanceBuilder()
    with pytest.raises(TypeError):
        instance_builder.add_item_type(3)
