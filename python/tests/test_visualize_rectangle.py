import io
import os

import pytest

pytest.importorskip("plotly")

import plotly.graph_objects as go

from packingsolver.visualize.rectangle import figure


DATA_DIR = os.path.join(
        os.path.dirname(__file__), "..", "..", "data", "rectangle", "tests")

CERTIFICATES = [
    "bin_packing_two_bin_types",
    "bin_packing_mixed_items_two_bins",
    "bin_packing_merge_identical_items_with_defects",
]


def certificate_path(name):
    return os.path.join(DATA_DIR, name, "solution.csv")


def number_of_bins(path):
    with open(path) as f:
        return sum(1 for line in f if line.startswith("BIN,"))


@pytest.mark.parametrize("name", CERTIFICATES)
def test_figure_path_and_stream(name):
    path = certificate_path(name)
    fig = figure(path)
    assert isinstance(fig, go.Figure)
    # At least bins, defects and item ids traces per bin.
    assert len(fig.data) >= 3 * number_of_bins(path)
    with open(path, newline='') as f:
        stream = io.StringIO(f.read())
    fig_stream = figure(stream)
    assert fig_stream.to_json() == fig.to_json()


@pytest.mark.parametrize("item_color", ["SAME", "ID", "GROUP_ID", "DENSITY"])
def test_figure_item_color(item_color):
    path = certificate_path("bin_packing_mixed_items_two_bins")
    fig = figure(path, item_color=item_color)
    assert isinstance(fig, go.Figure)
    assert len(fig.data) >= 3 * number_of_bins(path)
    names = [trace.name for trace in fig.data]
    if item_color == "SAME":
        assert "Items" in names
    if item_color == "DENSITY":
        assert "Gravity" in names


def test_figure_columns():
    path = certificate_path("bin_packing_two_bin_types")
    fig = figure(path, columns=1)
    rows, cols = fig._get_subplot_rows_columns()
    assert len(cols) == 1
    assert len(rows) == number_of_bins(path)


def test_figure_unknown_item_color():
    with pytest.raises(ValueError):
        figure(certificate_path("bin_packing_two_bin_types"), item_color="FOO")
