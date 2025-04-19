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
bins_z = []
bins_i = []
bins_j = []
bins_k = []
defects_x = []
defects_y = []
defects_z = []
defects_i = []
defects_j = []
defects_k = []
items_x = []
items_y = []
items_z = []
items_i = []
items_j = []
items_k = []
item_borders_x = []
item_borders_y = []
item_borders_z = []
items_colors = []
item_ids_x = []
item_ids_y = []
item_ids_z = []
item_ids = []

with open(args.csvpath, newline='') as csvfile:
    csvreader = csv.DictReader(csvfile, delimiter=',')
    for row in csvreader:
        i = int(row["BIN"])
        type_ = row["TYPE"]
        id_ = row["ID"]
        lx = int(row["LX"])
        ly = int(row["LY"])
        lz = int(row["LZ"])
        x1 = int(row["X"])
        y1 = int(row["Y"])
        z1 = int(row["Z"])
        x2 = x1 + lx
        y2 = y1 + ly
        z2 = z1 + lz

        if type_ == "BIN":
            bins_x.append([])
            bins_y.append([])
            bins_z.append([])
            bins_i.append([])
            bins_j.append([])
            bins_k.append([])
            defects_x.append([])
            defects_y.append([])
            defects_z.append([])
            defects_i.append([])
            defects_j.append([])
            defects_k.append([])
            items_x.append([])
            items_y.append([])
            items_z.append([])
            items_i.append([])
            items_j.append([])
            items_k.append([])
            items_colors.append([])
            item_borders_x.append([])
            item_borders_y.append([])
            item_borders_z.append([])
            item_ids_x.append([])
            item_ids_y.append([])
            item_ids_z.append([])
            item_ids.append([])

            a = len(bins_x[i])
            bins_x[i] += [x1, x1, x2, x2, x1, x1, x2, x2]
            bins_y[i] += [y1, y2, y2, y1, y1, y2, y2, y1]
            bins_z[i] += [z1, z1, z1, z1, z2, z2, z2, z2]
            bins_i[i] += [a + 7, a + 0, a + 0, a + 0, a + 4, a + 4,
                          a + 6, a + 6, a + 4, a + 0, a + 3, a + 2]
            bins_j[i] += [a + 3, a + 4, a + 1, a + 2, a + 5, a + 6,
                          a + 5, a + 2, a + 0, a + 1, a + 6, a + 3]
            bins_k[i] += [a + 0, a + 7, a + 2, a + 3, a + 6, a + 7,
                          a + 1, a + 1, a + 5, a + 5, a + 7, a + 6]
        elif type_ == "DEFECT":  # Defect.
            a = len(defects_x[i])
            defects_x[i] += [x1, x1, x2, x2, x1, x1, x2, x2]
            defects_y[i] += [y1, y2, y2, y1, y1, y2, y2, y1]
            defects_z[i] += [z1, z1, z1, z1, z2, z2, z2, z2]
            defects_i[i] += [a + 7, a + 0, a + 0, a + 0, a + 4, a + 4,
                             a + 6, a + 6, a + 4, a + 0, a + 3, a + 2]
            defects_j[i] += [a + 3, a + 4, a + 1, a + 2, a + 5, a + 6,
                             a + 5, a + 2, a + 0, a + 1, a + 6, a + 3]
            defects_k[i] += [a + 0, a + 7, a + 2, a + 3, a + 6, a + 7,
                             a + 1, a + 1, a + 5, a + 5, a + 7, a + 6]
        elif type_ == "ITEM":
            a = 0
            k = len(items_x[i])
            items_x[i].append([])
            items_y[i].append([])
            items_z[i].append([])
            items_i[i].append([])
            items_j[i].append([])
            items_k[i].append([])
            items_x[i][k] = [x1, x2, x1, x2, x1, x2, x1, x2]
            items_y[i][k] = [y1, y1, y2, y2, y1, y1, y2, y2]
            items_z[i][k] = [z1, z1, z1, z1, z2, z2, z2, z2]
            items_i[i][k] = [a + 0, a + 3, a + 4, a + 7, a + 0, a + 5, a + 2, a + 7, a + 0, a + 6, a + 1, a + 7]
            items_j[i][k] = [a + 1, a + 1, a + 5, a + 5, a + 1, a + 1, a + 3, a + 3, a + 2, a + 2, a + 3, a + 3]
            items_k[i][k] = [a + 2, a + 2, a + 6, a + 6, a + 4, a + 4, a + 6, a + 6, a + 4, a + 4, a + 5, a + 5]
            item_borders_x[i] += [x1, x1, x2, x2, x2, x2, x1, x1, x1, None, x1, x1, x2, x2, x2, x2, x1, x1, x1, None]
            item_borders_y[i] += [y1, y1, y1, y1, y2, y2, y2, y2, y1, None, y1, y1, y1, y1, y2, y2, y2, y2, y1, None]
            item_borders_z[i] += [z1, z2, z2, z1, z1, z2, z2, z1, z1, None, z2, z1, z1, z2, z2, z1, z1, z2, z2, None]
            item_ids_x[i].append((x1 + x2) / 2)
            item_ids_y[i].append((y1 + y2) / 2)
            item_ids_z[i].append((z1 + z2) / 2)
            item_ids[i].append(id_)

m = len(bins_x)
colors = px.colors.qualitative.Plotly
fig = plotly.subplots.make_subplots(
        rows=m,
        cols=1,
        shared_xaxes=True,
        specs=[[{'type': 'mesh3d'}] for _ in range(m)],
        vertical_spacing=0.001)

for i in range(0, m):

    fig.add_trace(go.Mesh3d(
        x=bins_x[i],
        y=bins_y[i],
        z=bins_z[i],
        i=bins_i[i],
        j=bins_j[i],
        k=bins_k[i],
        name="Bins",
        legendgroup="bins",
        showlegend=(i == 0),
        opacity=0.1,
        color="grey",
        flatshading=True),
        row=i + 1,
        col=1)

    fig.add_trace(go.Mesh3d(
        x=defects_x[i],
        y=defects_y[i],
        z=defects_z[i],
        i=defects_i[i],
        j=defects_j[i],
        k=defects_k[i],
        name="Defects",
        legendgroup="defects",
        showlegend=(i == 0),
        opacity=0.2,
        color="grey",
        flatshading=True),
        row=i + 1,
        col=1)

    for k in range(len(items_x[i])):
        fig.add_trace(go.Mesh3d(
            x=items_x[i][k],
            y=items_y[i][k],
            z=items_z[i][k],
            i=items_i[i][k],
            j=items_j[i][k],
            k=items_k[i][k],
            name="Items",
            legendgroup="items",
            showlegend=(i == 0 and k == 0),
            opacity=1,
            flatshading=True,
            color=colors[(int(item_ids[i][k]) % len(colors))]),
            row=i + 1,
            col=1)

    fig.add_trace(go.Scatter3d(
        x=item_borders_x[i],
        y=item_borders_y[i],
        z=item_borders_z[i],
        name="Item borders",
        legendgroup="items",
        showlegend=False,
        mode="lines",
        line=dict(color="black", width=10)),
        row=i + 1,
        col=1)

    fig.add_trace(go.Scatter3d(
        x=item_ids_x[i],
        y=item_ids_y[i],
        z=item_ids_z[i],
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
fig.update_scenes(aspectmode='data')
fig.show()
