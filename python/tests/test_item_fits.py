"""Infeasibility proven when an item type fits in no bin type."""

import importlib

import pytest


PROBLEM_TYPES = [
    "rectangleguillotine",
    "rectangle",
    "box",
    "boxstacks",
    "onedimensional",
    "irregular",
]


def instance(problem_type, objective):
    """An item type too long for the bin (and an item type which fits)."""
    ps = importlib.import_module("packingsolver." + problem_type)
    instance_builder = ps.InstanceBuilder()
    instance_builder.set_objective(getattr(ps.Objective, objective))
    if problem_type in ("rectangleguillotine", "rectangle"):
        instance_builder.add_bin_type(100, 60)
        instance_builder.add_item_type(10, 10)
        instance_builder.add_item_type(110, 10)
    elif problem_type == "box":
        instance_builder.add_bin_type(100, 60, 40)
        instance_builder.add_item_type(10, 10, 10)
        instance_builder.add_item_type(110, 10, 10)
    elif problem_type == "boxstacks":
        instance_builder.add_bin_type(100, 60, 40)
        instance_builder.add_item_type(10, 10, 10)
        # Different footprints can't be stacked together.
        instance_builder.add_item_type(110, 10, 10, stackability_id=1)
    elif problem_type == "onedimensional":
        instance_builder.add_bin_type(100)
        instance_builder.add_item_type(10)
        instance_builder.add_item_type(110)
    else:
        instance_builder.add_bin_type(ps.build_rectangle(0, 100, 0, 60))
        instance_builder.add_item_type(ps.build_rectangle(0, 10, 0, 10))
        instance_builder.add_item_type(ps.build_rectangle(0, 110, 0, 10))
    return ps, instance_builder.build()


def optimize(ps, inst):
    parameters = ps.OptimizeParameters()
    parameters.verbosity_level = 0
    parameters.time_limit = 5
    return ps.optimize(inst, parameters)


@pytest.mark.parametrize("objective", [
    "BinPacking",
    "BinPackingWithLeftovers",
    "Feasibility",
    "VariableSizedBinPacking",
])
@pytest.mark.parametrize("problem_type", PROBLEM_TYPES)
def test_item_fits_no_bin(problem_type, objective):
    ps, inst = instance(problem_type, objective)
    output = optimize(ps, inst)
    assert output.is_proven_infeasible
    assert output.time < 1


@pytest.mark.parametrize("problem_type", PROBLEM_TYPES)
def test_knapsack(problem_type):
    """With the knapsack objective, the item type is just not packed."""
    ps, inst = instance(problem_type, "Knapsack")
    output = optimize(ps, inst)
    assert not output.is_proven_infeasible
    assert output.solution.feasible()
    assert output.solution.number_of_items() == 1


def test_irregular_rotated_item_fits():
    """An item which only fits rotated is not reported as infeasible."""
    import packingsolver.irregular as psi
    instance_builder = psi.InstanceBuilder()
    instance_builder.set_objective(psi.Objective.BinPacking)
    instance_builder.add_bin_type(psi.build_rectangle(0, 100, 0, 60))
    instance_builder.add_item_type(
            psi.build_rectangle(0, 10, 0, 80),
            allowed_rotations=[(0, 0, False), (90, 90, False)])
    output = optimize(psi, instance_builder.build())
    assert not output.is_proven_infeasible
    assert output.solution.feasible()
