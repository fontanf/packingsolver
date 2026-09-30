"""Visualization of rectangleguillotine solutions.

Build a plotly figure from a rectangleguillotine solution certificate (CSV).
"""

import contextlib
import csv
import os
import plotly.graph_objects as go
import plotly.express as px
import plotly.subplots
import math


def _open(certificate):
    """Open a certificate given as a path, or wrap an already open stream."""
    if isinstance(certificate, (str, bytes, os.PathLike)):
        return open(certificate, newline='')
    return contextlib.nullcontext(certificate)


def figure(
        certificate,
        *,
        item_color="ID",
        columns=None):
    """Build a plotly figure of a rectangleguillotine solution.

    Args:
        certificate: path to CSV file (str or os.PathLike), or a text stream
            (e.g. io.StringIO) containing the CSV certificate, positioned at
            its start.
        item_color: color palette used among ["SAME", "ID"]
        columns: number of columns in the subplot grid

    Returns:
        plotly.graph_objects.Figure
    """
    if item_color not in ["SAME", "ID"]:
        raise ValueError(f"color palette {item_color} is unknown, please use one of the following: 'SAME', 'ID'")

    bins_x = []
    bins_y = []
    trims_x = []
    trims_y = []
    cuts_x = []
    cuts_y = []
    defects_x = []
    defects_y = []
    items_x = []
    items_y = []
    item_ids_x = []
    item_ids_y = []
    item_ids = []

    with _open(certificate) as csvfile:
        csvreader = csv.DictReader(csvfile, delimiter=',')
        for row in csvreader:
            parent = row["PARENT"]
            i = int(row["PLATE_ID"])
            t = int(row["TYPE"])
            depth = int(row["CUT"])
            w = int(row["WIDTH"])
            h = int(row["HEIGHT"])
            x1 = int(row["X"])
            y1 = int(row["Y"])
            x2 = x1 + w
            y2 = y1 + h

            if t == -4:  # Defect.
                defects_x[i] += [x1, x2, x2, x1, x1, None]
                defects_y[i] += [y1, y1, y2, y2, y1, None]
            elif not parent:  # Bin.
                bins_x.append([])
                bins_y.append([])
                trims_x.append([])
                trims_y.append([])
                cuts_x.append([])
                cuts_y.append([])
                defects_x.append([])
                defects_y.append([])
                items_x.append([])
                items_y.append([])
                item_ids_x.append([])
                item_ids_y.append([])
                item_ids.append([])
                bins_x[i] += [x1, x2, x2, x1, x1, None]
                bins_y[i] += [y1, y1, y2, y2, y1, None]
            elif depth == -1:  # Trims.
                trims_x[i] += [x1, x2, x2, x1, x1, None]
                trims_y[i] += [y1, y1, y2, y2, y1, None]
            elif t >= 0:  # Item.
                k = t
                while len(items_x[i]) <= k:
                    items_x[i].append([])
                    items_y[i].append([])
                items_x[i][k] += [x1, x2, x2, x1, x1, None]
                items_y[i][k] += [y1, y1, y2, y2, y1, None]
                item_ids_x[i].append((x1 + x2) / 2)
                item_ids_y[i].append((y1 + y2) / 2)
                item_ids[i].append(t)
            else:
                while len(cuts_x[i]) <= depth:
                    cuts_x[i].append([])
                    cuts_y[i].append([])
                cuts_x[i][depth] += [x1, x2, x2, x1, x1, None]
                cuts_y[i][depth] += [y1, y1, y2, y2, y1, None]

    m = len(bins_x)
    colors = px.colors.qualitative.Pastel
    number_of_cols = columns if columns is not None else math.ceil(math.sqrt(m))
    number_of_rows = math.ceil(m / number_of_cols)
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

        fig.add_trace(go.Scatter(
            x=trims_x[i],
            y=trims_y[i],
            name="Trims",
            legendgroup="trims",
            showlegend=(i == 0),
            marker=dict(
                color='black',
                size=1)),
            row=row,
            col=col)

        for k in range(len(cuts_x[i])):
            fig.add_trace(go.Scatter(
                x=cuts_x[i][k],
                y=cuts_y[i][k],
                name=str(k) + "-cuts",
                legendgroup=str(k) + "-cuts",
                showlegend=(i == 0),
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
                    row=row,
                    col=col)
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

        fig.add_trace(go.Scatter(
            x=item_ids_x[i],
            y=item_ids_y[i],
            name="Item ids",
            legendgroup="items",
            showlegend=False,
            mode="text",
            text=item_ids[i],
            textfont=dict(size=8),
            textposition="middle center"),
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
