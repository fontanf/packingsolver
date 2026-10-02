import _thread
import csv
import gc
import math
import os
import random
import re
import threading

import pytest

import packingsolver.boxstacks as psbs


DATA_DIR = os.path.join(
        os.path.dirname(__file__), "..", "..", "data", "boxstacks", "tests")


def quiet_parameters(**kwargs):
    parameters = psbs.OptimizeParameters()
    parameters.verbosity_level = 0
    # In anytime mode, the algorithms keep improving until the time limit
    # unless the bound proves the solution optimal.
    parameters.optimization_mode = psbs.OptimizationMode.NotAnytimeSequential
    parameters.time_limit = 10.0
    for name, value in kwargs.items():
        setattr(parameters, name, value)
    return parameters


def bin_packing_instance(item_copies):
    """'item_copies' 5x5x10 boxes in 10x10x10 bins."""
    instance_builder = psbs.InstanceBuilder()
    instance_builder.set_objective(psbs.Objective.BinPacking)
    instance_builder.add_bin_type(10, 10, 10, copies=10)
    instance_builder.add_item_type(5, 5, 10, copies=item_copies)
    return instance_builder.build()


def stack_instance(copies, z=5, bin_type_keywords=None, **item_keywords):
    """'copies' 10x10x'z' boxes in 10x10x10 bins: only stacking can put two
    boxes in the same bin.

    'item_keywords' and 'bin_type_keywords' are passed to 'add_item_type' and
    'add_bin_type'."""
    instance_builder = psbs.InstanceBuilder()
    instance_builder.set_objective(psbs.Objective.BinPacking)
    bin_type_id = instance_builder.add_bin_type(
            10, 10, 10, copies=10, **(bin_type_keywords or {}))
    instance_builder.add_item_type(10, 10, z, copies=copies, **item_keywords)
    return instance_builder, bin_type_id


def large_knapsack_instance(number_of_item_types, seed=0):
    rng = random.Random(seed)
    instance_builder = psbs.InstanceBuilder()
    instance_builder.set_objective(psbs.Objective.Knapsack)
    instance_builder.add_bin_type(1000, 1000, 1000)
    # Item types with the same stackability id must have the same footprint.
    for stackability_id in range(number_of_item_types):
        instance_builder.add_item_type(
                rng.randint(20, 200), rng.randint(20, 200), rng.randint(20, 200),
                profit=rng.randint(1, 1000),
                stackability_id=stackability_id)
    return instance_builder.build()


def test_instance_builder():
    instance = bin_packing_instance(5)
    assert instance.objective() == psbs.Objective.BinPacking
    assert instance.unloading_constraint() == psbs.UnloadingConstraint.None_
    assert instance.number_of_item_types() == 1
    assert instance.number_of_items() == 5
    assert instance.number_of_bin_types() == 1
    assert instance.number_of_bins() == 10
    assert instance.bin_type_id(9) == 0
    assert instance.item_volume() == 5 * 250
    assert instance.bin_volume() == 10 * 1000
    item_type = instance.item_type(0)
    assert (item_type.x, item_type.y, item_type.z) == (5, 5, 10)
    assert item_type.copies == 5
    assert item_type.copies_min == 5
    assert item_type.volume() == 250
    # Default rotations: oriented.
    assert item_type.rotations == [psbs.Rotation.XYZ]
    assert item_type.can_rotate(psbs.Rotation.XYZ)
    assert not item_type.can_rotate(psbs.Rotation.YXZ)
    assert item_type.stackability_id == 0
    assert item_type.nesting_height == 0
    bin_type = instance.bin_type(0)
    assert (bin_type.x, bin_type.y, bin_type.z) == (10, 10, 10)
    assert bin_type.copies == 10
    assert bin_type.maximum_weight == math.inf
    assert bin_type.defects == []
    assert instance.fits_some_bin(0)
    assert "Number of item types" in instance.format()
    with pytest.raises(IndexError):
        instance.item_type(1)
    with pytest.raises(IndexError):
        instance.bin_type(-1)
    with pytest.raises(IndexError):
        instance.bin_type_id(10)
    with pytest.raises(IndexError):
        instance.group(instance.number_of_groups())
    with pytest.raises(IndexError):
        instance.fits_some_bin(1)


def test_instance_builder_invalid_argument():
    instance_builder = psbs.InstanceBuilder()
    with pytest.raises(ValueError):
        instance_builder.add_item_type(-1, 5, 5)
    with pytest.raises(ValueError):
        instance_builder.add_item_type(5, 5, 5, group_id=-1)
    with pytest.raises(ValueError):
        instance_builder.add_item_type(5, 5, 5, copies_min=-2)
    # The attributes are keyword-only.
    with pytest.raises(TypeError):
        instance_builder.add_item_type(5, 5, 5, 3)
    with pytest.raises(TypeError):
        instance_builder.add_bin_type(10, 10, 10, 3)
    with pytest.raises(TypeError):
        instance_builder.add_item_type(5, 5, 5, unknown=3)


def test_add_item_type_keywords():
    instance_builder = psbs.InstanceBuilder()
    instance_builder.set_objective(psbs.Objective.Knapsack)
    instance_builder.add_bin_type(100, 100, 100)
    assert instance_builder.add_item_type(5, 5, 5) == 0
    item_type_id = instance_builder.add_item_type(
            2, 3, 4,
            rotations=[psbs.Rotation.YXZ, psbs.Rotation.XYZ],
            group_id=1,
            weight=2.5,
            stackability_id=3,
            nesting_height=1,
            maximum_stackability=2,
            maximum_weight_above=7.5,
            profit=12,
            copies=5,
            copies_min=2)
    assert item_type_id == 1
    instance = instance_builder.build()
    item_type = instance.item_type(item_type_id)
    assert (item_type.x, item_type.y, item_type.z) == (2, 3, 4)
    assert item_type.rotations == [psbs.Rotation.YXZ, psbs.Rotation.XYZ]
    assert item_type.can_rotate(psbs.Rotation.YXZ)
    assert not item_type.can_rotate(psbs.Rotation.ZYX)
    assert item_type.group_id == 1
    assert item_type.weight == 2.5
    assert item_type.stackability_id == 3
    assert item_type.nesting_height == 1
    assert item_type.maximum_stackability == 2
    assert item_type.maximum_weight_above == 7.5
    assert item_type.profit == 12
    assert item_type.copies == 5
    assert item_type.copies_min == 2
    assert instance.number_of_groups() == 2
    assert instance.group(1).item_types == [item_type_id]


def test_add_item_type_defaults():
    instance_builder = psbs.InstanceBuilder()
    instance_builder.set_objective(psbs.Objective.Knapsack)
    instance_builder.add_bin_type(100, 100, 100)
    instance_builder.add_item_type(2, 3, 4)
    # 'copies' given but not 'copies_min': 'copies_min' is resolved in
    # 'build()' (0 for a Knapsack objective).
    instance_builder.add_item_type(2, 3, 4, copies=4)
    instance = instance_builder.build()
    item_type = instance.item_type(0)
    assert item_type.profit == 24
    assert item_type.copies == 1
    assert item_type.copies_min == 0
    assert item_type.group_id == 0
    assert item_type.rotations == [psbs.Rotation.XYZ]
    assert item_type.weight == 0
    assert item_type.stackability_id == 0
    assert item_type.nesting_height == 0
    assert item_type.maximum_stackability == 2 ** 31 - 1
    assert item_type.maximum_weight_above == math.inf
    item_type = instance.item_type(1)
    assert item_type.profit == 24
    assert item_type.copies == 4
    assert item_type.copies_min == 0

    # For a Bin Packing objective, 'copies_min' defaults to 'copies'.
    instance_builder = psbs.InstanceBuilder()
    instance_builder.set_objective(psbs.Objective.BinPacking)
    instance_builder.add_bin_type(100, 100, 100)
    instance_builder.add_item_type(2, 3, 4, copies=4)
    assert instance_builder.build().item_type(0).copies_min == 4


def test_add_item_type_rotations():
    instance_builder = psbs.InstanceBuilder()
    instance_builder.add_bin_type(100, 100, 100)
    # An empty list is the same as no rotation given: only 'XYZ'.
    instance_builder.add_item_type(2, 3, 4, rotations=[])
    all_rotations = [psbs.Rotation.XYZ, psbs.Rotation.YXZ]
    instance_builder.add_item_type(2, 3, 4, rotations=all_rotations)
    # The rotations given replace the default 'XYZ' rotation.
    instance_builder.add_item_type(2, 3, 4, rotations=(psbs.Rotation.YXZ,))
    instance = instance_builder.build()
    assert instance.item_type(0).rotations == [psbs.Rotation.XYZ]
    assert instance.item_type(1).rotations == all_rotations
    assert instance.item_type(2).rotations == [psbs.Rotation.YXZ]
    assert not instance.item_type(2).can_rotate(psbs.Rotation.XYZ)
    with pytest.raises(TypeError):
        psbs.InstanceBuilder().add_item_type(2, 3, 4, rotations=[0.5])


def test_add_bin_type_keywords():
    instance_builder = psbs.InstanceBuilder()
    instance_builder.set_objective(psbs.Objective.VariableSizedBinPacking)
    assert instance_builder.add_bin_type(10, 10, 10) == 0
    bin_type_id = instance_builder.add_bin_type(
            20, 10, 10,
            cost=7,
            maximum_weight=100,
            maximum_stack_density=0.5,
            copies=3,
            copies_min=1)
    assert bin_type_id == 1
    instance_builder.add_item_type(5, 5, 5)
    instance = instance_builder.build()
    bin_type = instance.bin_type(bin_type_id)
    assert (bin_type.x, bin_type.y, bin_type.z) == (20, 10, 10)
    assert bin_type.cost == 7
    assert bin_type.maximum_weight == 100
    assert bin_type.maximum_stack_density == 0.5
    assert bin_type.copies == 3
    assert bin_type.copies_min == 1
    # Defaults.
    bin_type = instance.bin_type(0)
    assert bin_type.copies == 1
    assert bin_type.copies_min == 0
    assert bin_type.maximum_weight == math.inf
    assert bin_type.defects == []
    assert instance.number_of_bins() == 4


def test_removed_setters():
    for name in [
            "set_item_type_copies",
            "set_item_type_copies_min",
            "set_item_type_profit",
            "set_item_type_group",
            "set_item_type_weight",
            "set_item_type_stackability_id",
            "set_item_type_nesting_height",
            "set_item_type_maximum_stackability",
            "set_item_type_maximum_weight_above",
            "add_item_type_rotation",
            "set_bin_type_cost",
            "set_bin_type_copies",
            "set_bin_type_copies_min",
            "set_bin_type_maximum_weight",
            "set_bin_type_maximum_stack_density",
            "set_bin_type_semi_trailer_truck_parameters"]:
        assert not hasattr(psbs.InstanceBuilder, name), name
    # Kept.
    for name in [
            "set_objective",
            "add_defect",
            "set_group_weight_constraints",
            "set_unloading_constraint",
            "set_weight_tolerance",
            "set_item_types_oriented",
            "set_bin_types_infinite_copies"]:
        assert hasattr(psbs.InstanceBuilder, name), name


INSTANCE_BUILDER_HEADER = os.path.join(
        os.path.dirname(__file__), "..", "..",
        "include", "packingsolver", "boxstacks", "instance_builder.hpp")

# C++ per-type method -> Python keyword, when it differs from the method
# name suffix.
KEYWORD_NAMES = {
    "add_item_type_rotation": "rotations",
    "set_item_type_group": "group_id",
}

# C++ per-type methods intentionally not available as keywords.
NOT_BOUND = {
}


def keyword_names(function):
    """Names of the keyword-only arguments of a nanobind function."""
    signatures = [
        signature[0]
        for signature in getattr(function, "__nb_signature__", ())]
    if not signatures:
        signatures = [function.__doc__.splitlines()[0]]
    names = set()
    for signature in signatures:
        if "*," not in signature:
            continue
        keywords = signature.split("*,", 1)[1].rsplit(")", 1)[0]
        names.update(re.findall(r"(\w+)\s*:", keywords))
    return names


def test_instance_builder_header_sync():
    with open(INSTANCE_BUILDER_HEADER) as header_file:
        header = header_file.read()
    methods = re.findall(
            r"\b((?:set|add)_(item|bin)_type_\w+)\s*\(\s*\w+\s+(\w+)",
            header)
    methods = [
        (name, kind) for name, kind, first_parameter in methods
        if first_parameter == kind + "_type_id"]
    assert len(methods) >= 16
    keywords = {
        "item": keyword_names(psbs.InstanceBuilder.add_item_type),
        "bin": keyword_names(psbs.InstanceBuilder.add_bin_type),
    }
    assert "copies" in keywords["item"]
    assert "copies" in keywords["bin"]
    expected = {"item": set(), "bin": set()}
    for name, kind in methods:
        if name in NOT_BOUND:
            continue
        prefix = ("set_" if name.startswith("set_") else "add_") + kind + "_type_"
        keyword = KEYWORD_NAMES.get(name, name[len(prefix):])
        expected[kind].add(keyword)
        assert keyword in keywords[kind], (
                "C++ '" + name + "' is neither a keyword of 'add_" + kind
                + "_type' nor in NOT_BOUND")
    # No stale keyword.
    assert keywords["item"] == expected["item"]
    assert keywords["bin"] == expected["bin"]


def test_build_resets_builder():
    instance_builder = psbs.InstanceBuilder()
    instance_builder.add_bin_type(10, 10, 10)
    instance_builder.add_item_type(5, 5, 5)
    instance = instance_builder.build()
    assert instance.number_of_item_types() == 1
    assert instance_builder.build().number_of_item_types() == 0


def test_bin_packing():
    output = psbs.optimize(bin_packing_instance(5), quiet_parameters())
    solution = output.solution
    assert solution.feasible()
    assert solution.full()
    assert solution.number_of_items() == 5
    assert solution.number_of_bins() == 2
    assert output.bin_packing_bound == 2
    assert output.is_proven_optimal()

    items = []
    for bin_pos in range(solution.number_of_different_bins()):
        solution_bin = solution.bin(bin_pos)
        assert solution_bin.bin_type_id == 0
        for stack in solution_bin.stacks:
            assert 0 <= stack.x_start < stack.x_end <= 10
            assert 0 <= stack.y_start < stack.y_end <= 10
            assert stack.z_end == 10
            items.extend(stack.items)
    assert len(items) == 5
    for item in items:
        assert item.item_type_id == 0
        assert item.z_start == 0
        assert item.rotation == psbs.Rotation.XYZ
    assert solution.number_of_stacks() == 5
    with pytest.raises(IndexError):
        solution.bin(solution.number_of_different_bins())
    with pytest.raises(IndexError):
        solution.item_copies(1)
    with pytest.raises(IndexError):
        solution.bin_copies(1)


def test_knapsack():
    instance_builder = psbs.InstanceBuilder()
    instance_builder.set_objective(psbs.Objective.Knapsack)
    instance_builder.add_bin_type(10, 10, 10)
    # Only one of the two 10x10x6 items fits; the one with the largest profit
    # must be selected, and stacked with the 10x10x4 item.
    for z, profit in [(6, 5), (6, 7), (4, 3)]:
        instance_builder.add_item_type(10, 10, z, profit=profit)
    output = psbs.optimize(instance_builder.build(), quiet_parameters())
    assert output.solution.profit() == 10
    assert output.solution.item_copies(0) == 0
    assert output.solution.item_copies(1) == 1
    assert output.solution.item_copies(2) == 1
    assert output.solution.number_of_stacks() == 1
    assert output.knapsack_bound == 10
    assert output.is_proven_optimal()


def test_rotation():
    """A 10x5x2 box only fits in a 5x10x2 bin once rotated."""
    instance_builder = psbs.InstanceBuilder()
    instance_builder.set_objective(psbs.Objective.Knapsack)
    instance_builder.add_bin_type(5, 10, 2)
    instance_builder.add_item_type(10, 5, 2, rotations=[psbs.Rotation.YXZ])
    instance = instance_builder.build()
    assert instance.item_type(0).rotations == [psbs.Rotation.YXZ]
    output = psbs.optimize(instance, quiet_parameters())
    solution = output.solution
    assert solution.number_of_items() == 1
    # Default profit: the volume.
    assert solution.profit() == 100
    item = solution.bin(0).stacks[0].items[0]
    assert item.rotation == psbs.Rotation.YXZ


@pytest.mark.parametrize("rotation", [
    psbs.Rotation.ZYX,
    psbs.Rotation.YZX,
    psbs.Rotation.XZY,
    psbs.Rotation.ZXY,
])
def test_non_upright_rotations(rotation):
    """Items are packed upright in stacks: the other rotations are
    rejected."""
    instance_builder = psbs.InstanceBuilder()
    with pytest.raises(ValueError):
        instance_builder.add_item_type(10, 10, 2, rotations=[rotation])


@pytest.mark.parametrize("same_stackability_id, expected_number_of_bins", [
    (True, 1),
    (False, 2),
])
def test_stackability_id(same_stackability_id, expected_number_of_bins):
    instance_builder = psbs.InstanceBuilder()
    instance_builder.set_objective(psbs.Objective.BinPacking)
    instance_builder.add_bin_type(10, 10, 10, copies=5)
    for stackability_id in [0, 0 if same_stackability_id else 1]:
        instance_builder.add_item_type(10, 10, 5, stackability_id=stackability_id)
    instance = instance_builder.build()
    assert instance.item_type(1).stackability_id == (0 if same_stackability_id else 1)
    output = psbs.optimize(instance, quiet_parameters())
    assert output.solution.feasible()
    assert output.solution.number_of_bins() == expected_number_of_bins


def test_maximum_stackability():
    instance_builder, _ = stack_instance(4)
    output = psbs.optimize(instance_builder.build(), quiet_parameters())
    assert output.solution.number_of_bins() == 2
    assert output.solution.bin(0).stacks[0].items[1].z_start == 5

    instance_builder, _ = stack_instance(4, maximum_stackability=1)
    instance = instance_builder.build()
    assert instance.item_type(0).maximum_stackability == 1
    output = psbs.optimize(instance, quiet_parameters())
    assert output.solution.feasible()
    assert output.solution.number_of_bins() == 4


def test_nesting_height():
    # Without nesting, two 6-high boxes do not fit in a 10-high bin.
    instance_builder, _ = stack_instance(4, z=6)
    output = psbs.optimize(instance_builder.build(), quiet_parameters())
    assert output.solution.number_of_bins() == 4

    instance_builder, _ = stack_instance(4, z=6, nesting_height=2)
    instance = instance_builder.build()
    assert instance.item_type(0).nesting_height == 2
    output = psbs.optimize(instance, quiet_parameters())
    assert output.solution.feasible()
    assert output.solution.number_of_bins() == 2
    stack = output.solution.bin(0).stacks[0]
    assert [item.z_start for item in stack.items] == [0, 4]
    assert stack.z_end == 10


def test_weight_constraints():
    # Maximum bin weight.
    instance_builder, _ = stack_instance(
            2, weight=6, bin_type_keywords={"maximum_weight": 10})
    instance = instance_builder.build()
    assert instance.bin_type(0).maximum_weight == 10
    assert instance.item_weight() == 12
    output = psbs.optimize(instance, quiet_parameters())
    assert output.solution.feasible()
    assert output.solution.total_weight_feasible()
    assert output.solution.number_of_bins() == 2
    assert output.solution.item_weight() == 12

    # Maximum weight above.
    instance_builder, _ = stack_instance(2, weight=5, maximum_weight_above=1)
    instance = instance_builder.build()
    assert instance.item_type(0).maximum_weight_above == 1
    output = psbs.optimize(instance, quiet_parameters())
    assert output.solution.number_of_bins() == 2

    # Maximum stack density: a stack of two boxes weighs 6 over an area of
    # 100.
    instance_builder, _ = stack_instance(
            2, weight=3, bin_type_keywords={"maximum_stack_density": 0.05})
    instance = instance_builder.build()
    assert instance.bin_type(0).maximum_stack_density == 0.05
    output = psbs.optimize(instance, quiet_parameters())
    assert output.solution.number_of_bins() == 2


def test_groups():
    instance_builder = psbs.InstanceBuilder()
    instance_builder.set_objective(psbs.Objective.BinPacking)
    instance_builder.set_unloading_constraint(psbs.UnloadingConstraint.IncreasingX)
    instance_builder.add_bin_type(10, 10, 10, copies=5)
    for group_id in [0, 1]:
        instance_builder.add_item_type(5, 10, 10, group_id=group_id)
    instance_builder.set_group_weight_constraints(1, False)
    instance = instance_builder.build()
    assert instance.unloading_constraint() == psbs.UnloadingConstraint.IncreasingX
    assert instance.number_of_groups() == 2
    assert instance.group(1).item_types == [1]
    assert instance.group(1).number_of_items == 1
    assert instance.check_weight_constraints(0)
    assert not instance.check_weight_constraints(1)
    output = psbs.optimize(instance, quiet_parameters())
    assert output.solution.feasible()
    assert output.solution.number_of_bins() == 1


SEMI_TRAILER_TRUCK_PARAMETERS = {
    "tractor_weight": 8000,
    "front_axle_middle_axle_distance": 380,
    "front_axle_tractor_gravity_center_distance": 100,
    "front_axle_harness_distance": 320,
    "empty_trailer_weight": 6000,
    "harness_rear_axle_distance": 800,
    "trailer_gravity_center_rear_axle_distance": 400,
    "trailer_start_harness_distance": 100,
    "rear_axle_maximum_weight": 20000,
    "middle_axle_maximum_weight": 9300,
}


def read_bin_types_csv(instance, tmp_path):
    bins_path = str(tmp_path / "bins.csv")
    instance.write_bin_types(bins_path)
    with open(bins_path) as bins_file:
        return list(csv.DictReader(bins_file))


def test_semi_trailer_truck_parameters_keyword(tmp_path):
    instance_builder = psbs.InstanceBuilder()
    instance_builder.add_bin_type(
            1360, 240, 260,
            semi_trailer_truck_parameters=SEMI_TRAILER_TRUCK_PARAMETERS)
    # Missing keys keep their default values.
    instance_builder.add_bin_type(
            1360, 240, 260,
            semi_trailer_truck_parameters={
                "front_axle_middle_axle_distance": 380,
                "harness_rear_axle_distance": 800})
    # Not a semi-trailer truck.
    instance_builder.add_bin_type(1360, 240, 260)
    instance_builder.add_item_type(100, 200, 200)
    rows = read_bin_types_csv(instance_builder.build(), tmp_path)
    assert len(rows) == 3
    assert int(rows[0]["IS_SEMI_TRAILER_TRUCK"]) == 1
    for name, value in SEMI_TRAILER_TRUCK_PARAMETERS.items():
        assert float(rows[0][name.upper()]) == value
    assert int(rows[1]["IS_SEMI_TRAILER_TRUCK"]) == 1
    assert float(rows[1]["FRONT_AXLE_MIDDLE_AXLE_DISTANCE"]) == 380
    assert float(rows[1]["HARNESS_REAR_AXLE_DISTANCE"]) == 800
    assert float(rows[1]["TRACTOR_WEIGHT"]) == 0
    assert float(rows[1]["EMPTY_TRAILER_WEIGHT"]) == 0
    assert float(rows[1]["TRAILER_START_HARNESS_DISTANCE"]) == 0
    assert float(rows[1]["REAR_AXLE_MAXIMUM_WEIGHT"]) == math.inf
    assert float(rows[1]["MIDDLE_AXLE_MAXIMUM_WEIGHT"]) == math.inf
    assert int(rows[2]["IS_SEMI_TRAILER_TRUCK"]) == 0


def test_semi_trailer_truck_parameters_keyword_invalid():
    instance_builder = psbs.InstanceBuilder()
    with pytest.raises(ValueError, match="unknown_key"):
        instance_builder.add_bin_type(
                10, 10, 10, semi_trailer_truck_parameters={"unknown_key": 1})
    with pytest.raises(TypeError):
        instance_builder.add_bin_type(
                10, 10, 10, semi_trailer_truck_parameters=[1, 2])
    # The distances must be strictly positive.
    instance_builder = psbs.InstanceBuilder()
    instance_builder.add_bin_type(10, 10, 10, semi_trailer_truck_parameters={})
    instance_builder.add_item_type(5, 5, 5)
    with pytest.raises(ValueError):
        instance_builder.build()


def test_semi_trailer_truck():
    instance_builder = psbs.InstanceBuilder()
    instance_builder.set_objective(psbs.Objective.Knapsack)
    instance_builder.add_bin_type(
            1360, 240, 260,
            maximum_weight=24000,
            semi_trailer_truck_parameters=SEMI_TRAILER_TRUCK_PARAMETERS)
    instance_builder.add_item_type(100, 200, 200, weight=2000, copies=3)
    output = psbs.optimize(instance_builder.build(), quiet_parameters(time_limit=5.0))
    solution = output.solution
    assert solution.feasible()
    assert solution.axle_weights_feasible()
    assert solution.compute_middle_axle_weight_constraints_violation() == 0
    assert solution.compute_rear_axle_weight_constraints_violation() == 0


def test_read_csv():
    path = os.path.join(DATA_DIR, "variable_sized_bin_packing_two_bin_types")
    instance_builder = psbs.InstanceBuilder()
    instance_builder.read_item_types(os.path.join(path, "items.csv"))
    instance_builder.read_bin_types(os.path.join(path, "bins.csv"))
    instance_builder.read_parameters(os.path.join(path, "parameters.csv"))
    instance = instance_builder.build()
    assert instance.objective() == psbs.Objective.VariableSizedBinPacking
    assert instance.number_of_bin_types() == 2
    assert len(instance.item_type(0).rotations) == 2
    output = psbs.optimize(instance, quiet_parameters())
    # Reference solution: both items in the single 12x5x5 bin of cost 10.
    assert output.solution.cost() == 10
    assert output.solution.bin_copies(1) == 1
    assert output.solution.bin_copies(0) == 0


def test_read_missing_file():
    instance_builder = psbs.InstanceBuilder()
    with pytest.raises(Exception):
        instance_builder.read_item_types("this/file/does/not/exist.csv")


def test_write(tmp_path):
    instance_builder, _ = stack_instance(3, weight=2, stackability_id=4, nesting_height=1)
    instance = instance_builder.build()
    instance_path = str(tmp_path / "instance")
    instance.write(instance_path)
    instance_builder = psbs.InstanceBuilder()
    # The parameters file does not contain the objective.
    instance_builder.set_objective(psbs.Objective.BinPacking)
    instance_builder.read_item_types(instance_path + "_items.csv")
    instance_builder.read_bin_types(instance_path + "_bins.csv")
    instance_builder.read_parameters(instance_path + "_parameters.csv")
    instance_2 = instance_builder.build()
    assert instance_2.unloading_constraint() == psbs.UnloadingConstraint.None_
    assert instance_2.number_of_items() == 3
    assert instance_2.item_type(0).stackability_id == 4
    assert instance_2.item_type(0).nesting_height == 1
    assert instance_2.item_type(0).weight == 2
    assert instance_2.bin_type(0).copies == 10

    output = psbs.optimize(instance, quiet_parameters())
    certificate_path = str(tmp_path / "solution.csv")
    output.solution.write(certificate_path)
    assert os.path.getsize(certificate_path) > 0


def test_to_json():
    output = psbs.optimize(bin_packing_instance(5), quiet_parameters())
    json = output.to_json()
    assert isinstance(json, dict)
    assert json["BinPackingBound"] == 2
    assert json["Solution"] == output.solution.to_json()
    assert json["Solution"]["NumberOfBins"] == 2
    assert json["Solution"]["NumberOfStacks"] == 5


def test_parameters():
    parameters = psbs.OptimizeParameters()
    assert parameters.time_limit == math.inf
    assert parameters.optimization_mode == psbs.OptimizationMode.Anytime
    assert parameters.use_box_bounds
    parameters.time_limit = 1.5
    assert parameters.time_limit == 1.5
    parameters.optimization_mode = psbs.OptimizationMode.NotAnytimeSequential
    parameters.tree_search_guides = [0, 1]
    assert parameters.tree_search_guides == [0, 1]
    parameters.not_anytime_tree_search_queue_size = 64
    assert parameters.not_anytime_tree_search_queue_size == 64
    parameters.many_items_in_bins_threshold_2 = 32
    parameters.reduction_parameters.reduce = False
    assert not parameters.reduction_parameters.reduce
    assert parameters.reduction_parameters.merge_identical_items
    parameters.verbosity_level = 0
    output = psbs.optimize(bin_packing_instance(5), parameters)
    assert output.solution.number_of_bins() == 2


@pytest.mark.parametrize("reduce", [True, False])
def test_statistics(reduce):
    """The statistics of the algorithms are reported, also when they ran on
    the reduced instance."""
    instance_builder = psbs.InstanceBuilder()
    instance_builder.set_objective(psbs.Objective.Knapsack)
    instance_builder.add_bin_type(10, 10, 10)
    instance_builder.add_item_type(5, 5, 5, copies=10)
    parameters = quiet_parameters(
            optimization_mode=psbs.OptimizationMode.NotAnytimeSequential)
    parameters.reduction_parameters.reduce = reduce
    output = psbs.optimize(instance_builder.build(), parameters)
    assert output.number_of_sequential_onedimensional_rectangle_calls > 0
    assert output.sequential_onedimensional_rectangle_number_of_items == 8


def test_lifetimes():
    """Outputs and solutions stay valid after their instance is dropped."""
    output = psbs.optimize(bin_packing_instance(5), quiet_parameters())
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
    assert parameters.new_solution_callback == outputs.append
    psbs.optimize(bin_packing_instance(5), parameters)
    assert len(outputs) >= 1
    gc.collect()
    # The outputs passed to the callback are copies that can be kept.
    assert outputs[-1].solution.number_of_bins() == 2

    parameters.new_solution_callback = None
    assert parameters.new_solution_callback is None
    with pytest.raises(TypeError):
        parameters.new_solution_callback = 3


def test_new_solution_callback_exception():
    def callback(output):
        raise ValueError("stop")

    parameters = quiet_parameters()
    parameters.new_solution_callback = callback
    with pytest.raises(ValueError, match="stop"):
        psbs.optimize(bin_packing_instance(5), parameters)


@pytest.mark.parametrize("algorithm", [
    "use_box_bounds",
    "use_sequential_single_knapsack",
    "use_sequential_value_correction",
])
def test_linear_programming_algorithms(algorithm):
    """The box bounds solve LPs (through 'onedimensional'), including in the
    nested single-bin subproblems."""
    instance_builder = psbs.InstanceBuilder()
    instance_builder.set_objective(psbs.Objective.BinPacking)
    instance_builder.add_bin_type(10, 10, 10, copies=20)
    for stackability_id, (x, copies) in enumerate([(5, 20), (10, 6)]):
        instance_builder.add_item_type(
                x, 5, 5, copies=copies, stackability_id=stackability_id)
    parameters = quiet_parameters()
    setattr(parameters, algorithm, True)
    output = psbs.optimize(instance_builder.build(), parameters)
    assert output.bin_packing_bound >= 4
    if output.solution.feasible():
        assert output.solution.number_of_bins() >= output.bin_packing_bound


def test_time_limit():
    instance = large_knapsack_instance(500)
    parameters = quiet_parameters(
            optimization_mode=psbs.OptimizationMode.Anytime,
            time_limit=1.0)
    output = psbs.optimize(instance, parameters)
    # Only check that the time limit is enforced: whether a solution is found
    # within 1 s depends on the speed of the machine.
    assert output.time < 5
    assert output.solution.feasible()


def test_stackability_id_different_footprints():
    """Item types with the same stackability id must have the same x and y
    dimensions."""
    instance_builder = psbs.InstanceBuilder()
    instance_builder.add_bin_type(100, 60, 40)
    instance_builder.add_item_type(40, 30, 20, stackability_id=1)
    instance_builder.add_item_type(40, 30, 10, stackability_id=1)
    instance_builder.add_item_type(30, 40, 20, stackability_id=1)
    with pytest.raises(ValueError):
        instance_builder.build()
    # By default, all item types have the stackability id 0.
    instance_builder = psbs.InstanceBuilder()
    instance_builder.add_bin_type(100, 60, 40)
    instance_builder.add_item_type(40, 30, 20)
    instance_builder.add_item_type(30, 20, 10)
    with pytest.raises(ValueError):
        instance_builder.build()


def test_keyboard_interrupt():
    instance = large_knapsack_instance(500)
    parameters = quiet_parameters(
            optimization_mode=psbs.OptimizationMode.Anytime,
            time_limit=60.0)
    timer = threading.Timer(0.5, _thread.interrupt_main)
    timer.start()
    try:
        with pytest.raises(KeyboardInterrupt):
            psbs.optimize(instance, parameters)
    finally:
        timer.cancel()
