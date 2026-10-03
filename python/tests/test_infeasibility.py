"""Infeasibility proven by the one-dimensional (area / volume) relaxation."""

import importlib

import pytest


PROBLEM_TYPES = [
    "rectangleguillotine",
    "rectangle",
    "box",
    "boxstacks",
    "irregular",
]

OBJECTIVES = [
    "BinPacking",
    "BinPackingWithLeftovers",
    "Feasibility",
    "VariableSizedBinPacking",
]


def instance(problem_type, objective, copies):
    """'copies' squares (or cubes) of side 6 in a single bin of side 10: the
    area (volume) of 3 or more of them exceeds the one of the bin."""
    ps = importlib.import_module("packingsolver." + problem_type)
    instance_builder = ps.InstanceBuilder()
    instance_builder.set_objective(getattr(ps.Objective, objective))
    keywords = {"copies": copies}
    if problem_type in ("rectangleguillotine", "rectangle"):
        instance_builder.add_bin_type(10, 10)
        instance_builder.add_item_type(6, 6, **keywords)
    elif problem_type in ("box", "boxstacks"):
        # Cubes of side 6 in a 10x10x6 bin: same areas as above.
        instance_builder.add_bin_type(10, 10, 6)
        instance_builder.add_item_type(6, 6, 6, **keywords)
    else:
        instance_builder.add_bin_type(ps.build_rectangle(0, 10, 0, 10))
        instance_builder.add_item_type(ps.build_rectangle(0, 6, 0, 6), **keywords)
    return ps, instance_builder.build()


def optimize(ps, instance):
    parameters = ps.OptimizeParameters()
    parameters.verbosity_level = 0
    parameters.optimization_mode = ps.OptimizationMode.NotAnytimeSequential
    return ps.optimize(instance, parameters)


@pytest.mark.parametrize("objective", OBJECTIVES)
@pytest.mark.parametrize("problem_type", PROBLEM_TYPES)
def test_area_infeasible(problem_type, objective):
    """The area of the items exceeds the one of the bins: proven infeasible
    (the infeasibility of the relaxation used to be ignored)."""
    ps, inst = instance(problem_type, objective, copies=3)
    output = optimize(ps, inst)
    assert output.is_proven_infeasible
    assert not output.solution.feasible()


@pytest.mark.parametrize("objective", OBJECTIVES)
@pytest.mark.parametrize("problem_type", PROBLEM_TYPES)
def test_feasible(problem_type, objective):
    ps, inst = instance(problem_type, objective, copies=1)
    output = optimize(ps, inst)
    assert not output.is_proven_infeasible
    assert output.solution.feasible()
