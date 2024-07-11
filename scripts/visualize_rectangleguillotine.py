import argparse
import csv
import plotly.graph_objects as go
import plotly.express as px
import plotly.subplots

parser = argparse.ArgumentParser(description='')
parser.add_argument('csvpath', help='path to CSV file')
args = parser.parse_args()

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

with open(args.csvpath, newline='') as csvfile:
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

        if not parent:  # Bin.
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
            items_x[i] += [x1, x2, x2, x1, x1, None]
            items_y[i] += [y1, y1, y2, y2, y1, None]
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
        x=trims_x[i],
        y=trims_y[i],
        name="Trims",
        legendgroup="trims",
        showlegend=(i == 0),
        marker=dict(
            color='black',
            size=1)),
        row=i + 1,
        col=1)

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
        autosize=True,
        height=m*1000)
fig.update_xaxes(
        rangeslider=dict(visible=False))
fig.update_yaxes(
        scaleanchor="x",
        scaleratio=1)
fig.show()
