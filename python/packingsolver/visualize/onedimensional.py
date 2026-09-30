"""Build a plotly figure of a onedimensional solution certificate (CSV)."""

import contextlib
import csv
import os
import plotly.graph_objects as go
import plotly.express as px
import plotly.subplots


def _open(certificate):
    if isinstance(certificate, (str, os.PathLike)):
        return open(certificate, newline='')
    return contextlib.nullcontext(certificate)


def figure(
        certificate,
        *,
        item_color="ID",
        expand_copies=False):
    """Build the figure of a onedimensional solution certificate.

    certificate: path to CSV file, or text stream of its content
    item_color: color palette used among ["SAME", "ID"]
    expand_copies: draw one subplot per bin copy instead of one per bin type
    """
    if item_color not in ["SAME", "ID"]:
        raise ValueError(f"color palette {item_color} is unknown, please use one of the following: 'SAME', 'ID'")

    bins_x = []
    bins_y = []
    items_x = []
    items_y = []
    item_ids_x = []
    item_ids_y = []
    item_ids = []
    bin_copies = []

    with _open(certificate) as csvfile:
        csvreader = csv.DictReader(csvfile, delimiter=',')
        for row in csvreader:
            i = int(row["BIN"])
            type_ = row["TYPE"]
            id_ = int(row["ID"])
            x1 = int(row["X"])
            y1 = 0
            w = int(row["LX"])
            h = 1
            x2 = x1 + w
            y2 = y1 + h

            if type_ == "BIN":
                bin_copies.append(int(row["COPIES"]) if "COPIES" in row else 1)
                bins_x.append([])
                bins_y.append([])
                items_x.append([])
                items_y.append([])
                item_ids_x.append([])
                item_ids_y.append([])
                item_ids.append([])
                bins_x[i] += [x1, x2, x2, x1, x1, None]
                bins_y[i] += [y1, y1, y2, y2, y1, None]
            elif type_ == "ITEM":  # Item.
                k = id_
                while len(items_x[i]) <= k:
                    items_x[i].append([])
                    items_y[i].append([])
                items_x[i][k] += [x1, x2, x2, x1, x1, None]
                items_y[i][k] += [y1, y1, y2, y2, y1, None]
                item_ids_x[i].append((x1 + x2) / 2)
                item_ids_y[i].append((y1 + y2) / 2)
                item_ids[i].append(id_)

    if expand_copies:
        def expand(lst):
            out = []
            for v, c in zip(lst, bin_copies):
                out += [v] * c
            return out
        bins_x = expand(bins_x)
        bins_y = expand(bins_y)
        items_x = expand(items_x)
        items_y = expand(items_y)
        item_ids_x = expand(item_ids_x)
        item_ids_y = expand(item_ids_y)
        item_ids = expand(item_ids)

    m = len(bins_x)
    colors = px.colors.qualitative.Pastel
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
                    row=i + 1,
                    col=1)

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
            row=i + 1,
            col=1)

    # Plot.
    fig.update_layout(
            autosize=True)
    fig.update_xaxes(
            rangeslider=dict(visible=False))
    fig.update_yaxes(visible=False)
    return fig
