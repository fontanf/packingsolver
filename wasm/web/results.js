// The results of an optimization, for the web page: the key numbers of a
// solution (a tile each) and the chart of the progress of the optimization,
// from the JSON outputs of the solver. Without the page, to be tested.

// Bound of each objective in the output.
export const BOUNDS = {
    "knapsack": "KnapsackBound",
    "bin-packing": "BinPackingBound",
    "variable-sized-bin-packing": "VariableSizedBinPackingBound",
    "open-dimension-x": "OpenDimensionXBound",
    "open-dimension-y": "OpenDimensionYBound",
    "open-dimension-z": "OpenDimensionZBound",
};

// The value of a solution for each objective (from the 'Solution' object of
// the output), and its name. The used length along the open dimension is
// 'Width' and 'Height' for rectangleguillotine.
export const OBJECTIVE_VALUES = {
    "knapsack": {label: "Profit", value: (s) => s.ItemProfit},
    "bin-packing": {label: "Bins", value: (s) => s.NumberOfBins},
    "variable-sized-bin-packing": {label: "Cost", value: (s) => s.BinCost},
    "bin-packing-with-leftovers": {label: "Leftover", value: (s) => s.LeftoverValue},
    "bin-packing-cutting-cost": {label: "Cutting cost", value: (s) => s.CuttingCost},
    "open-dimension-x": {label: "Length", value: (s) => (s.XMax !== undefined)? s.XMax: s.Width},
    "open-dimension-y": {label: "Length", value: (s) => (s.YMax !== undefined)? s.YMax: s.Height},
    "open-dimension-z": {label: "Length", value: (s) => s.ZMax},
    "open-dimension-xy": {label: "Area", value: (s) => s.OpenDimensionXYArea},
};

const finite = (value) => (typeof value === "number" && Number.isFinite(value))? value: null;

// The value of the solution of an output, 'null' if it has none.
export function objectiveValue(output, objective) {
    const spec = OBJECTIVE_VALUES[objective];
    if (spec === undefined || output.Solution === undefined || output.Solution.NumberOfItems === 0)
        return null;
    return finite(spec.value(output.Solution));
}

// The bound of an output, 'null' if it has none.
export function boundValue(output, objective) {
    const key = BOUNDS[objective];
    return (key === undefined)? null: finite(output[key]);
}

// The relative gap between the value of a solution and a bound, from 0 (the
// solution is optimal) to 1; 'null' if one of them is missing.
export function gap(value, bound) {
    if (value === null || bound === null)
        return null;
    const scale = Math.max(Math.abs(value), Math.abs(bound));
    return (scale === 0)? 0: Math.abs(value - bound) / scale;
}

// The number of items of an instance in the JSON format, 'null' if some item
// types have an unlimited number of copies.
export function totalItems(instanceObject) {
    let total = 0;
    for (const itemType of instanceObject.item_types || []) {
        const copies = (itemType.copies !== undefined)? itemType.copies: 1;
        if (copies < 0)
            return null;
        total += copies;
    }
    return total;
}

// A number with at most 6 significant digits, without trailing zeros.
export function format(value) {
    if (typeof value !== "number")
        return String(value);
    return Number.isInteger(value)? String(value): String(Number(value.toPrecision(6)));
}

// A number of a tile: at most 6 significant digits, with thousands separators
// (3,570,400).
export function formatValue(value) {
    return Number(value.toPrecision(6)).toLocaleString("en-US", {maximumFractionDigits: 6});
}

// A ratio as a percentage: 2 significant digits below 1%, 1 decimal above.
export function formatPercent(ratio) {
    const percent = ratio * 100;
    if (percent === 0)
        return "0%";
    return format(Number((percent < 1)? percent.toPrecision(2): percent.toFixed(1))) + "%";
}

// The density of a solution: the items over the space used, the space of the
// bins minus the leftover (of the last bin), in the unit of the problem type
// (area, volume or length). For the open dimension objectives, the space used
// along the open dimension: 'DensityX' / 'DensityY' (only the irregular
// solutions have them). Otherwise, the space used is the items plus 'Waste'
// (the space used minus the items), or for irregular, which has no 'Waste',
// 'BinArea' minus 'LeftoverValue'. With nesting lengths (onedimensional), the
// items occupy less than their length: 'ItemNestedLength'.
function density(solution, objective) {
    const openDimensionDensity = {"open-dimension-x": solution.DensityX, "open-dimension-y": solution.DensityY}[objective];
    if (finite(openDimensionDensity) !== null)
        return openDimensionDensity;
    const items = [solution.ItemArea, solution.ItemVolume, solution.ItemLength]
        .map(finite).find((value) => value !== null);
    if (items === undefined)
        return null;
    const occupied = (finite(solution.ItemNestedLength) !== null)? solution.ItemNestedLength: items;
    let used = null;
    if (finite(solution.Waste) !== null)
        used = occupied + solution.Waste;
    else if (finite(solution.BinArea) !== null && finite(solution.LeftoverValue) !== null)
        used = solution.BinArea - solution.LeftoverValue;
    return (used !== null && used > 0)? occupied / used: null;
}

// The tiles of the key numbers of a solution ('{output, objective,
// totalItems}', 'totalItems' being missing for the solutions stored before
// it was added): '{label, value, note}' each.
export function summaryTiles(result) {
    const output = result.output;
    const solution = output.Solution;
    const tiles = [];
    const total = (result.totalItems !== undefined)? result.totalItems: null;
    tiles.push({
        label: "Items packed",
        value: (total !== null)?
            `${formatValue(solution.NumberOfItems)} / ${formatValue(total)}`: formatValue(solution.NumberOfItems),
    });
    const value = objectiveValue(output, result.objective);
    const bound = boundValue(output, result.objective);
    // Bin packing with leftovers: the number of bins first, then the leftover
    // (only for the problem types which have one).
    if (result.objective === "bin-packing-with-leftovers"
            && finite(solution.NumberOfBins) !== null && solution.NumberOfItems > 0)
        tiles.push({label: "Bins", value: formatValue(solution.NumberOfBins)});
    if (value !== null)
        tiles.push({label: OBJECTIVE_VALUES[result.objective].label, value: formatValue(value)});
    if (bound !== null)
        tiles.push({label: "Bound", value: formatValue(bound)});
    const relativeGap = gap(value, bound);
    if (relativeGap !== null) {
        tiles.push({label: "Gap", value: formatPercent(relativeGap),
            note: (relativeGap === 0)? "Optimal": undefined});
    }
    const solutionDensity = density(solution, result.objective);
    if (solutionDensity !== null && solution.NumberOfItems > 0)
        tiles.push({label: "Density", value: formatPercent(solutionDensity)});
    tiles.push({label: "Time", value: `${format(Number(output.Time.toFixed(2)))} s`});
    return tiles;
}

// The ratio between the last and the first time of the updates from which the
// time axis of the chart of the progress is logarithmic.
export const LOG_TIME_RATIO = 50;

// The chart of the progress of an optimization: the value of the solution and
// the bound over time, on one axis (they are in the same unit). 'progress' is
// the list of the updates of the solver ('{time, value, boundValue}'),
// 'result' its last solution, 'colors' the colors of the page ('{accent,
// muted, text, grid}'). 'null' if there is nothing to draw (no value for the
// objective, or a progress stored before the values were).
export function progressFigure(progress, result, colors) {
    const spec = OBJECTIVE_VALUES[result.objective];
    if (spec === undefined)
        return null;
    const end = Math.max(result.output.Time, ...progress.map((line) => line.time));
    // A step for each update: the value holds until the next one, until the
    // end. A marker where the value changes only.
    const series = (key, name, color) => {
        const lines = progress.filter((line) => finite(line[key]) !== null);
        if (lines.length === 0)
            return null;
        const x = lines.map((line) => line.time);
        const y = lines.map((line) => line[key]);
        const sizes = y.map((value, i) => (i === 0 || value !== y[i - 1])? 8: 0);
        if (x[x.length - 1] < end) {
            x.push(end);
            y.push(y[y.length - 1]);
            sizes.push(0);
        }
        return {
            type: "scatter",
            mode: "lines+markers",
            name,
            x,
            y,
            line: {color, width: 2, shape: "hv"},
            marker: {color, size: sizes},
            hovertemplate: "%{y}<extra>" + name + "</extra>",
        };
    };
    const data = [
        series("value", "Solution", colors.accent),
        // Context for the solution: recessive.
        series("boundValue", "Bound", colors.muted),
    ].filter((trace) => trace !== null);
    if (data.length === 0)
        return null;
    // Most improvements are usually found at the beginning: if the times span
    // a wide range, a logarithmic time axis shows them.
    const times = progress.map((line) => line.time).filter((time) => time > 0);
    const logTime = (times.length > 0 && end / Math.min(...times) >= LOG_TIME_RATIO);
    const axis = (title) => ({
        title: {text: title},
        gridcolor: colors.grid,
        linecolor: colors.grid,
        zeroline: false,
        automargin: true,
    });
    return {
        data,
        layout: {
            height: 260,
            // Room for the legend above the plot.
            margin: {l: 10, r: 10, t: (data.length > 1)? 40: 10, b: 10},
            paper_bgcolor: "rgba(0, 0, 0, 0)",
            plot_bgcolor: "rgba(0, 0, 0, 0)",
            font: {color: colors.text},
            xaxis: logTime?
                // Ticks on the powers of 10 only.
                {...axis("Time (s, logarithmic scale)"), type: "log", dtick: 1}:
                {...axis("Time (s)"), rangemode: "tozero"},
            yaxis: axis(spec.label),
            hovermode: "x unified",
            // A legend for the solution and the bound; a single series is named
            // by the axis.
            showlegend: data.length > 1,
            legend: {orientation: "h", x: 0, y: 1, yanchor: "bottom"},
        },
    };
}
