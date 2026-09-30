import gc
import math
import os
import random

import pytest

import packingsolver.onedimensional as pso


DATA_DIR = os.path.join(
        os.path.dirname(__file__), "..", "..", "data", "onedimensional", "tests")


def quiet_parameters(**kwargs):
    parameters = pso.OptimizeParameters()
    parameters.verbosity_level = 0
    for name, value in kwargs.items():
        setattr(parameters, name, value)
    return parameters


def bin_packing_instance(item_copies):
    """'item_copies' items of length 5 in bins of length 10."""
    instance_builder = pso.InstanceBuilder()
    instance_builder.set_objective(pso.Objective.BinPacking)
    bin_type_id = instance_builder.add_bin_type(10)
    instance_builder.set_bin_type_copies(bin_type_id, 10)
    item_type_id = instance_builder.add_item_type(5)
    instance_builder.set_item_type_copies(item_type_id, item_copies)
    return instance_builder.build()


def large_bin_packing_instance(number_of_item_types, seed=0):
    rng = random.Random(seed)
    instance_builder = pso.InstanceBuilder()
    instance_builder.set_objective(pso.Objective.BinPacking)
    instance_builder.add_bin_type(10000)
    instance_builder.set_bin_types_infinite_copies()
    for _ in range(number_of_item_types):
        item_type_id = instance_builder.add_item_type(rng.randint(100, 5000))
        instance_builder.set_item_type_copies(item_type_id, rng.randint(1, 5))
    return instance_builder.build()


def test_instance_builder():
    instance = bin_packing_instance(5)
    assert instance.objective() == pso.Objective.BinPacking
    assert instance.number_of_item_types() == 1
    assert instance.number_of_items() == 5
    assert instance.number_of_bin_types() == 1
    assert instance.number_of_bins() == 10
    assert instance.item_type(0).length == 5
    assert instance.item_type(0).copies == 5
    assert instance.item_type(0).copies_min == 5
    assert instance.bin_type(0).length == 10
    assert instance.bin_type(0).copies == 10
    assert instance.bin_type(0).number_of_resources() == 0
    assert instance.bin_type_id(9) == 0
    assert instance.previous_bin_length(1) == 10
    assert instance.item_length() == 25
    assert instance.bin_length() == 100
    assert instance.fits_some_bin(0)
    assert instance.item_type_fits_bin_type(0, 0)
    assert "Number of item types" in instance.format()
    with pytest.raises(IndexError):
        instance.item_type(1)
    with pytest.raises(IndexError):
        instance.bin_type(-1)
    with pytest.raises(IndexError):
        instance.bin_type_id(10)
    with pytest.raises(IndexError):
        instance.item_type_fits_bin_type(0, 1)


def test_instance_builder_item_type_attributes():
    instance_builder = pso.InstanceBuilder()
    instance_builder.set_objective(pso.Objective.BinPacking)
    bin_type_id = instance_builder.add_bin_type(10)
    instance_builder.set_bin_type_maximum_weight(bin_type_id, 100.0)
    instance_builder.add_bin_type_eligibility(bin_type_id, 3)
    item_type_id = instance_builder.add_item_type(4)
    instance_builder.set_item_type_length(item_type_id, 5)
    instance_builder.set_item_type_weight(item_type_id, 2.5)
    instance_builder.set_item_type_nesting_length(item_type_id, 1)
    instance_builder.set_item_type_maximum_stackability(item_type_id, 3)
    instance_builder.set_item_type_maximum_weight_after(item_type_id, 50.0)
    instance_builder.set_item_type_eligibility(item_type_id, 3)
    other_item_type_id = instance_builder.add_item_type(5)
    instance_builder.add_item_type_precedence(other_item_type_id, item_type_id)
    instance = instance_builder.build()
    item_type = instance.item_type(item_type_id)
    assert item_type.length == 5
    assert item_type.weight == 2.5
    assert item_type.nesting_length == 1
    assert item_type.maximum_stackability == 3
    assert item_type.maximum_weight_after == 50.0
    assert item_type.eligibility_id == 3
    assert instance.bin_type(0).maximum_weight == 100.0
    assert instance.bin_type(0).eligibility_ids == [3]
    precedences = instance.precedences()
    assert len(precedences) == 1
    assert precedences[0].dominated_item_type_id == other_item_type_id
    assert precedences[0].dominating_item_type_id == item_type_id
    assert len(instance.item_type(item_type_id).dominating_precedence_ids) == 1


def test_instance_builder_invalid_argument():
    instance_builder = pso.InstanceBuilder()
    with pytest.raises(ValueError):
        instance_builder.add_bin_type(-1)


def test_build_resets_builder():
    instance_builder = pso.InstanceBuilder()
    instance_builder.add_bin_type(10)
    instance_builder.add_item_type(5)
    instance = instance_builder.build()
    assert instance.number_of_item_types() == 1
    assert instance_builder.build().number_of_item_types() == 0


def test_bin_packing():
    output = pso.optimize(bin_packing_instance(5), quiet_parameters())
    solution = output.solution
    assert solution.feasible()
    assert solution.full()
    assert solution.number_of_items() == 5
    assert solution.number_of_bins() == 3
    assert output.bin_packing_bound == 3
    assert output.is_proven_optimal()

    items = [
        item
        for bin_pos in range(solution.number_of_different_bins())
        for _ in range(solution.bin(bin_pos).copies)
        for item in solution.bin(bin_pos).items]
    assert len(items) == 5
    for item in items:
        assert item.item_type_id == 0
        assert item.start in (0, 5)
    with pytest.raises(IndexError):
        solution.bin(solution.number_of_different_bins())


def test_knapsack():
    instance_builder = pso.InstanceBuilder()
    instance_builder.set_objective(pso.Objective.Knapsack)
    instance_builder.add_bin_type(10)
    # Only one of the two items of length 6 fits; the one with the largest
    # profit must be selected, together with the item of length 4.
    for length, profit in [(6, 5), (6, 7), (4, 3)]:
        item_type_id = instance_builder.add_item_type(length)
        instance_builder.set_item_type_profit(item_type_id, profit)
    output = pso.optimize(instance_builder.build(), quiet_parameters())
    assert output.solution.profit() == 10
    assert output.solution.item_copies(1) == 1
    assert output.solution.item_copies(0) == 0
    assert output.knapsack_bound == 10
    assert output.is_proven_optimal()
    with pytest.raises(IndexError):
        output.solution.item_copies(3)


def test_variable_sized_bin_packing():
    instance_builder = pso.InstanceBuilder()
    instance_builder.set_objective(pso.Objective.VariableSizedBinPacking)
    # Two small bins (cost 4 each) are cheaper than one large bin (cost 10).
    bin_type_id = instance_builder.add_bin_type(10)
    instance_builder.set_bin_type_cost(bin_type_id, 10)
    instance_builder.set_bin_type_copies(bin_type_id, 2)
    bin_type_id = instance_builder.add_bin_type(5)
    instance_builder.set_bin_type_cost(bin_type_id, 4)
    instance_builder.set_bin_type_copies(bin_type_id, 4)
    item_type_id = instance_builder.add_item_type(5)
    instance_builder.set_item_type_copies(item_type_id, 2)
    output = pso.optimize(instance_builder.build(), quiet_parameters())
    assert output.solution.feasible()
    assert output.solution.cost() == 8
    assert output.solution.bin_copies(1) == 2
    assert output.solution.bin_copies(0) == 0
    assert output.variable_sized_bin_packing_bound == 8
    assert output.is_proven_optimal()
    with pytest.raises(IndexError):
        output.solution.bin_copies(2)


def test_resources():
    instance_builder = pso.InstanceBuilder()
    instance_builder.set_objective(pso.Objective.BinPacking)
    bin_type_id = instance_builder.add_bin_type(100)
    instance_builder.set_bin_type_copies(bin_type_id, 3)
    resource_id = instance_builder.add_bin_type_resource(bin_type_id, 10.0)
    item_type_id = instance_builder.add_item_type(10)
    instance_builder.set_item_type_copies(item_type_id, 3)
    instance_builder.add_resource_consumption(
            bin_type_id, resource_id, item_type_id, [5.0])
    instance = instance_builder.build()
    assert instance.bin_type(0).number_of_resources() == 1
    output = pso.optimize(instance, quiet_parameters())
    solution = output.solution
    assert solution.feasible()
    assert solution.resource_feasible()
    assert solution.number_of_bins() == 2
    for bin_pos in range(solution.number_of_different_bins()):
        for consumption in solution.bin(bin_pos).resource_consumption:
            assert consumption <= 10.0


def test_read_csv():
    path = os.path.join(DATA_DIR, "knapsack_multiple_bins")
    instance_builder = pso.InstanceBuilder()
    instance_builder.read_item_types(os.path.join(path, "items.csv"))
    instance_builder.read_bin_types(os.path.join(path, "bins.csv"))
    instance_builder.read_parameters(os.path.join(path, "parameters.csv"))
    instance = instance_builder.build()
    assert instance.objective() == pso.Objective.Knapsack
    assert instance.number_of_item_types() == 4
    assert instance.number_of_bin_types() == 2
    output = pso.optimize(instance, quiet_parameters())
    assert output.solution.profit() == 56
    assert output.is_proven_optimal()


def test_read_json():
    path = os.path.join(DATA_DIR, "bin_packing_resource_capacity", "instance.json")
    instance_builder = pso.InstanceBuilder()
    instance_builder.read(path)
    instance = instance_builder.build()
    assert instance.objective() == pso.Objective.BinPacking
    assert instance.bin_type(0).number_of_resources() == 1
    output = pso.optimize(instance, quiet_parameters())
    assert output.solution.number_of_bins() == 2


def test_read_missing_file():
    instance_builder = pso.InstanceBuilder()
    with pytest.raises(Exception):
        instance_builder.read_item_types("this/file/does/not/exist.csv")


def test_write(tmp_path):
    instance = bin_packing_instance(5)
    instance_path = str(tmp_path / "instance.json")
    instance.write(instance_path, pso.InstanceFormat.Json)
    instance_builder = pso.InstanceBuilder()
    instance_builder.read(instance_path)
    assert instance_builder.build().number_of_items() == 5

    output = pso.optimize(instance, quiet_parameters())
    certificate_path = str(tmp_path / "solution.csv")
    output.solution.write(certificate_path)
    assert os.path.getsize(certificate_path) > 0


def test_to_json():
    output = pso.optimize(bin_packing_instance(5), quiet_parameters())
    json = output.to_json()
    assert isinstance(json, dict)
    assert json["BinPackingBound"] == 3
    assert json["Solution"] == output.solution.to_json()
    assert json["Solution"]["NumberOfBins"] == 3


def test_parameters():
    parameters = pso.OptimizeParameters()
    assert parameters.time_limit == math.inf
    parameters.time_limit = 1.5
    assert parameters.time_limit == 1.5
    parameters.optimization_mode = pso.OptimizationMode.NotAnytimeSequential
    parameters.tree_search_guides = [0, 1]
    assert parameters.tree_search_guides == [0, 1]
    parameters.not_anytime_tree_search_queue_size = 128
    assert parameters.not_anytime_tree_search_queue_size == 128
    parameters.reduction_parameters.reduce = False
    assert not parameters.reduction_parameters.reduce
    parameters.verbosity_level = 0
    output = pso.optimize(bin_packing_instance(5), parameters)
    assert output.solution.number_of_bins() == 3


def test_lifetimes():
    """Outputs and solutions stay valid after their instance is dropped."""
    output = pso.optimize(bin_packing_instance(5), quiet_parameters())
    gc.collect()
    solution = output.solution
    del output
    gc.collect()
    assert solution.number_of_bins() == 3
    assert solution.bin(0).bin_type_id == 0


def test_new_solution_callback():
    outputs = []
    parameters = quiet_parameters()
    parameters.new_solution_callback = outputs.append
    assert parameters.new_solution_callback == outputs.append
    pso.optimize(bin_packing_instance(5), parameters)
    assert len(outputs) >= 1
    gc.collect()
    # The outputs passed to the callback are copies that can be kept.
    assert outputs[-1].solution.number_of_bins() == 3

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
        pso.optimize(bin_packing_instance(5), parameters)


@pytest.mark.parametrize("algorithm", [
    "use_column_generation",
    "use_milp_assignment",
    "use_dual_feasible_functions",
])
def test_linear_programming_algorithms(algorithm):
    """Algorithms solving LPs/MILPs, including in nested subproblems."""
    instance_builder = pso.InstanceBuilder()
    instance_builder.set_objective(pso.Objective.BinPacking)
    bin_type_id = instance_builder.add_bin_type(100)
    instance_builder.set_bin_type_copies(bin_type_id, 50)
    for length, copies in [(30, 20), (50, 15), (70, 8)]:
        item_type_id = instance_builder.add_item_type(length)
        instance_builder.set_item_type_copies(item_type_id, copies)
    parameters = quiet_parameters(time_limit=30.0)
    setattr(parameters, algorithm, True)
    output = pso.optimize(instance_builder.build(), parameters)
    # Total item length is 1910.
    assert output.bin_packing_bound >= 20
    if output.solution.feasible():
        assert output.solution.number_of_bins() >= output.bin_packing_bound


def test_time_limit():
    instance = large_bin_packing_instance(300)
    parameters = quiet_parameters(time_limit=1.0)
    output = pso.optimize(instance, parameters)
    assert output.time < 5
    assert output.solution.number_of_items() > 0
