"""JSON (and CSV) instance format of the 'boxstacks' problem type."""

import json

import pytest

import packingsolver.boxstacks as psbs


# An instance using every feature of the format.
INSTANCE = {
    "objective": "knapsack",
    "unloading_constraint": "increasing-x",
    "no_check_weight_constraints": [1],
    "bin_types": [
        {
            "x": 13600,
            "y": 2400,
            "z": 2700,
            "cost": 50,
            "copies": 2,
            "copies_min": 1,
            "maximum_weight": 24000,
            "maximum_stack_density": 1.5,
            "semi_trailer_truck": {
                "tractor_weight": 7000,
                "front_axle_middle_axle_distance": 3800,
                "front_axle_tractor_gravity_center_distance": 1500,
                "front_axle_harness_distance": 3500,
                "empty_trailer_weight": 6500,
                "harness_rear_axle_distance": 7500,
                "trailer_gravity_center_rear_axle_distance": 5000,
                "trailer_start_harness_distance": 1500,
                "rear_axle_maximum_weight": 24000,
                "middle_axle_maximum_weight": 11500,
            },
            "defects": [
                {"x": 100, "y": 200, "width": 300, "height": 400},
            ],
        },
        {
            "x": 6000,
            "y": 2400,
            "z": 2500,
        },
    ],
    "item_types": [
        {
            "x": 1200,
            "y": 800,
            "z": 1000,
            "profit": 10,
            "copies": 5,
            "copies_min": 2,
            "group_id": 1,
            "weight": 300,
            "stackability_id": 3,
            "nesting_height": 50,
            "maximum_stackability": 4,
            "maximum_weight_above": 900,
            "rotations": ["XYZ", "YXZ"],
        },
        {
            "x": 1000,
            "y": 600,
            "z": 500,
        },
    ],
}


def read_json(path):
    instance_builder = psbs.InstanceBuilder()
    instance_builder.read(str(path))
    return instance_builder.build()


def write_instance_json(tmp_path, instance_json, name="instance.json"):
    path = tmp_path / name
    path.write_text(json.dumps(instance_json))
    return path


def test_read_json(tmp_path):
    instance = read_json(write_instance_json(tmp_path, INSTANCE))
    assert instance.objective() == psbs.Objective.Knapsack
    assert instance.unloading_constraint() == psbs.UnloadingConstraint.IncreasingX
    assert instance.check_weight_constraints(0)
    assert not instance.check_weight_constraints(1)

    assert instance.number_of_bin_types() == 2
    bin_type = instance.bin_type(0)
    assert (bin_type.x, bin_type.y, bin_type.z) == (13600, 2400, 2700)
    assert bin_type.cost == 50
    assert bin_type.copies == 2
    assert bin_type.copies_min == 1
    assert bin_type.maximum_weight == 24000
    assert bin_type.maximum_stack_density == 1.5
    assert len(bin_type.defects) == 1
    defect = bin_type.defects[0]
    assert (defect.x, defect.y, defect.width, defect.height) == (100, 200, 300, 400)
    # Defaults of the fields left out.
    assert instance.bin_type(1).maximum_weight == float("inf")
    assert instance.bin_type(1).defects == []

    assert instance.number_of_item_types() == 2
    item_type = instance.item_type(0)
    assert (item_type.x, item_type.y, item_type.z) == (1200, 800, 1000)
    assert item_type.profit == 10
    assert item_type.copies == 5
    assert item_type.copies_min == 2
    assert item_type.group_id == 1
    assert item_type.weight == 300
    assert item_type.stackability_id == 3
    assert item_type.nesting_height == 50
    assert item_type.maximum_stackability == 4
    assert item_type.maximum_weight_above == 900
    assert item_type.rotations == [psbs.Rotation.XYZ, psbs.Rotation.YXZ]
    # Defaults of the fields left out.
    assert instance.item_type(1).profit == 1000 * 600 * 500
    assert instance.item_type(1).rotations == [psbs.Rotation.XYZ]
    assert instance.item_type(1).maximum_weight_above == float("inf")


def test_json_round_trip(tmp_path):
    instance = read_json(write_instance_json(tmp_path, INSTANCE))
    path = tmp_path / "written.json"
    instance.write(str(path), psbs.InstanceFormat.Json)
    instance_2 = read_json(path)
    assert (instance_2.format(verbosity_level=2)
            == instance.format(verbosity_level=2))
    # Writing the instance read back gives the same file.
    path_2 = tmp_path / "written_2.json"
    instance_2.write(str(path_2), psbs.InstanceFormat.Json)
    assert json.loads(path_2.read_text()) == json.loads(path.read_text())


def test_csv_round_trip(tmp_path):
    """The CSV files now also contain the objective and the defects."""
    instance = read_json(write_instance_json(tmp_path, INSTANCE))
    prefix = str(tmp_path / "instance")
    instance.write(prefix)
    instance_builder = psbs.InstanceBuilder()
    instance_builder.read_item_types(prefix + "_items.csv")
    instance_builder.read_bin_types(prefix + "_bins.csv")
    instance_builder.read_defects(prefix + "_defects.csv")
    instance_builder.read_parameters(prefix + "_parameters.csv")
    instance_2 = instance_builder.build()
    assert instance_2.objective() == psbs.Objective.Knapsack
    assert (instance_2.format(verbosity_level=2)
            == instance.format(verbosity_level=2))


def test_csv_without_defects(tmp_path):
    instance_json = dict(INSTANCE)
    instance_json["bin_types"] = [INSTANCE["bin_types"][1]]
    instance = read_json(write_instance_json(tmp_path, instance_json))
    prefix = tmp_path / "instance"
    instance.write(str(prefix))
    assert not (tmp_path / "instance_defects.csv").exists()
    assert (tmp_path / "instance_parameters.csv").exists()


def test_optimize_json_instance(tmp_path):
    instance_json = {
        "objective": "bin-packing",
        "bin_types": [{"x": 10, "y": 10, "z": 10, "copies": 10}],
        "item_types": [{"x": 5, "y": 5, "z": 10, "copies": 5}],
    }
    instance = read_json(write_instance_json(tmp_path, instance_json))
    parameters = psbs.OptimizeParameters()
    parameters.verbosity_level = 0
    parameters.optimization_mode = psbs.OptimizationMode.NotAnytimeSequential
    output = psbs.optimize(instance, parameters)
    assert output.solution.number_of_bins() == 2


@pytest.mark.parametrize("change, message", [
    (lambda j: j.pop("objective"), "objective"),
    (lambda j: j.update(objective="not-an-objective"), "objective"),
    (lambda j: j.update(unloading_constraint="sideways"), "unloading_constraint"),
    (lambda j: j.update(no_check_weight_constraints=[-1]), "group id"),
    (lambda j: j["item_types"][0].update(rotations=["XXX"]), "rotation"),
])
def test_read_json_invalid(tmp_path, change, message):
    instance_json = json.loads(json.dumps(INSTANCE))
    change(instance_json)
    path = write_instance_json(tmp_path, instance_json)
    instance_builder = psbs.InstanceBuilder()
    with pytest.raises(ValueError, match=message):
        instance_builder.read(str(path))


def test_read_json_missing_file(tmp_path):
    instance_builder = psbs.InstanceBuilder()
    with pytest.raises(RuntimeError):
        instance_builder.read(str(tmp_path / "missing.json"))
