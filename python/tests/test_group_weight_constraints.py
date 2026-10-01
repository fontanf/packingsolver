"""'InstanceBuilder.set_group_weight_constraints' of the problem types with
groups."""

import importlib

import pytest


@pytest.mark.parametrize("problem_type", ["rectangle", "boxstacks"])
def test_negative_group_id(problem_type):
    """A negative group id is rejected (it used to be written out of the
    bounds of the vector of groups)."""
    ps = importlib.import_module("packingsolver." + problem_type)
    instance_builder = ps.InstanceBuilder()
    with pytest.raises(ValueError, match="group_id"):
        instance_builder.set_group_weight_constraints(-1, False)


def test_group_weight_constraints():
    import packingsolver.boxstacks as psbs
    instance_builder = psbs.InstanceBuilder()
    instance_builder.set_group_weight_constraints(2, False)
    instance_builder.add_bin_type(10, 10, 10)
    instance_builder.add_item_type(5, 5, 5)
    instance = instance_builder.build()
    assert instance.check_weight_constraints(0)
    assert not instance.check_weight_constraints(2)
