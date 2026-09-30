"""'Solution.write' to a stream and 'visualize', for every problem type."""

import importlib
import io
import pathlib

import pytest


PROBLEM_TYPES = [
    "rectangleguillotine",
    "rectangle",
    "box",
    "boxstacks",
    "onedimensional",
    "irregular",
]


def bin_packing_solution(problem_type):
    """Solve the small bin packing instance of the problem type's tests."""
    ps = importlib.import_module("packingsolver." + problem_type)
    tests = importlib.import_module("test_" + problem_type)
    parameters = ps.OptimizeParameters()
    parameters.verbosity_level = 0
    output = ps.optimize(tests.bin_packing_instance(5), parameters)
    assert output.solution.feasible()
    return ps, output.solution


@pytest.mark.parametrize("problem_type", PROBLEM_TYPES)
def test_write_stream(problem_type, tmp_path):
    _, solution = bin_packing_solution(problem_type)
    certificate_path = tmp_path / "certificate"
    solution.write(str(certificate_path))
    stream = io.StringIO()
    solution.write(stream)
    assert stream.getvalue() == certificate_path.read_text()
    assert stream.getvalue()


@pytest.mark.parametrize("problem_type", PROBLEM_TYPES)
def test_write_path_like(problem_type, tmp_path):
    _, solution = bin_packing_solution(problem_type)
    certificate_path = tmp_path / "certificate"
    solution.write(certificate_path)
    stream = io.StringIO()
    solution.write(stream)
    assert stream.getvalue() == certificate_path.read_text()


def test_write_invalid_argument():
    _, solution = bin_packing_solution("rectangle")
    with pytest.raises(TypeError):
        solution.write(3)


@pytest.mark.parametrize("problem_type", PROBLEM_TYPES)
def test_visualize(problem_type):
    pytest.importorskip("plotly")
    import plotly.graph_objects as go
    ps, solution = bin_packing_solution(problem_type)
    figure = ps.visualize(solution)
    assert isinstance(figure, go.Figure)
    assert len(figure.data) > 0
    # Same figure as from the certificate.
    visualize_module = importlib.import_module(
            "packingsolver.visualize." + problem_type)
    stream = io.StringIO()
    solution.write(stream)
    stream.seek(0)
    assert figure.to_json() == visualize_module.figure(stream).to_json()


def test_visualize_options():
    pytest.importorskip("plotly")
    ps, solution = bin_packing_solution("rectangle")
    assert (ps.visualize(solution, item_color="SAME").to_json()
            != ps.visualize(solution).to_json())
