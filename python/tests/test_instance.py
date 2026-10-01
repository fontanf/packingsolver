"""'Instance' behaviours shared by every problem type."""

import importlib

import pytest


def minimal_instance_builder(problem_type):
    """A builder with one bin type and one item type, and no objective set."""
    ps = importlib.import_module("packingsolver." + problem_type)
    instance_builder = ps.InstanceBuilder()
    if problem_type in ("rectangleguillotine", "rectangle"):
        instance_builder.add_bin_type(10, 10)
        instance_builder.add_item_type(5, 5)
    elif problem_type in ("box", "boxstacks"):
        instance_builder.add_bin_type(10, 10, 10)
        instance_builder.add_item_type(5, 5, 5)
    elif problem_type == "onedimensional":
        instance_builder.add_bin_type(10)
        instance_builder.add_item_type(5)
    elif problem_type == "irregular":
        instance_builder.add_bin_type(ps.build_rectangle(0, 10, 0, 10))
        instance_builder.add_item_type(ps.build_rectangle(0, 5, 0, 5))
    return ps, instance_builder


@pytest.mark.parametrize("problem_type", [
    "rectangleguillotine",
    "rectangle",
    "box",
    "boxstacks",
    "onedimensional",
    "irregular",
])
def test_default_objective(problem_type):
    """Without 'set_objective', the objective is 'BinPacking' (it used to be
    uninitialized for every problem type but 'rectangle')."""
    ps, instance_builder = minimal_instance_builder(problem_type)
    assert instance_builder.build().objective() == ps.Objective.BinPacking
