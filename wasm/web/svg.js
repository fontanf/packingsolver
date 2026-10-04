// Reader of the parts drawn in an SVG file.
//
// The contours of the parts are made of 'path' (lines and circular arcs),
// 'rect' (with circular rounded corners), 'circle', 'line', 'polyline' and
// 'polygon' elements, possibly in groups with transformations and cloned by
// 'use' elements. They are chained into closed loops; a loop and the loops
// directly inside it (its holes) form a part (see 'contours.js'). Styles
// (strokes, fills) don't matter.
//
// Bézier curves, ellipses and elliptical arcs aren't supported: a file
// containing some is rejected with an error. Texts and images are ignored.
//
// The parts are drawn as in SVG editors: the y axis of SVG points down, it is
// flipped. Lengths are converted to millimeters when the file gives the size
// of the drawing ('width' and 'height' of the root element, CSS units,
// 1px = 1/96in); otherwise they are kept in user units.

import {IDENTITY, apply, buildParts, compose, point, transformElement} from "./contours.js";

/////////////////////////////////////////////////////////////////////////////
// XML
/////////////////////////////////////////////////////////////////////////////

const ENTITIES = {amp: "&", lt: "<", gt: ">", quot: "\"", apos: "'"};

function decodeEntities(text) {
    return text.replace(/&(#x[0-9a-fA-F]+|#[0-9]+|[a-zA-Z]+);/g, (whole, name) => {
        if (name[0] === "#") {
            const code = (name[1] === "x")? parseInt(name.slice(2), 16): parseInt(name.slice(1), 10);
            return String.fromCodePoint(code);
        }
        return (name in ENTITIES)? ENTITIES[name]: whole;
    });
}

// Parse an XML document into a tree of '{name, attributes, children}' (names
// without their namespace prefix; text is ignored).
function parseXml(text) {
    const root = {name: "#document", attributes: {}, children: []};
    const stack = [root];
    const tag = /<!--[\s\S]*?-->|<!\[CDATA\[[\s\S]*?\]\]>|<![^>]*>|<\?[\s\S]*?\?>|<\/([^\s>]+)\s*>|<([^\s/>]+)((?:\s+[^\s=/>]+\s*=\s*(?:"[^"]*"|'[^']*'))*)\s*(\/?)>/g;
    const attribute = /([^\s=/>]+)\s*=\s*(?:"([^"]*)"|'([^']*)')/g;
    const localName = (name) => name.slice(name.indexOf(":") + 1);
    let match;
    while ((match = tag.exec(text)) !== null) {
        if (match[1] !== undefined) {
            // Closing tag.
            if (stack.length > 1)
                stack.pop();
        } else if (match[2] !== undefined) {
            const element = {name: localName(match[2]), attributes: {}, children: []};
            let a;
            attribute.lastIndex = 0;
            while ((a = attribute.exec(match[3])) !== null) {
                const value = decodeEntities((a[2] !== undefined)? a[2]: a[3]);
                // 'xlink:href' and 'href' are both 'href'.
                const name = (a[1].startsWith("xmlns"))? a[1]: localName(a[1]);
                element.attributes[name] = value;
            }
            stack[stack.length - 1].children.push(element);
            if (match[4] !== "/")
                stack.push(element);
        }
    }
    const svg = root.children.find((c) => c.name === "svg");
    if (svg === undefined)
        throw new Error("invalid SVG file: no 'svg' element.");
    return svg;
}

/////////////////////////////////////////////////////////////////////////////
// Units and transformations
/////////////////////////////////////////////////////////////////////////////

// Millimeters per unit (CSS units: 1in = 96px).
const MILLIMETERS = {mm: 1, cm: 10, in: 25.4, pt: 25.4 / 72, pc: 25.4 / 6, px: 25.4 / 96, "": 25.4 / 96};

// A length with its unit, in millimeters, or null if it can't be converted
// (missing, percentage, relative units).
function lengthInMillimeters(value) {
    if (value === undefined)
        return null;
    const match = /^\s*([-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?)\s*([a-z%]*)\s*$/.exec(value);
    if (match === null || !(match[2] in MILLIMETERS))
        return null;
    return Number(match[1]) * MILLIMETERS[match[2]];
}

function numbers(text) {
    return (text || "").match(/[-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?/g)?.map(Number) || [];
}

// Transformation of the root element: from the user units of the drawing to
// millimeters (if the size of the drawing is known), with the y axis flipped.
function rootTransformation(svg) {
    const width = lengthInMillimeters(svg.attributes.width);
    const height = lengthInMillimeters(svg.attributes.height);
    const viewBox = numbers(svg.attributes.viewBox);
    let scale = 1;
    let units = null;
    if (viewBox.length === 4 && viewBox[2] > 0 && viewBox[3] > 0) {
        if (width !== null && height !== null) {
            // 'preserveAspectRatio' 'meet' (the default): uniform scaling.
            scale = Math.min(width / viewBox[2], height / viewBox[3]);
            units = "millimeters";
        } else if (width !== null) {
            scale = width / viewBox[2];
            units = "millimeters";
        }
    } else if (width !== null || height !== null || svg.attributes.width === undefined) {
        // Without a view box, user units are pixels.
        scale = MILLIMETERS.px;
        units = "millimeters";
    }
    return {transformation: {a: scale, b: 0, c: 0, d: -scale, e: 0, f: 0}, units};
}

const DEGREES = Math.PI / 180;

// Transformation of a 'transform' attribute (in our convention: 'p -> [a b;
// c d] p + [e f]'; SVG's 'matrix(a b c d e f)' is '[a c; b d]').
function parseTransform(text) {
    let t = IDENTITY;
    const functions = /(matrix|translate|scale|rotate|skewX|skewY)\s*\(([^)]*)\)/g;
    let match;
    while ((match = functions.exec(text || "")) !== null) {
        const v = numbers(match[2]);
        let u;
        switch (match[1]) {
        case "matrix":
            u = {a: v[0], b: v[2], c: v[1], d: v[3], e: v[4], f: v[5]};
            break;
        case "translate":
            u = {...IDENTITY, e: v[0], f: v[1] || 0};
            break;
        case "scale":
            u = {...IDENTITY, a: v[0], d: (v.length > 1)? v[1]: v[0]};
            break;
        case "rotate": {
            const cos = Math.cos(v[0] * DEGREES);
            const sin = Math.sin(v[0] * DEGREES);
            const cx = v[1] || 0;
            const cy = v[2] || 0;
            u = compose(
                {...IDENTITY, e: cx, f: cy},
                compose({a: cos, b: -sin, c: sin, d: cos, e: 0, f: 0}, {...IDENTITY, e: -cx, f: -cy}));
            break;
        } case "skewX":
            u = {...IDENTITY, b: Math.tan(v[0] * DEGREES)};
            break;
        case "skewY":
            u = {...IDENTITY, c: Math.tan(v[0] * DEGREES)};
            break;
        }
        t = compose(t, u);
    }
    return t;
}

/////////////////////////////////////////////////////////////////////////////
// Elements
/////////////////////////////////////////////////////////////////////////////

class UnsupportedError extends Error {}

function unsupported(what, element) {
    const id = element.attributes.id;
    return new UnsupportedError(
        `${what} are not supported (${element.name}${(id !== undefined)? ` "${id}"`: ""}).`);
}

function lineSegment(start, end) {
    return {type: "LineSegment", start, end};
}

// Circular arc from 'start' to 'end' of radius 'radius', from an SVG arc
// command (the center is computed as in the SVG specification,
// "Conversion from endpoint to center parameterization"). Coordinates are
// those of the drawing (y axis pointing down): a positive sweep goes towards
// increasing angles.
function svgArc(start, end, radius, largeArc, sweep) {
    const dx = (end.x - start.x) / 2;
    const dy = (end.y - start.y) / 2;
    const halfChord = Math.hypot(dx, dy);
    // Radii too small are scaled up.
    const r = Math.max(radius, halfChord);
    const h = Math.sqrt(Math.max(0, r * r - halfChord * halfChord));
    // Unit vector perpendicular to the chord.
    const nx = -dy / halfChord;
    const ny = dx / halfChord;
    const sign = (largeArc === sweep)? -1: 1;
    const center = point(start.x + dx + sign * h * nx, start.y + dy + sign * h * ny);
    return {
        type: "CircularArc",
        start,
        end,
        center,
        orientation: sweep? "Anticlockwise": "Clockwise",
    };
}

// Elements of the 'd' attribute of a path: a list of subpaths
// '{elements, closed}'.
function pathSubpaths(d, element) {
    // Commands (strings) and numbers ('{value, text}': the text is needed
    // for the arc flags).
    const tokens = [];
    const token = /([MmLlHhVvZzAaCcSsQqTt])|([-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?)/g;
    let match;
    while ((match = token.exec(d)) !== null)
        tokens.push((match[1] !== undefined)? match[1]: {value: Number(match[2]), text: match[2]});
    const subpaths = [];
    let current = null;
    let position = point(0, 0);
    let start = point(0, 0);
    let command = null;
    let i = 0;
    const invalid = () => new Error(
        `invalid path data (${element.name}${element.attributes.id? ` "${element.attributes.id}"`: ""}).`);
    const nextNumber = () => {
        if (i >= tokens.length || typeof tokens[i] === "string")
            throw invalid();
        return tokens[i++].value;
    };
    // Arc flags may be written without separators ("a5 5 0 01 1 1"): the
    // first character of the number is the flag, the rest is the next
    // number.
    const nextFlag = () => {
        if (i >= tokens.length || typeof tokens[i] === "string")
            throw invalid();
        const text = tokens[i].text.replace(/^\+/, "");
        if (text[0] !== "0" && text[0] !== "1")
            throw invalid();
        const rest = text.slice(1);
        if (rest === "")
            ++i;
        else
            tokens[i] = {value: Number(rest), text: rest};
        return Number(text[0]);
    };
    const finish = (closed) => {
        if (current !== null && current.length > 0)
            subpaths.push({elements: current, closed});
        current = null;
    };
    while (i < tokens.length) {
        if (typeof tokens[i] === "string") {
            command = tokens[i++];
        } else if (command === null) {
            throw invalid();
        }
        const relative = command === command.toLowerCase();
        const base = relative? position: point(0, 0);
        const C = command.toUpperCase();
        if ("CSQT".includes(C))
            throw unsupported("Bézier curves", element);
        if (C === "Z") {
            if (current !== null && (position.x !== start.x || position.y !== start.y))
                current.push(lineSegment(position, start));
            position = start;
            finish(true);
            // Other numbers can't follow a 'Z'.
            command = null;
            continue;
        }
        if (C === "M") {
            finish(false);
            position = point(base.x + nextNumber(), base.y + nextNumber());
            start = position;
            current = [];
            // Subsequent pairs are lines.
            command = relative? "l": "L";
            continue;
        }
        if (current === null)
            current = [];
        let end;
        if (C === "L") {
            end = point(base.x + nextNumber(), base.y + nextNumber());
        } else if (C === "H") {
            end = point(base.x + nextNumber(), position.y);
        } else if (C === "V") {
            end = point(position.x, base.y + nextNumber());
        } else if (C === "A") {
            const rx = Math.abs(nextNumber());
            const ry = Math.abs(nextNumber());
            nextNumber();  // Rotation of the x axis: no effect on a circle.
            const largeArc = nextFlag();
            const sweep = nextFlag();
            end = point(base.x + nextNumber(), base.y + nextNumber());
            if (end.x === position.x && end.y === position.y)
                continue;
            if (rx === 0 || ry === 0) {
                current.push(lineSegment(position, end));
            } else {
                if (Math.abs(rx - ry) > 1e-9 * Math.max(rx, ry))
                    throw unsupported("Elliptical arcs", element);
                current.push(svgArc(position, end, rx, largeArc === 1, sweep === 1));
            }
            position = end;
            continue;
        }
        if (end.x !== position.x || end.y !== position.y)
            current.push(lineSegment(position, end));
        position = end;
    }
    finish(false);
    return subpaths;
}

function attribute(element, name, defaultValue = 0) {
    const value = element.attributes[name];
    return (value === undefined)? defaultValue: Number(numbers(value)[0] ?? defaultValue);
}

function points(element) {
    const v = numbers(element.attributes.points);
    const result = [];
    for (let i = 0; i + 1 < v.length; i += 2)
        result.push(point(v[i], v[i + 1]));
    return result;
}

// Subpaths of an element (see 'pathSubpaths'), in its user units.
function elementSubpaths(element) {
    switch (element.name) {
    case "path":
        return pathSubpaths(element.attributes.d || "", element);
    case "rect": {
        const x = attribute(element, "x");
        const y = attribute(element, "y");
        const w = attribute(element, "width");
        const h = attribute(element, "height");
        if (w <= 0 || h <= 0)
            return [];
        let rx = element.attributes.rx;
        let ry = element.attributes.ry;
        rx = (rx === undefined)? ry: rx;
        ry = (ry === undefined)? rx: ry;
        rx = Math.min(Number(rx || 0), w / 2);
        ry = Math.min(Number(ry || 0), h / 2);
        if (Math.abs(rx - ry) > 1e-9 * Math.max(rx, ry))
            throw unsupported("Rounded corners with different radii (elliptical)", element);
        const r = rx;
        if (r === 0) {
            const corners = [point(x, y), point(x + w, y), point(x + w, y + h), point(x, y + h)];
            return [{elements: corners.map((c, i) => lineSegment(c, corners[(i + 1) % 4])), closed: true}];
        }
        // Clockwise on the screen (y axis pointing down), with quarter
        // circles at the corners.
        const elements = [];
        const arc = (from, to, center) => ({
            type: "CircularArc", start: from, end: to, center, orientation: "Anticlockwise"});
        const p = (px, py) => point(px, py);
        const sides = [
            [p(x + r, y), p(x + w - r, y)],
            [p(x + w, y + r), p(x + w, y + h - r)],
            [p(x + w - r, y + h), p(x + r, y + h)],
            [p(x, y + h - r), p(x, y + r)],
        ];
        const centers = [p(x + w - r, y + r), p(x + w - r, y + h - r), p(x + r, y + h - r), p(x + r, y + r)];
        sides.forEach(([from, to], k) => {
            if (from.x !== to.x || from.y !== to.y)
                elements.push(lineSegment(from, to));
            elements.push(arc(to, sides[(k + 1) % 4][0], centers[k]));
        });
        return [{elements, closed: true}];
    } case "circle": {
        const center = point(attribute(element, "cx"), attribute(element, "cy"));
        const r = attribute(element, "r");
        if (r <= 0)
            return [];
        const start = point(center.x + r, center.y);
        return [{elements: [{type: "CircularArc", start, end: start, center, orientation: "Full"}], closed: true}];
    } case "ellipse": {
        const rx = attribute(element, "rx");
        const ry = attribute(element, "ry");
        if (Math.abs(rx - ry) > 1e-9 * Math.max(rx, ry))
            throw unsupported("Ellipses", element);
        return elementSubpaths({...element, name: "circle", attributes: {...element.attributes, r: String(rx)}});
    } case "line": {
        const start = point(attribute(element, "x1"), attribute(element, "y1"));
        const end = point(attribute(element, "x2"), attribute(element, "y2"));
        return [{elements: [lineSegment(start, end)], closed: false}];
    } case "polyline":
    case "polygon": {
        const vertices = points(element);
        const closed = element.name === "polygon";
        const elements = [];
        const n = vertices.length;
        for (let k = 0; k < (closed? n: n - 1); ++k) {
            const a = vertices[k];
            const b = vertices[(k + 1) % n];
            if (a.x !== b.x || a.y !== b.y)
                elements.push(lineSegment(a, b));
        }
        return [{elements, closed}];
    }
    }
    return [];
}

// Elements which are never drawn directly.
const NOT_DRAWN = new Set([
    "defs", "symbol", "clipPath", "mask", "pattern", "marker", "linearGradient",
    "radialGradient", "filter", "title", "desc", "metadata", "style", "script",
    "text", "image", "foreignObject",
]);

function hidden(element) {
    if (element.attributes.display === "none")
        return true;
    return /(^|;)\s*display\s*:\s*none/.test(element.attributes.style || "");
}

function indexIds(element, ids) {
    if (element.attributes.id !== undefined)
        ids.set(element.attributes.id, element);
    for (const child of element.children)
        indexIds(child, ids);
    return ids;
}

// Add the elements of the drawing, transformed by 't', to 'result'.
function collect(element, t, ids, result, depth = 0) {
    if (hidden(element))
        return;
    const local = compose(t, parseTransform(element.attributes.transform));
    if (element.name === "use") {
        const href = element.attributes.href || "";
        const target = ids.get(href.replace(/^#/, ""));
        if (target === undefined || depth > 16)
            return;
        const offset = {...IDENTITY, e: attribute(element, "x"), f: attribute(element, "y")};
        const placed = compose(local, offset);
        if (target.name === "symbol") {
            for (const child of target.children)
                collect(child, placed, ids, result, depth + 1);
        } else {
            collect(target, placed, ids, result, depth + 1);
        }
        return;
    }
    if (element.name === "g" || element.name === "svg" || element.name === "a" || element.name === "switch") {
        for (const child of element.children) {
            if (!NOT_DRAWN.has(child.name))
                collect(child, local, ids, result, depth);
        }
        return;
    }
    if (NOT_DRAWN.has(element.name))
        return;
    for (const subpath of elementSubpaths(element)) {
        const elements = subpath.elements.map((e) => {
            const transformed = transformElement(local, e);
            if (transformed === null)
                throw unsupported("Arcs scaled non-uniformly (ellipses)", element);
            return transformed;
        });
        if (elements.length === 0)
            continue;
        if (subpath.closed)
            result.loops.push(elements);
        else
            result.pieces.push(...elements);
    }
}

/////////////////////////////////////////////////////////////////////////////
// Parts
/////////////////////////////////////////////////////////////////////////////

// Read the parts of an SVG file. Returns '{parts, units, warnings}': each
// part is '{shape, holes, width, height, area}' ('shape' and 'holes' are
// "general" shapes), 'units' "millimeters" if the lengths could be
// converted, null otherwise (user units), 'warnings' a list of messages.
export function readParts(text) {
    const svg = parseXml(text);
    const {transformation, units} = rootTransformation(svg);
    const warnings = new Set();
    const result = {loops: [], pieces: []};
    collect(svg, transformation, indexIds(svg, new Map()), result);
    return {
        parts: buildParts(result, warnings),
        units,
        warnings: [...warnings],
    };
}
