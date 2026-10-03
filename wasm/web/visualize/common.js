// Shared by the visualizers: JavaScript ports of the Python modules in
// 'python/packingsolver/visualize/'. Each visualizer exports
// 'figure(certificate, options)', which returns '{data, layout}' for
// 'Plotly.newPlot', equal to the 'to_plotly_json()' of the Python figure
// (without its template).

// 'plotly.express.colors.qualitative.Plotly'.
export const PLOTLY_COLORS = [
    "#636EFA", "#EF553B", "#00CC96", "#AB63FA", "#FFA15A",
    "#19D3F3", "#FF6692", "#B6E880", "#FF97FF", "#FECB52",
];

// 'plotly.express.colors.qualitative.Pastel'.
export const PASTEL_COLORS = [
    "rgb(102, 197, 204)", "rgb(246, 207, 113)", "rgb(248, 156, 116)",
    "rgb(220, 176, 242)", "rgb(135, 197, 95)", "rgb(158, 185, 243)",
    "rgb(254, 136, 177)", "rgb(201, 219, 116)", "rgb(139, 224, 164)",
    "rgb(180, 151, 231)", "rgb(179, 179, 179)",
];

// Parse a CSV text with a header line, like Python's 'csv.DictReader': an
// array of objects mapping the column names to the (string) values. The
// certificates have no quoted fields.
export function parseCsv(text) {
    const lines = text.split(/\r?\n/).filter((line) => line !== "");
    if (lines.length === 0)
        return [];
    const header = lines[0].split(",");
    return lines.slice(1).map((line) => {
        const values = line.split(",");
        const row = {};
        header.forEach((name, i) => {
            row[name] = (i < values.length)? values[i]: null;
        });
        return row;
    });
}

// Python's 'sum' of the first 'n' values, in the same order (for identical
// floating-point results).
function sumFirst(values, n) {
    let sum = 0;
    for (let i = 0; i < n; ++i)
        sum += values[i];
    return sum;
}

// Layout of 'plotly.subplots.make_subplots(rows=rows, cols=cols,
// shared_xaxes=True, vertical_spacing=verticalSpacing, specs=...)' with the
// default horizontal spacing, for the grids used by the visualizers: every
// cell of type 'xy' ('type' undefined) or 'scene' ('type' "scene", e.g. for
// 'mesh3d' traces).
//
// Returns '{layout, subplot(row, col)}': 'subplot' returns the properties to
// add to a trace to place it in the cell ('row' and 'col' start at 1), e.g.
// '{xaxis: "x2", yaxis: "y2"}' or '{scene: "scene2"}'.
export function makeSubplots({rows, cols, verticalSpacing = 0.001, type = "xy"}) {
    const horizontalSpacing = 0.2 / cols;
    const widths = new Array(cols).fill((1 - horizontalSpacing * (cols - 1)) / cols);
    const heights = new Array(rows).fill((1 - verticalSpacing * (rows - 1)) / rows);

    // Rows go from top to bottom: the grid is built from the bottom row.
    const grid = [];
    for (let r = rows - 1; r >= 0; --r) {
        const gridRow = [];
        for (let c = 0; c < cols; ++c) {
            gridRow.push([
                sumFirst(widths, c) + c * horizontalSpacing,
                sumFirst(heights, r) + r * verticalSpacing,
            ]);
        }
        grid.push(gridRow);
    }

    const layout = {};
    const refs = [];
    let subplotId = 0;
    for (let r = 0; r < rows; ++r) {
        const refRow = [];
        for (let c = 0; c < cols; ++c) {
            ++subplotId;
            const suffix = (subplotId === 1)? "": String(subplotId);
            const xDomain = [grid[r][c][0], grid[r][c][0] + widths[c]];
            let yStart = grid[r][c][1];
            let yEnd = grid[r][c][1] + heights[rows - 1 - r];
            if (yStart < 0 && yStart > -0.01)
                yStart = 0;
            if (yEnd > 1 && yEnd < 1.01)
                yEnd = 1;
            if (type === "scene") {
                layout["scene" + suffix] = {domain: {x: xDomain, y: [yStart, yEnd]}};
                refRow.push({scene: "scene" + suffix});
            } else {
                layout["xaxis" + suffix] = {anchor: "y" + suffix, domain: xDomain};
                layout["yaxis" + suffix] = {anchor: "x" + suffix, domain: [yStart, yEnd]};
                refRow.push({xaxis: "x" + suffix, yaxis: "y" + suffix});
            }
        }
        refs.push(refRow);
    }

    // Shared x axes: each column follows the x axis of its bottom cell, whose
    // tick labels are the only ones shown.
    if (type !== "scene") {
        for (let c = 0; c < cols; ++c) {
            const bottom = refs[rows - 1][c].xaxis;
            for (let r = 0; r < rows - 1; ++r) {
                const axis = layout["xaxis" + refs[r][c].xaxis.slice(1)];
                axis.matches = bottom;
                axis.showticklabels = false;
            }
        }
    }

    return {
        layout,
        subplot: (row, col) => ({...refs[row - 1][col - 1]}),
    };
}
