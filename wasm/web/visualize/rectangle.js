// Visualization of rectangle solution certificates.
//
// Build a plotly figure from a certificate (CSV) written by the rectangle
// solver.
// JavaScript port of 'python/packingsolver/visualize/rectangle.py'.

import {PASTEL_COLORS, makeSubplots, parseCsv} from "./common.js";

// Python's 'str' of a float ('repr'): shortest round-trip digits, positional
// notation for exponents in [-4, 16), always with a fractional part.
function pyFloatStr(x) {
    if (Number.isNaN(x))
        return "nan";
    if (!Number.isFinite(x))
        return (x > 0)? "inf": "-inf";
    if (x === 0)
        return Object.is(x, -0)? "-0.0": "0.0";
    const sign = (x < 0)? "-": "";
    // Shortest round-trip digits and decimal exponent.
    const [mantissa, exponentText] = Math.abs(x).toExponential().split("e");
    const exponent = parseInt(exponentText, 10);
    const digits = mantissa.replace(".", "");
    if (exponent >= -4 && exponent < 16) {
        if (exponent < 0)
            return sign + "0." + "0".repeat(-exponent - 1) + digits;
        if (digits.length <= exponent + 1)
            return sign + digits + "0".repeat(exponent + 1 - digits.length) + ".0";
        return sign + digits.slice(0, exponent + 1) + "." + digits.slice(exponent + 1);
    }
    const exponentAbs = String(Math.abs(exponent)).padStart(2, "0");
    return sign + mantissa + "e" + ((exponent < 0)? "-": "+") + exponentAbs;
}

// Python's 'str' of a number: 'isFloat' tells whether the Python value is a
// float (e.g. '1.0') or an int (e.g. '1').
function pyStr(x, isFloat) {
    return isFloat? pyFloatStr(x): String(x);
}

// Python's 'round(x, n)' for a float: rounding of the exact binary value to
// 'n' decimals, ties to even.
function pyRound(x, n) {
    if (!Number.isFinite(x) || Math.abs(x) >= 1e21)
        return x;
    const negative = (x < 0) || Object.is(x, -0);
    // Exact decimal expansion (100 decimals is enough for the values met).
    const [integerPart, fractionalPart] = Math.abs(x).toFixed(100).split(".");
    const kept = integerPart + fractionalPart.slice(0, n);
    const rest = fractionalPart.slice(n);
    let roundUp = false;
    if (rest[0] > "5") {
        roundUp = true;
    } else if (rest[0] === "5") {
        if (/[1-9]/.test(rest.slice(1)))
            roundUp = true;
        else  // Tie: to even.
            roundUp = (parseInt(kept[kept.length - 1], 10) % 2 === 1);
    }
    // Increment the decimal digits string.
    let digits = kept.split("");
    if (roundUp) {
        let k = digits.length - 1;
        while (k >= 0 && digits[k] === "9") {
            digits[k] = "0";
            --k;
        }
        if (k >= 0)
            digits[k] = String(parseInt(digits[k], 10) + 1);
        else
            digits = ["1"].concat(digits);
    }
    const text = digits.slice(0, digits.length - n).join("")
        + "." + digits.slice(digits.length - n).join("");
    const value = parseFloat(text);
    return negative? -value: value;
}

// Build the figure of a rectangle solution certificate.
//
// text: content of the CSV certificate
// options.item_color: color palette used among ["SAME", "ID", "GROUP_ID", "DENSITY"]
// options.columns: number of columns in the subplot grid
//
// Return {data, layout} for 'Plotly.newPlot'.
export function figure(text, {item_color = "ID", columns = null} = {}) {
    if (!["SAME", "ID", "GROUP_ID", "DENSITY"].includes(item_color))
        throw new Error(`color palette ${item_color} is unknown, please use one of the following : 'SAME', 'ID', 'GROUP_ID', 'DENSITY'`);

    const binsX = [];
    const binsY = [];
    const binsWeights = [];
    const binsGravityCentersX = [];
    const binsGravityCentersY = [];
    const defectsX = [];
    const defectsY = [];
    const itemsX = [];
    const itemsY = [];
    const itemIdsX = [];
    const itemIdsY = [];
    const itemIds = [];
    const itemsDensity = [];
    const itemsWeights = [];
    const itemsLeft = [];
    const itemsRight = [];
    const itemsBottom = [];
    const itemsTop = [];

    for (const row of parseCsv(text)) {
        const i = parseInt(row["BIN"], 10);
        const type = row["TYPE"];
        const id = parseInt(row["ID"], 10);
        const w = parseInt(row["LX"], 10);
        const h = parseInt(row["LY"], 10);
        const x1 = parseInt(row["X"], 10);
        const y1 = parseInt(row["Y"], 10);
        const weight = ("WEIGHT" in row)? parseInt(row["WEIGHT"], 10): 0;
        const x2 = x1 + w;
        const y2 = y1 + h;

        if (type === "BIN") {
            for (const list of [
                    binsX, binsY, binsGravityCentersX, binsGravityCentersY,
                    defectsX, defectsY, itemsX, itemsY, itemIdsX, itemIdsY,
                    itemIds, itemsDensity, itemsWeights, itemsLeft,
                    itemsRight, itemsBottom, itemsTop])
                list.push([]);
            binsX[i].push(x1, x2, x2, x1, x1, null);
            binsY[i].push(y1, y1, y2, y2, y1, null);
            binsWeights.push(weight);
            binsGravityCentersX[i] = [0, 1, 1, 0, 0, null];
            binsGravityCentersY[i] = [0, 1, 1, 0, 0, null];
        } else if (type === "DEFECT") {  // Defect.
            defectsX[i].push(x1, x2, x2, x1, x1, null);
            defectsY[i].push(y1, y1, y2, y2, y1, null);
        } else if (type === "ITEM") {  // Item.
            let k;
            if (item_color === "SAME")
                k = 0;
            else if (item_color === "GROUP_ID" && "GROUP_ID" in row)
                k = parseInt(row["GROUP_ID"], 10);
            else
                k = parseInt(row["ID"], 10);
            while (itemsX[i].length <= k) {
                itemsX[i].push([]);
                itemsY[i].push([]);
                itemsDensity[i].push(0);
            }
            itemsWeights[i].push(weight);
            itemsLeft[i].push(x1);
            itemsRight[i].push(x2);
            itemsBottom[i].push(y1);
            itemsTop[i].push(y2);

            itemsX[i][k].push(x1, x2, x2, x1, x1, null);
            itemsY[i][k].push(y1, y1, y2, y2, y1, null);
            itemsDensity[i][k] = (h * w > 0)? weight / (h * w): 0;
            itemIdsX[i].push((x1 + x2) / 2);
            itemIdsY[i].push((y1 + y2) / 2);
            itemIds[i].push(id);
            if (binsWeights[i] > 0) {
                for (let j = 0; j < 5; ++j) {
                    binsGravityCentersX[i][j] += (x1 + w / 2) * weight / binsWeights[i];
                    binsGravityCentersY[i][j] += (y1 + h / 2) * weight / binsWeights[i];
                }
            }
        }
    }

    const m = binsX.length;
    for (let i = 0; i < m; ++i) {
        for (const j of [1, 2])
            binsGravityCentersX[i][j] += 8;
        for (const j of [2, 3])
            binsGravityCentersY[i][j] += 8;
    }

    // The repartitions are Python ints until a float is added to them, which
    // changes how they are printed.
    const binsGravityRepartitionX = [];
    const binsGravityRepartitionY = [];
    const binsGravityRepartitionXIsFloat = [];
    const binsGravityRepartitionYIsFloat = [];
    for (let i = 0; i < m; ++i) {
        binsGravityRepartitionX.push(0);
        binsGravityRepartitionY.push(0);
        binsGravityRepartitionXIsFloat.push(false);
        binsGravityRepartitionYIsFloat.push(false);
        for (let j = 0; j < itemsWeights[i].length; ++j) {
            if (itemsRight[i][j] <= binsGravityCentersX[i][0]) {
                binsGravityRepartitionX[i] += itemsWeights[i][j];
            } else if (itemsLeft[i][j] < binsGravityCentersX[i][0]) {
                const rate = (binsGravityCentersX[i][0] - itemsLeft[i][j]) / (itemsRight[i][j] - itemsLeft[i][j]);
                binsGravityRepartitionX[i] += itemsWeights[i][j] * rate;
                binsGravityRepartitionXIsFloat[i] = true;
            }

            if (itemsTop[i][j] <= binsGravityCentersY[i][0]) {
                binsGravityRepartitionY[i] += itemsWeights[i][j];
            } else if (itemsBottom[i][j] < binsGravityCentersY[i][0]) {
                const rate = (binsGravityCentersY[i][0] - itemsBottom[i][j]) / (itemsTop[i][j] - itemsBottom[i][j]);
                binsGravityRepartitionY[i] += itemsWeights[i][j] * rate;
                binsGravityRepartitionYIsFloat[i] = true;
            }
        }

        const totalWeight = itemsWeights[i].reduce((a, b) => a + b, 0);
        if (totalWeight > 0) {
            binsGravityRepartitionX[i] = binsGravityRepartitionX[i] / totalWeight * 100.0;
            binsGravityRepartitionY[i] = binsGravityRepartitionY[i] / totalWeight * 100.0;
            binsGravityRepartitionXIsFloat[i] = true;
            binsGravityRepartitionYIsFloat[i] = true;
        }
    }

    // const colors = PLOTLY_COLORS;
    const colors = PASTEL_COLORS;
    const numberOfCols = (columns !== null && columns !== undefined)? columns: Math.ceil(Math.sqrt(m));
    const numberOfRows = Math.ceil(m / numberOfCols);
    const subplots = makeSubplots({
        rows: numberOfRows,
        cols: numberOfCols,
        verticalSpacing: 0.001,
    });

    const data = [];
    const addTrace = (trace, row, col) => {
        data.push({...trace, type: "scatter", ...subplots.subplot(row, col)});
    };

    for (let i = 0; i < m; ++i) {
        const row = Math.floor(i / numberOfCols) + 1;
        const col = (i % numberOfCols) + 1;

        addTrace({
            x: binsX[i],
            y: binsY[i],
            name: "Bins",
            legendgroup: "bins",
            showlegend: (i === 0),
            marker: {color: "black", size: 1},
        }, row, col);

        addTrace({
            x: defectsX[i],
            y: defectsY[i],
            name: "Defects",
            legendgroup: "defects",
            showlegend: (i === 0),
            fillcolor: "crimson",
            fill: "toself",
            marker: {color: "black", size: 1},
        }, row, col);

        for (let k = 0; k < itemsX[i].length; ++k) {
            if (item_color === "SAME") {
                addTrace({
                    x: itemsX[i][k],
                    y: itemsY[i][k],
                    name: "Items",
                    legendgroup: "items",
                    showlegend: (i === 0 && k === 0),
                    fillcolor: "cornflowerblue",
                    fill: "toself",
                    marker: {color: "black", size: 1},
                }, row, col);

            } else if (item_color === "ID") {
                addTrace({
                    x: itemsX[i][k],
                    y: itemsY[i][k],
                    name: `Items ${k}`,
                    legendgroup: "item",
                    showlegend: i === 0,
                    fillcolor: colors[k % colors.length],
                    fill: "toself",
                    marker: {color: "black", size: 1},
                }, row, col);

            } else if (item_color === "GROUP_ID") {
                addTrace({
                    x: itemsX[i][k],
                    y: itemsY[i][k],
                    name: `Group ${k}`,
                    legendgroup: "items",
                    showlegend: true,
                    fillcolor: colors[k % colors.length],
                    fill: "toself",
                    marker: {color: "black", size: 1},
                }, row, col);

            } else if (item_color === "DENSITY") {
                const minDensity = Math.min(...itemsDensity[i]);
                const maxDensity = Math.max(...itemsDensity[i]);
                let c;
                if (minDensity !== maxDensity)
                    c = (itemsDensity[i][k] - minDensity) / (maxDensity - minDensity);
                else
                    c = 0.5;
                addTrace({
                    x: itemsX[i][k],
                    y: itemsY[i][k],
                    name: `Density ${pyFloatStr(pyRound(c * 100.0, 1))}%`,
                    legendgroup: "items",
                    showlegend: false,
                    fillcolor: "hsl(218, 100," + pyFloatStr(90 - 30 * c) + ")",
                    fill: "toself",
                    marker: {color: "black", size: 1},
                }, row, col);
            }
        }

        addTrace({
            x: itemIdsX[i],
            y: itemIdsY[i],
            name: "Item ids",
            legendgroup: "items",
            showlegend: false,
            mode: "text",
            // plotly.py converts the numbers of a 'text' array to strings.
            text: itemIds[i].map(String),
            textfont: {size: 8},
            textposition: "middle center",
        }, row, col);

        if (item_color === "DENSITY") {
            const repartitionX = pyRound(binsGravityRepartitionX[i], 2);
            const repartitionXIsFloat = binsGravityRepartitionXIsFloat[i];
            addTrace({
                text: "x:" + pyStr(repartitionX, repartitionXIsFloat)
                    + "% / " + pyStr(100 - repartitionX, repartitionXIsFloat) + "%",
                x: binsGravityCentersX[i],
                y: binsY[i],
                fill: "toself",
                name: "Gravity",
                legendgroup: "gravity x",
                showlegend: (i === 0),
                marker: {color: "red", size: 1},
            }, row, col);
            const repartitionY = pyRound(binsGravityRepartitionY[i], 2);
            const repartitionYIsFloat = binsGravityRepartitionYIsFloat[i];
            addTrace({
                text: "y:" + pyStr(repartitionY, repartitionYIsFloat)
                    + "% / " + pyStr(100 - repartitionY, repartitionYIsFloat) + "%",
                x: binsX[i],
                y: binsGravityCentersY[i],
                fill: "toself",
                name: "Gravity",
                legendgroup: "gravity y",
                showlegend: (i === 0),
                marker: {color: "red", size: 1},
            }, row, col);
        }
    }

    // Plot.
    const layout = subplots.layout;
    layout.autosize = true;
    layout.font = {size: 14};
    layout.legend = {font: {size: 14}, itemsizing: "constant"};
    // 'update_xaxes' without row/col: every x axis.
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
