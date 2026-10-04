// Form of the irregular problem type: bin and item types whose shapes are
// rectangles, circles, polygons or read from DXF or SVG files, converted to
// the
// "general" shapes of the JSON instance format (line segments and circular
// arcs).

import * as dxf from "./dxf.js";
import * as svg from "./svg.js";

// Shapes of a row, and the fields of their dimensions.
export const SHAPES = {
    rectangle: "Rectangle",
    circle: "Circle",
    polygon: "Polygon",
    file: "From file (DXF, SVG)",
};

// Allowed rotations of an item type, in degrees.
export const ROTATIONS = {
    none: {label: "None", rotations: [{start: 0, end: 0}]},
    half: {label: "Half turns", rotations: [0, 180].map((a) => ({start: a, end: a}))},
    quarter: {label: "Quarter turns", rotations: [0, 90, 180, 270].map((a) => ({start: a, end: a}))},
    any: {label: "Any angle", rotations: [{start: 0, end: 360}]},
};

export function defaultBinRow() {
    return {
        shape: "rectangle", width: 100, height: 100, radius: 50, vertices: "",
        copies: 1, cost: "", spacing: "", defects: [],
    };
}

// A defect: a rectangle placed at (x, y), a circle centered at (x, y), or a
// polygon given by its vertices in the bin.
export function defaultDefect() {
    return {shape: "rectangle", x: "", y: "", width: 10, height: 10, radius: 5, vertices: "", spacing: ""};
}

// A hole of an item: placed in the item as a defect in a bin.
export function defaultHole() {
    return {shape: "rectangle", x: "", y: "", width: 5, height: 5, radius: 2, vertices: ""};
}

// Shapes of the defects and of the holes.
const PLACED_SHAPES = {
    rectangle: "Rectangle",
    circle: "Circle",
    polygon: "Polygon",
};

export function defaultItemRow() {
    return {
        shape: "rectangle", width: 10, height: 10, radius: 5, vertices: "",
        copies: 1, profit: "", rotations: "none", mirror: false, holes: [],
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
    if (row.shape === "file") {
        if (!row.file)
            throw new Error("load a DXF or SVG file.");
        const shape = {...row.file.shape};
        if (row.file.holes.length > 0)
            shape.holes = row.file.holes;
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

function translateShape(shape, dx, dy) {
    const move = (p) => point(p.x + dx, p.y + dy);
    return {
        ...shape,
        elements: shape.elements.map((e) => {
            const moved = {...e, start: move(e.start), end: move(e.end)};
            if (e.center !== undefined)
                moved.center = move(e.center);
            return moved;
        }),
    };
}

// The shape of a defect in its bin, or of a hole in its item.
export function placedShape(defect) {
    const shape = rowShape(defect);
    if (defect.shape === "polygon")
        return shape;
    const position = (value, name) => {
        const number = Number(value);
        if (value === "" || value === undefined || !Number.isFinite(number))
            throw new Error(`invalid ${name}: "${value}".`);
        return number;
    };
    return translateShape(shape, position(defect.x, "x"), position(defect.y, "y"));
}

// Whether a point is strictly inside a polygon given by its vertices (as
// '[x, y]').
function strictlyInside(polygon, [x, y]) {
    let inside = false;
    for (let i = 0, j = polygon.length - 1; i < polygon.length; j = i++) {
        const [xi, yi] = polygon[i];
        const [xj, yj] = polygon[j];
        // On the edge.
        const cross = (xj - xi) * (y - yi) - (yj - yi) * (x - xi);
        if (Math.abs(cross) < 1e-9 * Math.max(1, Math.hypot(xj - xi, yj - yi))
                && x >= Math.min(xi, xj) && x <= Math.max(xi, xj)
                && y >= Math.min(yi, yj) && y <= Math.max(yi, yj)) {
            return false;
        }
        if ((yi > y) !== (yj > y) && x < (xj - xi) * (y - yi) / (yj - yi) + xi)
            inside = !inside;
    }
    return inside;
}

// The shape of an item row with its holes: the holes of its file, then the
// holes typed in the form, which must be inside the item.
export function itemShape(row) {
    const shape = rowShape(row);
    if ((row.holes || []).length === 0)
        return shape;
    const outline = shapePoints(shape.elements);
    const holes = row.holes.map((hole, j) => {
        try {
            const result = placedShape(hole);
            if (!shapePoints(result.elements).every((p) => strictlyInside(outline, p)))
                throw new Error("it must be inside the item.");
            return result;
        } catch (error) {
            throw new Error(`hole ${j + 1}: ${error.message}`);
        }
    });
    return {...shape, holes: [...(shape.holes || []), ...holes]};
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
            const cost = valueUsed(objective, false)? optionalNumber(row.cost, "cost"): undefined;
            if (cost !== undefined)
                binType.cost = cost;
            const spacing = optionalNumber(row.spacing, "item spacing");
            if (spacing !== undefined)
                binType.item_bin_minimum_spacing = spacing;
            if ((row.defects || []).length > 0) {
                binType.defects = row.defects.map((defect, j) => {
                    try {
                        const result = placedShape(defect);
                        const defectSpacing = optionalNumber(defect.spacing, "spacing");
                        if (defectSpacing !== undefined)
                            result.item_defect_minimum_spacing = defectSpacing;
                        return result;
                    } catch (error) {
                        throw new Error(`defect ${j + 1}: ${error.message}`);
                    }
                });
            }
            return binType;
        })),
        item_types: itemRows.map((row, i) => withRow("item", i, () => {
            // An item type can have several shapes: the 'shapes' form is also
            // the one 'instanceFigure' reads.
            const itemType = {
                shapes: [itemShape(row)],
                copies: copies(row.copies),
                allowed_rotations: ROTATIONS[row.rotations].rotations,
            };
            const profit = valueUsed(objective, true)? optionalNumber(row.profit, "profit"): undefined;
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
    } else if (row.shape === "file") {
        cell.append(fileInput(row, onStructureChange), fileDescription(row));
    }
    return cell;
}

function format(value) {
    return String(Number(value.toPrecision(6)));
}

// Description of the shape of a row loaded from a file.
function fileDescription(row) {
    const description = document.createElement("span");
    description.className = "file-description";
    if (row.fileError) {
        description.classList.add("error-text");
        description.textContent = row.fileError;
    } else if (row.file) {
        const holes = row.file.holes.length;
        description.textContent = `${row.file.name}: ${format(row.file.width)} × ${format(row.file.height)}`
            + ((holes > 0)? `, ${holes} hole${(holes > 1)? "s": ""}`: "")
            + ((row.file.warnings.length > 0)? ` (${row.file.warnings.join("; ")})`: "");
    } else {
        description.textContent = "Choose a DXF or SVG file containing one part.";
    }
    return description;
}

// Read the parts of a DXF or SVG file: '{parts, units, warnings}' (see
// 'readParts' of 'dxf.js' and 'svg.js'), each part named after the file.
export async function readShapeFile(file) {
    const extension = (/\.([^.]*)$/.exec(file.name) || ["", ""])[1].toLowerCase();
    const readers = {dxf: dxf.readParts, svg: svg.readParts};
    if (!(extension in readers))
        throw new Error(`${file.name}: only DXF and SVG files are supported.`);
    const result = readers[extension](await file.text());
    const name = file.name.replace(/\.[^.]*$/, "");
    result.parts.forEach((part, i) => {
        part.name = (result.parts.length > 1)? `${name} (${i + 1})`: name;
        part.warnings = [];
    });
    return result;
}

// A button choosing the file of a row (a file input can't show the file
// of a row once the table is rendered again).
function fileInput(row, onStructureChange) {
    const container = document.createElement("span");
    const button = document.createElement("button");
    button.type = "button";
    button.textContent = row.file? "Change file": "Choose a file";
    const element = document.createElement("input");
    element.type = "file";
    element.accept = ".dxf,.svg";
    element.hidden = true;
    element.setAttribute("aria-label", "DXF or SVG file");
    button.addEventListener("click", () => element.click());
    container.append(button, element);
    element.addEventListener("change", async () => {
        const file = element.files[0];
        if (file === undefined)
            return;
        try {
            const {parts, warnings} = await readShapeFile(file);
            if (parts.length === 0)
                throw new Error(`no closed contour found in ${file.name}.`);
            if (parts.length > 1) {
                throw new Error(`${file.name} contains ${parts.length} parts: `
                    + `use "Load item types from a file" to load them all.`);
            }
            // The warnings of a file loaded in a row are shown in the row.
            row.file = {...parts[0], warnings};
            row.fileError = null;
        } catch (error) {
            row.file = null;
            row.fileError = error.message;
        }
        onStructureChange();
    });
    return container;
}

// The defects of a bin row or the holes of an item row ('key' "defects"
// or "holes", 'kind' "Defect" or "Hole"): a line for each. Defects have a
// spacing.
function placedShapesCell(row, key, kind, onChange, onStructureChange) {
    const cell = document.createElement("div");
    cell.className = "placed-shapes";
    const labelled = (text, element) => {
        const label = document.createElement("label");
        label.className = "inline";
        label.append(text, element);
        return label;
    };
    row[key].forEach((placed, i) => {
        const line = document.createElement("div");
        line.className = "dimensions";
        line.appendChild(select(placed, "shape", `${kind} shape`, PLACED_SHAPES, onStructureChange));
        if (placed.shape !== "polygon") {
            line.append(
                labelled("X", input(placed, "x", `${kind} x`, NUMBER, onChange)),
                labelled("Y", input(placed, "y", `${kind} y`, NUMBER, onChange)));
        }
        if (placed.shape === "rectangle") {
            line.append(
                labelled("W", input(placed, "width", `${kind} width`, NUMBER, onChange)),
                labelled("H", input(placed, "height", `${kind} height`, NUMBER, onChange)));
        } else if (placed.shape === "circle") {
            line.append(labelled("R", input(placed, "radius", `${kind} radius`, NUMBER, onChange)));
        } else {
            line.appendChild(input(placed, "vertices", `${kind} vertices`, {
                type: "text",
                placeholder: "x y, x y, x y, ...",
                className: "vertices",
                spellcheck: false,
            }, onChange));
        }
        if (key === "defects") {
            line.appendChild(labelled("Spacing",
                input(placed, "spacing", "Defect spacing", {...NUMBER, placeholder: "0"}, onChange)));
        }
        const remove = document.createElement("button");
        remove.type = "button";
        remove.textContent = "×";
        remove.title = `Remove the ${kind.toLowerCase()}`;
        remove.addEventListener("click", () => {
            row[key].splice(i, 1);
            onStructureChange();
        });
        line.appendChild(remove);
        cell.appendChild(line);
    });
    return cell;
}

// A button adding a defect or a hole to a row.
function addPlacedShapeButton(row, key, kind, defaultValue, onStructureChange) {
    const add = document.createElement("button");
    add.type = "button";
    add.textContent = `Add a ${kind.toLowerCase()}`;
    add.addEventListener("click", () => {
        row[key].push(defaultValue());
        onStructureChange();
    });
    return add;
}

// Whether the profit of the items (cost of the bins) is used for an
// objective: only for the knapsack (variable-sized bin packing) objective.
export function valueUsed(objective, isItem) {
    return objective === (isItem? "knapsack": "variable-sized-bin-packing");
}

// Render a bin ('isItem' false) or item type table, for an objective.
// 'onTableChange' is called when a value changes, 'onStructureChange' when
// the table must be rendered again (shape changed, row removed).
export function renderTable(table, rows, isItem, objective, onTableChange, onStructureChange) {
    table.replaceChildren();
    const header = table.createTHead().insertRow();
    const withValue = valueUsed(objective, isItem);
    const labels = ["", "Shape", "Dimensions", "Copies"];
    if (withValue)
        labels.push(isItem? "Profit": "Cost");
    if (isItem)
        labels.push("Rotations", "Mirror");
    else
        labels.push("Item spacing");
    for (const label of [...labels, ""]) {
        const th = document.createElement("th");
        th.textContent = label;
        header.appendChild(th);
    }
    const body = table.createTBody();
    rows.forEach((row, rowIndex) => {
        const tr = body.insertRow();
        // The thumbnail of the row, drawn again when one of its values
        // changes.
        const thumbnail = document.createElementNS(SVG_NAMESPACE, "svg");
        thumbnail.setAttribute("class", "thumbnail");
        thumbnail.setAttribute("role", "img");
        drawThumbnail(thumbnail, row, isItem);
        const onChange = () => {
            drawThumbnail(thumbnail, row, isItem);
            onTableChange();
        };
        tr.insertCell().appendChild(thumbnail);
        tr.insertCell().appendChild(select(row, "shape", "Shape", SHAPES, onStructureChange));
        tr.insertCell().appendChild(dimensionsCell(row, onChange, onStructureChange));
        tr.insertCell().appendChild(input(row, "copies", "Copies", {...NUMBER, min: "1", step: "1"}, onChange));
        if (withValue) {
            const valueKey = isItem? "profit": "cost";
            tr.insertCell().appendChild(input(row, valueKey, isItem? "Profit": "Cost",
                {...NUMBER, placeholder: "default"}, onChange));
        }
        if (isItem) {
            const rotationLabels = Object.fromEntries(
                Object.entries(ROTATIONS).map(([key, value]) => [key, value.label]));
            tr.insertCell().appendChild(select(row, "rotations", "Rotations", rotationLabels, onChange));
            tr.insertCell().appendChild(input(row, "mirror", "Mirror", {type: "checkbox"}, onChange));
        } else {
            tr.insertCell().appendChild(input(row, "spacing", "Item spacing",
                {...NUMBER, placeholder: "0"}, onChange));
        }
        const remove = document.createElement("button");
        remove.type = "button";
        remove.textContent = "Remove";
        remove.addEventListener("click", () => {
            rows.splice(rowIndex, 1);
            onStructureChange();
        });
        const buttons = tr.insertCell();
        buttons.className = "row-buttons";
        // The defects of a bin, or the holes of an item, on a line below it
        // if it has some; the button to add one is on its row.
        const [key, kind, defaultValue] = isItem?
            ["holes", "Hole", defaultHole]: ["defects", "Defect", defaultDefect];
        row[key] = row[key] || [];
        buttons.appendChild(addPlacedShapeButton(row, key, kind, defaultValue, onStructureChange));
        buttons.appendChild(remove);
        if (row[key].length > 0) {
            const line = body.insertRow();
            line.className = "placed-shapes-line";
            const cell = line.insertCell();
            cell.colSpan = labels.length + 1;
            const label = document.createElement("span");
            label.className = "placed-shapes-label";
            label.textContent = isItem? "Holes": "Defects";
            cell.append(label, placedShapesCell(row, key, kind, onChange, onStructureChange));
        }
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

// Fill colors of the thumbnails.
const BIN_COLOR = "#e8e8e8";
const ITEM_COLOR = "#636efa";
const DEFECT_COLOR = "#ef553b";

function pathData(elements) {
    return "M" + shapePoints(elements).map(([x, y]) => `${x} ${y}`).join(" L") + " Z";
}

// The thumbnail of a row: '{viewBox, paths, title}' where each path is
// '{d, fill}' (its holes in the same path, filled with the even-odd rule),
// or '{error}' if the row is invalid. The y axis points up, as in the
// solutions.
export function rowThumbnail(row, isItem) {
    let shape;
    let defects = [];
    try {
        shape = isItem? itemShape(row): rowShape(row);
        if (!isItem)
            defects = (row.defects || []).map(placedShape);
    } catch (error) {
        return {error: error.message};
    }
    const outline = shapePoints(shape.elements);
    const xs = outline.map((p) => p[0]);
    const ys = outline.map((p) => p[1]);
    const xMin = Math.min(...xs);
    const xMax = Math.max(...xs);
    const yMin = Math.min(...ys);
    const yMax = Math.max(...ys);
    const margin = 0.03 * Math.max(xMax - xMin, yMax - yMin);
    const d = [shape, ...(shape.holes || [])].map((s) => pathData(s.elements)).join(" ");
    return {
        // In the coordinates flipped vertically.
        viewBox: [xMin - margin, -yMax - margin, xMax - xMin + 2 * margin, yMax - yMin + 2 * margin],
        paths: [
            {d, fill: isItem? ITEM_COLOR: BIN_COLOR},
            ...defects.map((defect) => ({d: pathData(defect.elements), fill: DEFECT_COLOR})),
        ],
        title: `${format(xMax - xMin)} × ${format(yMax - yMin)}`,
    };
}

const SVG_NAMESPACE = "http://www.w3.org/2000/svg";

// Draw the thumbnail of a row in an 'svg' element.
function drawThumbnail(element, row, isItem) {
    const thumbnail = rowThumbnail(row, isItem);
    element.replaceChildren();
    const title = document.createElementNS(SVG_NAMESPACE, "title");
    title.textContent = thumbnail.error || thumbnail.title;
    element.appendChild(title);
    element.classList.toggle("invalid", thumbnail.error !== undefined);
    if (thumbnail.error !== undefined) {
        element.removeAttribute("viewBox");
        return;
    }
    element.setAttribute("viewBox", thumbnail.viewBox.join(" "));
    const group = document.createElementNS(SVG_NAMESPACE, "g");
    group.setAttribute("transform", "scale(1 -1)");
    for (const {d, fill} of thumbnail.paths) {
        const path = document.createElementNS(SVG_NAMESPACE, "path");
        path.setAttribute("d", d);
        path.setAttribute("fill", fill);
        path.setAttribute("fill-rule", "evenodd");
        path.setAttribute("stroke", "black");
        path.setAttribute("stroke-width", "1");
        path.setAttribute("vector-effect", "non-scaling-stroke");
        group.appendChild(path);
    }
    element.appendChild(group);
}
