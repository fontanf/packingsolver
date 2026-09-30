import io
import os

import pytest

go = pytest.importorskip("plotly.graph_objects")

from packingsolver.visualize.onedimensional import figure


DATA_DIR = os.path.join(
    os.path.dirname(os.path.abspath(__file__)),
    "..", "..", "data", "onedimensional")

CERTIFICATES = [
    os.path.join(DATA_DIR, "tests", "bin_packing_eligibility", "solution.csv"),
    os.path.join(DATA_DIR, "tests", "variable_sized_bin_packing_single_bin_type", "solution.csv"),
    os.path.join(DATA_DIR, "tests", "knapsack_single_bin_with_side_constraint_still_merges", "solution.csv"),
]


def read(path):
    with open(path, newline='') as f:
        return f.read()


def number_of_bins(path, expand_copies=False):
    number_of_bins = 0
    for line in read(path).splitlines()[1:]:
        row = line.split(",")
        if row[0] == "BIN":
            number_of_bins += int(row[2]) if expand_copies else 1
    return number_of_bins


@pytest.mark.parametrize("path", CERTIFICATES)
def test_figure_path_and_stream(path):
    fig = figure(path)
    assert isinstance(fig, go.Figure)
    bin_traces = [trace for trace in fig.data if trace.name == "Bins"]
    assert len(bin_traces) == number_of_bins(path)
    assert len(fig.data) > len(bin_traces)
    fig_stream = figure(io.StringIO(read(path)))
    assert fig.to_json() == fig_stream.to_json()


@pytest.mark.parametrize("path", CERTIFICATES)
def test_figure_item_color_same(path):
    fig = figure(path, item_color="SAME")
    item_traces = [trace for trace in fig.data if trace.name == "Items"]
    assert len(item_traces) > 0
    assert all(trace.fillcolor == "cornflowerblue" for trace in item_traces)
    assert figure(io.StringIO(read(path)), item_color="SAME").to_json() == fig.to_json()


def test_figure_expand_copies():
    # Solution with a bin used twice.
    path = os.path.join(DATA_DIR, "users", "2024-04-09_solution.csv")
    fig = figure(path)
    fig_expanded = figure(path, expand_copies=True)
    bin_traces = [trace for trace in fig.data if trace.name == "Bins"]
    bin_traces_expanded = [trace for trace in fig_expanded.data if trace.name == "Bins"]
    assert len(bin_traces) == number_of_bins(path)
    assert len(bin_traces_expanded) == number_of_bins(path, expand_copies=True)
    assert len(bin_traces_expanded) > len(bin_traces)
    fig_stream = figure(io.StringIO(read(path)), expand_copies=True)
    assert fig_expanded.to_json() == fig_stream.to_json()


def test_figure_unknown_item_color():
    with pytest.raises(ValueError):
        figure(CERTIFICATES[0], item_color="UNKNOWN")
