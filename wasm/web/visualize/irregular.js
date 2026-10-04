// Visualization of irregular solution certificates and instances.
//
// Build a plotly figure from a certificate (JSON) written by the irregular
// solver, or from an irregular instance (JSON).
//
// JavaScript port of 'python/packingsolver/visualize/irregular.py': the
// figures are '{data, layout}' objects equal to the JSON of the Python figures
// (without their template).

import {PASTEL_COLORS, PLOTLY_COLORS, makeSubplots} from "./common.js";

// 'numpy.linspace(start, stop, num)', with the same floating-point operations.
function linspace(start, stop, num) {
    const div = num - 1;
    const delta = stop - start;
    const step = delta / div;
    const y = new Array(num);
    for (let i = 0; i < num; ++i) {
        // numpy: 'arange * step', or '(arange / div) * delta' if the step is
        // zero; then '+ start'.
        y[i] = ((step === 0)? (i / div) * delta: i * step) + start;
    }
    if (num > 1)
        y[num - 1] = stop;
    return y;
}

function certificateElement(element) {
    const t = element.type;
    const xs = element.xs;
    const ys = element.ys;
    const xe = element.xe;
    const ye = element.ye;
    if (t === "CircularArc")
        return [t, xs, ys, xe, ye, element.xc, element.yc, element.orientation];
    return [t, xs, ys, xe, ye, null, null, null];
}

function instanceElement(element) {
    const t = element.type;
    const xs = element.start.x;
    const ys = element.start.y;
    const xe = element.end.x;
    const ye = element.end.y;
    if (t === "CircularArc")
        return [t, xs, ys, xe, ye, element.center.x, element.center.y, element.orientation];
    return [t, xs, ys, xe, ye, null, null, null];
}

function shapePath(
        pathX,
        pathY,
        shape,
        isHole,
        readElement,
        handleFull) {
    // How to draw a filled circle segment?
    // https://community.plotly.com/t/how-to-draw-a-filled-circle-segment/59583
    // https://stackoverflow.com/questions/70965145/can-plotly-for-python-plot-a-polygon-with-one-or-multiple-holes-in-it
    const elements = (!isHole)? shape: [...shape].reverse();
    for (const element of elements) {
        let [t, xs, ys, xe, ye, xc, yc, orientation] = readElement(element);
        let rc;
        if (t === "CircularArc")
            rc = Math.sqrt((xc - xs) ** 2 + (yc - ys) ** 2);

        // A hole is drawn in the opposite direction, so that it isn't
        // filled.
        if (isHole) {
            [xs, ys, xe, ye] = [xe, ye, xs, ys];
            if (["Anticlockwise", "anticlockwise", "A", "a"].includes(orientation))
                orientation = "Clockwise";
            else if (["Clockwise", "clockwise", "C", "c"].includes(orientation))
                orientation = "Anticlockwise";
        }

        if (pathX.length === 0 || pathX[pathX.length - 1] === null) {
            pathX.push(xs);
            pathY.push(ys);
        }

        if (t === "LineSegment") {
            pathX.push(xe);
            pathY.push(ye);
        } else if (t === "CircularArc") {
            const startCos = (xs - xc) / rc;
            const startSin = (ys - yc) / rc;
            const startAngle = Math.atan2(startSin, startCos);
            const endCos = (xe - xc) / rc;
            const endSin = (ye - yc) / rc;
            let endAngle = Math.atan2(endSin, endCos);
            if (handleFull && ["Full", "full", "F", "f"].includes(orientation))
                endAngle += (isHole? -2: 2) * Math.PI;
            if (["Anticlockwise", "anticlockwise", "A", "a"].includes(orientation)
                    && endAngle <= startAngle)
                endAngle += 2 * Math.PI;
            if (["Clockwise", "clockwise", "C", "c"].includes(orientation)
                    && endAngle >= startAngle)
                endAngle -= 2 * Math.PI;

            const angles = linspace(startAngle, endAngle, 1024);
            for (let i = 1; i < angles.length; ++i) {
                pathX.push(xc + rc * Math.cos(angles[i]));
                pathY.push(yc + rc * Math.sin(angles[i]));
            }
        }
    }
    pathX.push(null);
    pathY.push(null);
}

function certificateShapePath(pathX, pathY, shape, isHole = false) {
    shapePath(pathX, pathY, shape, isHole, certificateElement, true);
}

function instanceShapePath(pathX, pathY, shape, isHole = false) {
    shapePath(pathX, pathY, shape, isHole, instanceElement, false);
}

function certificatePaths(certificate) {
    const binsX = [];
    const binsY = [];
    const defectsX = [];
    const defectsY = [];
    const itemsX = [];
    const itemsY = [];

    const j = JSON.parse(certificate);

    j.bins.forEach((solutionBin, binPos) => {
        binsX.push([]);
        binsY.push([]);
        defectsX.push([]);
        defectsY.push([]);
        itemsX.push([]);
        itemsY.push([]);

        certificateShapePath(binsX[binPos], binsY[binPos], solutionBin.shape);
        for (const defect of (("defects" in solutionBin)? solutionBin.defects: [])) {
            certificateShapePath(defectsX[binPos], defectsY[binPos], defect.shape);
            for (const hole of (("holes" in defect)? defect.holes: []))
                certificateShapePath(defectsX[binPos], defectsY[binPos], hole, true);
        }
        for (const solutionItem of solutionBin.items) {
            const itemId = solutionItem.id;
            while (itemsX[binPos].length <= itemId) {
                itemsX[binPos].push([]);
                itemsY[binPos].push([]);
            }
            for (const itemShape of solutionItem.item_shapes) {
                certificateShapePath(
                        itemsX[binPos][itemId],
                        itemsY[binPos][itemId],
                        itemShape.shape);
                for (const hole of (("holes" in itemShape)? itemShape.holes: []))
                    certificateShapePath(itemsX[binPos][itemId], itemsY[binPos][itemId], hole, true);
            }
        }
    });

    return [binsX, binsY, defectsX, defectsY, itemsX, itemsY];
}

function grid(m, columns) {
    const numberOfCols = (columns !== null && columns !== undefined)?
        columns: Math.ceil(Math.sqrt(m));
    const numberOfRows = Math.ceil(m / numberOfCols);
    return [numberOfRows, numberOfCols];
}

// 'fig.add_trace(trace, row=row, col=col)': place the trace in the cell.
function addTrace(data, subplots, numberOfRows, numberOfCols, trace, row, col) {
    if (row < 1 || row > numberOfRows || col < 1 || col > numberOfCols) {
        throw new Error(
            "The (row, col) pair sent is out of range. "
            + "Use Figure.print_grid to view the subplot grid. ");
    }
    data.push({type: "scatter", ...trace, ...subplots.subplot(row, col)});
}

// Build the figure of an irregular solution certificate.
//
// certificate: content of the JSON file
// item_color: color palette used among ["SAME", "ID"]
// columns: number of columns in the subplot grid
//
// Return {data, layout}.
export function figure(
        certificate,
        {
            item_color = "ID",
            columns = null,
        } = {}) {
    if (!["SAME", "ID"].includes(item_color))
        throw new Error(`color palette ${item_color} is unknown, please use one of the following: 'SAME', 'ID'`);

    const [binsX, binsY, defectsX, defectsY, itemsX, itemsY] =
        certificatePaths(certificate);

    const colors = PASTEL_COLORS;
    const m = binsX.length;
    const [numberOfRows, numberOfCols] = grid(m, columns);
    const subplots = makeSubplots({
        rows: numberOfRows,
        cols: numberOfCols,
        verticalSpacing: 0.001,
    });
    const data = [];
    const add = (trace, row, col) =>
        addTrace(data, subplots, numberOfRows, numberOfCols, trace, row, col);

    for (let i = 0; i < m; ++i) {
        const row = Math.floor(i / numberOfCols) + 1;
        const col = (i % numberOfCols) + 1;

        add({
            x: binsX[i],
            y: binsY[i],
            name: "Bins",
            legendgroup: "bins",
            showlegend: (i === 0),
            marker: {
                color: "black",
                size: 1,
            },
        }, row, col);

        add({
            x: defectsX[i],
            y: defectsY[i],
            name: "Defects",
            legendgroup: "defects",
            showlegend: (i === 0),
            fillcolor: "crimson",
            fill: "toself",
            marker: {
                color: "black",
                size: 1,
            },
        }, row, col);

        for (let k = 0; k < itemsX[i].length; ++k) {
            if (item_color === "SAME") {
                // As in the Python module: placed in row 'i + 1', column 1.
                add({
                    x: itemsX[i][k],
                    y: itemsY[i][k],
                    name: "Items",
                    legendgroup: "items",
                    showlegend: (i === 0 && k === 0),
                    fillcolor: "cornflowerblue",
                    fill: "toself",
                    marker: {
                        color: "black",
                        size: 1,
                    },
                }, i + 1, 1);
            } else if (item_color === "ID") {
                add({
                    x: itemsX[i][k],
                    y: itemsY[i][k],
                    name: `Items ${k}`,
                    legendgroup: "item",
                    showlegend: i === 0,
                    fillcolor: colors[k % colors.length],
                    fill: "toself",
                    marker: {
                        color: "black",
                        size: 1,
                    },
                }, row, col);
            }
        }
    }

    // Plot.
    const layout = subplots.layout;
    Object.assign(layout, {
        autosize: true,
        font: {size: 14},
        legend: {font: {size: 14}, itemsizing: "constant"},
    });
    updateXaxes(layout, {rangeslider: {visible: false}});
    for (let i = 0; i < m; ++i) {
        const row = Math.floor(i / numberOfCols) + 1;
        const col = (i % numberOfCols) + 1;
        updateYaxis(layout, subplots, row, col, {
            scaleanchor: (i === 0)? "x": `x${i + 1}`,
            scaleratio: 1,
        });
    }
    return {data, layout};
}

// 'fig.update_xaxes(**properties)': update every x axis.
function updateXaxes(layout, properties) {
    for (const key of Object.keys(layout)) {
        if (/^xaxis\d*$/.test(key))
            Object.assign(layout[key], properties);
    }
}

// 'fig.update_yaxes(**properties, row=row, col=col)'.
function updateYaxis(layout, subplots, row, col, properties) {
    const yaxis = subplots.subplot(row, col).yaxis;
    Object.assign(layout["yaxis" + yaxis.slice(1)], properties);
}

// Compute the image size used to export the figure of a certificate.
//
// certificate: content of the JSON file
// columns: number of columns in the subplot grid
// scale: scale factor for cell dimensions
// width: image width in pixels, computed from the bins if null
// height: image height in pixels, computed from the bins if null
//
// Return the array [width, height].
export function exportSize(
        certificate,
        {
            columns = null,
            scale = 1.0,
            width = null,
            height = null,
        } = {}) {
    const [binsX, binsY] = certificatePaths(certificate);
    const m = binsX.length;
    const extent = (values) => {
        const defined = values.filter((value) => value !== null);
        return Math.max(...defined) - Math.min(...defined);
    };
    const maxBinLx = Math.max(...binsX.map(extent));
    const maxBinLy = Math.max(...binsY.map(extent));
    const [numberOfRows, numberOfCols] = grid(m, columns);
    const cellWidth = Math.trunc(maxBinLx * scale);
    const cellHeight = Math.trunc(maxBinLy * scale);
    const exportWidth = (width !== null && width !== undefined)?
        width: numberOfCols * cellWidth + 160;
    const exportHeight = (height !== null && height !== undefined)?
        height: numberOfRows * cellHeight + 170;
    return [exportWidth, exportHeight];
}

// Build the figure of an irregular instance.
//
// instance: content of the JSON file
//
// Return {data, layout}.
export function instanceFigure(instance, options = {}) {
    const binTypesX = [];
    const binTypesY = [];
    const defectsX = [];
    const defectsY = [];
    const itemTypesX = [];
    const itemTypesY = [];

    const j = JSON.parse(instance);

    j.bin_types.forEach((binType, binTypeId) => {
        binTypesX.push([]);
        binTypesY.push([]);
        defectsX.push([]);
        defectsY.push([]);

        instanceShapePath(
                binTypesX[binTypeId],
                binTypesY[binTypeId],
                binType.elements);
        for (const defect of (("defects" in binType)? binType.defects: [])) {
            instanceShapePath(
                    defectsX[binTypeId],
                    defectsY[binTypeId],
                    defect.elements);
            for (const hole of (("holes" in defect)? defect.holes: [])) {
                instanceShapePath(
                        defectsX[binTypeId],
                        defectsY[binTypeId],
                        hole.elements,
                        true);
            }
        }
    });

    j.item_types.forEach((itemType, itemTypeId) => {
        itemTypesX.push([]);
        itemTypesY.push([]);
        for (const itemShape of itemType.shapes) {
            instanceShapePath(
                    itemTypesX[itemTypeId],
                    itemTypesY[itemTypeId],
                    itemShape.elements);
            for (const hole of (("holes" in itemShape)? itemShape.holes: [])) {
                instanceShapePath(
                        itemTypesX[itemTypeId],
                        itemTypesY[itemTypeId],
                        hole.elements,
                        true);
            }
        }
    });

    const m = binTypesX.length;
    const n = itemTypesX.length;
    // Unused, as in the Python module.
    const colors = PLOTLY_COLORS; // eslint-disable-line no-unused-vars
    const subplots = makeSubplots({
        rows: m + n,
        cols: 1,
        verticalSpacing: 0.001,
    });
    const data = [];
    const add = (trace, row, col) =>
        addTrace(data, subplots, m + n, 1, trace, row, col);

    for (let i = 0; i < m; ++i) {

        add({
            x: binTypesX[i],
            y: binTypesY[i],
            name: "Bin types",
            legendgroup: "bin types",
            showlegend: (i === 0),
            marker: {
                color: "black",
                size: 1,
            },
        }, i + 1, 1);

        add({
            x: defectsX[i],
            y: defectsY[i],
            name: "Defects",
            legendgroup: "defects",
            showlegend: (i === 0),
            fillcolor: "crimson",
            fill: "toself",
            marker: {
                color: "black",
                size: 1,
            },
        }, i + 1, 1);
    }

    for (let i = 0; i < n; ++i) {

        add({
            x: itemTypesX[i],
            y: itemTypesY[i],
            name: "Item types",
            legendgroup: "item types",
            showlegend: (i === 0),
            fillcolor: "cornflowerblue",
            fill: "toself",
            marker: {
                color: "black",
                size: 1,
            },
        }, m + i + 1, 1);
    }

    // Plot.
    const layout = subplots.layout;
    Object.assign(layout, {
        autosize: true,
        height: (m + n) * 1000,
    });
    updateXaxes(layout, {rangeslider: {visible: false}});
    for (let i = 0; i < m + n; ++i) {
        updateYaxis(layout, subplots, i + 1, 1, {
            scaleanchor: (i === 0)? "x": `x${i + 1}`,
            scaleratio: 1,
        });
    }
    return {data, layout};
}
