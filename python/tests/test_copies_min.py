"""Optional item copies ('copies_min' < 'copies') are only allowed with the
knapsack objective."""

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


def build(problem_type, objective, **item_keywords):
    ps = importlib.import_module("packingsolver." + problem_type)
    instance_builder = ps.InstanceBuilder()
    instance_builder.set_objective(getattr(ps.Objective, objective))
    if problem_type in ("rectangleguillotine", "rectangle"):
        instance_builder.add_bin_type(10, 10)
        instance_builder.add_item_type(5, 5, **item_keywords)
    elif problem_type in ("box", "boxstacks"):
        instance_builder.add_bin_type(10, 10, 10)
        instance_builder.add_item_type(5, 5, 5, **item_keywords)
    elif problem_type == "onedimensional":
        instance_builder.add_bin_type(10)
        instance_builder.add_item_type(5, **item_keywords)
    else:
        instance_builder.add_bin_type(ps.build_rectangle(0, 10, 0, 10))
        instance_builder.add_item_type(ps.build_rectangle(0, 5, 0, 5), **item_keywords)
    return instance_builder.build()


@pytest.mark.parametrize("objective", [
    "BinPacking",
    "BinPackingWithLeftovers",
    "Feasibility",
    "VariableSizedBinPacking",
])
@pytest.mark.parametrize("problem_type", PROBLEM_TYPES)
def test_optional_copies_rejected(problem_type, objective):
    with pytest.raises(ValueError, match="copies_min"):
        build(problem_type, objective, copies=3, copies_min=1)


@pytest.mark.parametrize("problem_type", PROBLEM_TYPES)
def test_copies_min_defaults_to_copies(problem_type):
    """When 'copies_min' isn't set, it is resolved to 'copies'."""
    instance = build(problem_type, "BinPacking", copies=3)
    assert instance.item_type(0).copies_min == 3
    instance = build(problem_type, "BinPacking", copies=3, copies_min=3)
    assert instance.item_type(0).copies_min == 3


@pytest.mark.parametrize("problem_type", PROBLEM_TYPES)
def test_knapsack_optional_copies(problem_type):
    instance = build(problem_type, "Knapsack", copies=3, copies_min=1)
    assert instance.item_type(0).copies_min == 1
    instance = build(problem_type, "Knapsack", copies=3)
    assert instance.item_type(0).copies_min == 0
