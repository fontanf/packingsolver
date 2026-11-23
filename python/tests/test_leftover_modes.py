"""The leftover modes of the 'box' and 'boxstacks' problem types, the
leftover value types, and the nested length of the items of the
'onedimensional' problem type."""

import packingsolver.box as psb
import packingsolver.boxstacks as psbs
import packingsolver.onedimensional as pso
import packingsolver.rectangle as psr


def solve(module, instance):
    parameters = module.OptimizeParameters()
    parameters.optimization_mode = module.OptimizationMode.NotAnytimeSequential
    return module.optimize(instance, parameters).solution


def test_box_leftover_modes():
    # 12 items of 30 x 20 x 10 in a 100 x 50 x 40 bin: the volume of the best
    # leftover of each mode (its value is its squared volume).
    expected = {
        psb.LeftoverMode.X: 40 * 50 * 40,
        psb.LeftoverMode.Y: 100 * 30 * 40,
        psb.LeftoverMode.Z: 100 * 50 * 20,
        psb.LeftoverMode.XYZ: 100 * 50 * 40 - 12 * 30 * 20 * 10,
    }
    for leftover_mode, leftover_value in expected.items():
        instance_builder = psb.InstanceBuilder()
        instance_builder.set_objective(psb.Objective.BinPackingWithLeftovers)
        instance_builder.set_leftover_mode(leftover_mode)
        instance_builder.add_bin_type(100, 50, 40, copies=3)
        instance_builder.add_item_type(30, 20, 10, copies=12)
        solution = solve(psb, instance_builder.build())
        assert solution.number_of_bins() == 1
        assert solution.leftover_value() == leftover_value ** 2, leftover_mode


def test_boxstacks_leftover_modes():
    # 3 stacks of 30 x 20 in a 100 x 50 x 40 bin.
    for leftover_mode, leftover_value in [
            (psbs.LeftoverMode.X, 40 * 50 * 40),
            (psbs.LeftoverMode.XY, 100 * 50 * 40 - 60 * 40 * 40)]:
        instance_builder = psbs.InstanceBuilder()
        instance_builder.set_objective(psbs.Objective.BinPackingWithLeftovers)
        instance_builder.set_leftover_mode(leftover_mode)
        instance_builder.add_bin_type(100, 50, 40, copies=3)
        instance_builder.add_item_type(30, 20, 10, copies=12)
        solution = solve(psbs, instance_builder.build())
        assert solution.number_of_bins() == 1
        assert solution.leftover_value() >= leftover_value ** 2, leftover_mode


def test_rectangle_leftover_value_type():
    # Nine 3 x 2 items in a 12 x 10 bin. Valued by its squared area, the best
    # leftover is the L-shape around a 9 x 6 block (area 66). Valued by depth,
    # it is the 6 x 10 rectangle on the right of two columns of items (area
    # 60, value 6^2 (3 * 10 - 6) / 12 = 72).
    expected = {
        psr.LeftoverValueType.SquaredArea: 66 ** 2,
        psr.LeftoverValueType.Depth: 72,
    }
    for leftover_value_type, leftover_value in expected.items():
        instance_builder = psr.InstanceBuilder()
        instance_builder.set_objective(psr.Objective.BinPackingWithLeftovers)
        instance_builder.set_leftover_value_type(leftover_value_type)
        instance_builder.add_bin_type(12, 10, copies=1)
        instance_builder.add_item_type(3, 2, oriented=True, copies=9)
        solution = solve(psr, instance_builder.build())
        assert solution.number_of_bins() == 1
        assert abs(solution.leftover_value() - leftover_value) <= 1e-6, leftover_value_type


def test_onedimensional_item_nested_length():
    # 8 items of length 70, with a nesting length of 10: they occupy 490.
    instance_builder = pso.InstanceBuilder()
    instance_builder.set_objective(pso.Objective.BinPacking)
    instance_builder.add_bin_type(500, copies=10)
    instance_builder.add_item_type(70, nesting_length=10, copies=8)
    solution = solve(pso, instance_builder.build())
    assert solution.number_of_bins() == 1
    assert solution.item_length() == 560
    assert solution.item_nested_length() == 490
