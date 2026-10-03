// Reader of the parts drawn in a DXF file (ASCII format).
//
// The contours of the parts are made of LINE, ARC, CIRCLE, LWPOLYLINE and
// POLYLINE entities (polyline segments may be arcs: "bulges"), possibly in
// blocks placed with INSERT entities. They are chained into closed loops; a
// loop and the loops directly inside it (its holes) form a part. A loop inside
// a hole is another part.
//
// The parts are returned as "general" shapes of the JSON instance format of
// PackingSolver (line segments and circular arcs), anticlockwise, moved so
// that their bounding box starts at (0, 0). Their holes are anticlockwise too.
//
// Unsupported entities (SPLINE, ELLIPSE), contours which aren't closed and
// arcs scaled non-uniformly are reported in 'warnings'. Annotations (texts,
// dimensions, hatches...) are ignored.

/////////////////////////////////////////////////////////////////////////////
// Parsing
/////////////////////////////////////////////////////////////////////////////

// Group code / value pairs of a DXF file.
function readPairs(text) {
    const lines = text.split(/\r?\n/);
    const pairs = [];
    for (let i = 0; i + 1 < lines.length; i += 2) {
        const code = Number.parseInt(lines[i].trim(), 10);
        if (Number.isNaN(code))
            throw new Error(`invalid DXF file: group code expected on line ${i + 1}.`);
        pairs.push([code, lines[i + 1].trim()]);
    }
    return pairs;
}

// Entities of a list of pairs: '{type, codes}' where 'codes' lists the
// pairs of the entity.
function readEntities(pairs, begin, end) {
    const entities = [];
    let current = null;
    for (let i = begin; i < end; ++i) {
        const [code, value] = pairs[i];
        if (code === 0) {
            current = {type: value, codes: []};
            entities.push(current);
        } else if (current !== null) {
            current.codes.push([code, value]);
        }
    }
    return entities;
}

// First value of a group code, as a number.
function number(entity, code, defaultValue = 0) {
    const pair = entity.codes.find(([c]) => c === code);
    return (pair === undefined)? defaultValue: Number(pair[1]);
}

function string(entity, code, defaultValue = "") {
    const pair = entity.codes.find(([c]) => c === code);
    return (pair === undefined)? defaultValue: pair[1];
}

// Sections of the file: name -> [begin, end) indices in 'pairs'.
function readSections(pairs) {
    const sections = {};
    for (let i = 0; i < pairs.length; ++i) {
        if (pairs[i][0] === 0 && pairs[i][1] === "SECTION" && pairs[i + 1][0] === 2) {
            const name = pairs[i + 1][1];
            let j = i + 2;
            while (j < pairs.length && !(pairs[j][0] === 0 && pairs[j][1] === "ENDSEC"))
                ++j;
            sections[name] = [i + 2, j];
            i = j;
        }
    }
    return sections;
}

const UNITS = {
    1: "inches", 2: "feet", 4: "millimeters", 5: "centimeters", 6: "meters",
};

function readUnits(pairs, sections) {
    if (!("HEADER" in sections))
        return null;
    const [begin, end] = sections.HEADER;
    for (let i = begin; i + 1 < end; ++i) {
        if (pairs[i][0] === 9 && pairs[i][1] === "$INSUNITS")
            return UNITS[Number(pairs[i + 1][1])] || null;
    }
    return null;
}

/////////////////////////////////////////////////////////////////////////////
// Geometry
/////////////////////////////////////////////////////////////////////////////

// Elements: {type: "LineSegment", start, end} or {type: "CircularArc",
// start, end, center, orientation: "Anticlockwise" | "Clockwise" | "Full"}.

function point(x, y) {
    return {x, y};
}

// 2D transformation 'p -> [a b; c d] p + [e f]'.
const IDENTITY = {a: 1, b: 0, c: 0, d: 1, e: 0, f: 0};

function apply(t, p) {
    return point(t.a * p.x + t.b * p.y + t.e, t.c * p.x + t.d * p.y + t.f);
}

function compose(t, u) {
    // t after u.
    return {
        a: t.a * u.a + t.b * u.c, b: t.a * u.b + t.b * u.d,
        c: t.c * u.a + t.d * u.c, d: t.c * u.b + t.d * u.d,
        e: t.a * u.e + t.b * u.f + t.e, f: t.c * u.e + t.d * u.f + t.f,
    };
}

// Transformation of the object coordinate system of an entity: a 2D entity
// with the extrusion direction (0, 0, -1) is drawn mirrored (x -> -x).
function ocs(entity) {
    return (number(entity, 230, 1) < 0)? {...IDENTITY, a: -1}: IDENTITY;
}

const reverseOrientation = {Anticlockwise: "Clockwise", Clockwise: "Anticlockwise", Full: "Full"};

// Transform an element; arcs require a similarity (uniform scaling).
function transformElement(t, element, warnings) {
    const result = {...element, start: apply(t, element.start), end: apply(t, element.end)};
    if (element.type === "CircularArc") {
        const scaleX = Math.hypot(t.a, t.c);
        const scaleY = Math.hypot(t.b, t.d);
        if (Math.abs(scaleX - scaleY) > 1e-9 * Math.max(scaleX, scaleY)) {
            warnings.add("arcs scaled non-uniformly (ellipses) are not supported");
            return null;
        }
        result.center = apply(t, element.center);
        if (t.a * t.d - t.b * t.c < 0)
            result.orientation = reverseOrientation[element.orientation];
    }
    return result;
}

function arcPoint(center, radius, angle) {
    return point(center.x + radius * Math.cos(angle), center.y + radius * Math.sin(angle));
}

// Elements of the segment of a polyline from 'p' to 'q' with a bulge
// (tangent of a quarter of the arc angle; positive: anticlockwise).
function bulgeElement(p, q, bulge) {
    if (Math.abs(bulge) < 1e-12)
        return {type: "LineSegment", start: p, end: q};
    const angle = 4 * Math.atan(bulge);
    const chord = Math.hypot(q.x - p.x, q.y - p.y);
    const radius = chord / (2 * Math.sin(Math.abs(angle) / 2));
    // The center is on the perpendicular bisector of the chord.
    const mx = (p.x + q.x) / 2;
    const my = (p.y + q.y) / 2;
    // Distance from the center to the chord. The center is on the left of
    // the chord (p -> q) for an anticlockwise arc smaller than a half turn,
    // or a clockwise arc larger than a half turn.
    const distance = Math.sqrt(Math.max(0, radius * radius - chord * chord / 4));
    const sign = ((bulge > 0) === (Math.abs(angle) < Math.PI))? 1: -1;
    const nx = -(q.y - p.y) / chord;
    const ny = (q.x - p.x) / chord;
    return {
        type: "CircularArc",
        start: p,
        end: q,
        center: point(mx + sign * distance * nx, my + sign * distance * ny),
        orientation: (bulge > 0)? "Anticlockwise": "Clockwise",
    };
}

// Elements of a polyline given by its vertices and bulges.
function polylineElements(vertices, bulges, closed) {
    const elements = [];
    const n = vertices.length;
    for (let i = 0; i < (closed? n: n - 1); ++i) {
        const p = vertices[i];
        const q = vertices[(i + 1) % n];
        if (p.x === q.x && p.y === q.y)
            continue;
        elements.push(bulgeElement(p, q, bulges[i] || 0));
    }
    return elements;
}

// The elements of the entities, transformed by 't'. Closed contours (circles,
// closed polylines) are returned as loops, the other elements as pieces to
// chain.
function collect(entities, blocks, t, result, warnings, depth = 0) {
    for (let i = 0; i < entities.length; ++i) {
        const entity = entities[i];
        const local = compose(t, ocs(entity));
        const add = (elements, closed) => {
            const transformed = elements.map((e) => transformElement(local, e, warnings));
            if (transformed.some((e) => e === null))
                return;
            if (closed)
                result.loops.push(transformed);
            else
                result.pieces.push(...transformed);
        };
        if (entity.type === "LINE") {
            const start = point(number(entity, 10), number(entity, 20));
            const end = point(number(entity, 11), number(entity, 21));
            if (start.x !== end.x || start.y !== end.y)
                add([{type: "LineSegment", start, end}], false);
        } else if (entity.type === "ARC") {
            const center = point(number(entity, 10), number(entity, 20));
            const radius = number(entity, 40);
            const a0 = number(entity, 50) * Math.PI / 180;
            const a1 = number(entity, 51) * Math.PI / 180;
            add([{
                type: "CircularArc",
                start: arcPoint(center, radius, a0),
                end: arcPoint(center, radius, a1),
                center,
                orientation: "Anticlockwise",
            }], false);
        } else if (entity.type === "CIRCLE") {
            const center = point(number(entity, 10), number(entity, 20));
            const start = point(center.x + number(entity, 40), center.y);
            add([{type: "CircularArc", start, end: start, center, orientation: "Full"}], true);
        } else if (entity.type === "LWPOLYLINE") {
            const vertices = [];
            const bulges = [];
            for (const [code, value] of entity.codes) {
                if (code === 10) {
                    vertices.push(point(Number(value), 0));
                    bulges.push(0);
                } else if (code === 20 && vertices.length > 0) {
                    vertices[vertices.length - 1].y = Number(value);
                } else if (code === 42 && vertices.length > 0) {
                    bulges[bulges.length - 1] = Number(value);
                }
            }
            const closed = (number(entity, 70) & 1) === 1;
            add(polylineElements(vertices, bulges, closed), closed);
        } else if (entity.type === "POLYLINE") {
            const vertices = [];
            const bulges = [];
            let j = i + 1;
            for (; j < entities.length && entities[j].type !== "SEQEND"; ++j) {
                if (entities[j].type === "VERTEX") {
                    vertices.push(point(number(entities[j], 10), number(entities[j], 20)));
                    bulges.push(number(entities[j], 42));
                }
            }
            i = j;
            const closed = (number(entity, 70) & 1) === 1;
            add(polylineElements(vertices, bulges, closed), closed);
        } else if (entity.type === "INSERT") {
            const block = blocks[string(entity, 2)];
            if (block === undefined || depth > 16) {
                warnings.add(`block "${string(entity, 2)}" not found`);
                continue;
            }
            const angle = number(entity, 50) * Math.PI / 180;
            const sx = number(entity, 41, 1);
            const sy = number(entity, 42, 1);
            const cos = Math.cos(angle);
            const sin = Math.sin(angle);
            // Insertion point, rotation, scale, then the block's base point.
            const insert = {
                a: cos * sx, b: -sin * sy, c: sin * sx, d: cos * sy,
                e: number(entity, 10), f: number(entity, 20),
            };
            const base = {...IDENTITY, e: -block.base.x, f: -block.base.y};
            collect(block.entities, blocks, compose(local, compose(insert, base)),
                result, warnings, depth + 1);
        } else if (entity.type === "SPLINE" || entity.type === "ELLIPSE") {
            warnings.add(`${entity.type} entities are not supported`);
        }
    }
}

function readBlocks(pairs, sections) {
    const blocks = {};
    if (!("BLOCKS" in sections))
        return blocks;
    const entities = readEntities(pairs, ...sections.BLOCKS);
    let current = null;
    for (const entity of entities) {
        if (entity.type === "BLOCK") {
            current = {
                base: point(number(entity, 10), number(entity, 20)),
                entities: [],
            };
            blocks[string(entity, 2)] = current;
        } else if (entity.type === "ENDBLK") {
            current = null;
        } else if (current !== null) {
            current.entities.push(entity);
        }
    }
    return blocks;
}

/////////////////////////////////////////////////////////////////////////////
// Loops
/////////////////////////////////////////////////////////////////////////////

function reverseElement(element) {
    const reversed = {...element, start: element.end, end: element.start};
    if (element.type === "CircularArc")
        reversed.orientation = reverseOrientation[element.orientation];
    return reversed;
}

// Chain the pieces into closed loops: pieces whose ends are closer than
// 'tolerance' are joined.
function chain(pieces, tolerance, warnings) {
    const loops = [];
    const used = new Array(pieces.length).fill(false);
    const key = (p) => `${Math.round(p.x / tolerance)},${Math.round(p.y / tolerance)}`;
    // Ends of the pieces, by rounded coordinates (and the neighboring cells,
    // for points close to a cell boundary).
    const ends = new Map();
    const neighbors = (p) => {
        const keys = [];
        const kx = Math.round(p.x / tolerance);
        const ky = Math.round(p.y / tolerance);
        for (let dx = -1; dx <= 1; ++dx)
            for (let dy = -1; dy <= 1; ++dy)
                keys.push(`${kx + dx},${ky + dy}`);
        return keys;
    };
    pieces.forEach((piece, i) => {
        for (const [p, isStart] of [[piece.start, true], [piece.end, false]]) {
            const k = key(p);
            if (!ends.has(k))
                ends.set(k, []);
            ends.get(k).push({i, isStart});
        }
    });
    const close = (p, q) => Math.hypot(p.x - q.x, p.y - q.y) <= tolerance;
    const next = (p) => {
        for (const k of neighbors(p)) {
            for (const {i, isStart} of (ends.get(k) || [])) {
                if (used[i])
                    continue;
                const piece = pieces[i];
                if (close(isStart? piece.start: piece.end, p))
                    return {i, isStart};
            }
        }
        return null;
    };
    let open = 0;
    for (let first = 0; first < pieces.length; ++first) {
        if (used[first])
            continue;
        used[first] = true;
        const loop = [pieces[first]];
        let closed = false;
        for (;;) {
            const last = loop[loop.length - 1];
            if (loop.length > 1 && close(last.end, loop[0].start)) {
                closed = true;
                break;
            }
            const found = next(last.end);
            if (found === null)
                break;
            used[found.i] = true;
            const piece = pieces[found.i];
            loop.push(found.isStart? piece: reverseElement(piece));
        }
        if (!closed && loop.length === 1 && loop[0].type === "CircularArc"
                && close(loop[0].start, loop[0].end)) {
            closed = true;
        }
        if (closed) {
            // Make the loop exactly closed.
            for (let i = 0; i < loop.length; ++i)
                loop[(i + 1) % loop.length] = {...loop[(i + 1) % loop.length], start: loop[i].end};
            loops.push(loop);
        } else {
            ++open;
        }
    }
    if (open > 0)
        warnings.add(`${open} contour(s) not closed were ignored`);
    return loops;
}

// Points of a loop, arcs being approximated by segments.
function loopPoints(loop) {
    const points = [];
    for (const element of loop) {
        if (element.type !== "CircularArc") {
            points.push(element.start);
            continue;
        }
        const {start, end, center} = element;
        const radius = Math.hypot(start.x - center.x, start.y - center.y);
        const a0 = Math.atan2(start.y - center.y, start.x - center.x);
        let sweep;
        if (element.orientation === "Full") {
            sweep = 2 * Math.PI;
        } else {
            sweep = Math.atan2(end.y - center.y, end.x - center.x) - a0;
            if (element.orientation === "Anticlockwise" && sweep <= 0)
                sweep += 2 * Math.PI;
            if (element.orientation === "Clockwise" && sweep >= 0)
                sweep -= 2 * Math.PI;
        }
        const steps = Math.max(2, Math.ceil(Math.abs(sweep) / (Math.PI / 64)));
        for (let i = 0; i < steps; ++i)
            points.push(arcPoint(center, radius, a0 + sweep * i / steps));
    }
    return points;
}

function signedArea(points) {
    let area = 0;
    points.forEach((p, i) => {
        const q = points[(i + 1) % points.length];
        area += p.x * q.y - q.x * p.y;
    });
    return area / 2;
}

function containsPoint(points, p) {
    let inside = false;
    for (let i = 0, j = points.length - 1; i < points.length; j = i++) {
        const a = points[i];
        const b = points[j];
        if ((a.y > p.y) !== (b.y > p.y)
                && p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x) {
            inside = !inside;
        }
    }
    return inside;
}

function anticlockwise(loop, area) {
    return (area >= 0)? loop: loop.slice().reverse().map(reverseElement);
}

function boundingBox(points) {
    return {
        xMin: Math.min(...points.map((p) => p.x)),
        xMax: Math.max(...points.map((p) => p.x)),
        yMin: Math.min(...points.map((p) => p.y)),
        yMax: Math.max(...points.map((p) => p.y)),
    };
}

function translateLoop(loop, dx, dy) {
    const move = (p) => point(p.x + dx, p.y + dy);
    return loop.map((e) => {
        const moved = {...e, start: move(e.start), end: move(e.end)};
        if (e.type === "CircularArc")
            moved.center = move(e.center);
        return moved;
    });
}

/////////////////////////////////////////////////////////////////////////////
// Parts
/////////////////////////////////////////////////////////////////////////////

// Read the parts of a DXF file. Returns '{parts, units, warnings}': each part
// is '{shape, holes, width, height, area}' ('shape' and 'holes' are "general"
// shapes), 'units' the unit of the drawing if given ("millimeters"...),
// 'warnings' a list of messages.
export function readParts(text) {
    const pairs = readPairs(text);
    const sections = readSections(pairs);
    if (!("ENTITIES" in sections))
        throw new Error("invalid DXF file: no ENTITIES section.");
    const warnings = new Set();
    const result = {loops: [], pieces: []};
    collect(readEntities(pairs, ...sections.ENTITIES), readBlocks(pairs, sections),
        IDENTITY, result, warnings);

    // Tolerance for joining the ends of the pieces, relative to the size of
    // the drawing.
    const allPoints = [...result.loops.flat(), ...result.pieces].flatMap((e) => [e.start, e.end]);
    let tolerance = 1e-6;
    if (allPoints.length > 0) {
        const box = boundingBox(allPoints);
        tolerance = 1e-6 * Math.max(1, box.xMax - box.xMin, box.yMax - box.yMin);
    }
    const loops = [...result.loops, ...chain(result.pieces, tolerance, warnings)].map((loop) => {
        const points = loopPoints(loop);
        const area = signedArea(points);
        return {loop: anticlockwise(loop, area), points, area: Math.abs(area)};
    }).filter((l) => l.area > 0);

    // Nesting: the loops containing each loop.
    for (const l of loops) {
        const p = l.points[0];
        l.parents = loops.filter((m) => m !== l && m.area > l.area && containsPoint(m.points, p));
        l.depth = l.parents.length;
    }
    const parts = [];
    for (const outer of loops.filter((l) => l.depth % 2 === 0)) {
        const holes = loops.filter((l) => l.depth === outer.depth + 1 && l.parents.includes(outer));
        const box = boundingBox(outer.points);
        const dx = -box.xMin;
        const dy = -box.yMin;
        parts.push({
            shape: {type: "general", elements: translateLoop(outer.loop, dx, dy)},
            holes: holes.map((h) => ({type: "general", elements: translateLoop(h.loop, dx, dy)})),
            width: box.xMax - box.xMin,
            height: box.yMax - box.yMin,
            area: outer.area - holes.reduce((sum, h) => sum + h.area, 0),
            // Position in the drawing, to order the parts.
            x: box.xMin,
            y: box.yMin,
        });
    }
    // In reading order: top to bottom, then left to right.
    parts.sort((p, q) => (q.y + q.height) - (p.y + p.height) || p.x - q.x);
    return {
        parts: parts.map(({x, y, ...part}) => part),
        units: readUnits(pairs, sections),
        warnings: [...warnings],
    };
}
