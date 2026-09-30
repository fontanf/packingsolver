import gc
import math
import os

import pytest

import packingsolver.rectangleguillotine as psg


DATA_DIR = os.path.join(
        os.path.dirname(__file__), "..", "..", "data", "rectangleguillotine", "tests")


def quiet_parameters(**kwargs):
    parameters = psg.OptimizeParameters()
    parameters.verbosity_level = 0
    for name, value in kwargs.items():
        setattr(parameters, name, value)
    return parameters


def bin_packing_instance(item_copies):
    """'item_copies' 5x5 squares in 10x10 bins."""
    instance_builder = psg.InstanceBuilder()
    instance_builder.set_objective(psg.Objective.BinPacking)
    bin_type_id = instance_builder.add_bin_type(10, 10)
    instance_builder.set_bin_type_copies(bin_type_id, 10)
    item_type_id = instance_builder.add_item_type(5, 5)
    instance_builder.set_item_type_copies(item_type_id, item_copies)
    return instance_builder.build()


def solution_item_nodes(solution):
    """Item nodes (not the bin node) of all bins of a solution."""
    return [
        node
        for bin_pos in range(solution.number_of_different_bins())
        for node in solution.bin(bin_pos).nodes
        if node.f != -1 and node.item_type_id >= 0]


def test_instance_builder():
    instance = bin_packing_instance(5)
    assert instance.objective() == psg.Objective.BinPacking
    assert instance.number_of_item_types() == 1
    assert instance.number_of_items() == 5
    assert instance.number_of_bin_types() == 1
    assert instance.number_of_bins() == 10
    assert instance.bin_type_id(9) == 0
    assert instance.item_type(0).w == 5
    assert instance.item_type(0).h == 5
    assert instance.item_type(0).area() == 25
    assert instance.item_type(0).copies == 5
    assert not instance.item_type(0).oriented
    assert instance.bin_type(0).w == 10
    assert instance.bin_type(0).h == 10
    assert instance.bin_type(0).copies == 10
    assert instance.item_area() == 125
    assert "Number of item types" in instance.format()
    with pytest.raises(IndexError):
        instance.item_type(1)
    with pytest.raises(IndexError):
        instance.bin_type(-1)
    with pytest.raises(IndexError):
        instance.bin_type_id(10)
    with pytest.raises(IndexError):
        instance.stack_size(instance.number_of_stacks())


def test_instance_builder_invalid_argument():
    instance_builder = psg.InstanceBuilder()
    with pytest.raises(ValueError):
        instance_builder.add_item_type(-1, 5)


def test_build_resets_builder():
    instance_builder = psg.InstanceBuilder()
    instance_builder.add_bin_type(10, 10)
    instance_builder.add_item_type(5, 5)
    instance = instance_builder.build()
    assert instance.number_of_item_types() == 1
    assert instance_builder.build().number_of_item_types() == 0


def test_guillotine_parameters():
    instance_builder = psg.InstanceBuilder()
    instance_builder.set_objective(psg.Objective.Knapsack)
    instance_builder.add_bin_type(100, 50)
    instance_builder.add_item_type(10, 20)
    instance_builder.set_number_of_stages(2)
    instance_builder.set_cut_type(psg.CutType.Exact)
    instance_builder.set_first_stage_orientation(psg.CutOrientation.Horizontal)
    instance_builder.set_minimum_distance_1_cuts(3)
    instance_builder.set_maximum_distance_1_cuts(40)
    instance_builder.set_minimum_waste_length(2)
    instance_builder.set_maximum_number_1_cuts(4)
    instance_builder.set_cut_through_defects(True)
    instance_builder.set_cut_thickness(1)
    instance_builder.set_fixed_cutting_cost(1, 5)
    instance_builder.set_variable_cutting_cost(2, 7)
    instance_builder.set_waste_cost(3)
    instance = instance_builder.build()
    parameters = instance.parameters()
    assert parameters.number_of_stages == 2
    assert parameters.cut_type == psg.CutType.Exact
    assert parameters.first_stage_orientation == psg.CutOrientation.Horizontal
    assert parameters.minimum_distance_1_cuts == 3
    assert parameters.maximum_distance_1_cuts == 40
    assert parameters.minimum_distance_2_cuts == 0
    assert parameters.maximum_distance_2_cuts == -1
    assert parameters.minimum_waste_length == 2
    assert parameters.maximum_number_1_cuts == 4
    assert parameters.maximum_number_2_cuts == -1
    assert parameters.cut_through_defects
    assert parameters.cut_thickness == 1
    assert len(parameters.cutting_costs) >= 3
    assert parameters.cutting_costs[1].fixed == 5
    assert parameters.cutting_costs[2].variable == 7
    assert parameters.waste_cost == 3


def test_predefined():
    instance_builder = psg.InstanceBuilder()
    instance_builder.add_bin_type(100, 50)
    instance_builder.add_item_type(10, 20)
    instance_builder.set_predefined("2EHO")
    instance = instance_builder.build()
    assert instance.parameters().number_of_stages == 2
    assert instance.parameters().cut_type == psg.CutType.Exact
    assert instance.parameters().first_stage_orientation == psg.CutOrientation.Horizontal
    assert instance.all_item_types_oriented()


def test_trims_defects_stacks():
    instance_builder = psg.InstanceBuilder()
    instance_builder.set_objective(psg.Objective.Knapsack)
    bin_type_id = instance_builder.add_bin_type(100, 50)
    instance_builder.add_trims(
            bin_type_id,
            1, psg.TrimType.Hard,
            2, psg.TrimType.Soft,
            3, psg.TrimType.Hard,
            4, psg.TrimType.Soft)
    instance_builder.add_defect(bin_type_id, 20, 10, 5, 6)
    instance_builder.add_item_type(10, 20, stack_id=0)
    instance_builder.add_item_type(15, 20, stack_id=0)
    instance = instance_builder.build()
    bin_type = instance.bin_type(0)
    assert bin_type.left_trim == 1
    assert bin_type.left_trim_type == psg.TrimType.Hard
    assert bin_type.right_trim == 2
    assert bin_type.right_trim_type == psg.TrimType.Soft
    assert bin_type.bottom_trim == 3
    assert bin_type.top_trim == 4
    assert bin_type.area() == (100 - 1 - 2) * (50 - 3 - 4)
    assert instance.number_of_defects() == 1
    defect = bin_type.defects[0]
    assert (defect.x, defect.y, defect.w, defect.h) == (20, 10, 5, 6)
    assert (defect.left(), defect.right(), defect.bottom(), defect.top()) == (20, 25, 10, 16)
    assert instance.item_type(1).stack_id == 0
    assert instance.number_of_stacks() == 1
    assert instance.stack_size(0) == 2
    assert {instance.item(0, 0), instance.item(0, 1)} == {0, 1}
    with pytest.raises(IndexError):
        instance.item(0, 2)


def test_bin_packing():
    output = psg.optimize(bin_packing_instance(5), quiet_parameters())
    solution = output.solution
    assert solution.feasible()
    assert solution.full()
    assert solution.number_of_items() == 5
    assert solution.number_of_bins() == 2
    assert output.bin_packing_bound == 2
    assert output.is_proven_optimal()
    with pytest.raises(IndexError):
        solution.bin(solution.number_of_different_bins())
    with pytest.raises(IndexError):
        solution.item_copies(1)
    with pytest.raises(IndexError):
        solution.bin_copies(1)


def test_solution_nodes():
    output = psg.optimize(bin_packing_instance(5), quiet_parameters())
    solution = output.solution
    for bin_pos in range(solution.number_of_different_bins()):
        solution_bin = solution.bin(bin_pos)
        assert solution_bin.bin_type_id == 0
        assert solution_bin.copies >= 1
        nodes = solution_bin.nodes
        # The first node is the bin itself.
        root = nodes[0]
        assert root.f == -1
        assert root.d == 0
        assert (root.l, root.r, root.b, root.t) == (0, 10, 0, 10)
        for node_id, node in enumerate(nodes):
            assert 0 <= node.l <= node.r <= 10
            assert 0 <= node.b <= node.t <= 10
            for child_id in node.children:
                assert nodes[child_id].f == node_id
    items = solution_item_nodes(solution)
    assert sum(
            solution.bin(bin_pos).copies
            for bin_pos in range(solution.number_of_different_bins())
            for node in solution.bin(bin_pos).nodes
            if node.f != -1 and node.item_type_id >= 0) == 5
    for node in items:
        assert node.item_type_id == 0
        assert node.r - node.l == 5
        assert node.t - node.b == 5


def test_number_of_stages():
    instance_builder = psg.InstanceBuilder()
    instance_builder.set_objective(psg.Objective.BinPacking)
    bin_type_id = instance_builder.add_bin_type(10, 10)
    instance_builder.set_bin_type_copies(bin_type_id, 10)
    item_type_id = instance_builder.add_item_type(5, 5)
    instance_builder.set_item_type_copies(item_type_id, 4)
    instance_builder.set_number_of_stages(2)
    instance_builder.set_cut_type(psg.CutType.Exact)
    instance_builder.set_first_stage_orientation(psg.CutOrientation.Horizontal)
    output = psg.optimize(instance_builder.build(), quiet_parameters())
    solution = output.solution
    assert solution.feasible()
    assert solution.number_of_stages_feasible()
    assert solution.number_of_bins() == 1
    assert output.is_proven_optimal()
    assert solution.bin(0).first_cut_orientation == psg.CutOrientation.Horizontal
    assert solution.bin(0).number_of_stages_real <= 2


def test_knapsack():
    instance_builder = psg.InstanceBuilder()
    instance_builder.set_objective(psg.Objective.Knapsack)
    instance_builder.add_bin_type(10, 10)
    # The 10x6 item with profit 6 and the 10x4 item fill the bin; the area
    # bound is tight.
    for width, height, profit in [(10, 6, 6), (10, 4, 4), (10, 6, 3)]:
        item_type_id = instance_builder.add_item_type(width, height, oriented=True)
        instance_builder.set_item_type_profit(item_type_id, profit)
    output = psg.optimize(instance_builder.build(), quiet_parameters())
    assert output.solution.profit() == 10
    assert output.solution.item_copies(0) == 1
    assert output.solution.item_copies(1) == 1
    assert output.solution.item_copies(2) == 0
    assert output.knapsack_bound == pytest.approx(10)
    assert output.is_proven_optimal()


def test_open_dimension_x():
    instance_builder = psg.InstanceBuilder()
    instance_builder.set_objective(psg.Objective.OpenDimensionX)
    instance_builder.add_bin_type(1000, 10)
    item_type_id = instance_builder.add_item_type(10, 10, oriented=True)
    instance_builder.set_item_type_copies(item_type_id, 5)
    output = psg.optimize(instance_builder.build(), quiet_parameters())
    assert output.solution.full()
    assert output.solution.width() == 50
    assert output.open_dimension_x_bound == 50
    assert output.is_proven_optimal()


def test_read_csv():
    path = os.path.join(DATA_DIR, "bin_packing_3nvo")
    instance_builder = psg.InstanceBuilder()
    instance_builder.read_item_types(os.path.join(path, "items.csv"))
    instance_builder.read_bin_types(os.path.join(path, "bins.csv"))
    instance_builder.read_parameters(os.path.join(path, "parameters.csv"))
    instance = instance_builder.build()
    assert instance.objective() == psg.Objective.BinPacking
    assert instance.parameters().number_of_stages == 3
    assert instance.parameters().cut_type == psg.CutType.NonExact
    assert instance.parameters().first_stage_orientation == psg.CutOrientation.Vertical
    output = psg.optimize(instance, quiet_parameters())
    # The items exactly fill one 20x20 bin.
    assert output.solution.number_of_bins() == 1
    assert output.solution.full()
    assert output.is_proven_optimal()


def test_read_defects():
    path = os.path.join(DATA_DIR, "knapsack_2nvo_defects_2")
    instance_builder = psg.InstanceBuilder()
    instance_builder.read_item_types(os.path.join(path, "items.csv"))
    instance_builder.read_bin_types(os.path.join(path, "bins.csv"))
    instance_builder.read_defects(os.path.join(path, "defects.csv"))
    instance_builder.read_parameters(os.path.join(path, "parameters.csv"))
    instance = instance_builder.build()
    assert instance.objective() == psg.Objective.Knapsack
    assert instance.number_of_defects() == 1
    defect = instance.bin_type(0).defects[0]
    assert (defect.x, defect.y, defect.w, defect.h) == (20, 5, 10, 5)


def test_read_missing_file():
    instance_builder = psg.InstanceBuilder()
    with pytest.raises(Exception):
        instance_builder.read_item_types("this/file/does/not/exist.csv")


def test_write(tmp_path):
    instance_builder = psg.InstanceBuilder()
    instance_builder.set_objective(psg.Objective.BinPacking)
    bin_type_id = instance_builder.add_bin_type(10, 10)
    instance_builder.set_bin_type_copies(bin_type_id, 10)
    item_type_id = instance_builder.add_item_type(5, 5)
    instance_builder.set_item_type_copies(item_type_id, 5)
    instance_builder.set_number_of_stages(2)
    instance = instance_builder.build()
    instance_path = str(tmp_path / "instance.json")
    instance.write(instance_path, psg.InstanceFormat.Json)
    instance_builder = psg.InstanceBuilder()
    instance_builder.read(instance_path)
    instance_read = instance_builder.build()
    assert instance_read.number_of_items() == 5
    assert instance_read.parameters().number_of_stages == 2

    output = psg.optimize(instance, quiet_parameters())
    certificate_path = str(tmp_path / "solution.csv")
    output.solution.write(certificate_path)
    assert os.path.getsize(certificate_path) > 0


def test_to_json():
    output = psg.optimize(bin_packing_instance(5), quiet_parameters())
    json = output.to_json()
    assert isinstance(json, dict)
    assert json["BinPackingBound"] == 2
    assert json["Solution"] == output.solution.to_json()
    assert json["Solution"]["NumberOfBins"] == 2
    assert json["Solution"]["Feasible"]["Feasible"]


def test_parameters():
    parameters = psg.OptimizeParameters()
    assert parameters.time_limit == math.inf
    parameters.time_limit = 1.5
    assert parameters.time_limit == 1.5
    parameters.optimization_mode = psg.OptimizationMode.NotAnytimeSequential
    parameters.tree_search_guides = [0, 1]
    assert parameters.tree_search_guides == [0, 1]
    parameters.use_tree_search = True
    assert parameters.use_tree_search
    parameters.many_items_in_bins_threshold_2 = 32
    assert parameters.many_items_in_bins_threshold_2 == 32
    parameters.reduction_parameters.reduce = False
    assert not parameters.reduction_parameters.reduce
    parameters.reduction_parameters.merge_identical_items = False
    assert not parameters.reduction_parameters.merge_identical_items
    parameters.verbosity_level = 0
    output = psg.optimize(bin_packing_instance(5), parameters)
    assert output.solution.number_of_bins() == 2


def test_lifetimes():
    """Outputs and solutions stay valid after their instance is dropped."""
    output = psg.optimize(bin_packing_instance(5), quiet_parameters())
    gc.collect()
    solution = output.solution
    del output
    gc.collect()
    assert solution.number_of_bins() == 2
    assert solution.bin(0).bin_type_id == 0
    assert solution.bin(0).nodes[0].f == -1


def test_new_solution_callback():
    outputs = []
    parameters = quiet_parameters()
    parameters.new_solution_callback = outputs.append
    psg.optimize(bin_packing_instance(5), parameters)
    assert len(outputs) >= 1
    gc.collect()
    assert outputs[-1].solution.number_of_bins() == 2


def test_column_generation():
    """Column generation (LP solved with HiGHS in the Python module)."""
    instance_builder = psg.InstanceBuilder()
    instance_builder.set_objective(psg.Objective.BinPacking)
    bin_type_id = instance_builder.add_bin_type(100, 100)
    instance_builder.set_bin_type_copies(bin_type_id, 20)
    for width, height, copies in [(30, 40, 6), (50, 20, 5), (70, 35, 3)]:
        item_type_id = instance_builder.add_item_type(width, height, oriented=True)
        instance_builder.set_item_type_copies(item_type_id, copies)
    parameters = quiet_parameters(time_limit=10.0, use_column_generation=True)
    output = psg.optimize(instance_builder.build(), parameters)
    assert output.bin_packing_bound >= 2
    if output.solution.feasible():
        assert output.solution.number_of_bins() >= output.bin_packing_bound


def test_column_generation_strips():
    """Column generation strips (LP solved with HiGHS in the Python module)."""
    instance_builder = psg.InstanceBuilder()
    instance_builder.set_objective(psg.Objective.Knapsack)
    instance_builder.add_bin_type(100, 100)
    for width, height, profit, copies in [(30, 40, 15, 3), (50, 20, 11, 4), (70, 35, 30, 2)]:
        item_type_id = instance_builder.add_item_type(width, height, oriented=True)
        instance_builder.set_item_type_profit(item_type_id, profit)
        instance_builder.set_item_type_copies(item_type_id, copies)
    instance_builder.set_number_of_stages(2)
    parameters = quiet_parameters(time_limit=10.0, use_column_generation_strips=True)
    output = psg.optimize(instance_builder.build(), parameters)
    assert output.solution.feasible()
    assert output.solution.profit() > 0
    assert output.solution.profit() <= output.knapsack_bound + 1e-6
