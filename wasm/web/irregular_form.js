// Form of the irregular problem type: bin and item types whose shapes are
// rectangles, circles, polygons or read from DXF files, converted to the
// "general" shapes of the JSON instance format (line segments and circular
// arcs).

import {readParts} from "./dxf.js";

// Shapes of a row, and the fields of their dimensions.
export const SHAPES = {
    rectangle: "Rectangle",
    circle: "Circle",
    polygon: "Polygon",
    dxf: "From DXF",
};

// Allowed rotations of an item type, in degrees.
export const ROTATIONS = {
    none: {label: "None", rotations: [{start: 0, end: 0}]},
    half: {label: "Half turns", rotations: [0, 180].map((a) => ({start: a, end: a}))},
    quarter: {label: "Quarter turns", rotations: [0, 90, 180, 270].map((a) => ({start: a, end: a}))},
    any: {label: "Any angle", rotations: [{start: 0, end: 360}]},
};

export function defaultBinRow() {
    return {shape: "rectangle", width: 100, height: 100, radius: 50, vertices: "", copies: 1, cost: ""};
}

export function defaultItemRow() {
    return {
        shape: "rectangle", width: 10, height: 10, radius: 5, vertices: "",
        copies: 1, profit: "", rotations: "none", mirror: false,
    };
}

// The example of the irregular form.
export const EXAMPLE = {
    objective: "bin-packing-with-leftovers",
    binTypes: [{shape: "rectangle", width: 100, height: 50, copies: 5}],
    itemTypes: [
        {shape: "rectangle", width: 20, height: 10, copies: 10, rotations: "quarter"},
        {shape: "circle", radius: 8, copies: 6},
        {shape: "polygon", vertices: "0 0, 20 0, 20 10, 10 10, 10 20, 0 20", copies: 5, rotations: "quarter"},
        {shape: "polygon", vertices: "0 0, 15 0, 0 15", copies: 8, rotations: "quarter"},
    ],
};

/////////////////////////////////////////////////////////////////////////////
// Shapes
/////////////////////////////////////////////////////////////////////////////

function point(x, y) {
    return {x, y};
}

function lineSegment(start, end) {
    return {type: "LineSegment", start, end};
}

// Parse vertices written as "x y, x y, ...".
export function parseVertices(text) {
    const vertices = text.split(",").map((s) => s.trim()).filter((s) => s !== "").map((s) => {
        const coordinates = s.split(/\s+/).map(Number);
        if (coordinates.length !== 2 || !coordinates.every(Number.isFinite))
            throw new Error(`invalid vertex "${s}": expected "x y".`);
        return point(coordinates[0], coordinates[1]);
    });
    if (vertices.length < 3)
        throw new Error("a polygon needs at least 3 vertices.");
    return vertices;
}

// Twice the signed area of a polygon (positive if anticlockwise).
function signedArea(vertices) {
    let area = 0;
    vertices.forEach((v, i) => {
        const w = vertices[(i + 1) % vertices.length];
        area += v.x * w.y - w.x * v.y;
    });
    return area;
}

function positiveNumber(value, name) {
    const number = Number(value);
    if (value === "" || !Number.isFinite(number) || number <= 0)
        throw new Error(`invalid ${name}: "${value}".`);
    return number;
}

// The shape of a row, as a "general" shape of the JSON format, with its
// holes if it has any.
export function rowShape(row) {
    if (row.shape === "dxf") {
        if (!row.dxf)
            throw new Error("load a DXF file.");
        const shape = {...row.dxf.shape};
        if (row.dxf.holes.length > 0)
            shape.holes = row.dxf.holes;
        return shape;
    }
    if (row.shape === "rectangle") {
        const width = positiveNumber(row.width, "width");
        const height = positiveNumber(row.height, "height");
        const corners = [point(0, 0), point(width, 0), point(width, height), point(0, height)];
        return {
            type: "general",
            elements: corners.map((c, i) => lineSegment(c, corners[(i + 1) % 4])),
        };
    }
    if (row.shape === "circle") {
        const radius = positiveNumber(row.radius, "radius");
        return {
            type: "general",
            elements: [{
                type: "CircularArc",
                start: point(radius, 0),
                end: point(radius, 0),
                center: point(0, 0),
                orientation: "Full",
            }],
        };
    }
    if (row.shape === "polygon") {
        let vertices = parseVertices(row.vertices);
        if (signedArea(vertices) === 0)
            throw new Error("the polygon has no area.");
        // Shapes are anticlockwise.
        if (signedArea(vertices) < 0)
            vertices = vertices.slice().reverse();
        return {
            type: "general",
            elements: vertices.map((v, i) => lineSegment(v, vertices[(i + 1) % vertices.length])),
        };
    }
    throw new Error(`unknown shape "${row.shape}".`);
}

/////////////////////////////////////////////////////////////////////////////
// Instance
/////////////////////////////////////////////////////////////////////////////

function optionalNumber(value, name) {
    if (value === "" || value === null || value === undefined)
        return undefined;
    const number = Number(value);
    if (!Number.isFinite(number))
        throw new Error(`invalid ${name}: "${value}".`);
    return number;
}

function copies(value) {
    const number = Number(value);
    if (!Number.isInteger(number) || number < 1)
        throw new Error(`invalid number of copies: "${value}".`);
    return number;
}

// The instance in the JSON format. Errors mention the row they come from.
export function instance(objective, binRows, itemRows) {
    if (binRows.length === 0)
        throw new Error("add at least one bin type.");
    if (itemRows.length === 0)
        throw new Error("add at least one item type.");
    const withRow = (kind, i, f) => {
        try {
            return f();
        } catch (error) {
            throw new Error(`${kind} type ${i + 1}: ${error.message}`);
        }
    };
    return {
        objective,
        bin_types: binRows.map((row, i) => withRow("bin", i, () => {
            const shape = rowShape(row);
            if (shape.holes !== undefined)
                throw new Error("a bin can't have holes.");
            const binType = {...shape, copies: copies(row.copies)};
            const cost = optionalNumber(row.cost, "cost");
            if (cost !== undefined)
                binType.cost = cost;
            return binType;
        })),
        item_types: itemRows.map((row, i) => withRow("item", i, () => {
            // An item type can have several shapes: the 'shapes' form is also
            // the one 'instanceFigure' reads.
            const itemType = {
                shapes: [rowShape(row)],
                copies: copies(row.copies),
                allowed_rotations: ROTATIONS[row.rotations].rotations,
            };
            const profit = optionalNumber(row.profit, "profit");
            if (profit !== undefined)
                itemType.profit = profit;
            if (row.mirror)
                itemType.allow_mirroring = true;
            return itemType;
        })),
    };
}

/////////////////////////////////////////////////////////////////////////////
// Tables
/////////////////////////////////////////////////////////////////////////////

function input(row, key, label, attributes, onChange) {
    const element = document.createElement("input");
    element.setAttribute("aria-label", label);
    Object.assign(element, attributes);
    if (element.type === "checkbox") {
        element.checked = Boolean(row[key]);
        element.addEventListener("change", () => { row[key] = element.checked; onChange(); });
    } else {
        element.value = row[key];
        element.addEventListener("input", () => { row[key] = element.value; onChange(); });
    }
    return element;
}

function select(row, key, label, options, onChange) {
    const element = document.createElement("select");
    element.setAttribute("aria-label", label);
    for (const [value, text] of Object.entries(options)) {
        const option = document.createElement("option");
        option.value = value;
        option.textContent = text;
        element.appendChild(option);
    }
    element.value = row[key];
    element.addEventListener("change", () => { row[key] = element.value; onChange(); });
    return element;
}

const NUMBER = {type: "number", min: "0", step: "any"};

// The inputs of the dimensions of a row, which depend on its shape.
function dimensionsCell(row, onChange, onStructureChange) {
    const cell = document.createElement("div");
    cell.className = "dimensions";
    const labelled = (text, element) => {
        const label = document.createElement("label");
        label.className = "inline";
        label.append(text, element);
        return label;
    };
    if (row.shape === "rectangle") {
        cell.append(
            labelled("W", input(row, "width", "Width", NUMBER, onChange)),
            labelled("H", input(row, "height", "Height", NUMBER, onChange)));
    } else if (row.shape === "circle") {
        cell.append(labelled("R", input(row, "radius", "Radius", NUMBER, onChange)));
    } else if (row.shape === "polygon") {
        const vertices = input(row, "vertices", "Vertices", {
            type: "text",
            placeholder: "x y, x y, x y, ...",
            className: "vertices",
            spellcheck: false,
        }, onChange);
        cell.append(vertices);
    } else if (row.shape === "dxf") {
        cell.append(dxfInput(row, onStructureChange), dxfDescription(row));
    }
    return cell;
}

function format(value) {
    return String(Number(value.toPrecision(6)));
}

// Description of the shape of a row loaded from a DXF file.
function dxfDescription(row) {
    const description = document.createElement("span");
    description.className = "dxf-description";
    if (row.dxfError) {
        description.classList.add("error-text");
        description.textContent = row.dxfError;
    } else if (row.dxf) {
        const holes = row.dxf.holes.length;
        description.textContent = `${row.dxf.name}: ${format(row.dxf.width)} × ${format(row.dxf.height)}`
            + ((holes > 0)? `, ${holes} hole${(holes > 1)? "s": ""}`: "")
            + ((row.dxf.warnings.length > 0)? ` (${row.dxf.warnings.join("; ")})`: "");
    } else {
        description.textContent = "Choose a DXF file containing one part.";
    }
    return description;
}

// Read the parts of a DXF file: '{parts, units, warnings}' (see
// 'readParts'), each part named after the file.
export async function readDxfFile(file) {
    const result = readParts(await file.text());
    const name = file.name.replace(/\.dxf$/i, "");
    result.parts.forEach((part, i) => {
        part.name = (result.parts.length > 1)? `${name} (${i + 1})`: name;
        part.warnings = [];
    });
    return result;
}

// A button choosing the DXF file of a row (a file input can't show the file
// of a row once the table is rendered again).
function dxfInput(row, onStructureChange) {
    const container = document.createElement("span");
    const button = document.createElement("button");
    button.type = "button";
    button.textContent = row.dxf? "Change file": "Choose a DXF file";
    const element = document.createElement("input");
    element.type = "file";
    element.accept = ".dxf";
    element.hidden = true;
    element.setAttribute("aria-label", "DXF file");
    button.addEventListener("click", () => element.click());
    container.append(button, element);
    element.addEventListener("change", async () => {
        const file = element.files[0];
        if (file === undefined)
            return;
        try {
            const {parts, warnings} = await readDxfFile(file);
            if (parts.length === 0)
                throw new Error(`no closed contour found in ${file.name}.`);
            if (parts.length > 1) {
                throw new Error(`${file.name} contains ${parts.length} parts: `
                    + `use "Load item types from a DXF file" to load them all.`);
            }
            // The warnings of a file loaded in a row are shown in the row.
            row.dxf = {...parts[0], warnings};
            row.dxfError = null;
        } catch (error) {
            row.dxf = null;
            row.dxfError = error.message;
        }
        onStructureChange();
    });
    return container;
}

// Render a bin ('isItem' false) or item type table. 'onChange' is called
// when a value changes, 'onStructureChange' when the table must be rendered
// again (shape changed, row removed).
export function renderTable(table, rows, isItem, onChange, onStructureChange) {
    table.replaceChildren();
    const header = table.createTHead().insertRow();
    const labels = ["Shape", "Dimensions", "Copies", isItem? "Profit": "Cost"];
    if (isItem)
        labels.push("Rotations", "Mirror");
    for (const label of [...labels, ""]) {
        const th = document.createElement("th");
        th.textContent = label;
        header.appendChild(th);
    }
    const body = table.createTBody();
    rows.forEach((row, rowIndex) => {
        const tr = body.insertRow();
        tr.insertCell().appendChild(select(row, "shape", "Shape", SHAPES, onStructureChange));
        tr.insertCell().appendChild(dimensionsCell(row, onChange, onStructureChange));
        tr.insertCell().appendChild(input(row, "copies", "Copies", {...NUMBER, min: "1", step: "1"}, onChange));
        const valueKey = isItem? "profit": "cost";
        tr.insertCell().appendChild(input(row, valueKey, isItem? "Profit": "Cost",
            {...NUMBER, placeholder: "default"}, onChange));
        if (isItem) {
            const rotationLabels = Object.fromEntries(
                Object.entries(ROTATIONS).map(([key, value]) => [key, value.label]));
            tr.insertCell().appendChild(select(row, "rotations", "Rotations", rotationLabels, onChange));
            tr.insertCell().appendChild(input(row, "mirror", "Mirror", {type: "checkbox"}, onChange));
        }
        const remove = document.createElement("button");
        remove.type = "button";
        remove.textContent = "Remove";
        remove.addEventListener("click", () => {
            rows.splice(rowIndex, 1);
            onStructureChange();
        });
        tr.insertCell().appendChild(remove);
    });
}

/////////////////////////////////////////////////////////////////////////////
// Preview
/////////////////////////////////////////////////////////////////////////////

// Points of a shape's outline: arcs are approximated by segments.
export function shapePoints(elements) {
    const points = [];
    for (const element of elements) {
        if (element.type !== "CircularArc") {
            points.push([element.start.x, element.start.y], [element.end.x, element.end.y]);
            continue;
        }
        const {start, end, center} = element;
        const radius = Math.hypot(start.x - center.x, start.y - center.y);
        const startAngle = Math.atan2(start.y - center.y, start.x - center.x);
        let sweep;
        if (element.orientation === "Full") {
            sweep = 2 * Math.PI;
        } else {
            sweep = Math.atan2(end.y - center.y, end.x - center.x) - startAngle;
            if (element.orientation === "Anticlockwise" && sweep <= 0)
                sweep += 2 * Math.PI;
            if (element.orientation === "Clockwise" && sweep >= 0)
                sweep -= 2 * Math.PI;
        }
        const steps = Math.max(2, Math.ceil(Math.abs(sweep) / (Math.PI / 32)));
        for (let i = 0; i <= steps; ++i) {
            const angle = startAngle + sweep * i / steps;
            points.push([center.x + radius * Math.cos(angle), center.y + radius * Math.sin(angle)]);
        }
    }
    return points;
}

// Number of columns of the preview grid.
const PREVIEW_COLUMNS = 4;

// Height of a row of the preview grid, and gap between two rows (for the
// tick labels and the titles), in pixels.
const PREVIEW_ROW_HEIGHT = 260;
const PREVIEW_ROW_GAP = 70;

// Figure ('{data, layout}' for 'Plotly.react') drawing each bin and item type
// of an instance built by 'instance', in its own cell.
export function previewFigure(instanceObject) {
    const cells = [
        ...instanceObject.bin_types.map((t, i) => ({
            title: `Bin type ${i + 1} (×${t.copies})`, shapes: [t], color: "#e8e8e8"})),
        ...instanceObject.item_types.map((t, i) => ({
            title: `Item type ${i + 1} (×${t.copies})`, shapes: t.shapes, color: "#636efa"})),
    ];
    const columns = Math.min(PREVIEW_COLUMNS, cells.length);
    const rows = Math.ceil(cells.length / columns);
    const gap = 0.04;
    const width = (1 - gap * (columns - 1)) / columns;
    const plotHeight = PREVIEW_ROW_HEIGHT * rows + PREVIEW_ROW_GAP * (rows - 1);
    const rowGap = PREVIEW_ROW_GAP / plotHeight;
    const height = PREVIEW_ROW_HEIGHT / plotHeight;
    const data = [];
    const layout = {
        showlegend: false,
        height: plotHeight + 60,
        margin: {l: 40, r: 10, t: 30, b: 30},
        annotations: [],
    };
    cells.forEach((cell, i) => {
        const row = Math.floor(i / columns);
        const column = i % columns;
        const suffix = (i === 0)? "": String(i + 1);
        const x = [column * (width + gap), column * (width + gap) + width];
        const y = [1 - row * (height + rowGap) - height, 1 - row * (height + rowGap)];
        layout["xaxis" + suffix] = {domain: x, anchor: "y" + suffix, zeroline: false};
        layout["yaxis" + suffix] = {
            domain: y, anchor: "x" + suffix, zeroline: false,
            scaleanchor: "x" + suffix, scaleratio: 1,
        };
        layout.annotations.push({
            text: cell.title, showarrow: false,
            x: (x[0] + x[1]) / 2, y: y[1], xref: "paper", yref: "paper",
            xanchor: "center", yanchor: "bottom",
        });
        for (const shape of cell.shapes) {
            // The outline, then its holes drawn over it in the color of the
            // background.
            const loops = [
                [shape.elements, cell.color],
                ...(shape.holes || []).map((h) => [h.elements, "white"]),
            ];
            for (const [loop, color] of loops) {
                const points = shapePoints(loop);
                data.push({
                    type: "scatter",
                    x: points.map((p) => p[0]),
                    y: points.map((p) => p[1]),
                    mode: "lines",
                    fill: "toself", fillcolor: color,
                    line: {color: "black", width: 1},
                    xaxis: "x" + suffix, yaxis: "y" + suffix,
                    hoverinfo: "x+y",
                });
            }
        }
    });
    return {data, layout};
}
