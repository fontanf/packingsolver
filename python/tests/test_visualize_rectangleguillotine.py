import csv
import io
import os

import pytest

pytest.importorskip("plotly")

from packingsolver.visualize.rectangleguillotine import figure  # noqa: E402


DATA_DIR = os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "..", "..", "data", "rectangleguillotine", "tests")

CERTIFICATES = [
        "bin_packing_3nvr",
        "knapsack_2nvo_defects_2",
        "knapsack_uo_trims",
]


def certificate_path(name):
    return os.path.join(DATA_DIR, name, "solution.csv")


def number_of_bins(path):
    with open(path, newline='') as f:
        return sum(
                1 for row in csv.DictReader(f)
                if not row["PARENT"] and int(row["TYPE"]) != -4)


@pytest.mark.parametrize("name", CERTIFICATES)
def test_figure_path_and_stream(name):
    import plotly.graph_objects as go
    path = certificate_path(name)
    fig = figure(path)
    assert isinstance(fig, go.Figure)
    m = number_of_bins(path)
    assert m >= 1
    assert len([trace for trace in fig.data if trace.name == "Bins"]) == m
    # Bins, defects, trims and item ids traces for each bin, plus cuts/items.
    assert len(fig.data) > 4 * m
    with open(path, newline='') as f:
        stream = io.StringIO(f.read())
    assert figure(stream).to_json() == fig.to_json()


def test_figure_item_color_same():
    path = certificate_path("bin_packing_3nvr")
    fig_same = figure(path, item_color="SAME")
    items = [trace for trace in fig_same.data if trace.name == "Items"]
    assert len(items) > 0
    assert all(trace.fillcolor == "cornflowerblue" for trace in items)
    fig_id = figure(path, item_color="ID")
    assert any(trace.name.startswith("Items ") for trace in fig_id.data)
    assert fig_same.to_json() != fig_id.to_json()


def test_figure_columns():
    path = certificate_path("bin_packing_3nvr")
    m = number_of_bins(path)
    fig = figure(path, columns=1)
    rows, cols = fig._get_subplot_rows_columns()
    assert len(cols) == 1
    assert len(rows) == m


def test_figure_unknown_item_color():
    with pytest.raises(ValueError):
        figure(certificate_path("bin_packing_3nvr"), item_color="FOO")
