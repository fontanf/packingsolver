"""Visualization of irregular solution certificates and instances.

Build a plotly figure from a certificate (JSON) written by the irregular
solver, or from an irregular instance (JSON).
"""

import contextlib
import json
import os
import plotly.graph_objects as go
import plotly.express as px
import plotly.subplots
import numpy as np
import math


def _open(file):
    if isinstance(file, (str, os.PathLike)):
        return open(file, 'r')
    return contextlib.nullcontext(file)


def _certificate_element(element):
    t = element["type"]
    xs = element["xs"]
    ys = element["ys"]
    xe = element["xe"]
    ye = element["ye"]
    if t == "CircularArc":
        return t, xs, ys, xe, ye, element["xc"], element["yc"], element["orientation"]
    return t, xs, ys, xe, ye, None, None, None


def _instance_element(element):
    t = element["type"]
    xs = element["start"]["x"]
    ys = element["start"]["y"]
    xe = element["end"]["x"]
    ye = element["end"]["y"]
    if t == "CircularArc":
        return t, xs, ys, xe, ye, element["center"]["x"], element["center"]["y"], element["orientation"]
    return t, xs, ys, xe, ye, None, None, None


def _shape_path(
        path_x,
        path_y,
        shape,
        is_hole,
        read_element,
        handle_full):
    # How to draw a filled circle segment?
    # https://community.plotly.com/t/how-to-draw-a-filled-circle-segment/59583
    # https://stackoverflow.com/questions/70965145/can-plotly-for-python-plot-a-polygon-with-one-or-multiple-holes-in-it
    for element in (shape if not is_hole else reversed(shape)):
        t, xs, ys, xe, ye, xc, yc, orientation = read_element(element)
        if t == "CircularArc":
            rc = math.sqrt((xc - xs)**2 + (yc - ys)**2)

        if is_hole:
            xs, ys, xe, ye = xe, ye, xs, ys

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
            if handle_full and orientation in ["Full", "full", "F", "f"]:
                end_angle += 2 * math.pi
            if (orientation in ["Anticlockwise", "anticlockwise", "A", "a"]
                    and end_angle <= start_angle):
                end_angle += 2 * math.pi
            if (orientation in ["Clockwise", "clockwise", "C", "c"]
                    and end_angle >= start_angle):
                end_angle -= 2 * math.pi

            t = np.linspace(start_angle, end_angle, 1024)
            x = xc + rc * np.cos(t)
            y = yc + rc * np.sin(t)
            for xa, ya in zip(x[1:], y[1:]):
                path_x.append(xa)
                path_y.append(ya)
    path_x.append(None)
    path_y.append(None)


def _certificate_shape_path(path_x, path_y, shape, is_hole=False):
    _shape_path(path_x, path_y, shape, is_hole, _certificate_element, True)


def _instance_shape_path(path_x, path_y, shape, is_hole=False):
    _shape_path(path_x, path_y, shape, is_hole, _instance_element, False)


def _certificate_paths(certificate):
    bins_x = []
    bins_y = []
    defects_x = []
    defects_y = []
    items_x = []
    items_y = []

    with _open(certificate) as f:
        j = json.load(f)

        for bin_pos, solution_bin in enumerate(j["bins"]):
            bins_x.append([])
            bins_y.append([])
            defects_x.append([])
            defects_y.append([])
            items_x.append([])
            items_y.append([])

            _certificate_shape_path(bins_x[bin_pos], bins_y[bin_pos], solution_bin["shape"])
            for defect in (solution_bin["defects"]
                           if "defects" in solution_bin else []):
                _certificate_shape_path(defects_x[bin_pos], defects_y[bin_pos], defect["shape"])
                for hole in (defect["holes"]
                             if "holes" in defect else []):
                    _certificate_shape_path(defects_x[bin_pos], defects_y[bin_pos], hole, True)
            for solution_item in solution_bin["items"]:
                item_id = solution_item["id"]
                while len(items_x[bin_pos]) <= item_id:
                    items_x[bin_pos].append([])
                    items_y[bin_pos].append([])
                for item_shape in solution_item["item_shapes"]:
                    _certificate_shape_path(items_x[bin_pos][item_id],
                                            items_y[bin_pos][item_id],
                                            item_shape["shape"])
                    for hole in (item_shape["holes"]
                                 if "holes" in item_shape else []):
                        _certificate_shape_path(items_x[bin_pos][item_id], items_y[bin_pos][item_id], hole, True)

    return bins_x, bins_y, defects_x, defects_y, items_x, items_y


def _grid(m, columns):
    number_of_cols = columns if columns is not None else math.ceil(math.sqrt(m))
    number_of_rows = math.ceil(m / number_of_cols)
    return number_of_rows, number_of_cols


def figure(
        certificate,
        *,
        item_color="ID",
        columns=None):
    """Build the figure of an irregular solution certificate.

    certificate: path to JSON file, or text stream of its content
    item_color: color palette used among ["SAME", "ID"]
    columns: number of columns in the subplot grid

    Return a plotly.graph_objects.Figure.
    """
    if item_color not in ["SAME", "ID"]:
        raise ValueError(f"color palette {item_color} is unknown, please use one of the following: 'SAME', 'ID'")

    bins_x, bins_y, defects_x, defects_y, items_x, items_y = \
        _certificate_paths(certificate)

    colors = px.colors.qualitative.Pastel
    m = len(bins_x)
    number_of_rows, number_of_cols = _grid(m, columns)
    fig = plotly.subplots.make_subplots(
            rows=number_of_rows,
            cols=number_of_cols,
            shared_xaxes=True,
            vertical_spacing=0.001)

    for i in range(0, m):
        row = (i // number_of_cols) + 1
        col = (i % number_of_cols) + 1

        fig.add_trace(go.Scatter(
            x=bins_x[i],
            y=bins_y[i],
            name="Bins",
            legendgroup="bins",
            showlegend=(i == 0),
            marker=dict(
                color='black',
                size=1)),
            row=row,
            col=col)

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
            row=row,
            col=col)

        for k in range(len(items_x[i])):
            if item_color == 'SAME':
                fig.add_trace(go.Scatter(
                    x=items_x[i][k],
                    y=items_y[i][k],
                    name="Items",
                    legendgroup="items",
                    showlegend=(i == 0 and k == 0),
                    fillcolor="cornflowerblue",
                    fill="toself",
                    marker=dict(
                        color='black',
                        size=1)),
                    row=i + 1,
                    col=1)
            elif item_color == 'ID':
                fig.add_trace(go.Scatter(
                    x=items_x[i][k],
                    y=items_y[i][k],
                    name=f"Items {k}",
                    legendgroup="item",
                    showlegend=i == 0,
                    fillcolor=colors[k % len(colors)],
                    fill="toself",
                    marker=dict(
                        color='black',
                        size=1)),
                    row=row,
                    col=col)

    # Plot.
    fig.update_layout(
            autosize=True,
            font=dict(size=14),
            legend=dict(font=dict(size=14), itemsizing='constant'))
    fig.update_xaxes(
            rangeslider=dict(visible=False))
    for i in range(0, m):
        row = (i // number_of_cols) + 1
        col = (i % number_of_cols) + 1
        fig.update_yaxes(
                scaleanchor="x" if i == 0 else f"x{i + 1}",
                scaleratio=1,
                row=row,
                col=col)
    return fig


def export_size(
        certificate,
        *,
        columns=None,
        scale=1.0,
        width=None,
        height=None):
    """Compute the image size used to export the figure of a certificate.

    certificate: path to JSON file, or text stream of its content
    columns: number of columns in the subplot grid
    scale: scale factor for cell dimensions
    width: image width in pixels, computed from the bins if None
    height: image height in pixels, computed from the bins if None

    Return the tuple (width, height).
    """
    bins_x, bins_y, _, _, _, _ = _certificate_paths(certificate)
    m = len(bins_x)
    max_bin_lx = max(max(x for x in bx if x is not None) - min(x for x in bx if x is not None) for bx in bins_x)
    max_bin_ly = max(max(y for y in by if y is not None) - min(y for y in by if y is not None) for by in bins_y)
    number_of_rows, number_of_cols = _grid(m, columns)
    cell_width = int(max_bin_lx * scale)
    cell_height = int(max_bin_ly * scale)
    export_width = width if width is not None else number_of_cols * cell_width + 160
    export_height = height if height is not None else number_of_rows * cell_height + 170
    return export_width, export_height


def instance_figure(instance):
    """Build the figure of an irregular instance.

    instance: path to JSON file, or text stream of its content

    Return a plotly.graph_objects.Figure.
    """
    bin_types_x = []
    bin_types_y = []
    defects_x = []
    defects_y = []
    item_types_x = []
    item_types_y = []

    with _open(instance) as f:
        j = json.load(f)

        for bin_type_id, bin_type in enumerate(j["bin_types"]):
            bin_types_x.append([])
            bin_types_y.append([])
            defects_x.append([])
            defects_y.append([])

            _instance_shape_path(
                    bin_types_x[bin_type_id],
                    bin_types_y[bin_type_id],
                    bin_type["elements"])
            for defect in (bin_type["defects"]
                           if "defects" in bin_type else []):
                _instance_shape_path(
                        defects_x[bin_type_id],
                        defects_y[bin_type_id],
                        defect["elements"])
                for hole in (defect["holes"]
                             if "holes" in defect else []):
                    _instance_shape_path(
                            defects_x[bin_type_id],
                            defects_y[bin_type_id],
                            hole["elements"],
                            True)

        for item_type_id, item_type in enumerate(j["item_types"]):
            item_types_x.append([])
            item_types_y.append([])
            for item_shape in item_type["shapes"]:
                _instance_shape_path(
                        item_types_x[item_type_id],
                        item_types_y[item_type_id],
                        item_shape["elements"])
                for hole in (item_shape["holes"]
                             if "holes" in item_shape else []):
                    _instance_shape_path(
                            item_types_x[item_type_id],
                            item_types_y[item_type_id],
                            hole["elements"],
                            True)

    m = len(bin_types_x)
    n = len(item_types_x)
    colors = px.colors.qualitative.Plotly
    fig = plotly.subplots.make_subplots(
            rows=m + n,
            cols=1,
            shared_xaxes=True,
            vertical_spacing=0.001)

    for i in range(0, m):

        fig.add_trace(go.Scatter(
            x=bin_types_x[i],
            y=bin_types_y[i],
            name="Bin types",
            legendgroup="bin types",
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

    for i in range(0, n):

        fig.add_trace(go.Scatter(
            x=item_types_x[i],
            y=item_types_y[i],
            name="Item types",
            legendgroup="item types",
            showlegend=(i == 0),
            fillcolor="cornflowerblue",
            fill="toself",
            marker=dict(
                color='black',
                size=1)),
            row=m + i + 1,
            col=1)

    # Plot.
    fig.update_layout(
            autosize=True,
            height=(m + n)*1000)
    fig.update_xaxes(
            rangeslider=dict(visible=False))
    for i in range(0, m + n):
        fig.update_yaxes(
                scaleanchor="x" if i == 0 else f"x{i + 1}",
                scaleratio=1,
                row=i + 1,
                col=1)
    return fig
