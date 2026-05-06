import argparse
import csv
import plotly.graph_objects as go
import plotly.express as px
import plotly.subplots
import math

parser = argparse.ArgumentParser(description='')
parser.add_argument('csvpath', help='path to CSV file')
parser.add_argument('itemcolor', nargs='?', default='ID', help='color palette used among ["SAME", "ID", "GROUP_ID", "DENSITY"]')
parser.add_argument('-o', '--output', help='save image to file instead of opening browser (e.g. output.png)')
parser.add_argument('--width', type=int, default=None, help='image width in pixels for PNG export')
parser.add_argument('--height', type=int, default=None, help='image height in pixels for PNG export')
args = parser.parse_args()

if args.itemcolor not in ["SAME", "ID", "GROUP_ID", "DENSITY"]:
    raise ValueError(f"color palette {args.itemcolor} is unknown, please use one of the following : 'SAME', 'ID', 'GROUP_ID', 'DENSITY'")

bins_x = []
bins_y = []
bins_weights = []
bins_gravity_centers_x = []
bins_gravity_centers_y = []
defects_x = []
defects_y = []
items_x = []
items_y = []
item_ids_x = []
item_ids_y = []
item_ids = []
items_density = []
items_weights = []
items_left = []
items_right = []
items_bottom = []
items_top = []

with open(args.csvpath, newline='') as csvfile:
    csvreader = csv.DictReader(csvfile, delimiter=',')
    for row in csvreader:
        i = int(row["BIN"])
        type_ = row["TYPE"]
        id_ = int(row["ID"])
        w = int(row["LX"])
        h = int(row["LY"])
        x1 = int(row["X"])
        y1 = int(row["Y"])
        weight = int(row.get("WEIGHT", 0))
        x2 = x1 + w
        y2 = y1 + h

        if type_ == "BIN":
            bins_x.append([])
            bins_y.append([])
            bins_gravity_centers_x.append([])
            bins_gravity_centers_y.append([])
            defects_x.append([])
            defects_y.append([])
            items_x.append([])
            items_y.append([])
            item_ids_x.append([])
            item_ids_y.append([])
            item_ids.append([])
            items_density.append([])
            items_weights.append([])
            items_left.append([])
            items_right.append([])
            items_bottom.append([])
            items_top.append([])
            bins_x[i] += [x1, x2, x2, x1, x1, None]
            bins_y[i] += [y1, y1, y2, y2, y1, None]
            bins_weights.append(weight)
            bins_gravity_centers_x[i] = [0, 1, 1, 0, 0, None]
            bins_gravity_centers_y[i] = [0, 1, 1, 0, 0, None]
        elif type_ == "DEFECT":  # Defect.
            defects_x[i] += [x1, x2, x2, x1, x1, None]
            defects_y[i] += [y1, y1, y2, y2, y1, None]
        elif type_ == "ITEM":  # Item.git
            if args.itemcolor == "SAME":
                k = 0
            elif args.itemcolor == "GROUP_ID" and "GROUP_ID" in row.keys():
                k = int(row["GROUP_ID"])
            else:
                k = int(row["ID"])
            while len(items_x[i]) <= k:
                items_x[i].append([])
                items_y[i].append([])
                items_density[i].append(0)
            items_weights[i].append(weight)
            items_left[i].append(x1)
            items_right[i].append(x2)
            items_bottom[i].append(y1)
            items_top[i].append(y2)

            items_x[i][k] += [x1, x2, x2, x1, x1, None]
            items_y[i][k] += [y1, y1, y2, y2, y1, None]
            items_density[i][k] = weight / (h * w) if h * w > 0 else 0
            item_ids_x[i].append((x1 + x2) / 2)
            item_ids_y[i].append((y1 + y2) / 2)
            item_ids[i].append(id_)
            if bins_weights[i] > 0:
                for j in {0, 1, 2, 3, 4}:
                    bins_gravity_centers_x[i][j] += (x1 + w/2) * weight / bins_weights[i]
                    bins_gravity_centers_y[i][j] += (y1 + h/2) * weight / bins_weights[i]

m = len(bins_x)
for i in range(0, m):
    for j in {1, 2}:
        bins_gravity_centers_x[i][j] += 8
    for j in {2, 3}:
        bins_gravity_centers_y[i][j] += 8

bins_gravity_repartition_x = []
bins_gravity_repartition_y = []
for i in range(0, m):
    bins_gravity_repartition_x.append(0)
    bins_gravity_repartition_y.append(0)
    for j in range(0, len(items_weights[i])):
        if items_right[i][j] <= bins_gravity_centers_x[i][0]:
            bins_gravity_repartition_x[i] += items_weights[i][j]
        elif items_left[i][j] < bins_gravity_centers_x[i][0]:
            rate = (bins_gravity_centers_x[i][0] - items_left[i][j]) / (items_right[i][j] - items_left[i][j])
            bins_gravity_repartition_x[i] += items_weights[i][j] * rate

        if items_top[i][j] <= bins_gravity_centers_y[i][0]:
            bins_gravity_repartition_y[i] += items_weights[i][j]
        elif items_bottom[i][j] < bins_gravity_centers_y[i][0]:
            rate = (bins_gravity_centers_y[i][0] - items_bottom[i][j]) / (items_top[i][j] - items_bottom[i][j])
            bins_gravity_repartition_y[i] += items_weights[i][j] * rate

    total_weight = sum(items_weights[i])
    if total_weight > 0:
        bins_gravity_repartition_x[i] = bins_gravity_repartition_x[i] / total_weight * 100.0
        bins_gravity_repartition_y[i] = bins_gravity_repartition_y[i] / total_weight * 100.0

# colors = px.colors.qualitative.Plotly
colors = px.colors.qualitative.Pastel
number_of_rows = math.ceil(math.sqrt(m))
fig = plotly.subplots.make_subplots(
        rows=number_of_rows,
        cols=number_of_rows,
        shared_xaxes=True,
        vertical_spacing=0.001)

for i in range(0, m):
    row = (i // number_of_rows) + 1
    col = (i % number_of_rows) + 1

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

    for k in range(0, len(items_x[i])):
        if args.itemcolor == 'SAME':
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

        elif args.itemcolor == 'ID':
            fig.add_trace(go.Scatter(
                x=items_x[i][k],
                y=items_y[i][k],
                name=f"Items {k}",
                legendgroup=f"item",
                showlegend=i==0,
                fillcolor=colors[(k % len(colors))],
                fill="toself",
                marker=dict(
                    color='black',
                    size=1)),
                row=row,
                col=col)

        elif args.itemcolor == 'GROUP_ID':
            fig.add_trace(go.Scatter(
                x=items_x[i][k],
                y=items_y[i][k],
                name=f"Group {k}",
                legendgroup="items",
                showlegend=True,
                fillcolor=colors[(k % len(colors))],
                fill="toself",
                marker=dict(
                    color='black',
                    size=1)),
                row=row,
                col=col)

        elif args.itemcolor == 'DENSITY':
            if min(items_density[i]) != max(items_density[i]):
                c = (items_density[i][k] - min(items_density[i])) / (max(items_density[i]) - min(items_density[i]))
            else:
                c = 0.5
            fig.add_trace(go.Scatter(
                x=items_x[i][k],
                y=items_y[i][k],
                name=f"Density {round(c*100.0, 1)}%",
                legendgroup="items",
                showlegend=False,
                fillcolor="hsl(218, 100,"+str(90 - 30 * c)+")",
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

    if args.itemcolor == 'DENSITY':
        fig.add_trace(go.Scatter(
            text="x:" + str(round(bins_gravity_repartition_x[i], 2)) + "% / " + str(100 - round(bins_gravity_repartition_x[i], 2)) +"%",
            x=bins_gravity_centers_x[i],
            y=bins_y[i],
            fill="toself",
            name="Gravity",
            legendgroup="gravity x",
            showlegend=(i == 0),
            marker=dict(
                color='red',
                size=1)),
            row=row,
            col=col)
        fig.add_trace(go.Scatter(
            text="y:" + str(round(bins_gravity_repartition_y[i], 2)) + "% / " + str(100 - round(bins_gravity_repartition_y[i], 2)) +"%",
            x=bins_x[i],
            y=bins_gravity_centers_y[i],
            fill="toself",
            name="Gravity",
            legendgroup="gravity y",
            showlegend=(i == 0),
            marker=dict(
                color='red',
                size=1)),
            row=row,
            col=col)


# Plot.
fig.update_layout(
        autosize=True)
fig.update_xaxes(
        rangeslider=dict(visible=False))
fig.update_yaxes(
        scaleanchor="x",
        scaleratio=1)
if args.output:
    fig.write_image(args.output, width=args.width, height=args.height)
else:
    fig.show()
