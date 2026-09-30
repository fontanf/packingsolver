import gc
import math
import os
import re

import pytest

import packingsolver.box as psb


DATA_DIR = os.path.join(
        os.path.dirname(__file__), "..", "..", "data", "box", "tests")


def quiet_parameters(**kwargs):
    parameters = psb.OptimizeParameters()
    parameters.verbosity_level = 0
    for name, value in kwargs.items():
        setattr(parameters, name, value)
    return parameters


def bin_packing_instance(item_copies):
    """'item_copies' 5x5x5 cubes in 10x10x10 bins (8 cubes per bin)."""
    instance_builder = psb.InstanceBuilder()
    instance_builder.set_objective(psb.Objective.BinPacking)
    instance_builder.add_bin_type(10, 10, 10, copies=10)
    instance_builder.add_item_type(5, 5, 5, copies=item_copies)
    return instance_builder.build()


def test_instance_builder():
    instance = bin_packing_instance(9)
    assert instance.objective() == psb.Objective.BinPacking
    assert instance.number_of_item_types() == 1
    assert instance.number_of_items() == 9
    assert instance.number_of_bin_types() == 1
    assert instance.number_of_bins() == 10
    assert instance.bin_type_id(0) == 0
    item_type = instance.item_type(0)
    assert (item_type.x, item_type.y, item_type.z) == (5, 5, 5)
    assert item_type.copies == 9
    assert item_type.copies_min == 9
    assert item_type.volume() == 125
    # Default rotation: identity only.
    assert item_type.rotations == [psb.Rotation.XYZ]
    assert item_type.can_rotate(psb.Rotation.XYZ)
    assert not item_type.can_rotate(psb.Rotation.YXZ)
    bin_type = instance.bin_type(0)
    assert (bin_type.x, bin_type.y, bin_type.z) == (10, 10, 10)
    assert bin_type.copies == 10
    assert bin_type.volume() == 1000
    assert instance.item_volume() == 9 * 125
    assert instance.fits_some_bin(0)
    assert "Number of item types" in instance.format()
    with pytest.raises(IndexError):
        instance.item_type(1)
    with pytest.raises(IndexError):
        instance.bin_type(-1)
    with pytest.raises(IndexError):
        instance.bin_type_id(10)
    with pytest.raises(IndexError):
        instance.fits_some_bin(1)


def test_item_type_rotations():
    instance_builder = psb.InstanceBuilder()
    instance_builder.add_bin_type(10, 10, 10)
    instance_builder.add_item_type(
            1, 2, 3, rotations=[psb.Rotation.XYZ, psb.Rotation.ZYX])
    # The list is the full set of allowed rotations: the identity is not
    # added when it is not listed.
    instance_builder.add_item_type(1, 2, 3, rotations=[psb.Rotation.YXZ])
    # An empty list is the same as omitting the keyword.
    instance_builder.add_item_type(1, 2, 3, rotations=[])
    instance = instance_builder.build()
    assert instance.item_type(0).rotations == [
        psb.Rotation.XYZ, psb.Rotation.ZYX]
    assert instance.item_type(1).rotations == [psb.Rotation.YXZ]
    assert not instance.item_type(1).can_rotate(psb.Rotation.XYZ)
    assert instance.item_type(2).rotations == [psb.Rotation.XYZ]


def test_add_item_type_keywords():
    instance_builder = psb.InstanceBuilder()
    instance_builder.add_bin_type(10, 10, 10)
    item_type_id = instance_builder.add_item_type(
            1, 2, 3,
            rotations=[psb.Rotation.XYZ, psb.Rotation.YXZ],
            weight=4.5,
            profit=12,
            copies=7,
            copies_min=3)
    assert item_type_id == 0
    instance = instance_builder.build()
    item_type = instance.item_type(0)
    assert (item_type.x, item_type.y, item_type.z) == (1, 2, 3)
    assert item_type.rotations == [psb.Rotation.XYZ, psb.Rotation.YXZ]
    assert item_type.weight == 4.5
    assert item_type.profit == 12
    assert item_type.copies == 7
    assert item_type.copies_min == 3


def test_add_item_type_defaults():
    instance_builder = psb.InstanceBuilder()
    instance_builder.set_objective(psb.Objective.BinPacking)
    instance_builder.add_bin_type(10, 10, 10)
    instance_builder.add_item_type(1, 2, 3)
    # Only some keywords: the other attributes keep their defaults.
    instance_builder.add_item_type(2, 2, 2, copies=5)
    instance = instance_builder.build()
    item_type = instance.item_type(0)
    assert item_type.profit == 6  # The volume.
    assert item_type.copies == 1
    # Resolved to 'copies' by 'build()' (except for Knapsack objectives).
    assert item_type.copies_min == 1
    assert item_type.weight == 0
    assert item_type.rotations == [psb.Rotation.XYZ]
    item_type = instance.item_type(1)
    assert item_type.profit == 8
    assert item_type.copies == 5
    assert item_type.copies_min == 5


def test_add_bin_type_keywords():
    instance_builder = psb.InstanceBuilder()
    bin_type_id = instance_builder.add_bin_type(
            10, 20, 30,
            cost=42,
            maximum_weight=1000,
            copies=4,
            copies_min=2)
    assert bin_type_id == 0
    instance_builder.add_item_type(1, 1, 1)
    instance = instance_builder.build()
    bin_type = instance.bin_type(0)
    assert (bin_type.x, bin_type.y, bin_type.z) == (10, 20, 30)
    assert bin_type.cost == 42
    assert bin_type.maximum_weight == 1000
    assert bin_type.copies == 4
    assert bin_type.copies_min == 2


def test_add_bin_type_defaults():
    instance_builder = psb.InstanceBuilder()
    instance_builder.add_bin_type(10, 20, 30)
    instance_builder.add_bin_type(10, 20, 30, copies=3)
    instance_builder.add_item_type(1, 1, 1)
    instance = instance_builder.build()
    bin_type = instance.bin_type(0)
    assert bin_type.copies == 1
    assert bin_type.copies_min == 0
    assert bin_type.maximum_weight == math.inf
    bin_type = instance.bin_type(1)
    assert bin_type.copies == 3
    assert bin_type.copies_min == 0
    assert bin_type.maximum_weight == math.inf
    # Omitting 'cost' gives the same cost as the C++ default.
    assert bin_type.cost == instance.bin_type(0).cost


REMOVED_METHODS = [
    "set_bin_type_cost",
    "set_bin_type_maximum_weight",
    "set_bin_type_copies",
    "set_bin_type_copies_min",
    "add_item_type_rotation",
    "set_item_type_weight",
    "set_item_type_profit",
    "set_item_type_copies",
    "set_item_type_copies_min",
]


@pytest.mark.parametrize("name", REMOVED_METHODS)
def test_removed_methods(name):
    assert not hasattr(psb.InstanceBuilder, name)


def test_kept_methods():
    for name in [
            "set_objective",
            "read",
            "read_parameters",
            "read_bin_types",
            "read_item_types",
            "set_weight_tolerance",
            "add_bin_type_resource",
            "add_resource_consumption",
            "set_bin_types_infinite_x",
            "set_bin_types_infinite_y",
            "set_bin_types_infinite_copies",
            "set_bin_types_unweighted",
            "set_item_types_profits_auto",
            "set_item_types_infinite_copies",
            "set_item_types_unweighted",
            "set_item_types_oriented",
            "build"]:
        assert hasattr(psb.InstanceBuilder, name), name


HEADER_PATH = os.path.join(
        os.path.dirname(__file__), "..", "..",
        "include", "packingsolver", "box", "instance_builder.hpp")

# C++ per-type method -> Python keyword, for the ones not named after the
# method suffix.
RENAMED = {
    "add_item_type_rotation": "rotations",
}

# C++ per-type methods deliberately not exposed as keywords.
NOT_BOUND = {
    "add_bin_type_resource":
        "creates a resource with its own id; kept as a builder method",
}


def keyword_names(function):
    """Keyword-only argument names of a nanobind function."""
    names = set()
    signatures = getattr(function, "__nb_signature__", None)
    if signatures:
        texts = [signature[0] for signature in signatures]
    else:
        texts = [function.__doc__]
    for text in texts:
        match = re.search(r"\*,(.*?)\)\s*->", text, re.S)
        if match is None:
            continue
        names.update(re.findall(r"(\w+)\s*:", match.group(1)))
    return names


def test_header_sync():
    with open(HEADER_PATH) as f:
        header = f.read()
    declarations = re.findall(
            r"\b((?:set|add)_(item|bin)_type_\w+)\s*\(\s*"
            r"(?:ItemTypeId|BinTypeId)\s+(\w+)",
            header)
    methods = {
        kind: sorted({
            name for name, k, parameter in declarations
            if k == kind and parameter == kind + "_type_id"})
        for kind in ("item", "bin")}
    assert "set_item_type_copies" in methods["item"]
    assert "set_bin_type_copies" in methods["bin"]
    keywords = {
        "item": keyword_names(psb.InstanceBuilder.add_item_type),
        "bin": keyword_names(psb.InstanceBuilder.add_bin_type),
    }
    assert "copies" in keywords["item"]
    assert "copies" in keywords["bin"]
    for kind in ("item", "bin"):
        prefix_length = len("set_" + kind + "_type_")
        for name in methods[kind]:
            if name in NOT_BOUND:
                continue
            keyword = RENAMED.get(name, name[prefix_length:])
            assert keyword in keywords[kind], (
                    name + " is neither a keyword of add_" + kind
                    + "_type nor in NOT_BOUND")
    # The NOT_BOUND and RENAMED entries are still in the header.
    all_methods = set(methods["item"]) | set(methods["bin"])
    for name in list(NOT_BOUND) + list(RENAMED):
        assert name in all_methods, name


def test_instance_builder_invalid_argument():
    instance_builder = psb.InstanceBuilder()
    with pytest.raises(ValueError):
        instance_builder.add_item_type(-1, 5, 5)
    with pytest.raises(ValueError):
        instance_builder.add_item_type(5, 5, 5, copies=0)
    with pytest.raises(ValueError):
        instance_builder.add_bin_type(10, 10, 10, copies=1, copies_min=2)
    # Keyword-only arguments.
    with pytest.raises(TypeError):
        instance_builder.add_item_type(5, 5, 5, 2)
    with pytest.raises(TypeError):
        instance_builder.add_bin_type(10, 10, 10, 2)


def test_build_resets_builder():
    instance_builder = psb.InstanceBuilder()
    instance_builder.add_bin_type(10, 10, 10)
    instance_builder.add_item_type(5, 5, 5)
    instance = instance_builder.build()
    assert instance.number_of_item_types() == 1
    assert instance_builder.build().number_of_item_types() == 0


def test_bin_packing():
    output = psb.optimize(bin_packing_instance(9), quiet_parameters())
    solution = output.solution
    assert solution.feasible()
    assert solution.full()
    assert solution.number_of_items() == 9
    assert solution.number_of_bins() == 2
    assert output.bin_packing_bound == 2
    assert output.is_proven_optimal()

    items = [
        item
        for bin_pos in range(solution.number_of_different_bins())
        for item in solution.bin(bin_pos).items]
    assert len(items) == 9
    for item in items:
        assert item.item_type_id == 0
        assert item.rotation == psb.Rotation.XYZ
        assert 0 <= item.x <= 5 and 0 <= item.y <= 5 and 0 <= item.z <= 5
    with pytest.raises(IndexError):
        solution.bin(solution.number_of_different_bins())
    with pytest.raises(IndexError):
        solution.item_copies(1)
    with pytest.raises(IndexError):
        solution.bin_copies(1)


def test_knapsack():
    instance_builder = psb.InstanceBuilder()
    instance_builder.set_objective(psb.Objective.Knapsack)
    instance_builder.add_bin_type(10, 10, 10)
    # Only one of the two 10x10x6 items fits; the one with the largest profit
    # must be selected, together with the 10x10x4 item.
    for x, y, z, profit in [(10, 10, 6, 5), (10, 10, 6, 7), (10, 10, 4, 3)]:
        instance_builder.add_item_type(x, y, z, profit=profit)
    output = psb.optimize(instance_builder.build(), quiet_parameters())
    assert output.solution.profit() == 10
    assert output.solution.item_copies(1) == 1
    assert output.solution.item_copies(0) == 0
    assert output.knapsack_bound >= 10


def test_variable_sized_bin_packing():
    instance_builder = psb.InstanceBuilder()
    instance_builder.set_objective(psb.Objective.VariableSizedBinPacking)
    # Two small bins cost 14, one large bin costs 10.
    instance_builder.add_bin_type(6, 5, 5, cost=7, copies=2)
    instance_builder.add_bin_type(12, 5, 5, cost=10)
    instance_builder.add_item_type(6, 5, 5, copies=2)
    output = psb.optimize(instance_builder.build(), quiet_parameters())
    assert output.solution.full()
    assert output.solution.cost() == 10
    assert output.solution.bin_copies(1) == 1
    assert output.variable_sized_bin_packing_bound <= 10


def test_open_dimension_x():
    instance_builder = psb.InstanceBuilder()
    instance_builder.set_objective(psb.Objective.OpenDimensionX)
    instance_builder.add_bin_type(100, 20, 10)
    instance_builder.add_item_type(10, 10, 10, copies=4)
    output = psb.optimize(instance_builder.build(), quiet_parameters())
    assert output.solution.full()
    assert output.solution.x_max() == 20
    assert output.open_dimension_x_bound <= 20


def test_read_csv():
    path = os.path.join(DATA_DIR, "knapsack_4_items")
    instance_builder = psb.InstanceBuilder()
    instance_builder.read_item_types(os.path.join(path, "items.csv"))
    instance_builder.read_bin_types(os.path.join(path, "bins.csv"))
    instance_builder.read_parameters(os.path.join(path, "parameters.csv"))
    instance = instance_builder.build()
    assert instance.objective() == psb.Objective.Knapsack
    assert instance.number_of_items() == 4
    output = psb.optimize(instance, quiet_parameters())
    # The expected solution packs every item.
    assert output.solution.number_of_items() == 4
    assert output.is_proven_optimal()


def test_read_missing_file():
    instance_builder = psb.InstanceBuilder()
    with pytest.raises(Exception):
        instance_builder.read_item_types("this/file/does/not/exist.csv")


def test_write(tmp_path):
    instance = bin_packing_instance(9)
    instance_path = str(tmp_path / "instance.json")
    instance.write(instance_path, psb.InstanceFormat.Json)
    instance_builder = psb.InstanceBuilder()
    instance_builder.read(instance_path)
    instance_2 = instance_builder.build()
    assert instance_2.number_of_items() == 9
    assert instance_2.item_type(0).z == 5

    output = psb.optimize(instance, quiet_parameters())
    certificate_path = str(tmp_path / "solution.csv")
    output.solution.write(certificate_path)
    assert os.path.getsize(certificate_path) > 0


def test_to_json():
    output = psb.optimize(bin_packing_instance(9), quiet_parameters())
    json = output.to_json()
    assert isinstance(json, dict)
    assert json["BinPackingBound"] == 2
    assert "OpenDimensionZBound" in json
    assert json["Solution"] == output.solution.to_json()
    assert json["Solution"]["NumberOfBins"] == 2


def test_parameters():
    parameters = psb.OptimizeParameters()
    assert parameters.time_limit == math.inf
    assert parameters.linear_programming_solver_name in list(
            psb.LinearProgrammingSolver)
    parameters.time_limit = 1.5
    assert parameters.time_limit == 1.5
    parameters.optimization_mode = psb.OptimizationMode.NotAnytimeSequential
    parameters.tree_search_guides = [0, 1]
    assert parameters.tree_search_guides == [0, 1]
    parameters.many_items_in_bins_threshold_2 = 32
    assert parameters.many_items_in_bins_threshold_2 == 32
    parameters.reduction_parameters.reduce = False
    assert not parameters.reduction_parameters.reduce
    parameters.reduction_parameters.merge_identical_items = False
    assert not parameters.reduction_parameters.merge_identical_items
    parameters.verbosity_level = 0
    output = psb.optimize(bin_packing_instance(9), parameters)
    assert output.solution.number_of_bins() == 2


def test_lifetimes():
    """Outputs and solutions stay valid after their instance is dropped."""
    output = psb.optimize(bin_packing_instance(9), quiet_parameters())
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
    psb.optimize(bin_packing_instance(9), parameters)
    assert len(outputs) >= 1
    gc.collect()
    assert outputs[-1].solution.number_of_bins() == 2


@pytest.mark.parametrize("algorithm", [
    "use_column_generation",
    "use_sequential_value_correction",
    "use_sequential_single_knapsack",
])
def test_multiple_bins_algorithms(algorithm):
    """Algorithms for multiple bins, the first one solving LPs."""
    instance_builder = psb.InstanceBuilder()
    instance_builder.set_objective(psb.Objective.BinPacking)
    instance_builder.add_bin_type(100, 100, 100, copies=20)
    for x, y, z, copies in [(50, 50, 60, 10), (50, 50, 40, 10)]:
        instance_builder.add_item_type(x, y, z, copies=copies)
    parameters = quiet_parameters(time_limit=10.0)
    setattr(parameters, algorithm, True)
    output = psb.optimize(instance_builder.build(), parameters)
    # A 60-item and a 40-item stack in each quarter of the bin: 4 pairs per bin.
    assert output.bin_packing_bound >= 3
    assert output.solution.feasible()
    assert output.solution.full()
    assert output.solution.number_of_bins() >= output.bin_packing_bound
