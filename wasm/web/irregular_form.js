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
    general: "General shape",
    file: "From file (DXF, SVG)",
};

// Allowed rotations of an item type, in degrees.
export const ROTATIONS = {
    none: {label: "None", rotations: [{start: 0, end: 0}]},
    half: {label: "Half turns", rotations: [0, 180].map((a) => ({start: a, end: a}))},
    quarter: {label: "Quarter turns", rotations: [0, 90, 180, 270].map((a) => ({start: a, end: a}))},
    any: {label: "Any angle", rotations: [{start: 0, end: 360}]},
    // The ranges of the row ('rotation_ranges').
    custom: {label: "Custom", rotations: null},
};

// A range of allowed angles of an item type, in degrees: a single angle if
// 'start' and 'end' are equal; the item mirrored for this range if 'mirror'.
export function defaultRotationRange() {
    return {start: "0", end: "0", mirror: false};
}

// Another shape of an item type, placed in the item as a hole.
export function defaultExtraShape() {
    return {shape: "rectangle", x: "", y: "", width: 10, height: 10, radius: 5, vertices: "", elements: "",
        holes: []};
}

export function defaultBinRow() {
    return {
        shape: "rectangle", width: 100, height: 100, radius: 50, vertices: "", elements: "",
        copies: 1, unlimited_copies: false, copies_min: "", cost: "", spacing: "", defects: [], fixed_items: [],
    };
}

// A defect: a rectangle placed at (x, y), a circle centered at (x, y), or a
// polygon given by its vertices in the bin.
export function defaultDefect() {
    return {shape: "rectangle", x: "", y: "", width: 10, height: 10, radius: 5, vertices: "", elements: "",
        spacing: "", defect_type: "", holes: []};
}

// A fixed item of a bin: an item placed before the search. It refers to the
// row of its item type ('itemRow', null if its id is invalid), so that it
// follows the row when other rows are removed; 'bl_corner' is the position
// of the item once mirrored (if 'mirror') then rotated by 'angle' degrees.
export function defaultFixedItem(itemRow) {
    return {itemRow, x: "", y: "", angle: "", mirror: false};
}

// The fixed items of a bin row whose item type still exists (or whose id is
// invalid).
function fixedItems(row, itemRows) {
    return (row.fixed_items || []).filter((f) => f.itemRow === null || itemRows.includes(f.itemRow));
}

// The points of a fixed item: mirrored (x -> -x) if 'mirror', rotated by
// 'angle' degrees, then moved to its position.
function fixedItemPoints(elements, fixedItem) {
    const angle = (Number(fixedItem.angle) || 0) * Math.PI / 180;
    const cos = Math.cos(angle);
    const sin = Math.sin(angle);
    return shapePoints(elements).map(([x, y]) => {
        const mx = fixedItem.mirror? -x: x;
        return [cos * mx - sin * y + Number(fixedItem.x), sin * mx + cos * y + Number(fixedItem.y)];
    });
}

// A hole of an item: placed in the item as a defect in a bin.
export function defaultHole() {
    return {shape: "rectangle", x: "", y: "", width: 5, height: 5, radius: 2, vertices: "", elements: ""};
}

// Shapes of the defects and of the holes.
const PLACED_SHAPES = {
    rectangle: "Rectangle",
    circle: "Circle",
    polygon: "Polygon",
    general: "General shape",
};

export function defaultItemRow() {
    return {
        shape: "rectangle", width: 10, height: 10, radius: 5, vertices: "", elements: "",
        copies: 1, unlimited_copies: false, copies_min: "", profit: "", rotations: "none", mirror: false, holes: [],
        rotation_ranges: [defaultRotationRange()], extra_shapes: [],
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

// The orientations of the arcs of a general shape, and their letters.
const ORIENTATIONS = {a: "Anticlockwise", c: "Clockwise", f: "Full"};
const ORIENTATION_LETTERS = {Anticlockwise: "a", Clockwise: "c", Full: "f"};

// Placeholder of the inputs of the general shapes.
export const GENERAL_SHAPE_PLACEHOLDER =
    "L x_start y_start x_end y_end, A x_start y_start x_center y_center x_end y_end a|c|f, ...";

function close(a, b) {
    return Math.abs(a - b) <= 1e-9 * Math.max(1, Math.abs(a), Math.abs(b));
}

// Parse the elements of a general shape, written as
// "L x_start y_start x_end y_end" (line segment) and
// "A x_start y_start x_center y_center x_end y_end o" (circular arc, 'o' its
// orientation: 'a' anticlockwise, 'c' clockwise, 'f' full circle), separated
// by commas. Each element must start where the previous one ends, and the
// shape must be closed.
export function parseGeneralShape(text) {
    const parts = text.split(",").map((s) => s.trim()).filter((s) => s !== "");
    if (parts.length === 0)
        throw new Error("a general shape needs at least one element.");
    const elements = parts.map((part, i) => {
        const tokens = part.split(/\s+/);
        const numbers = (values) => values.map((v) => {
            const number = Number(v);
            if (v === "" || !Number.isFinite(number))
                throw new Error(`element ${i}: invalid number "${v}".`);
            return number;
        });
        if (tokens[0] === "L") {
            if (tokens.length !== 5)
                throw new Error(`element ${i}: expected "L x_start y_start x_end y_end".`);
            const [xs, ys, xe, ye] = numbers(tokens.slice(1));
            return {type: "LineSegment", start: point(xs, ys), end: point(xe, ye)};
        }
        if (tokens[0] === "A") {
            if (tokens.length !== 8 || !(tokens[7] in ORIENTATIONS))
                throw new Error(`element ${i}: expected "A x_start y_start x_center y_center x_end y_end a|c|f".`);
            const [xs, ys, xc, yc, xe, ye] = numbers(tokens.slice(1, 7));
            return {
                type: "CircularArc",
                start: point(xs, ys),
                end: point(xe, ye),
                center: point(xc, yc),
                orientation: ORIENTATIONS[tokens[7]],
            };
        }
        throw new Error(`element ${i}: unknown element "${tokens[0]}" (L or A).`);
    });
    elements.forEach((element, i) => {
        const next = elements[(i + 1) % elements.length];
        if (!close(element.end.x, next.start.x) || !close(element.end.y, next.start.y)) {
            throw new Error((i + 1 < elements.length)?
                `element ${i + 1} doesn't start where element ${i} ends.`:
                "the shape isn't closed: the last element doesn't end where the first one starts.");
        }
    });
    return elements;
}

// The text of the elements of a general shape (see 'parseGeneralShape').
export function formatGeneralShape(elements) {
    const text = (v) => String(v);
    return elements.map((e) => (e.type === "CircularArc")?
        `A ${text(e.start.x)} ${text(e.start.y)} ${text(e.center.x)} ${text(e.center.y)} `
            + `${text(e.end.x)} ${text(e.end.y)} ${ORIENTATION_LETTERS[e.orientation]}`:
        `L ${text(e.start.x)} ${text(e.start.y)} ${text(e.end.x)} ${text(e.end.y)}`).join(", ");
}

// The elements of a shape in the opposite direction.
function reverseElements(elements) {
    const opposite = {Anticlockwise: "Clockwise", Clockwise: "Anticlockwise", Full: "Full"};
    return elements.slice().reverse().map((e) => {
        const reversed = {...e, start: e.end, end: e.start};
        if (e.type === "CircularArc")
            reversed.orientation = opposite[e.orientation];
        return reversed;
    });
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
    if (row.shape === "general") {
        let elements = parseGeneralShape(row.elements || "");
        // Shapes are anticlockwise (the arcs approximated by segments).
        const area = signedArea(shapePoints(elements).map(([x, y]) => point(x, y)));
        if (Math.abs(area) < 1e-12)
            throw new Error("the general shape has no area.");
        if (area < 0)
            elements = reverseElements(elements);
        return {type: "general", elements};
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
    // The polygons and the general shapes are given in the bin (item).
    if (defect.shape === "polygon" || defect.shape === "general")
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

// The holes typed in the form, which must be inside the shape.
function placedHoles(shape, holes, kind) {
    const outline = shapePoints(shape.elements);
    return holes.map((hole, j) => {
        try {
            const result = placedShape(hole);
            if (!shapePoints(result.elements).every((p) => strictlyInside(outline, p)))
                throw new Error(`it must be inside the ${kind}.`);
            return result;
        } catch (error) {
            throw new Error(`hole ${j}: ${error.message}`);
        }
    });
}

// The shape of an item row with its holes: the holes of its file, then the
// holes typed in the form, which must be inside the item.
export function itemShape(row) {
    const shape = rowShape(row);
    if ((row.holes || []).length === 0)
        return shape;
    return {...shape, holes: [...(shape.holes || []), ...placedHoles(shape, row.holes, "item")]};
}

// The allowed rotations of an item row, in the JSON format.
export function itemRotations(row) {
    if (row.rotations !== "custom")
        return ROTATIONS[row.rotations].rotations;
    const ranges = row.rotation_ranges || [];
    if (ranges.length === 0)
        throw new Error("add at least one range of rotations.");
    return ranges.map((range, j) => {
        const angle = (value, name) => {
            const number = Number(value);
            if (value === "" || !Number.isFinite(number))
                throw new Error(`rotation range ${j}: invalid ${name}: "${value}".`);
            return number;
        };
        const result = {start: angle(range.start, "start"), end: angle(range.end, "end")};
        if (result.start > result.end)
            throw new Error(`rotation range ${j}: the start is greater than the end.`);
        if (range.mirror)
            result.mirror = true;
        return result;
    });
}

// The shapes of an item row: its shape with its holes, then its other shapes.
export function itemShapes(row) {
    return [itemShape(row), ...(row.extra_shapes || []).map((extra, j) => {
        try {
            // With its holes, which must be inside it.
            return defectShape(extra, "shape");
        } catch (error) {
            throw new Error(`shape ${j + 1}: ${error.message}`);
        }
    })];
}

// The shape of a defect in its bin (or of another shape of an item, in the
// item), with its holes, which must be inside it.
export function defectShape(defect, kind = "defect") {
    const shape = placedShape(defect);
    if ((defect.holes || []).length === 0)
        return shape;
    return {...shape, holes: placedHoles(shape, defect.holes, kind)};
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
    // The copies of the item types used by the fixed items.
    const fixedCopies = itemRows.map(() => 0);
    if (binRows.length === 0)
        throw new Error("add at least one bin type.");
    if (itemRows.length === 0)
        throw new Error("add at least one item type.");
    const withRow = (kind, i, f) => {
        try {
            return f();
        } catch (error) {
            throw new Error(`${kind} type ${i}: ${error.message}`);
        }
    };
    const binTypes = binRows.map((row, i) => withRow("bin", i, () => {
        const shape = rowShape(row);
        if (shape.holes !== undefined)
            throw new Error("a bin can't have holes.");
        const binType = {...shape, copies: rowCopies(row, objective, false)};
        const copiesMin = copiesMinUsed(objective, false)? optionalNumber(row.copies_min, "minimum copies"): undefined;
        if (copiesMin !== undefined)
            binType.copies_min = copiesMin;
        const fixed = fixedItems(row, itemRows);
        if (fixed.length > 0) {
            if (binType.copies === -1)
                throw new Error("a bin type with fixed items can't have unlimited copies.");
            binType.fixed_items = fixed.map((fixedItem, j) => {
                const name = `fixed item ${j}`;
                if (fixedItem.itemRow === null)
                    throw new Error(`${name}: invalid item type id.`);
                const itemTypeId = itemRows.indexOf(fixedItem.itemRow);
                fixedCopies[itemTypeId] += binType.copies;
                const position = (value, key) => {
                    const number = Number(value);
                    if (value === "" || !Number.isFinite(number))
                        throw new Error(`${name}: invalid ${key}: "${value}".`);
                    return number;
                };
                const result = {
                    item_type_id: itemTypeId,
                    bl_corner: {x: position(fixedItem.x, "x"), y: position(fixedItem.y, "y")},
                };
                if (fixedItem.angle !== "")
                    result.angle = position(fixedItem.angle, "angle");
                if (fixedItem.mirror)
                    result.mirror = true;
                // The angle and the mirroring must be allowed for the item
                // type.
                const angle = result.angle || 0;
                const mirror = Boolean(fixedItem.mirror);
                const itemRow = fixedItem.itemRow;
                // The rotations of the item type, and their mirrored copies
                // with 'allow_mirroring'.
                let rotations = [];
                try {
                    rotations = itemRotations(itemRow);
                } catch (error) {
                    // Reported for the item type.
                }
                if (itemRow.mirror && itemRow.rotations !== "custom")
                    rotations = [...rotations, ...rotations.map((r) => ({...r, mirror: true}))];
                const allowed = rotations.some((r) => angle >= r.start - 1e-9 && angle <= r.end + 1e-9
                    && Boolean(r.mirror) === mirror);
                if (!allowed) {
                    throw new Error(`${name}: the angle ${angle}${mirror? " mirrored": ""} `
                        + `isn't allowed for item type ${itemTypeId}.`);
                }
                return result;
            });
        }
        const cost = valueUsed(objective, false)? optionalNumber(row.cost, "cost"): undefined;
        if (cost !== undefined)
            binType.cost = cost;
        const spacing = optionalNumber(row.spacing, "item spacing");
        if (spacing !== undefined)
            binType.item_bin_minimum_spacing = spacing;
        if ((row.defects || []).length > 0) {
            binType.defects = row.defects.map((defect, j) => {
                try {
                    const result = defectShape(defect);
                    const defectSpacing = optionalNumber(defect.spacing, "spacing");
                    if (defectSpacing !== undefined)
                        result.item_defect_minimum_spacing = defectSpacing;
                    // The type of the defect, for the quality rules.
                    const defectType = optionalNumber(defect.defect_type, "type");
                    if (defectType !== undefined) {
                        if (!Number.isInteger(defectType) || defectType < 0)
                            throw new Error(`invalid type: "${defect.defect_type}".`);
                        result.defect_type = defectType;
                    }
                    return result;
                } catch (error) {
                    throw new Error(`defect ${j}: ${error.message}`);
                }
            });
        }
        return binType;
    }));
    const itemTypes = itemRows.map((row, i) => withRow("item", i, () => {
        // An item type can have several shapes: the 'shapes' form is also
        // the one 'instanceFigure' reads.
        const itemType = {
            shapes: itemShapes(row),
            copies: rowCopies(row, objective, true),
            allowed_rotations: itemRotations(row),
        };
        const copiesMin = copiesMinUsed(objective, true)? optionalNumber(row.copies_min, "minimum copies"): undefined;
        if (copiesMin !== undefined)
            itemType.copies_min = copiesMin;
        const profit = valueUsed(objective, true)? optionalNumber(row.profit, "profit"): undefined;
        if (profit !== undefined)
            itemType.profit = profit;
        // With custom rotations, the mirroring is given per range.
        if (row.mirror && row.rotations !== "custom")
            itemType.allow_mirroring = true;
        return itemType;
    }));
    // The item types must have enough copies for the fixed items.
    itemTypes.forEach((itemType, i) => {
        if (fixedCopies[i] > 0 && itemType.copies !== -1 && itemType.copies < fixedCopies[i]) {
            throw new Error(`item type ${i}: ${itemType.copies} copies, but the fixed items need ${fixedCopies[i]}.`);
        }
    });
    return {objective, bin_types: binTypes, item_types: itemTypes};
}

/////////////////////////////////////////////////////////////////////////////
// From an instance
/////////////////////////////////////////////////////////////////////////////

// The spellings of the element types and the orientations of the arcs
// accepted by the solver.
const ELEMENT_TYPES = {
    LineSegment: "LineSegment", line_segment: "LineSegment", L: "LineSegment", l: "LineSegment",
    CircularArc: "CircularArc", circular_arc: "CircularArc", C: "CircularArc", c: "CircularArc",
};
const ARC_ORIENTATIONS = {
    Anticlockwise: "Anticlockwise", anticlockwise: "Anticlockwise", A: "Anticlockwise", a: "Anticlockwise",
    Clockwise: "Clockwise", clockwise: "Clockwise", C: "Clockwise", c: "Clockwise",
    Full: "Full", full: "Full", F: "Full", f: "Full",
};

// The fields of an object which the form doesn't read: added to 'ignored'
// (with their path), since the solver doesn't read them either.
function ignoreOthers(object, known, path, ignored) {
    for (const key of Object.keys(object)) {
        if (!known.includes(key))
            ignored.add(`${path}.${key}`);
    }
}

// The elements of a "general" shape of the JSON format, with the spellings
// of the form.
function shapeElements(json, path) {
    return (json.elements || []).map((element, i) => {
        const type = ELEMENT_TYPES[element.type];
        if (type === undefined)
            throw new Error(`${path}.elements[${i}]: unknown element type "${element.type}".`);
        const result = {type, start: point(element.start.x, element.start.y), end: point(element.end.x, element.end.y)};
        if (type === "CircularArc") {
            const orientation = ARC_ORIENTATIONS[element.orientation];
            if (orientation === undefined)
                throw new Error(`${path}.elements[${i}]: unknown orientation "${element.orientation}".`);
            result.center = point(element.center.x, element.center.y);
            result.orientation = orientation;
        }
        return result;
    });
}

// The fields of a row (or of a placed shape, 'placed') for a shape of the
// JSON format: a rectangle or a circle (at the origin, unless 'placed'), a
// polygon, else a general shape.
function shapeFields(json, path, placed, ignored) {
    const known = ["type", "x", "y", "width", "height", "radius", "vertices", "elements", "holes"];
    // 'is_path' (always false for the shapes of an instance) isn't read.
    ignoreOthers(json, [...known, "is_path"], path, ignored);
    const x = json.x || 0;
    const y = json.y || 0;
    if (json.type === "rectangle" && (placed || (x === 0 && y === 0)))
        return {shape: "rectangle", x, y, width: json.width, height: json.height};
    if (json.type === "circle" && (placed || (x === 0 && y === 0)))
        return {shape: "circle", x, y, radius: json.radius};
    if (json.type === "polygon")
        return {shape: "polygon", vertices: json.vertices.map((v) => `${v.x} ${v.y}`).join(", ")};
    let elements;
    if (json.type === "rectangle") {
        const corners = [point(x, y), point(x + json.width, y), point(x + json.width, y + json.height),
            point(x, y + json.height)];
        elements = corners.map((c, i) => lineSegment(c, corners[(i + 1) % 4]));
    } else if (json.type === "circle") {
        const start = point(x + json.radius, y);
        elements = [{type: "CircularArc", start, end: start, center: point(x, y), orientation: "Full"}];
    } else if (json.type === "general") {
        elements = shapeElements(json, path);
    } else {
        throw new Error(`${path}: unknown shape type "${json.type}".`);
    }
    return {shape: "general", elements: formatGeneralShape(elements)};
}

// The fields of a row which aren't given: their defaults.
function withDefaults(defaults, fields) {
    return {...defaults, ...fields};
}

// The holes of a shape of the JSON format, as placed shapes.
function holesFields(json, path, ignored) {
    return (json.holes || []).map((hole, j) =>
        withDefaults(defaultHole(), shapeFields(hole, `${path}.holes[${j}]`, true, ignored)));
}

function sameRotations(a, b) {
    return a.length === b.length && a.every((r, i) => r.start === b[i].start && r.end === b[i].end);
}

// The rotations of an item type of the JSON format: one of the 'ROTATIONS',
// or custom ranges.
function rotationFields(json) {
    const ranges = (json.allowed_rotations || []).map((r) => ({start: r.start, end: r.end, mirror: Boolean(r.mirror)}));
    if (ranges.length === 0)
        ranges.push({start: 0, end: 0, mirror: false});
    const mirror = Boolean(json.allow_mirroring);
    if (ranges.every((r) => !r.mirror)) {
        for (const [key, value] of Object.entries(ROTATIONS)) {
            if (value.rotations !== null && sameRotations(ranges, value.rotations))
                return {rotations: key, mirror};
        }
    }
    // 'allow_mirroring' adds a mirrored copy of each range which isn't
    // mirrored.
    const all = mirror? [...ranges, ...ranges.filter((r) => !r.mirror).map((r) => ({...r, mirror: true}))]: ranges;
    return {
        rotations: "custom",
        mirror: false,
        rotation_ranges: all.map((r) => ({start: String(r.start), end: String(r.end), mirror: r.mirror})),
    };
}

// The copies of a bin or item type of the JSON format.
function copiesFields(json) {
    const copies = (json.copies === undefined)? 1: json.copies;
    const fields = (copies === -1)? {copies: 1, unlimited_copies: true}: {copies, unlimited_copies: false};
    if (json.copies_min !== undefined && json.copies_min !== -1)
        fields.copies_min = json.copies_min;
    return fields;
}

// The rows of the bin and item types of an instance of the JSON format:
// '{binRows, itemRows}'. The fields which the form doesn't read (and the
// solver neither) are added to 'ignored'.
export function rowsFromInstance(json, ignored) {
    const itemRows = (json.item_types || []).map((itemType, i) => {
        const path = `item_types[${i}]`;
        ignoreOthers(itemType, ["type", "x", "y", "width", "height", "radius", "vertices", "elements", "holes",
            "is_path", "shapes", "copies", "copies_min", "profit", "allowed_rotations", "allow_mirroring"],
            path, ignored);
        const shapes = itemType.shapes || [itemType];
        if (shapes.length === 0)
            throw new Error(`${path}: an item type needs at least one shape.`);
        const first = shapes[0];
        const firstPath = (itemType.shapes !== undefined)? `${path}.shapes[0]`: path;
        const row = withDefaults(defaultItemRow(), {
            ...shapeFields(itemType.shapes? first: {...first, holes: undefined}, firstPath, false, new Set()),
            holes: holesFields(first, firstPath, ignored),
            extra_shapes: shapes.slice(1).map((shape, j) => ({
                ...withDefaults(defaultExtraShape(), shapeFields(shape, `${path}.shapes[${j + 1}]`, true, ignored)),
                holes: holesFields(shape, `${path}.shapes[${j + 1}]`, ignored),
            })),
            ...copiesFields(itemType),
            ...rotationFields(itemType),
        });
        if (itemType.shapes !== undefined)
            shapeFields(first, firstPath, false, ignored);
        if (itemType.profit !== undefined)
            row.profit = itemType.profit;
        // The default of the form when not custom.
        if (row.rotations !== "custom")
            row.rotation_ranges = [defaultRotationRange()];
        return row;
    });
    const binRows = (json.bin_types || []).map((binType, i) => {
        const path = `bin_types[${i}]`;
        ignoreOthers(binType, ["type", "x", "y", "width", "height", "radius", "vertices", "elements", "holes",
            "is_path", "copies", "copies_min", "cost", "item_bin_minimum_spacing", "defects", "fixed_items"],
            path, ignored);
        // The holes of a bin are ignored by the solver.
        if (binType.holes !== undefined)
            ignored.add(`${path}.holes`);
        const row = withDefaults(defaultBinRow(), {
            ...shapeFields({...binType, holes: undefined}, path, false, new Set()),
            ...copiesFields(binType),
        });
        if (binType.cost !== undefined && binType.cost !== -1)
            row.cost = binType.cost;
        if (binType.item_bin_minimum_spacing !== undefined)
            row.spacing = binType.item_bin_minimum_spacing;
        row.defects = (binType.defects || []).map((defect, j) => {
            const defectPath = `${path}.defects[${j}]`;
            ignoreOthers(defect, ["type", "x", "y", "width", "height", "radius", "vertices", "elements", "holes",
                "is_path", "defect_type", "item_defect_minimum_spacing"], defectPath, ignored);
            const fields = withDefaults(defaultDefect(), shapeFields(
                {...defect, defect_type: undefined, item_defect_minimum_spacing: undefined}, defectPath, true, new Set()));
            fields.holes = holesFields(defect, defectPath, ignored);
            if (defect.defect_type !== undefined && defect.defect_type !== -1)
                fields.defect_type = defect.defect_type;
            if (defect.item_defect_minimum_spacing !== undefined)
                fields.spacing = defect.item_defect_minimum_spacing;
            return fields;
        });
        row.fixed_items = (binType.fixed_items || []).map((fixedItem, j) => {
            const fixedPath = `${path}.fixed_items[${j}]`;
            ignoreOthers(fixedItem, ["item_type_id", "bl_corner", "angle", "mirror"], fixedPath, ignored);
            const itemRow = itemRows[fixedItem.item_type_id];
            if (itemRow === undefined)
                throw new Error(`${fixedPath}: invalid item type id ${fixedItem.item_type_id}.`);
            return {
                ...defaultFixedItem(itemRow),
                x: fixedItem.bl_corner.x,
                y: fixedItem.bl_corner.y,
                angle: (fixedItem.angle === undefined)? "": fixedItem.angle,
                mirror: Boolean(fixedItem.mirror),
            };
        });
        return row;
    });
    return {binRows, itemRows};
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
    } else if (row.shape === "general") {
        cell.append(input(row, "elements", "Elements", {
            type: "text",
            placeholder: GENERAL_SHAPE_PLACEHOLDER,
            className: "vertices general",
            spellcheck: false,
        }, onChange));
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
    // A line with the shape of a placed shape, and a button to remove it.
    const placedLine = (placed, kind, onRemove) => {
        const line = document.createElement("div");
        line.className = "dimensions";
        line.appendChild(select(placed, "shape", `${kind} shape`, PLACED_SHAPES, onStructureChange));
        if (placed.shape !== "polygon" && placed.shape !== "general") {
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
        } else if (placed.shape === "general") {
            line.appendChild(input(placed, "elements", `${kind} elements`, {
                type: "text",
                placeholder: GENERAL_SHAPE_PLACEHOLDER,
                className: "vertices general",
                spellcheck: false,
            }, onChange));
        } else {
            line.appendChild(input(placed, "vertices", `${kind} vertices`, {
                type: "text",
                placeholder: "x y, x y, x y, ...",
                className: "vertices",
                spellcheck: false,
            }, onChange));
        }
        const remove = document.createElement("button");
        remove.type = "button";
        remove.textContent = "×";
        remove.title = `Remove the ${kind.toLowerCase()}`;
        remove.addEventListener("click", () => {
            onRemove();
            onStructureChange();
        });
        return [line, remove];
    };
    row[key].forEach((placed, i) => {
        const [line, remove] = placedLine(placed, kind, () => row[key].splice(i, 1));
        if (key === "defects") {
            line.appendChild(labelled("Spacing",
                input(placed, "spacing", "Defect spacing", {...NUMBER, placeholder: "0"}, onChange)));
            // The type of the defect, for the quality rules.
            line.appendChild(labelled("Type",
                input(placed, "defect_type", "Defect type", {...NUMBER, step: "1", placeholder: "none"}, onChange)));
        }
        // A defect, or another shape of an item, can have holes, on lines
        // below it.
        const withHoles = (key === "defects" || key === "extra_shapes");
        if (withHoles) {
            placed.holes = placed.holes || [];
            line.appendChild(addPlacedShapeButton(placed, "holes", "Hole", defaultHole, onStructureChange));
        }
        line.appendChild(remove);
        cell.appendChild(line);
        if (withHoles) {
            placed.holes.forEach((hole, j) => {
                const [holeLine, holeRemove] = placedLine(hole, "Hole", () => placed.holes.splice(j, 1));
                holeLine.classList.add("hole-line");
                holeLine.prepend(Object.assign(document.createElement("span"),
                    {className: "note", textContent: "Hole"}));
                holeLine.appendChild(holeRemove);
                cell.appendChild(holeLine);
            });
        }
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

// Whether the copies of the items (bins) can be unlimited for an objective:
// only for the knapsack objective (all but the knapsack objective).
export function unlimitedCopiesAllowed(objective, isItem) {
    if (isItem)
        return objective === "knapsack";
    return objective !== "knapsack" && !objective.startsWith("open-dimension");
}

// The copies of a row: -1 if unlimited.
function rowCopies(row, objective, isItem) {
    if (row.unlimited_copies && unlimitedCopiesAllowed(objective, isItem))
        return -1;
    return copies(row.copies);
}

// Whether the minimum number of copies of the items (bins) is used for an
// objective: only for the knapsack (variable-sized bin packing) objective
// (all the items must be packed with the other objectives).
export function copiesMinUsed(objective, isItem) {
    return objective === (isItem? "knapsack": "variable-sized-bin-packing");
}

// Whether the profit of the items (cost of the bins) is used for an
// objective: only for the knapsack (variable-sized bin packing) objective.
export function valueUsed(objective, isItem) {
    return objective === (isItem? "knapsack": "variable-sized-bin-packing");
}

// The copies of a row, and if they can be unlimited, a checkbox which
// disables them.
function copiesCell(row, unlimited, onChange, onStructureChange) {
    const copies = input(row, "copies", "Copies", {...NUMBER, min: "1", step: "1"}, onChange);
    if (!unlimited)
        return copies;
    if (row.unlimited_copies) {
        copies.disabled = true;
        copies.value = "";
        copies.placeholder = "unlimited";
    }
    const checkbox = input(row, "unlimited_copies", "Unlimited copies", {type: "checkbox", title: "Unlimited"},
        onStructureChange);
    const cell = document.createElement("div");
    cell.className = "unlimited";
    cell.append(copies, checkbox);
    return cell;
}

// Rows whose details line is open.
const openDetails = new WeakSet();

// The custom rotations of an item row: a line for each range, and a button to
// add one.
function rotationRangesCell(row, onChange, onStructureChange) {
    const cell = document.createElement("div");
    cell.className = "placed-shapes";
    row.rotation_ranges = row.rotation_ranges || [];
    const labelled = (text, element) => {
        const label = document.createElement("label");
        label.className = "inline";
        label.append(text, element);
        return label;
    };
    row.rotation_ranges.forEach((range, i) => {
        const line = document.createElement("div");
        line.className = "dimensions";
        line.append(
            labelled("Start", input(range, "start", `Rotation range ${i} start`, {type: "number", step: "any"}, onChange)),
            labelled("End", input(range, "end", `Rotation range ${i} end`, {type: "number", step: "any"}, onChange)),
            labelled("Mirror", input(range, "mirror", `Rotation range ${i} mirror`, {type: "checkbox"}, onChange)));
        const remove = document.createElement("button");
        remove.type = "button";
        remove.textContent = "×";
        remove.title = "Remove the range";
        remove.addEventListener("click", () => {
            row.rotation_ranges.splice(i, 1);
            onStructureChange();
        });
        line.appendChild(remove);
        cell.appendChild(line);
    });
    const add = document.createElement("button");
    add.type = "button";
    add.textContent = "Add a range";
    add.title = "A range of allowed angles, in degrees (a single angle if the start and the end are equal)";
    add.addEventListener("click", () => {
        row.rotation_ranges.push(defaultRotationRange());
        onStructureChange();
    });
    cell.appendChild(add);
    return cell;
}

// Render a bin ('isItem' false) or item type table, for an objective.
// 'onTableChange' is called when a value changes, 'onStructureChange' when
// the table must be rendered again (shape changed, row removed).
// 'rowActions', if given, '{duplicate(rowIndex), remove(rowIndex)}': the
// actions of the buttons of the rows ("Duplicate", "Remove").
export function renderTable(table, rows, isItem, objective, onTableChange, onStructureChange, itemRows = [],
        rowActions = null) {
    table.replaceChildren();
    const header = table.createTHead().insertRow();
    const withValue = valueUsed(objective, isItem);
    // The id of each row (its position, from 0, as in the C++ code), then its
    // thumbnail.
    const labels = ["Id", "", "Shape", "Dimensions", "Copies"];
    if (withValue)
        labels.push(isItem? "Profit": "Cost");
    if (isItem)
        labels.push("Rotations", "Mirror");
    else
        labels.push("Item spacing");
    const unlimited = unlimitedCopiesAllowed(objective, isItem);
    for (const label of [...labels, ""]) {
        const th = document.createElement("th");
        th.textContent = label;
        if (label === "Copies" && unlimited) {
            const note = document.createElement("span");
            note.className = "header-note";
            note.textContent = "(or unlimited)";
            th.append(document.createElement("br"), note);
        }
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
        drawThumbnail(thumbnail, row, isItem, itemRows);
        const onChange = () => {
            drawThumbnail(thumbnail, row, isItem, itemRows);
            onTableChange();
        };
        const id = tr.insertCell();
        id.className = "row-id";
        id.textContent = String(rowIndex);
        tr.insertCell().appendChild(thumbnail);
        tr.insertCell().appendChild(select(row, "shape", "Shape", SHAPES, onStructureChange));
        tr.insertCell().appendChild(dimensionsCell(row, onChange, onStructureChange));
        const copies = copiesCell(row, unlimited, onChange, onStructureChange);
        // A single copy of the bin for the open dimension objectives.
        if (!isItem && objective.startsWith("open-dimension")) {
            copies.disabled = true;
            copies.title = "A single bin for the open dimension objectives";
        }
        tr.insertCell().appendChild(copies);
        if (withValue) {
            const valueKey = isItem? "profit": "cost";
            tr.insertCell().appendChild(input(row, valueKey, isItem? "Profit": "Cost",
                {...NUMBER, placeholder: "default"}, onChange));
        }
        if (isItem) {
            const rotationLabels = Object.fromEntries(
                Object.entries(ROTATIONS).map(([key, value]) => [key, value.label]));
            // Custom rotations open a line of ranges below the row.
            tr.insertCell().appendChild(select(row, "rotations", "Rotations", rotationLabels, onStructureChange));
            const mirror = input(row, "mirror", "Mirror", {type: "checkbox"}, onChange);
            if (row.rotations === "custom") {
                mirror.disabled = true;
                mirror.title = "With custom rotations, the mirroring is given per range";
            }
            tr.insertCell().appendChild(mirror);
        } else {
            tr.insertCell().appendChild(input(row, "spacing", "Item spacing",
                {...NUMBER, placeholder: "0"}, onChange));
        }
        const remove = document.createElement("button");
        remove.type = "button";
        remove.textContent = "Remove";
        remove.addEventListener("click", () => {
            if (rowActions !== null) {
                rowActions.remove(rowIndex);
                return;
            }
            rows.splice(rowIndex, 1);
            onStructureChange();
        });
        const buttons = tr.insertCell();
        buttons.className = "row-buttons";
        // The details of the row, on a line below it: the minimum copies, for
        // the objectives which use them.
        const withDetails = copiesMinUsed(objective, isItem);
        let more = null;
        if (withDetails) {
            more = document.createElement("button");
            more.type = "button";
            // Enabled for the examples too (see 'lockForm' in 'app.js').
            more.className = "details-toggle";
            const open = openDetails.has(row);
            more.textContent = open? "Less": "More";
            more.setAttribute("aria-expanded", String(open));
            if (row.copies_min !== "" && row.copies_min !== undefined) {
                more.classList.add("modified");
                more.title = "Set: minimum copies";
            }
            more.addEventListener("click", () => {
                if (openDetails.has(row))
                    openDetails.delete(row);
                else
                    openDetails.add(row);
                onStructureChange();
            });
        }
        // The defects of a bin, or the holes of an item, on a line below it
        // if it has some; the button to add one is on its row.
        const [key, kind, defaultValue] = isItem?
            ["holes", "Hole", defaultHole]: ["defects", "Defect", defaultDefect];
        row[key] = row[key] || [];
        buttons.appendChild(addPlacedShapeButton(row, key, kind, defaultValue, onStructureChange));
        // The other shapes of an item, on a line below it if it has some.
        if (isItem) {
            row.extra_shapes = row.extra_shapes || [];
            buttons.appendChild(addPlacedShapeButton(row, "extra_shapes", "Shape", defaultExtraShape, onStructureChange));
        }
        // The fixed items of a bin, on a line below it if it has some.
        if (!isItem) {
            row.fixed_items = fixedItems(row, itemRows);
            const add = document.createElement("button");
            add.type = "button";
            add.textContent = "Add a fixed item";
            add.disabled = (itemRows.length === 0);
            add.addEventListener("click", () => {
                row.fixed_items.push(defaultFixedItem(itemRows[0]));
                onStructureChange();
            });
            buttons.appendChild(add);
        }
        if (more !== null)
            buttons.appendChild(more);
        if (rowActions !== null) {
            const duplicate = document.createElement("button");
            duplicate.type = "button";
            duplicate.textContent = "Duplicate";
            duplicate.addEventListener("click", () => rowActions.duplicate(rowIndex));
            buttons.appendChild(duplicate);
        }
        buttons.appendChild(remove);
        if (withDetails && openDetails.has(row)) {
            const line = body.insertRow();
            line.className = "details-line";
            const cell = line.insertCell();
            cell.colSpan = labels.length + 1;
            const fields = document.createElement("div");
            fields.className = "details";
            const label = document.createElement("label");
            label.className = "inline";
            label.append("Minimum copies", input(row, "copies_min", "Minimum copies",
                {...NUMBER, step: "1", placeholder: "0"}, onChange));
            fields.appendChild(label);
            cell.appendChild(fields);
        }
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
        const addLine = (text, content) => {
            const line = body.insertRow();
            line.className = "placed-shapes-line";
            const cell = line.insertCell();
            cell.colSpan = labels.length + 1;
            const label = document.createElement("span");
            label.className = "placed-shapes-label";
            label.textContent = text;
            cell.append(label, content);
        };
        if (isItem && row.extra_shapes.length > 0)
            addLine("Other shapes", placedShapesCell(row, "extra_shapes", "Shape", onChange, onStructureChange));
        if (isItem && row.rotations === "custom")
            addLine("Rotations", rotationRangesCell(row, onChange, onStructureChange));
        if (!isItem && row.fixed_items.length > 0) {
            const line = body.insertRow();
            line.className = "placed-shapes-line";
            const cell = line.insertCell();
            cell.colSpan = labels.length + 1;
            const label = document.createElement("span");
            label.className = "placed-shapes-label";
            label.textContent = "Fixed items";
            cell.append(label, fixedItemsCell(row, itemRows, onChange, onStructureChange));
        }
    });
}

// The fixed items of a bin row: a line for each.
function fixedItemsCell(row, itemRows, onChange, onStructureChange) {
    const cell = document.createElement("div");
    cell.className = "placed-shapes";
    const labelled = (text, element) => {
        const label = document.createElement("label");
        label.className = "inline";
        label.append(text, element);
        return label;
    };
    row.fixed_items.forEach((fixedItem, i) => {
        const line = document.createElement("div");
        line.className = "dimensions";
        // The id of the item type (its position, from 0, as in the "Id"
        // column).
        const itemTypeId = document.createElement("input");
        itemTypeId.type = "number";
        itemTypeId.min = "0";
        itemTypeId.max = String(itemRows.length - 1);
        itemTypeId.step = "1";
        itemTypeId.className = "item-type-id";
        itemTypeId.value = (fixedItem.itemRow !== null)? String(itemRows.indexOf(fixedItem.itemRow)): "";
        itemTypeId.setAttribute("aria-label", `Fixed item ${i} item type id`);
        itemTypeId.addEventListener("input", () => {
            const id = Number(itemTypeId.value);
            fixedItem.itemRow = (itemTypeId.value !== "" && Number.isInteger(id) && id >= 0 && id < itemRows.length)?
                itemRows[id]: null;
            onChange();
        });
        line.append(
            labelled("Item type", itemTypeId),
            labelled("X", input(fixedItem, "x", "Fixed item x", {type: "number", step: "any"}, onChange)),
            labelled("Y", input(fixedItem, "y", "Fixed item y", {type: "number", step: "any"}, onChange)),
            labelled("Angle", input(fixedItem, "angle", "Fixed item angle",
                {type: "number", step: "any", placeholder: "0"}, onChange)),
            labelled("Mirror", input(fixedItem, "mirror", "Fixed item mirror", {type: "checkbox"}, onChange)));
        const remove = document.createElement("button");
        remove.type = "button";
        remove.textContent = "×";
        remove.title = "Remove the fixed item";
        remove.addEventListener("click", () => {
            row.fixed_items.splice(i, 1);
            onStructureChange();
        });
        line.appendChild(remove);
        cell.appendChild(line);
    });
    return cell;
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
export function rowThumbnail(row, isItem, itemRows = []) {
    let shapes;
    let defects = [];
    try {
        shapes = isItem? itemShapes(row): [rowShape(row)];
        if (!isItem)
            defects = (row.defects || []).map(defectShape);
    } catch (error) {
        return {error: error.message};
    }
    const outline = shapes.flatMap((shape) => shapePoints(shape.elements));
    const xs = outline.map((p) => p[0]);
    const ys = outline.map((p) => p[1]);
    const xMin = Math.min(...xs);
    const xMax = Math.max(...xs);
    const yMin = Math.min(...ys);
    const yMax = Math.max(...ys);
    const margin = 0.03 * Math.max(xMax - xMin, yMax - yMin);
    return {
        // In the coordinates flipped vertically.
        viewBox: [xMin - margin, -yMax - margin, xMax - xMin + 2 * margin, yMax - yMin + 2 * margin],
        paths: [
            ...shapes.map((shape) => ({
                d: [shape, ...(shape.holes || [])].map((s) => pathData(s.elements)).join(" "),
                fill: isItem? ITEM_COLOR: BIN_COLOR})),
            ...defects.map((defect) => ({
                d: [defect, ...(defect.holes || [])].map((s) => pathData(s.elements)).join(" "),
                fill: DEFECT_COLOR})),
            ...(isItem? []: fixedItemPaths(row, itemRows)),
        ],
        title: `${format(xMax - xMin)} × ${format(yMax - yMin)}`,
    };
}

// The paths of the fixed items of a bin row (those whose item type or
// position is invalid are not drawn).
function fixedItemPaths(row, itemRows) {
    const paths = [];
    for (const fixedItem of fixedItems(row, itemRows)) {
        if (fixedItem.itemRow === null || fixedItem.x === "" || fixedItem.y === "")
            continue;
        let shapes;
        try {
            shapes = itemShapes(fixedItem.itemRow);
        } catch (error) {
            continue;
        }
        for (const shape of shapes) {
            const d = [shape, ...(shape.holes || [])].map((s) =>
                "M" + fixedItemPoints(s.elements, fixedItem).map(([x, y]) => `${x} ${y}`).join(" L") + " Z").join(" ");
            paths.push({d, fill: ITEM_COLOR});
        }
    }
    return paths;
}

const SVG_NAMESPACE = "http://www.w3.org/2000/svg";

// Draw the thumbnail of a row in an 'svg' element.
function drawThumbnail(element, row, isItem, itemRows = []) {
    const thumbnail = rowThumbnail(row, isItem, itemRows);
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
