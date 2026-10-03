// Build a plotly figure of a onedimensional solution certificate (CSV).
// JavaScript port of 'python/packingsolver/visualize/onedimensional.py'.

import {PASTEL_COLORS, makeSubplots, parseCsv} from "./common.js";

// Build the figure of a onedimensional solution certificate.
//
// text: content of the CSV file
// options.item_color: color palette used among ["SAME", "ID"]
// options.expand_copies: draw one subplot per bin copy instead of one per bin
// type
export function figure(text, options = {}) {
    const {
        item_color: itemColor = "ID",
        expand_copies: expandCopies = false,
    } = options;
    if (!["SAME", "ID"].includes(itemColor))
        throw new Error(`color palette ${itemColor} is unknown, please use one of the following: 'SAME', 'ID'`);

    let binsX = [];
    let binsY = [];
    let itemsX = [];
    let itemsY = [];
    let itemIdsX = [];
    let itemIdsY = [];
    let itemIds = [];
    const binCopies = [];

    for (const row of parseCsv(text)) {
        const i = parseInt(row["BIN"], 10);
        const type = row["TYPE"];
        const id = parseInt(row["ID"], 10);
        const x1 = parseInt(row["X"], 10);
        const y1 = 0;
        const w = parseInt(row["LX"], 10);
        const h = 1;
        const x2 = x1 + w;
        const y2 = y1 + h;

        if (type === "BIN") {
            binCopies.push(("COPIES" in row)? parseInt(row["COPIES"], 10): 1);
            binsX.push([]);
            binsY.push([]);
            itemsX.push([]);
            itemsY.push([]);
            itemIdsX.push([]);
            itemIdsY.push([]);
            itemIds.push([]);
            binsX[i].push(x1, x2, x2, x1, x1, null);
            binsY[i].push(y1, y1, y2, y2, y1, null);
        } else if (type === "ITEM") {  // Item.
            const k = id;
            while (itemsX[i].length <= k) {
                itemsX[i].push([]);
                itemsY[i].push([]);
            }
            itemsX[i][k].push(x1, x2, x2, x1, x1, null);
            itemsY[i][k].push(y1, y1, y2, y2, y1, null);
            itemIdsX[i].push((x1 + x2) / 2);
            itemIdsY[i].push((y1 + y2) / 2);
            itemIds[i].push(id);
        }
    }

    if (expandCopies) {
        const expand = (lst) => {
            const out = [];
            lst.forEach((v, j) => {
                if (j >= binCopies.length)
                    return;
                for (let c = 0; c < binCopies[j]; ++c)
                    out.push(v);
            });
            return out;
        };
        binsX = expand(binsX);
        binsY = expand(binsY);
        itemsX = expand(itemsX);
        itemsY = expand(itemsY);
        itemIdsX = expand(itemIdsX);
        itemIdsY = expand(itemIdsY);
        itemIds = expand(itemIds);
    }

    const m = binsX.length;
    const colors = PASTEL_COLORS;
    const subplots = makeSubplots({rows: m, cols: 1, verticalSpacing: 0.001});
    const data = [];

    for (let i = 0; i < m; ++i) {
        const subplot = subplots.subplot(i + 1, 1);

        data.push({
            type: "scatter",
            x: [...binsX[i]],
            y: [...binsY[i]],
            name: "Bins",
            legendgroup: "bins",
            showlegend: (i === 0),
            marker: {color: "black", size: 1},
            ...subplot,
        });

        for (let k = 0; k < itemsX[i].length; ++k) {
            if (itemColor === "SAME") {
                data.push({
                    type: "scatter",
                    x: [...itemsX[i][k]],
                    y: [...itemsY[i][k]],
                    name: "Items",
                    legendgroup: "items",
                    showlegend: (i === 0 && k === 0),
                    fillcolor: "cornflowerblue",
                    fill: "toself",
                    marker: {color: "black", size: 1},
                    ...subplot,
                });
            } else if (itemColor === "ID") {
                data.push({
                    type: "scatter",
                    x: [...itemsX[i][k]],
                    y: [...itemsY[i][k]],
                    name: `Items ${k}`,
                    legendgroup: "item",
                    showlegend: i === 0,
                    fillcolor: colors[k % colors.length],
                    fill: "toself",
                    marker: {color: "black", size: 1},
                    ...subplot,
                });
            }
        }

        data.push({
            type: "scatter",
            x: [...itemIdsX[i]],
            y: [...itemIdsY[i]],
            name: "Item ids",
            legendgroup: "items",
            showlegend: false,
            mode: "text",
            // plotly.py coerces the numbers of "text" to strings.
            text: itemIds[i].map(String),
            textfont: {size: 8},
            textposition: "middle center",
            ...subplot,
        });
    }

    // Plot.
    const layout = subplots.layout;
    layout.autosize = true;
    for (const key of Object.keys(layout)) {
        if (key.startsWith("xaxis"))
            layout[key].rangeslider = {visible: false};
        else if (key.startsWith("yaxis"))
            layout[key].visible = false;
    }
    return {data, layout};
}
