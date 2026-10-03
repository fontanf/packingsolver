// Visualization of boxstacks solutions.
//
// Builds a plotly figure from a boxstacks solution certificate (CSV), with one
// 3D subplot per bin.
//
// JavaScript port of 'python/packingsolver/visualize/boxstacks.py'.

import {PASTEL_COLORS, makeSubplots, parseCsv} from "./common.js";

// Build the figure of a boxstacks solution certificate.
//
// Args:
//     text: content of the CSV certificate.
//     options:
//         item_color: color palette used among ["SAME", "ID"].
//         columns: number of columns in the subplot grid.
//         zoom: camera zoom factor (higher: closer/bigger).
//         expand_copies: draw one subplot per bin copy instead of one per bin
//             type.
//         legend: show the legend; false hides it, freeing up space for the
//             plot itself.
//
// Returns:
//     {data, layout} for 'Plotly.newPlot'.
export function figure(text, options = {}) {
    const {
        item_color = "ID",
        columns = null,
        zoom = 1.0,
        expand_copies = false,
        legend = true,
    } = options;
    if (!["SAME", "ID"].includes(item_color))
        throw new Error(`color palette ${item_color} is unknown, please use one of the following: 'SAME', 'ID'`);

    let binsX = [];
    let binsY = [];
    let binsZ = [];
    let binsI = [];
    let binsJ = [];
    let binsK = [];
    let defectsX = [];
    let defectsY = [];
    let defectsZ = [];
    let defectsI = [];
    let defectsJ = [];
    let defectsK = [];
    let itemsX = [];
    let itemsY = [];
    let itemsZ = [];
    let itemsI = [];
    let itemsJ = [];
    let itemsK = [];
    let itemBordersX = [];
    let itemBordersY = [];
    let itemBordersZ = [];
    let itemIdsX = [];
    let itemIdsY = [];
    let itemIdsZ = [];
    let itemIds = [];
    const binCopies = [];

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
            binCopies.push(("COPIES" in row)? parseInt(row["COPIES"], 10): 1);
            for (const lists of [
                binsX, binsY, binsZ, binsI, binsJ, binsK,
                defectsX, defectsY, defectsZ, defectsI, defectsJ, defectsK,
                itemsX, itemsY, itemsZ, itemsI, itemsJ, itemsK,
                itemBordersX, itemBordersY, itemBordersZ,
                itemIdsX, itemIdsY, itemIdsZ, itemIds]) {
                lists.push([]);
            }

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

    if (expand_copies) {
        // Repeat each bin's lists as many times as the bin has copies.
        const expand = (lst) => {
            const out = [];
            lst.forEach((v, b) => {
                if (b >= binCopies.length)
                    return;
                for (let c = 0; c < binCopies[b]; ++c)
                    out.push(v);
            });
            return out;
        };
        binsX = expand(binsX);
        binsY = expand(binsY);
        binsZ = expand(binsZ);
        binsI = expand(binsI);
        binsJ = expand(binsJ);
        binsK = expand(binsK);
        defectsX = expand(defectsX);
        defectsY = expand(defectsY);
        defectsZ = expand(defectsZ);
        defectsI = expand(defectsI);
        defectsJ = expand(defectsJ);
        defectsK = expand(defectsK);
        itemsX = expand(itemsX);
        itemsY = expand(itemsY);
        itemsZ = expand(itemsZ);
        itemsI = expand(itemsI);
        itemsJ = expand(itemsJ);
        itemsK = expand(itemsK);
        itemBordersX = expand(itemBordersX);
        itemBordersY = expand(itemBordersY);
        itemBordersZ = expand(itemBordersZ);
        itemIdsX = expand(itemIdsX);
        itemIdsY = expand(itemIdsY);
        itemIdsZ = expand(itemIdsZ);
        itemIds = expand(itemIds);
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
            if (item_color === "SAME")
                color = "cornflowerblue";
            else
                color = colors[pyMod(parseInt(itemIds[i][k], 10), colors.length)];
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
    // 'update_xaxes' / 'update_yaxes' on a figure without 2D axes still
    // create the default 'xaxis' / 'yaxis'.
    layout.xaxis = {rangeslider: {visible: false}};
    layout.yaxis = {scaleanchor: "x", scaleratio: 1};
    for (const key of Object.keys(layout)) {
        if (!/^scene\d*$/.test(key))
            continue;
        layout[key].aspectmode = "data";
        layout[key].camera = {
            center: {x: 0, y: 0, z: -0.25},
            eye: {x: -1.75 / zoom, y: -1.5 / zoom, z: 0.75 / zoom},
        };
    }
    return {data, layout};
}

// Return the default [width, height] in pixels of the exported image.
//
// Args:
//     text: content of the CSV certificate.
//     options:
//         columns: number of columns in the subplot grid.
//         scale: scale factor for cell dimensions.
//         expand_copies: one subplot per bin copy instead of one per bin type.
//         width: image width in pixels; overrides the computed one.
//         height: image height in pixels; overrides the computed one.
export function exportSize(text, options = {}) {
    const {
        columns = null,
        scale = 1.0,
        expand_copies = false,
        width = null,
        height = null,
    } = options;
    let m = 0;
    let maxBinLx = 0;
    let maxBinLy = 0;
    for (const row of parseCsv(text)) {
        if (row["TYPE"] === "BIN") {
            maxBinLx = Math.max(maxBinLx, parseInt(row["LX"], 10));
            maxBinLy = Math.max(maxBinLy, parseInt(row["LY"], 10));
            m += expand_copies? (("COPIES" in row)? parseInt(row["COPIES"], 10): 1): 1;
        }
    }
    const numberOfCols = (columns !== null && columns !== undefined)? columns: Math.ceil(Math.sqrt(m));
    const numberOfRows = Math.ceil(m / numberOfCols);
    const cellWidth = Math.trunc(maxBinLx * scale);
    const cellHeight = Math.trunc(maxBinLy * scale);
    const exportWidth = (width !== null && width !== undefined)? width: numberOfCols * cellWidth + 160;
    const exportHeight = (height !== null && height !== undefined)? height: numberOfRows * cellHeight + 170;
    return [exportWidth, exportHeight];
}

// Python's '%' (the result has the sign of the divisor).
function pyMod(a, n) {
    return ((a % n) + n) % n;
}
