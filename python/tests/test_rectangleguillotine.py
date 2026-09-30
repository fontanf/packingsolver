import gc
import math
import os
import re

import pytest

import packingsolver.rectangleguillotine as psg


DATA_DIR = os.path.join(
        os.path.dirname(__file__), "..", "..", "data", "rectangleguillotine", "tests")


def quiet_parameters(**kwargs):
    parameters = psg.OptimizeParameters()
    parameters.verbosity_level = 0
    for name, value in kwargs.items():
        setattr(parameters, name, value)
    return parameters


def bin_packing_instance(item_copies):
    """'item_copies' 5x5 squares in 10x10 bins."""
    instance_builder = psg.InstanceBuilder()
    instance_builder.set_objective(psg.Objective.BinPacking)
    instance_builder.add_bin_type(10, 10, copies=10)
    instance_builder.add_item_type(5, 5, copies=item_copies)
    return instance_builder.build()


def solution_item_nodes(solution):
    """Item nodes (not the bin node) of all bins of a solution."""
    return [
        node
        for bin_pos in range(solution.number_of_different_bins())
        for node in solution.bin(bin_pos).nodes
        if node.f != -1 and node.item_type_id >= 0]


def test_instance_builder():
    instance = bin_packing_instance(5)
    assert instance.objective() == psg.Objective.BinPacking
    assert instance.number_of_item_types() == 1
    assert instance.number_of_items() == 5
    assert instance.number_of_bin_types() == 1
    assert instance.number_of_bins() == 10
    assert instance.bin_type_id(9) == 0
    assert instance.item_type(0).w == 5
    assert instance.item_type(0).h == 5
    assert instance.item_type(0).area() == 25
    assert instance.item_type(0).copies == 5
    assert not instance.item_type(0).oriented
    assert instance.bin_type(0).w == 10
    assert instance.bin_type(0).h == 10
    assert instance.bin_type(0).copies == 10
    assert instance.item_area() == 125
    assert "Number of item types" in instance.format()
    with pytest.raises(IndexError):
        instance.item_type(1)
    with pytest.raises(IndexError):
        instance.bin_type(-1)
    with pytest.raises(IndexError):
        instance.bin_type_id(10)
    with pytest.raises(IndexError):
        instance.stack_size(instance.number_of_stacks())


def test_instance_builder_invalid_argument():
    instance_builder = psg.InstanceBuilder()
    with pytest.raises(ValueError):
        instance_builder.add_item_type(-1, 5)


def test_build_resets_builder():
    instance_builder = psg.InstanceBuilder()
    instance_builder.add_bin_type(10, 10)
    instance_builder.add_item_type(5, 5)
    instance = instance_builder.build()
    assert instance.number_of_item_types() == 1
    assert instance_builder.build().number_of_item_types() == 0


def test_guillotine_parameters():
    instance_builder = psg.InstanceBuilder()
    instance_builder.set_objective(psg.Objective.Knapsack)
    instance_builder.add_bin_type(100, 50)
    instance_builder.add_item_type(10, 20)
    instance_builder.set_number_of_stages(2)
    instance_builder.set_cut_type(psg.CutType.Exact)
    instance_builder.set_first_stage_orientation(psg.CutOrientation.Horizontal)
    instance_builder.set_minimum_distance_1_cuts(3)
    instance_builder.set_maximum_distance_1_cuts(40)
    instance_builder.set_minimum_waste_length(2)
    instance_builder.set_maximum_number_1_cuts(4)
    instance_builder.set_cut_through_defects(True)
    instance_builder.set_cut_thickness(1)
    instance_builder.set_fixed_cutting_cost(1, 5)
    instance_builder.set_variable_cutting_cost(2, 7)
    instance_builder.set_waste_cost(3)
    instance = instance_builder.build()
    parameters = instance.parameters()
    assert parameters.number_of_stages == 2
    assert parameters.cut_type == psg.CutType.Exact
    assert parameters.first_stage_orientation == psg.CutOrientation.Horizontal
    assert parameters.minimum_distance_1_cuts == 3
    assert parameters.maximum_distance_1_cuts == 40
    assert parameters.minimum_distance_2_cuts == 0
    assert parameters.maximum_distance_2_cuts == -1
    assert parameters.minimum_waste_length == 2
    assert parameters.maximum_number_1_cuts == 4
    assert parameters.maximum_number_2_cuts == -1
    assert parameters.cut_through_defects
    assert parameters.cut_thickness == 1
    assert len(parameters.cutting_costs) >= 3
    assert parameters.cutting_costs[1].fixed == 5
    assert parameters.cutting_costs[2].variable == 7
    assert parameters.waste_cost == 3


def test_predefined():
    instance_builder = psg.InstanceBuilder()
    instance_builder.add_bin_type(100, 50)
    instance_builder.add_item_type(10, 20)
    instance_builder.set_predefined("2EHO")
    instance = instance_builder.build()
    assert instance.parameters().number_of_stages == 2
    assert instance.parameters().cut_type == psg.CutType.Exact
    assert instance.parameters().first_stage_orientation == psg.CutOrientation.Horizontal
    assert instance.all_item_types_oriented()


def test_trims_defects_stacks():
    instance_builder = psg.InstanceBuilder()
    instance_builder.set_objective(psg.Objective.Knapsack)
    bin_type_id = instance_builder.add_bin_type(
            100, 50,
            trims={
                "left_trim": 1, "left_trim_type": psg.TrimType.Hard,
                "right_trim": 2, "right_trim_type": psg.TrimType.Soft,
                "bottom_trim": 3, "bottom_trim_type": psg.TrimType.Hard,
                "top_trim": 4, "top_trim_type": psg.TrimType.Soft})
    instance_builder.add_defect(bin_type_id, 20, 10, 5, 6)
    instance_builder.add_item_type(10, 20, stack_id=0)
    instance_builder.add_item_type(15, 20, stack_id=0)
    instance = instance_builder.build()
    bin_type = instance.bin_type(0)
    assert bin_type.left_trim == 1
    assert bin_type.left_trim_type == psg.TrimType.Hard
    assert bin_type.right_trim == 2
    assert bin_type.right_trim_type == psg.TrimType.Soft
    assert bin_type.bottom_trim == 3
    assert bin_type.top_trim == 4
    assert bin_type.area() == (100 - 1 - 2) * (50 - 3 - 4)
    assert instance.number_of_defects() == 1
    defect = bin_type.defects[0]
    assert (defect.x, defect.y, defect.w, defect.h) == (20, 10, 5, 6)
    assert (defect.left(), defect.right(), defect.bottom(), defect.top()) == (20, 25, 10, 16)
    assert instance.item_type(1).stack_id == 0
    assert instance.number_of_stacks() == 1
    assert instance.stack_size(0) == 2
    assert {instance.item(0, 0), instance.item(0, 1)} == {0, 1}
    with pytest.raises(IndexError):
        instance.item(0, 2)



def test_add_item_type_keywords():
    instance_builder = psg.InstanceBuilder()
    instance_builder.set_objective(psg.Objective.Knapsack)
    instance_builder.add_bin_type(1000, 700)
    item_type_id = instance_builder.add_item_type(
            250, 200, oriented=True, stack_id=0, profit=30, copies=3, copies_min=1)
    assert item_type_id == 0
    instance = instance_builder.build()
    item_type = instance.item_type(0)
    assert item_type.w == 250
    assert item_type.h == 200
    assert item_type.oriented
    assert item_type.stack_id == 0
    assert item_type.profit == 30
    assert item_type.copies == 3
    assert item_type.copies_min == 1


def test_add_item_type_defaults():
    instance_builder = psg.InstanceBuilder()
    instance_builder.set_objective(psg.Objective.Knapsack)
    instance_builder.add_bin_type(1000, 700)
    instance_builder.add_item_type(250, 200)
    instance_builder.add_item_type(10, 20, copies=4)
    instance = instance_builder.build()
    item_type = instance.item_type(0)
    assert not item_type.oriented
    assert item_type.profit == 250 * 200
    assert item_type.copies == 1
    # 'copies_min' is resolved in 'build': 0 for the Knapsack objective.
    assert item_type.copies_min == 0
    assert instance.item_type(1).profit == 10 * 20
    assert instance.item_type(1).copies == 4
    assert instance.item_type(1).copies_min == 0

    # For the other objectives, 'copies_min' defaults to 'copies'.
    instance_builder = psg.InstanceBuilder()
    instance_builder.set_objective(psg.Objective.BinPacking)
    instance_builder.add_bin_type(1000, 700, copies=10)
    instance_builder.add_item_type(250, 200, copies=3)
    instance = instance_builder.build()
    assert instance.item_type(0).copies_min == 3


def test_add_item_type_keyword_only():
    instance_builder = psg.InstanceBuilder()
    with pytest.raises(TypeError):
        instance_builder.add_item_type(250, 200, True)
    with pytest.raises(TypeError):
        instance_builder.add_bin_type(1000, 700, 10)


def test_add_item_type_invalid_keyword_value():
    instance_builder = psg.InstanceBuilder()
    with pytest.raises(ValueError):
        instance_builder.add_item_type(250, 200, copies_min=-2)


def test_add_bin_type_keywords():
    instance_builder = psg.InstanceBuilder()
    instance_builder.set_objective(psg.Objective.VariableSizedBinPacking)
    bin_type_id = instance_builder.add_bin_type(1000, 700, cost=10, copies=5, copies_min=2)
    assert bin_type_id == 0
    instance_builder.add_item_type(250, 200)
    instance = instance_builder.build()
    bin_type = instance.bin_type(0)
    assert bin_type.w == 1000
    assert bin_type.h == 700
    assert bin_type.cost == 10
    assert bin_type.copies == 5
    assert bin_type.copies_min == 2
    assert instance.number_of_bins() == 5


def test_add_bin_type_defaults():
    instance_builder = psg.InstanceBuilder()
    instance_builder.add_bin_type(1000, 700)
    instance_builder.add_item_type(250, 200)
    instance = instance_builder.build()
    bin_type = instance.bin_type(0)
    assert bin_type.cost == 1000 * 700
    assert bin_type.copies == 1
    assert bin_type.copies_min == 0
    assert (bin_type.left_trim, bin_type.right_trim) == (0, 0)
    assert (bin_type.bottom_trim, bin_type.top_trim) == (0, 0)
    assert bin_type.left_trim_type == psg.TrimType.Hard
    assert bin_type.right_trim_type == psg.TrimType.Soft
    assert bin_type.bottom_trim_type == psg.TrimType.Hard
    assert bin_type.top_trim_type == psg.TrimType.Soft


def test_add_bin_type_trims():
    instance_builder = psg.InstanceBuilder()
    instance_builder.add_bin_type(
            100, 50,
            trims={
                "left_trim": 1, "left_trim_type": psg.TrimType.Soft,
                "right_trim": 2, "right_trim_type": psg.TrimType.Hard,
                "bottom_trim": 3, "bottom_trim_type": psg.TrimType.Soft,
                "top_trim": 4, "top_trim_type": psg.TrimType.Hard})
    # Missing keys: no trim, default trim types.
    instance_builder.add_bin_type(100, 50, trims={"left_trim": 5, "top_trim": 6})
    instance_builder.add_item_type(10, 10)
    instance = instance_builder.build()
    bin_type = instance.bin_type(0)
    assert (bin_type.left_trim, bin_type.right_trim) == (1, 2)
    assert (bin_type.bottom_trim, bin_type.top_trim) == (3, 4)
    assert bin_type.left_trim_type == psg.TrimType.Soft
    assert bin_type.right_trim_type == psg.TrimType.Hard
    assert bin_type.bottom_trim_type == psg.TrimType.Soft
    assert bin_type.top_trim_type == psg.TrimType.Hard
    bin_type = instance.bin_type(1)
    assert (bin_type.left_trim, bin_type.right_trim) == (5, 0)
    assert (bin_type.bottom_trim, bin_type.top_trim) == (0, 6)
    assert bin_type.left_trim_type == psg.TrimType.Hard
    assert bin_type.right_trim_type == psg.TrimType.Soft
    assert bin_type.bottom_trim_type == psg.TrimType.Hard
    assert bin_type.top_trim_type == psg.TrimType.Soft
    assert bin_type.area() == (100 - 5) * (50 - 6)


def test_add_bin_type_trims_invalid():
    instance_builder = psg.InstanceBuilder()
    with pytest.raises(ValueError):
        instance_builder.add_bin_type(100, 50, trims={"left": 1})
    with pytest.raises(ValueError):
        instance_builder.add_bin_type(100, 50, trims={"left_trim": 100})


def test_removed_setters():
    for name in [
            "set_item_type_profit",
            "set_item_type_copies",
            "set_item_type_copies_min",
            "set_bin_type_cost",
            "set_bin_type_copies",
            "set_bin_type_copies_min",
            "add_trims"]:
        assert not hasattr(psg.InstanceBuilder, name), name


INSTANCE_BUILDER_HEADER = os.path.join(
        os.path.dirname(__file__), "..", "..", "include", "packingsolver",
        "rectangleguillotine", "instance_builder.hpp")

# Per-type C++ methods whose keyword name (or target method) isn't simply
# derived from the setter name: C++ name -> (Python method, keyword).
RENAMED = {
    "add_trims": ("add_bin_type", "trims"),
}

# Per-type C++ methods that are not keywords of 'add_item_type' /
# 'add_bin_type': C++ name -> reason.
NOT_BOUND = {
    "add_defect": "creates a defect; kept as a separate method",
    "add_bin_type_resource": "creates a resource and returns its id; kept as a separate method",
    "add_resource_consumption": "needs a resource id and an item type id; kept as a separate method",
}


def keyword_arguments(function):
    """Keyword-only argument names of a nanobind function."""
    signatures = getattr(function, "__nb_signature__", None)
    if signatures:
        texts = [signature[0] for signature in signatures]
    else:
        texts = [function.__doc__.splitlines()[0]]
    names = set()
    for text in texts:
        arguments = text[text.index("(") + 1:text.rindex(")")]
        if "*," not in arguments:
            continue
        keyword_part = arguments[arguments.index("*,") + 2:]
        names.update(re.findall(r"(\w+)\s*:", keyword_part))
    return names


def per_type_methods():
    """Per-type methods declared in the C++ instance builder header."""
    with open(INSTANCE_BUILDER_HEADER) as header_file:
        header = header_file.read()
    names = set(re.findall(
        r"\b((?:set|add)_(?:item|bin)_type_\w+)\s*\(", header))
    names.update(re.findall(
        r"\b(\w+)\s*\(\s*(?:BinTypeId\s+bin_type_id|ItemTypeId\s+item_type_id)\b",
        header))
    return names


def test_per_type_methods_sync():
    keywords = {
        "add_item_type": keyword_arguments(psg.InstanceBuilder.add_item_type),
        "add_bin_type": keyword_arguments(psg.InstanceBuilder.add_bin_type),
    }
    assert {"oriented", "stack_id", "profit", "copies", "copies_min"} <= keywords["add_item_type"]
    assert {"cost", "trims", "copies", "copies_min"} <= keywords["add_bin_type"]
    methods = per_type_methods()
    # Sanity check of the header parsing.
    assert "set_item_type_copies" in methods
    assert "add_trims" in methods
    for name in sorted(methods):
        if name in NOT_BOUND:
            assert hasattr(psg.InstanceBuilder, name), name
            continue
        if name in RENAMED:
            method, keyword = RENAMED[name]
        else:
            match = re.fullmatch(r"(?:set|add)_(item|bin)_type_(\w+)", name)
            assert match is not None, (
                    name + ": per-type method neither mapped to a keyword "
                    "(RENAMED) nor listed in NOT_BOUND")
            method, keyword = "add_" + match.group(1) + "_type", match.group(2)
        assert keyword in keywords[method], (
                name + ": no keyword '" + keyword + "' in '" + method + "'")
        assert not hasattr(psg.InstanceBuilder, name), name
    for name in list(RENAMED) + list(NOT_BOUND):
        assert name in methods, name + ": not in the C++ header anymore"


def test_bin_packing():
    output = psg.optimize(bin_packing_instance(5), quiet_parameters())
    solution = output.solution
    assert solution.feasible()
    assert solution.full()
    assert solution.number_of_items() == 5
    assert solution.number_of_bins() == 2
    assert output.bin_packing_bound == 2
    assert output.is_proven_optimal()
    with pytest.raises(IndexError):
        solution.bin(solution.number_of_different_bins())
    with pytest.raises(IndexError):
        solution.item_copies(1)
    with pytest.raises(IndexError):
        solution.bin_copies(1)


def test_solution_nodes():
    output = psg.optimize(bin_packing_instance(5), quiet_parameters())
    solution = output.solution
    for bin_pos in range(solution.number_of_different_bins()):
        solution_bin = solution.bin(bin_pos)
        assert solution_bin.bin_type_id == 0
        assert solution_bin.copies >= 1
        nodes = solution_bin.nodes
        # The first node is the bin itself.
        root = nodes[0]
        assert root.f == -1
        assert root.d == 0
        assert (root.l, root.r, root.b, root.t) == (0, 10, 0, 10)
        for node_id, node in enumerate(nodes):
            assert 0 <= node.l <= node.r <= 10
            assert 0 <= node.b <= node.t <= 10
            for child_id in node.children:
                assert nodes[child_id].f == node_id
    items = solution_item_nodes(solution)
    assert sum(
            solution.bin(bin_pos).copies
            for bin_pos in range(solution.number_of_different_bins())
            for node in solution.bin(bin_pos).nodes
            if node.f != -1 and node.item_type_id >= 0) == 5
    for node in items:
        assert node.item_type_id == 0
        assert node.r - node.l == 5
        assert node.t - node.b == 5


def test_number_of_stages():
    instance_builder = psg.InstanceBuilder()
    instance_builder.set_objective(psg.Objective.BinPacking)
    instance_builder.add_bin_type(10, 10, copies=10)
    instance_builder.add_item_type(5, 5, copies=4)
    instance_builder.set_number_of_stages(2)
    instance_builder.set_cut_type(psg.CutType.Exact)
    instance_builder.set_first_stage_orientation(psg.CutOrientation.Horizontal)
    output = psg.optimize(instance_builder.build(), quiet_parameters())
    solution = output.solution
    assert solution.feasible()
    assert solution.number_of_stages_feasible()
    assert solution.number_of_bins() == 1
    assert output.is_proven_optimal()
    assert solution.bin(0).first_cut_orientation == psg.CutOrientation.Horizontal
    assert solution.bin(0).number_of_stages_real <= 2


def test_knapsack():
    instance_builder = psg.InstanceBuilder()
    instance_builder.set_objective(psg.Objective.Knapsack)
    instance_builder.add_bin_type(10, 10)
    # The 10x6 item with profit 6 and the 10x4 item fill the bin; the area
    # bound is tight.
    for width, height, profit in [(10, 6, 6), (10, 4, 4), (10, 6, 3)]:
        instance_builder.add_item_type(width, height, oriented=True, profit=profit)
    output = psg.optimize(instance_builder.build(), quiet_parameters())
    assert output.solution.profit() == 10
    assert output.solution.item_copies(0) == 1
    assert output.solution.item_copies(1) == 1
    assert output.solution.item_copies(2) == 0
    assert output.knapsack_bound == pytest.approx(10)
    assert output.is_proven_optimal()


def test_open_dimension_x():
    instance_builder = psg.InstanceBuilder()
    instance_builder.set_objective(psg.Objective.OpenDimensionX)
    instance_builder.add_bin_type(1000, 10)
    instance_builder.add_item_type(10, 10, oriented=True, copies=5)
    output = psg.optimize(instance_builder.build(), quiet_parameters())
    assert output.solution.full()
    assert output.solution.width() == 50
    assert output.open_dimension_x_bound == 50
    assert output.is_proven_optimal()


def test_read_csv():
    path = os.path.join(DATA_DIR, "bin_packing_3nvo")
    instance_builder = psg.InstanceBuilder()
    instance_builder.read_item_types(os.path.join(path, "items.csv"))
    instance_builder.read_bin_types(os.path.join(path, "bins.csv"))
    instance_builder.read_parameters(os.path.join(path, "parameters.csv"))
    instance = instance_builder.build()
    assert instance.objective() == psg.Objective.BinPacking
    assert instance.parameters().number_of_stages == 3
    assert instance.parameters().cut_type == psg.CutType.NonExact
    assert instance.parameters().first_stage_orientation == psg.CutOrientation.Vertical
    output = psg.optimize(instance, quiet_parameters())
    # The items exactly fill one 20x20 bin.
    assert output.solution.number_of_bins() == 1
    assert output.solution.full()
    assert output.is_proven_optimal()


def test_read_defects():
    path = os.path.join(DATA_DIR, "knapsack_2nvo_defects_2")
    instance_builder = psg.InstanceBuilder()
    instance_builder.read_item_types(os.path.join(path, "items.csv"))
    instance_builder.read_bin_types(os.path.join(path, "bins.csv"))
    instance_builder.read_defects(os.path.join(path, "defects.csv"))
    instance_builder.read_parameters(os.path.join(path, "parameters.csv"))
    instance = instance_builder.build()
    assert instance.objective() == psg.Objective.Knapsack
    assert instance.number_of_defects() == 1
    defect = instance.bin_type(0).defects[0]
    assert (defect.x, defect.y, defect.w, defect.h) == (20, 5, 10, 5)


def test_read_missing_file():
    instance_builder = psg.InstanceBuilder()
    with pytest.raises(Exception):
        instance_builder.read_item_types("this/file/does/not/exist.csv")


def test_write(tmp_path):
    instance_builder = psg.InstanceBuilder()
    instance_builder.set_objective(psg.Objective.BinPacking)
    instance_builder.add_bin_type(10, 10, copies=10)
    instance_builder.add_item_type(5, 5, copies=5)
    instance_builder.set_number_of_stages(2)
    instance = instance_builder.build()
    instance_path = str(tmp_path / "instance.json")
    instance.write(instance_path, psg.InstanceFormat.Json)
    instance_builder = psg.InstanceBuilder()
    instance_builder.read(instance_path)
    instance_read = instance_builder.build()
    assert instance_read.number_of_items() == 5
    assert instance_read.parameters().number_of_stages == 2

    output = psg.optimize(instance, quiet_parameters())
    certificate_path = str(tmp_path / "solution.csv")
    output.solution.write(certificate_path)
    assert os.path.getsize(certificate_path) > 0


def test_to_json():
    output = psg.optimize(bin_packing_instance(5), quiet_parameters())
    json = output.to_json()
    assert isinstance(json, dict)
    assert json["BinPackingBound"] == 2
    assert json["Solution"] == output.solution.to_json()
    assert json["Solution"]["NumberOfBins"] == 2
    assert json["Solution"]["Feasible"]["Feasible"]


def test_parameters():
    parameters = psg.OptimizeParameters()
    assert parameters.time_limit == math.inf
    parameters.time_limit = 1.5
    assert parameters.time_limit == 1.5
    parameters.optimization_mode = psg.OptimizationMode.NotAnytimeSequential
    parameters.tree_search_guides = [0, 1]
    assert parameters.tree_search_guides == [0, 1]
    parameters.use_tree_search = True
    assert parameters.use_tree_search
    parameters.many_items_in_bins_threshold_2 = 32
    assert parameters.many_items_in_bins_threshold_2 == 32
    parameters.reduction_parameters.reduce = False
    assert not parameters.reduction_parameters.reduce
    parameters.reduction_parameters.merge_identical_items = False
    assert not parameters.reduction_parameters.merge_identical_items
    parameters.verbosity_level = 0
    output = psg.optimize(bin_packing_instance(5), parameters)
    assert output.solution.number_of_bins() == 2


def test_lifetimes():
    """Outputs and solutions stay valid after their instance is dropped."""
    output = psg.optimize(bin_packing_instance(5), quiet_parameters())
    gc.collect()
    solution = output.solution
    del output
    gc.collect()
    assert solution.number_of_bins() == 2
    assert solution.bin(0).bin_type_id == 0
    assert solution.bin(0).nodes[0].f == -1


def test_new_solution_callback():
    outputs = []
    parameters = quiet_parameters()
    parameters.new_solution_callback = outputs.append
    psg.optimize(bin_packing_instance(5), parameters)
    assert len(outputs) >= 1
    gc.collect()
    assert outputs[-1].solution.number_of_bins() == 2


def test_column_generation():
    """Column generation (LP solved with HiGHS in the Python module)."""
    instance_builder = psg.InstanceBuilder()
    instance_builder.set_objective(psg.Objective.BinPacking)
    instance_builder.add_bin_type(100, 100, copies=20)
    for width, height, copies in [(30, 40, 6), (50, 20, 5), (70, 35, 3)]:
        instance_builder.add_item_type(width, height, oriented=True, copies=copies)
    parameters = quiet_parameters(time_limit=10.0, use_column_generation=True)
    output = psg.optimize(instance_builder.build(), parameters)
    assert output.bin_packing_bound >= 2
    if output.solution.feasible():
        assert output.solution.number_of_bins() >= output.bin_packing_bound


def test_column_generation_strips():
    """Column generation strips (LP solved with HiGHS in the Python module)."""
    instance_builder = psg.InstanceBuilder()
    instance_builder.set_objective(psg.Objective.Knapsack)
    instance_builder.add_bin_type(100, 100)
    for width, height, profit, copies in [(30, 40, 15, 3), (50, 20, 11, 4), (70, 35, 30, 2)]:
        instance_builder.add_item_type(
                width, height, oriented=True, profit=profit, copies=copies)
    instance_builder.set_number_of_stages(2)
    parameters = quiet_parameters(time_limit=10.0, use_column_generation_strips=True)
    output = psg.optimize(instance_builder.build(), parameters)
    assert output.solution.feasible()
    assert output.solution.profit() > 0
    assert output.solution.profit() <= output.knapsack_bound + 1e-6
