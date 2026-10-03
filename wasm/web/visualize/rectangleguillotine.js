// Visualization of rectangleguillotine solutions.
//
// Build a plotly figure from a rectangleguillotine solution certificate (CSV).
// JavaScript port of 'python/packingsolver/visualize/rectangleguillotine.py'.

import {PASTEL_COLORS, makeSubplots, parseCsv} from "./common.js";

// Build a plotly figure of a rectangleguillotine solution.
//
// Args:
//     text: content of the CSV certificate
//     options.item_color: color palette used among ["SAME", "ID"]
//     options.columns: number of columns in the subplot grid
//
// Returns:
//     {data, layout} for 'Plotly.newPlot'
export function figure(text, {item_color = "ID", columns = null} = {}) {
    if (!["SAME", "ID"].includes(item_color))
        throw new Error(`color palette ${item_color} is unknown, please use one of the following: 'SAME', 'ID'`);

    const binsX = [];
    const binsY = [];
    const trimsX = [];
    const trimsY = [];
    const cutsX = [];
    const cutsY = [];
    const defectsX = [];
    const defectsY = [];
    const itemsX = [];
    const itemsY = [];
    const itemIdsX = [];
    const itemIdsY = [];
    const itemIds = [];

    for (const row of parseCsv(text)) {
        const parent = row["PARENT"];
        const i = parseInt(row["PLATE_ID"], 10);
        const t = parseInt(row["TYPE"], 10);
        const depth = parseInt(row["CUT"], 10);
        const w = parseInt(row["WIDTH"], 10);
        const h = parseInt(row["HEIGHT"], 10);
        const x1 = parseInt(row["X"], 10);
        const y1 = parseInt(row["Y"], 10);
        const x2 = x1 + w;
        const y2 = y1 + h;

        if (t === -4) {  // Defect.
            defectsX[i].push(x1, x2, x2, x1, x1, null);
            defectsY[i].push(y1, y1, y2, y2, y1, null);
        } else if (!parent) {  // Bin.
            for (const list of [
                    binsX, binsY, trimsX, trimsY, cutsX, cutsY, defectsX,
                    defectsY, itemsX, itemsY, itemIdsX, itemIdsY, itemIds])
                list.push([]);
            binsX[i].push(x1, x2, x2, x1, x1, null);
            binsY[i].push(y1, y1, y2, y2, y1, null);
        } else if (depth === -1) {  // Trims.
            trimsX[i].push(x1, x2, x2, x1, x1, null);
            trimsY[i].push(y1, y1, y2, y2, y1, null);
        } else if (t >= 0) {  // Item.
            const k = t;
            while (itemsX[i].length <= k) {
                itemsX[i].push([]);
                itemsY[i].push([]);
            }
            itemsX[i][k].push(x1, x2, x2, x1, x1, null);
            itemsY[i][k].push(y1, y1, y2, y2, y1, null);
            itemIdsX[i].push((x1 + x2) / 2);
            itemIdsY[i].push((y1 + y2) / 2);
            itemIds[i].push(t);
        } else {
            while (cutsX[i].length <= depth) {
                cutsX[i].push([]);
                cutsY[i].push([]);
            }
            cutsX[i][depth].push(x1, x2, x2, x1, x1, null);
            cutsY[i][depth].push(y1, y1, y2, y2, y1, null);
        }
    }

    const m = binsX.length;
    const colors = PASTEL_COLORS;
    const numberOfCols = (columns !== null && columns !== undefined)?
        columns: Math.ceil(Math.sqrt(m));
    const numberOfRows = Math.ceil(m / numberOfCols);
    const subplots = makeSubplots({
        rows: numberOfRows,
        cols: numberOfCols,
        verticalSpacing: 0.001,
    });
    const data = [];
    const layout = subplots.layout;
    const marker = () => ({color: "black", size: 1});

    for (let i = 0; i < m; ++i) {
        const row = Math.floor(i / numberOfCols) + 1;
        const col = (i % numberOfCols) + 1;
        const cell = subplots.subplot(row, col);

        data.push({
            type: "scatter",
            x: binsX[i],
            y: binsY[i],
            name: "Bins",
            legendgroup: "bins",
            showlegend: i === 0,
            marker: marker(),
            ...cell,
        });

        data.push({
            type: "scatter",
            x: defectsX[i],
            y: defectsY[i],
            name: "Defects",
            legendgroup: "defects",
            showlegend: i === 0,
            fillcolor: "crimson",
            fill: "toself",
            marker: marker(),
            ...cell,
        });

        data.push({
            type: "scatter",
            x: trimsX[i],
            y: trimsY[i],
            name: "Trims",
            legendgroup: "trims",
            showlegend: i === 0,
            marker: marker(),
            ...cell,
        });

        for (let k = 0; k < cutsX[i].length; ++k) {
            data.push({
                type: "scatter",
                x: cutsX[i][k],
                y: cutsY[i][k],
                name: String(k) + "-cuts",
                legendgroup: String(k) + "-cuts",
                showlegend: i === 0,
                marker: marker(),
                ...cell,
            });
        }

        for (let k = 0; k < itemsX[i].length; ++k) {
            if (item_color === "SAME") {
                data.push({
                    type: "scatter",
                    x: itemsX[i][k],
                    y: itemsY[i][k],
                    name: "Items",
                    legendgroup: "items",
                    showlegend: i === 0 && k === 0,
                    fillcolor: "cornflowerblue",
                    fill: "toself",
                    marker: marker(),
                    ...cell,
                });
            } else if (item_color === "ID") {
                data.push({
                    type: "scatter",
                    x: itemsX[i][k],
                    y: itemsY[i][k],
                    name: `Items ${k}`,
                    legendgroup: "item",
                    showlegend: i === 0,
                    fillcolor: colors[k % colors.length],
                    fill: "toself",
                    marker: marker(),
                    ...cell,
                });
            }
        }

        data.push({
            type: "scatter",
            x: itemIdsX[i],
            y: itemIdsY[i],
            name: "Item ids",
            legendgroup: "items",
            showlegend: false,
            mode: "text",
            // plotly.py validates 'text' as strings.
            text: itemIds[i].map(String),
            textfont: {size: 8},
            textposition: "middle center",
            ...cell,
        });
    }

    // Plot.
    layout.autosize = true;
    layout.font = {size: 14};
    layout.legend = {font: {size: 14}, itemsizing: "constant"};
    // 'update_xaxes': every x axis of the grid.
    for (const key of Object.keys(layout)) {
        if (/^xaxis\d*$/.test(key))
            layout[key].rangeslider = {visible: false};
    }
    for (let i = 0; i < m; ++i) {
        const row = Math.floor(i / numberOfCols) + 1;
        const col = (i % numberOfCols) + 1;
        const yaxis = "yaxis" + subplots.subplot(row, col).yaxis.slice(1);
        layout[yaxis].scaleanchor = (i === 0)? "x": `x${i + 1}`;
        layout[yaxis].scaleratio = 1;
    }

    return {data, layout};
}
