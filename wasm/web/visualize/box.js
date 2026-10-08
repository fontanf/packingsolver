// Visualization of box solution certificates.
//
// Build a plotly figure from a solution certificate (CSV) of the box problem
// type. JavaScript port of 'python/packingsolver/visualize/box.py'.

import {PASTEL_COLORS, makeSubplots, parseCsv} from "./common.js";

// Build the figure of a box solution certificate.
//
// Args:
//     text: content of the CSV certificate file
//     options:
//         item_color: color palette used among ["SAME", "ID"]
//         columns: number of columns in the subplot grid
//         zoom: camera zoom factor (higher: closer/bigger)
//         legend: show the legend; if false, hide the legend, freeing up
//             space for the plot itself
//
// Returns:
//     {data, layout} for 'Plotly.newPlot'
export function figure(text, options = {}) {
    const {
        item_color = "ID",
        columns = null,
        zoom = 1.0,
        legend = true,
    } = options;
    if (!["SAME", "ID"].includes(item_color))
        throw new Error(`color palette ${item_color} is unknown, please use one of the following: 'SAME', 'ID'`);

    const binsX = [], binsY = [], binsZ = [];
    const binsI = [], binsJ = [], binsK = [];
    const defectsX = [], defectsY = [], defectsZ = [];
    const defectsI = [], defectsJ = [], defectsK = [];
    const itemsX = [], itemsY = [], itemsZ = [];
    const itemsI = [], itemsJ = [], itemsK = [];
    const itemBordersX = [], itemBordersY = [], itemBordersZ = [];
    const itemIdsX = [], itemIdsY = [], itemIdsZ = [];
    const itemIds = [];
    let maxBinLx = 0;
    let maxBinLy = 0;
    let maxBinLz = 0;

    for (const row of parseCsv(text)) {
        const i = parseInt(row["BIN"], 10);
        const type = row["TYPE"];
        const id = row["ID"];
        const lx = parseInt(row["LX"], 10);
        const ly = parseInt(row["LY"], 10);
        const lz = parseInt(row["LZ"], 10);
        const x1 = parseInt(row["X"], 10);
        const y1 = parseInt(row["Y"], 10);
        const z1 = parseInt(row["Z"], 10);
        const x2 = x1 + lx;
        const y2 = y1 + ly;
        const z2 = z1 + lz;

        if (type === "BIN") {
            maxBinLx = Math.max(maxBinLx, lx);
            maxBinLy = Math.max(maxBinLy, ly);
            maxBinLz = Math.max(maxBinLz, lz);
            for (const list of [
                binsX, binsY, binsZ, binsI, binsJ, binsK,
                defectsX, defectsY, defectsZ, defectsI, defectsJ, defectsK,
                itemsX, itemsY, itemsZ, itemsI, itemsJ, itemsK,
                itemBordersX, itemBordersY, itemBordersZ,
                itemIdsX, itemIdsY, itemIdsZ, itemIds])
                list.push([]);

            const a = binsX[i].length;
            binsX[i].push(x1, x1, x2, x2, x1, x1, x2, x2);
            binsY[i].push(y1, y2, y2, y1, y1, y2, y2, y1);
            binsZ[i].push(z1, z1, z1, z1, z2, z2, z2, z2);
            binsI[i].push(a + 7, a + 0, a + 0, a + 0, a + 4, a + 4,
                          a + 6, a + 6, a + 4, a + 0, a + 3, a + 2);
            binsJ[i].push(a + 3, a + 4, a + 1, a + 2, a + 5, a + 6,
                          a + 5, a + 2, a + 0, a + 1, a + 6, a + 3);
            binsK[i].push(a + 0, a + 7, a + 2, a + 3, a + 6, a + 7,
                          a + 1, a + 1, a + 5, a + 5, a + 7, a + 6);
        } else if (type === "DEFECT") {  // Defect.
            const a = defectsX[i].length;
            defectsX[i].push(x1, x1, x2, x2, x1, x1, x2, x2);
            defectsY[i].push(y1, y2, y2, y1, y1, y2, y2, y1);
            defectsZ[i].push(z1, z1, z1, z1, z2, z2, z2, z2);
            defectsI[i].push(a + 7, a + 0, a + 0, a + 0, a + 4, a + 4,
                             a + 6, a + 6, a + 4, a + 0, a + 3, a + 2);
            defectsJ[i].push(a + 3, a + 4, a + 1, a + 2, a + 5, a + 6,
                             a + 5, a + 2, a + 0, a + 1, a + 6, a + 3);
            defectsK[i].push(a + 0, a + 7, a + 2, a + 3, a + 6, a + 7,
                             a + 1, a + 1, a + 5, a + 5, a + 7, a + 6);
        } else if (type === "ITEM") {
            const a = 0;
            const eps = 0.1;
            const mx1 = x1 + eps, mx2 = x2 - eps;
            const my1 = y1 + eps, my2 = y2 - eps;
            const mz1 = z1 + eps, mz2 = z2 - eps;
            itemsX[i].push([mx1, mx2, mx1, mx2, mx1, mx2, mx1, mx2]);
            itemsY[i].push([my1, my1, my2, my2, my1, my1, my2, my2]);
            itemsZ[i].push([mz1, mz1, mz1, mz1, mz2, mz2, mz2, mz2]);
            itemsI[i].push([a + 0, a + 3, a + 4, a + 7, a + 0, a + 5, a + 2, a + 7, a + 0, a + 6, a + 1, a + 7]);
            itemsJ[i].push([a + 1, a + 1, a + 5, a + 5, a + 1, a + 1, a + 3, a + 3, a + 2, a + 2, a + 3, a + 3]);
            itemsK[i].push([a + 2, a + 2, a + 6, a + 6, a + 4, a + 4, a + 6, a + 6, a + 4, a + 4, a + 5, a + 5]);
            itemBordersX[i].push(x1, x1, x2, x2, x2, x2, x1, x1, x1, null, x1, x1, x2, x2, x2, x2, x1, x1, x1, null);
            itemBordersY[i].push(y1, y1, y1, y1, y2, y2, y2, y2, y1, null, y1, y1, y1, y1, y2, y2, y2, y2, y1, null);
            itemBordersZ[i].push(z1, z2, z2, z1, z1, z2, z2, z1, z1, null, z2, z1, z1, z2, z2, z1, z1, z2, z2, null);
            itemIdsX[i].push((x1 + x2) / 2);
            itemIdsY[i].push((y1 + y2) / 2);
            itemIdsZ[i].push((z1 + z2) / 2);
            itemIds[i].push(id);
        }
    }

    const m = binsX.length;
    const colors = PASTEL_COLORS;
    const numberOfCols = (columns !== null && columns !== undefined)? columns: Math.ceil(Math.sqrt(m));
    const numberOfRows = Math.ceil(m / numberOfCols);
    const subplots = makeSubplots({
        rows: numberOfRows,
        cols: numberOfCols,
        verticalSpacing: 0.001,
        type: "scene",
    });
    const data = [];
    const layout = subplots.layout;

    for (let i = 0; i < m; ++i) {
        const row = Math.floor(i / numberOfCols) + 1;
        const col = (i % numberOfCols) + 1;
        const cell = subplots.subplot(row, col);

        data.push({
            color: "grey",
            flatshading: true,
            i: binsI[i],
            j: binsJ[i],
            k: binsK[i],
            legendgroup: "bins",
            name: "Bins",
            opacity: 0.1,
            showlegend: (i === 0),
            x: binsX[i],
            y: binsY[i],
            z: binsZ[i],
            type: "mesh3d",
            ...cell,
        });

        data.push({
            color: "grey",
            flatshading: true,
            i: defectsI[i],
            j: defectsJ[i],
            k: defectsK[i],
            legendgroup: "defects",
            name: "Defects",
            opacity: 0.2,
            showlegend: (i === 0),
            x: defectsX[i],
            y: defectsY[i],
            z: defectsZ[i],
            type: "mesh3d",
            ...cell,
        });

        for (let k = 0; k < itemsX[i].length; ++k) {
            let color;
            if (item_color === "SAME") {
                color = "cornflowerblue";
            } else {
                // Python's '%' is non-negative for a positive divisor.
                const n = colors.length;
                color = colors[((parseInt(itemIds[i][k], 10) % n) + n) % n];
            }
            data.push({
                color: color,
                flatshading: true,
                i: itemsI[i][k],
                j: itemsJ[i][k],
                k: itemsK[i][k],
                legendgroup: "items",
                name: (item_color === "SAME")? "Items": `Items ${itemIds[i][k]}`,
                opacity: 1,
                showlegend: (i === 0 && k === 0),
                x: itemsX[i][k],
                y: itemsY[i][k],
                z: itemsZ[i][k],
                type: "mesh3d",
                ...cell,
            });
        }

        data.push({
            legendgroup: "items",
            line: {color: "black", width: 1},
            mode: "lines",
            name: "Item borders",
            showlegend: false,
            x: itemBordersX[i],
            y: itemBordersY[i],
            z: itemBordersZ[i],
            type: "scatter3d",
            ...cell,
        });

        data.push({
            legendgroup: "items",
            mode: "text",
            name: "Item ids",
            showlegend: false,
            text: itemIds[i],
            textfont: {size: 8},
            textposition: "middle center",
            x: itemIdsX[i],
            y: itemIdsY[i],
            z: itemIdsZ[i],
            type: "scatter3d",
            ...cell,
        });
    }

    // Plot.
    layout.autosize = true;
    layout.font = {size: 14};
    layout.showlegend = legend;
    layout.legend = {font: {size: 14}, itemsizing: "constant"};
    // The grid only has scenes: plotly.py's 'update_xaxes' and 'update_yaxes'
    // then update the default 'xaxis' and 'yaxis', which they create.
    layout.xaxis = {rangeslider: {visible: false}};
    layout.yaxis = {scaleanchor: "x", scaleratio: 1};
    // 'update_scenes'.
    const largest = Math.max(maxBinLx, maxBinLy, maxBinLz, 1);
    for (const name of Object.keys(layout)) {
        if (!name.startsWith("scene"))
            continue;
        // All the bins at the same scale: the axes of all the scenes span
        // the largest dimensions of the bins.
        layout[name].aspectmode = "manual";
        layout[name].aspectratio = {x: maxBinLx / largest, y: maxBinLy / largest, z: maxBinLz / largest};
        layout[name].xaxis = {range: [0, maxBinLx]};
        layout[name].yaxis = {range: [0, maxBinLy]};
        layout[name].zaxis = {range: [0, maxBinLz]};
        layout[name].camera = {
            center: {x: 0, y: 0, z: -0.25},
            eye: {x: -1.75 / zoom, y: -1.5 / zoom, z: 0.75 / zoom},
        };
    }

    return {data, layout};
}
