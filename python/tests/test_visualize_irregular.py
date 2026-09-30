import io
import os

import pytest

pytest.importorskip("plotly")

from packingsolver.visualize.irregular import figure, instance_figure


DATA_DIR = os.path.join(
        os.path.dirname(__file__), "..", "..", "data", "irregular", "tests")


def data_path(name):
    return os.path.join(DATA_DIR, name)


def stream(path):
    with open(path) as f:
        return io.StringIO(f.read())


@pytest.mark.parametrize("name, number_of_bins", [
    ("multiple_bins_solution.json", 2),
    ("polygon_with_hole_solution.json", 1),
    ("item_defect_minimum_spacing_solution.json", 1),
])
def test_figure_path_and_stream(name, number_of_bins):
    path = data_path(name)
    fig_path = figure(path)
    fig_stream = figure(stream(path))
    # One bin trace and one defect trace per bin, plus the items.
    assert len(fig_path.data) > 2 * number_of_bins
    bin_traces = [trace for trace in fig_path.data if trace.name == "Bins"]
    assert len(bin_traces) == number_of_bins
    assert fig_path.to_json() == fig_stream.to_json()


def test_figure_holes():
    fig = figure(data_path("polygon_with_hole_solution.json"))
    item_traces = [trace for trace in fig.data
                   if trace.name.startswith("Items")]
    # An item with a hole: the path contains two closed contours.
    assert any(sum(1 for x in trace.x if x is None) >= 2
               for trace in item_traces)


def test_figure_options():
    path = data_path("multiple_bins_solution.json")
    fig_one_column = figure(path, columns=1)
    fig_two_columns = figure(path, columns=2)
    assert fig_one_column.to_json() != fig_two_columns.to_json()
    fig_same = figure(stream(path), item_color="SAME", columns=1)
    assert fig_same.to_json() == figure(
            path, item_color="SAME", columns=1).to_json()
    assert all(trace.name == "Items" for trace in fig_same.data
               if trace.name not in ("Bins", "Defects"))
    with pytest.raises(ValueError):
        figure(path, item_color="UNKNOWN")


@pytest.mark.parametrize("name", [
    "periodic_packing_item_bin_spacing.json",
    "periodic_packing_item_item_spacing.json",
    "periodic_packing_single_item_exact_fit_triangle.json",
])
def test_instance_figure_path_and_stream(name):
    path = data_path(name)
    fig_path = instance_figure(path)
    fig_stream = instance_figure(stream(path))
    # One bin type trace and one defect trace per bin type, plus one trace
    # per item type.
    assert len(fig_path.data) >= 3
    assert any(trace.name == "Bin types" for trace in fig_path.data)
    assert any(trace.name == "Item types" for trace in fig_path.data)
    assert fig_path.to_json() == fig_stream.to_json()
