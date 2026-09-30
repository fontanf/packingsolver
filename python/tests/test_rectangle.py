import _thread
import gc
import math
import os
import random
import threading

import pytest

import packingsolver.rectangle as psr


DATA_DIR = os.path.join(
        os.path.dirname(__file__), "..", "..", "data", "rectangle", "tests")


def quiet_parameters(**kwargs):
    parameters = psr.OptimizeParameters()
    parameters.verbosity_level = 0
    for name, value in kwargs.items():
        setattr(parameters, name, value)
    return parameters


def bin_packing_instance(item_copies):
    """'item_copies' 5x5 squares in 10x10 bins."""
    instance_builder = psr.InstanceBuilder()
    instance_builder.set_objective(psr.Objective.BinPacking)
    bin_type_id = instance_builder.add_bin_type(10, 10)
    instance_builder.set_bin_type_copies(bin_type_id, 10)
    item_type_id = instance_builder.add_item_type(5, 5)
    instance_builder.set_item_type_copies(item_type_id, item_copies)
    return instance_builder.build()


def large_knapsack_instance(number_of_item_types, seed=0):
    rng = random.Random(seed)
    instance_builder = psr.InstanceBuilder()
    instance_builder.set_objective(psr.Objective.Knapsack)
    instance_builder.add_bin_type(1000, 1000)
    for _ in range(number_of_item_types):
        item_type_id = instance_builder.add_item_type(
                rng.randint(20, 200), rng.randint(20, 200))
        instance_builder.set_item_type_profit(item_type_id, rng.randint(1, 1000))
    return instance_builder.build()


def test_instance_builder():
    instance = bin_packing_instance(5)
    assert instance.objective() == psr.Objective.BinPacking
    assert instance.number_of_item_types() == 1
    assert instance.number_of_items() == 5
    assert instance.number_of_bin_types() == 1
    assert instance.item_type(0).x == 5
    assert instance.item_type(0).copies == 5
    assert not instance.item_type(0).oriented
    assert instance.bin_type(0).x == 10
    assert instance.bin_type(0).copies == 10
    assert "Number of item types" in instance.format()
    with pytest.raises(IndexError):
        instance.item_type(1)
    with pytest.raises(IndexError):
        instance.bin_type(-1)


def test_instance_builder_invalid_argument():
    instance_builder = psr.InstanceBuilder()
    with pytest.raises(ValueError):
        instance_builder.add_item_type(-1, 5)


def test_build_resets_builder():
    instance_builder = psr.InstanceBuilder()
    instance_builder.add_bin_type(10, 10)
    instance_builder.add_item_type(5, 5)
    instance = instance_builder.build()
    assert instance.number_of_item_types() == 1
    assert instance_builder.build().number_of_item_types() == 0


def test_bin_packing():
    output = psr.optimize(bin_packing_instance(5), quiet_parameters())
    solution = output.solution
    assert solution.feasible()
    assert solution.full()
    assert solution.number_of_items() == 5
    assert solution.number_of_bins() == 2
    assert output.bin_packing_bound == 2
    assert output.is_proven_optimal()

    items = [
        item
        for bin_pos in range(solution.number_of_different_bins())
        for item in solution.bin(bin_pos).items]
    assert len(items) == 5
    for item in items:
        assert item.item_type_id == 0
        assert 0 <= item.x <= 5 and 0 <= item.y <= 5
    with pytest.raises(IndexError):
        solution.bin(solution.number_of_different_bins())


def test_knapsack():
    instance_builder = psr.InstanceBuilder()
    instance_builder.set_objective(psr.Objective.Knapsack)
    instance_builder.add_bin_type(10, 10)
    # Only one of the two 10x6 items fits; the one with the largest profit
    # must be selected, together with the 10x4 item.
    for x, y, profit in [(10, 6, 5), (10, 6, 7), (10, 4, 3)]:
        item_type_id = instance_builder.add_item_type(x, y, oriented=True)
        instance_builder.set_item_type_profit(item_type_id, profit)
    output = psr.optimize(instance_builder.build(), quiet_parameters())
    assert output.solution.profit() == 10
    assert output.solution.item_copies(1) == 1
    assert output.solution.item_copies(0) == 0
    assert output.knapsack_bound == 10
    assert output.is_proven_optimal()


def test_read_csv():
    path = os.path.join(DATA_DIR, "bin_packing_full_bin_item_exact_fit")
    instance_builder = psr.InstanceBuilder()
    instance_builder.read_item_types(os.path.join(path, "items.csv"))
    instance_builder.read_bin_types(os.path.join(path, "bins.csv"))
    instance_builder.read_parameters(os.path.join(path, "parameters.csv"))
    instance = instance_builder.build()
    assert instance.objective() == psr.Objective.BinPacking
    output = psr.optimize(instance, quiet_parameters())
    assert output.solution.number_of_bins() == 2


def test_read_missing_file():
    instance_builder = psr.InstanceBuilder()
    with pytest.raises(Exception):
        instance_builder.read_item_types("this/file/does/not/exist.csv")


def test_write(tmp_path):
    instance = bin_packing_instance(5)
    instance_path = str(tmp_path / "instance.json")
    instance.write(instance_path, psr.InstanceFormat.Json)
    instance_builder = psr.InstanceBuilder()
    instance_builder.read(instance_path)
    assert instance_builder.build().number_of_items() == 5

    output = psr.optimize(instance, quiet_parameters())
    certificate_path = str(tmp_path / "solution.csv")
    output.solution.write(certificate_path)
    assert os.path.getsize(certificate_path) > 0


def test_to_json():
    output = psr.optimize(bin_packing_instance(5), quiet_parameters())
    json = output.to_json()
    assert isinstance(json, dict)
    assert json["BinPackingBound"] == 2
    assert json["Solution"] == output.solution.to_json()
    assert json["Solution"]["NumberOfBins"] == 2


def test_parameters():
    parameters = psr.OptimizeParameters()
    assert parameters.time_limit == math.inf
    parameters.time_limit = 1.5
    assert parameters.time_limit == 1.5
    parameters.optimization_mode = psr.OptimizationMode.NotAnytimeSequential
    parameters.tree_search_guides = [0, 1]
    assert parameters.tree_search_guides == [0, 1]
    parameters.tree_search_directions = [psr.Direction.X]
    parameters.reduction_parameters.reduce = False
    assert not parameters.reduction_parameters.reduce
    parameters.verbosity_level = 0
    output = psr.optimize(bin_packing_instance(5), parameters)
    assert output.solution.number_of_bins() == 2


def test_lifetimes():
    """Outputs and solutions stay valid after their instance is dropped."""
    output = psr.optimize(bin_packing_instance(5), quiet_parameters())
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
    psr.optimize(bin_packing_instance(5), parameters)
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
        psr.optimize(bin_packing_instance(5), parameters)


@pytest.mark.parametrize("algorithm", [
    "use_column_generation",
    "use_benders_decomposition",
    "use_bar_relaxation",
    "use_conservative_scales",
])
def test_linear_programming_algorithms(algorithm):
    """Algorithms solving LPs, including in nested subproblems."""
    instance_builder = psr.InstanceBuilder()
    instance_builder.set_objective(psr.Objective.BinPacking)
    bin_type_id = instance_builder.add_bin_type(100, 100)
    instance_builder.set_bin_type_copies(bin_type_id, 50)
    for x, y, copies in [(30, 40, 20), (50, 20, 15), (70, 35, 8)]:
        item_type_id = instance_builder.add_item_type(x, y, oriented=True)
        instance_builder.set_item_type_copies(item_type_id, copies)
    parameters = quiet_parameters(time_limit=30.0)
    setattr(parameters, algorithm, True)
    output = psr.optimize(instance_builder.build(), parameters)
    assert output.bin_packing_bound >= 6
    if output.solution.feasible():
        assert output.solution.number_of_bins() >= output.bin_packing_bound


def test_time_limit():
    instance = large_knapsack_instance(500)
    parameters = quiet_parameters(time_limit=1.0)
    output = psr.optimize(instance, parameters)
    # Only check that the time limit is enforced: how good the solution is
    # after 1 s depends on the load of the machine.
    assert output.time < 5
    assert output.solution.feasible()


def test_keyboard_interrupt():
    instance = large_knapsack_instance(500)
    parameters = quiet_parameters(time_limit=60.0)
    timer = threading.Timer(0.5, _thread.interrupt_main)
    timer.start()
    try:
        with pytest.raises(KeyboardInterrupt):
            psr.optimize(instance, parameters)
    finally:
        timer.cancel()
