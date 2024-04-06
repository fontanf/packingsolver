import argparse
import json
import plotly.graph_objects as go
import plotly.express as px
import plotly.subplots
import numpy as np
import math


def shape_path(path_x, path_y, shape, is_hole=False):
    # How to draw a filled circle segment?
    # https://community.plotly.com/t/how-to-draw-a-filled-circle-segment/59583
    # https://stackoverflow.com/questions/70965145/can-plotly-for-python-plot-a-polygon-with-one-or-multiple-holes-in-it
    for element in (shape if not is_hole else reversed(shape)):
        t = element["type"]
        xs = element["xs"]
        ys = element["ys"]
        xe = element["xe"]
        ye = element["ye"]
        if t == "CircularArc":
            xc = element["xc"]
            yc = element["yc"]
            anticlockwise = 1 if element["anticlockwise"] else 0
            rc = math.sqrt((xc - xs)**2 + (yc - ys)**2)

        if len(path_x) == 0 or path_x[-1] is None:
            path_x.append(xs)
            path_y.append(ys)

        if t == "LineSegment":
            path_x.append(xe)
            path_y.append(ye)
        elif t == "CircularArc":
            start_cos = (xs - xc) / rc
            start_sin = (ys - yc) / rc
            start_angle = math.atan2(start_sin, start_cos)
            end_cos = (xe - xc) / rc
            end_sin = (ye - yc) / rc
            end_angle = math.atan2(end_sin, end_cos)
            if anticlockwise and end_angle <= start_angle:
                end_angle += 2 * math.pi
            if not anticlockwise and end_angle >= start_angle:
                end_angle -= 2 * math.pi

            t = np.linspace(start_angle, end_angle, 1024)
            x = xc + rc * np.cos(t)
            y = yc + rc * np.sin(t)
            for xa, ya in zip(x[1:], y[1:]):
                path_x.append(xa)
                path_y.append(ya)
    path_x.append(None)
    path_y.append(None)


parser = argparse.ArgumentParser(description='')
parser.add_argument('csvpath', help='path to JSON file')
args = parser.parse_args()

bins_x = []
bins_y = []
defects_x = []
defects_y = []
items_x = []
items_y = []

with open(args.csvpath, 'r') as f:
    j = json.load(f)

    for bin_pos, solution_bin in enumerate(j["bins"]):
        bins_x.append([])
        bins_y.append([])
        defects_x.append([])
        defects_y.append([])
        items_x.append([])
        items_y.append([])

        shape_path(bins_x[bin_pos], bins_y[bin_pos], solution_bin["shape"])
        for defect in (solution_bin["defects"]
                       if "defects" in solution_bin else []):
            shape_path(defects_x[bin_pos], defects_y[bin_pos], defect["shape"])
            for hole in defect["holes"]:
                shape_path(defects_x[bin_pos], defects_y[bin_pos], hole, True)
        for solution_item in solution_bin["items"]:
            for item_shape in solution_item["item_shapes"]:
                shape_path(items_x[bin_pos],
                           items_y[bin_pos],
                           item_shape["shape"])
                for hole in (item_shape["holes"]
                             if "holes" in item_shape else []):
                    shape_path(items_x[bin_pos], items_y[bin_pos], hole, True)

m = len(bins_x)
colors = px.colors.qualitative.Plotly
fig = plotly.subplots.make_subplots(
        rows=m,
        cols=1,
        shared_xaxes=True,
        vertical_spacing=0.001)

for i in range(0, m):

    fig.add_trace(go.Scatter(
        x=bins_x[i],
        y=bins_y[i],
        name="Bins",
        legendgroup="bins",
        showlegend=(i == 0),
        marker=dict(
            color='black',
            size=1)),
        row=i + 1,
        col=1)

    fig.add_trace(go.Scatter(
        x=defects_x[i],
        y=defects_y[i],
        name="Defects",
        legendgroup="defects",
        showlegend=(i == 0),
        fillcolor="crimson",
        fill="toself",
        marker=dict(
            color='black',
            size=1)),
        row=i + 1,
        col=1)

    fig.add_trace(go.Scatter(
        x=items_x[i],
        y=items_y[i],
        name="Items",
        legendgroup="items",
        showlegend=(i == 0),
        fillcolor="cornflowerblue",
        fill="toself",
        marker=dict(
            color='black',
            size=1)),
        row=i + 1,
        col=1)

# Plot.
fig.update_layout(
        autosize=True,
        height=m*1000)
fig.update_xaxes(
        rangeslider=dict(visible=False))
fig.update_yaxes(
        scaleanchor="x",
        scaleratio=1)
fig.show()
