"""Default bin costs of the three-dimensional problem types."""

import importlib

import pytest


@pytest.mark.parametrize("problem_type", ["box", "boxstacks"])
def test_default_bin_cost_is_volume(problem_type):
    """As documented, the default cost of a bin type is its volume (it used to
    be the area of its base)."""
    ps = importlib.import_module("packingsolver." + problem_type)
    instance_builder = ps.InstanceBuilder()
    instance_builder.add_bin_type(10, 20, 30)
    # A cost of -1 means the default.
    instance_builder.add_bin_type(10, 20, 30, cost=-1)
    instance_builder.add_bin_type(10, 20, 30, cost=7)
    instance_builder.add_item_type(1, 1, 1)
    instance = instance_builder.build()
    assert instance.bin_type(0).cost == 10 * 20 * 30
    assert instance.bin_type(1).cost == 10 * 20 * 30
    assert instance.bin_type(2).cost == 7
