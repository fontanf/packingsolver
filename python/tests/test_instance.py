"""'Instance' behaviours shared by every problem type."""

import importlib
import io
import json
import os
import pathlib

import pytest


DATA_DIR = os.path.join(os.path.dirname(__file__), "..", "..", "data")

# A JSON instance of each problem type.
JSON_INSTANCES = {
    "rectangleguillotine": os.path.join(
        DATA_DIR, "rectangleguillotine", "tests",
        "bin_packing_merge_identical_items_respects_resources", "instance.json"),
    "rectangle": os.path.join(
        DATA_DIR, "rectangle", "tests",
        "variable_sized_bin_packing_remove_dominated_bin_types_not_removed_with_negative_penalty_resource_on_b",
        "instance.json"),
    "box": os.path.join(
        DATA_DIR, "box", "tests",
        "knapsack_remove_negative_profit_items_skipped_with_negative_penalty_resource",
        "instance.json"),
    "onedimensional": os.path.join(
        DATA_DIR, "onedimensional", "tests",
        "bin_packing_merge_identical_items_respects_resources", "instance.json"),
    "irregular": os.path.join(
        DATA_DIR, "irregular", "tests",
        "periodic_packing_single_item_exact_fit_rectangle.json"),
}

BOXSTACKS_JSON_INSTANCE = {
    "objective": "bin-packing",
    "bin_types": [{"x": 100, "y": 60, "z": 40, "copies": 2}],
    "item_types": [{"x": 40, "y": 30, "z": 20, "copies": 3}],
}


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


def json_instance_path(problem_type, tmp_path):
    if problem_type == "boxstacks":
        path = tmp_path / "instance.json"
        path.write_text(json.dumps(BOXSTACKS_JSON_INSTANCE))
        return str(path)
    return JSON_INSTANCES[problem_type]


def instance_summary(instance):
    return (
        instance.objective(),
        instance.number_of_item_types(),
        instance.number_of_items(),
        instance.number_of_bin_types(),
        instance.number_of_bins())


@pytest.mark.parametrize("problem_type", [
    "rectangleguillotine",
    "rectangle",
    "box",
    "boxstacks",
    "onedimensional",
    "irregular",
])
def test_read_stream(problem_type, tmp_path):
    """A JSON instance can be read from a path, a text stream or a path-like
    object, with the same result."""
    ps = importlib.import_module("packingsolver." + problem_type)
    path = json_instance_path(problem_type, tmp_path)
    instances = []
    with open(path) as file:
        text = file.read()
    for source in [path, io.StringIO(text), pathlib.Path(path)]:
        instance_builder = ps.InstanceBuilder()
        instance_builder.read(source)
        instances.append(instance_builder.build())
    summaries = [instance_summary(instance) for instance in instances]
    assert summaries[1] == summaries[0]
    assert summaries[2] == summaries[0]
    # Invalid JSON and objects which are neither paths nor text streams.
    with pytest.raises(Exception):
        ps.InstanceBuilder().read(io.StringIO("{"))
    with pytest.raises(TypeError):
        ps.InstanceBuilder().read(42)
