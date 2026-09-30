import io
import os

import pytest

pytest.importorskip("plotly")

import plotly.graph_objects as go  # noqa: E402

from packingsolver.visualize.boxstacks import figure  # noqa: E402


DATA_DIR = os.path.join(
        os.path.dirname(__file__), "..", "..", "data", "boxstacks", "tests")

CERTIFICATES = [
    os.path.join(DATA_DIR, "bin_packing_postal_cartons_eur_pallets", "solution.csv"),
    os.path.join(DATA_DIR, "variable_sized_bin_packing_two_pallet_types_time_limit", "solution.csv"),
    os.path.join(DATA_DIR, "knapsack_multi_bin_svc_zero_copies_item_type", "solution.csv"),
]

# Two bins of the same type, three copies each.
COPIES_CERTIFICATE = (
    "TYPE,ID,COPIES,BIN,STACK,X,Y,Z,LX,LY,LZ,GROUP_ID,ROTATION\n"
    "BIN,0,3,0,-1,0,0,0,10,10,10,,\n"
    "STACK,0,1,0,0,0,0,0,5,5,5,,\n"
    "ITEM,0,1,0,0,0,0,0,5,5,5,0,XYZ\n"
    "BIN,0,3,1,-1,0,0,0,10,10,10,,\n"
    "STACK,0,1,1,0,0,0,0,5,5,5,,\n"
    "ITEM,1,1,1,0,0,0,0,5,5,5,0,XYZ\n"
)


def count_rows(path, type_):
    with open(path) as f:
        return sum(1 for line in f if line.startswith(type_ + ","))


@pytest.mark.parametrize("path", CERTIFICATES)
def test_figure_path_and_stream(path):
    fig = figure(path)
    assert isinstance(fig, go.Figure)
    # Per bin: bins, defects, item borders and item ids traces, plus one
    # trace per item.
    number_of_bins = count_rows(path, "BIN")
    number_of_items = count_rows(path, "ITEM")
    assert len(fig.data) == 4 * number_of_bins + number_of_items
    with open(path) as f:
        stream = io.StringIO(f.read())
    assert figure(stream).to_json() == fig.to_json()


@pytest.mark.parametrize("path", CERTIFICATES)
def test_figure_options(path):
    fig = figure(path, item_color="SAME", columns=1, zoom=2.0, legend=False)
    assert isinstance(fig, go.Figure)
    assert fig.layout.showlegend is False
    items = [t for t in fig.data if t.legendgroup == "items" and t.type == "mesh3d"]
    assert items
    assert all(t.color == "cornflowerblue" for t in items)
    assert fig.layout.scene.camera.eye.x == pytest.approx(-1.75 / 2.0)


def test_figure_invalid_item_color():
    with pytest.raises(ValueError):
        figure(CERTIFICATES[0], item_color="UNKNOWN")


def test_figure_expand_copies():
    fig = figure(io.StringIO(COPIES_CERTIFICATE))
    assert len(fig.data) == 2 * 5
    fig = figure(io.StringIO(COPIES_CERTIFICATE), expand_copies=True)
    assert len(fig.data) == 6 * 5
    scenes = [k for k in fig.layout if k.startswith("scene")]
    assert len(scenes) == 6
