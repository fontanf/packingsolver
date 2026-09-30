import csv
import io
import os

import pytest

pytest.importorskip("plotly")

from packingsolver.visualize.box import figure


DATA_DIR = os.path.join(
        os.path.dirname(__file__), "..", "..", "data", "box", "tests")

CERTIFICATES = [
    "knapsack_merge_identical_items_requires_matching_profit",
    "variable_sized_bin_packing_mandatory_bins",
    "open_dimension_x_4_different_items_xz",
]


def certificate_path(name):
    return os.path.join(DATA_DIR, name, "solution.csv")


def count_rows(path):
    number_of_bins = 0
    number_of_items = 0
    with open(path, newline='') as f:
        for row in csv.DictReader(f):
            if row["TYPE"] == "BIN":
                number_of_bins += 1
            elif row["TYPE"] == "ITEM":
                number_of_items += 1
    return number_of_bins, number_of_items


@pytest.mark.parametrize("name", CERTIFICATES)
def test_figure_path_and_stream(name):
    import plotly.graph_objects as go

    path = certificate_path(name)
    number_of_bins, number_of_items = count_rows(path)
    fig = figure(path)
    assert isinstance(fig, go.Figure)
    # Per bin: bin, defects, item borders and item ids traces; plus one
    # trace per item.
    assert len(fig.data) == 4 * number_of_bins + number_of_items
    with open(path) as f:
        stream = io.StringIO(f.read())
    assert fig.to_json() == figure(stream).to_json()


def test_figure_options():
    path = certificate_path(CERTIFICATES[1])
    default = figure(path)
    fig = figure(path, item_color="SAME", columns=1, zoom=2.0, legend=False)
    assert fig.to_json() != default.to_json()
    assert fig.layout.showlegend is False
    assert fig.layout.scene.camera.eye.x == pytest.approx(-1.75 / 2.0)
    items = [trace for trace in fig.data if trace.legendgroup == "items"
             and trace.type == "mesh3d"]
    assert items
    assert all(trace.color == "cornflowerblue" for trace in items)
    assert all(trace.name == "Items" for trace in items)


def test_figure_unknown_item_color():
    with pytest.raises(ValueError):
        figure(certificate_path(CERTIFICATES[0]), item_color="FOO")
