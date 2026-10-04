// Contours of the parts read from drawings (DXF, SVG files): elements
// (line segments and circular arcs), chained into closed loops, nested into
// parts with holes.
//
// Elements: {type: "LineSegment", start, end} or {type: "CircularArc",
// start, end, center, orientation: "Anticlockwise" | "Clockwise" | "Full"}.
// Transformations: 'p -> [a b; c d] p + [e f]'.
/////////////////////////////////////////////////////////////////////////////
// Geometry
/////////////////////////////////////////////////////////////////////////////

export function point(x, y) {
    return {x, y};
}

// 2D transformation 'p -> [a b; c d] p + [e f]'.
export const IDENTITY = {a: 1, b: 0, c: 0, d: 1, e: 0, f: 0};

export function apply(t, p) {
    return point(t.a * p.x + t.b * p.y + t.e, t.c * p.x + t.d * p.y + t.f);
}

export function compose(t, u) {
    // t after u.
    return {
        a: t.a * u.a + t.b * u.c, b: t.a * u.b + t.b * u.d,
        c: t.c * u.a + t.d * u.c, d: t.c * u.b + t.d * u.d,
        e: t.a * u.e + t.b * u.f + t.e, f: t.c * u.e + t.d * u.f + t.f,
    };
}

export const reverseOrientation = {Anticlockwise: "Clockwise", Clockwise: "Anticlockwise", Full: "Full"};

// Transform an element; arcs require a similarity (uniform scaling).
export function transformElement(t, element, warnings) {
    const result = {...element, start: apply(t, element.start), end: apply(t, element.end)};
    if (element.type === "CircularArc") {
        const scaleX = Math.hypot(t.a, t.c);
        const scaleY = Math.hypot(t.b, t.d);
        if (Math.abs(scaleX - scaleY) > 1e-9 * Math.max(scaleX, scaleY)) {
            if (warnings !== undefined)
                warnings.add("arcs scaled non-uniformly (ellipses) are not supported");
            return null;
        }
        result.center = apply(t, element.center);
        if (t.a * t.d - t.b * t.c < 0)
            result.orientation = reverseOrientation[element.orientation];
    }
    return result;
}

export function arcPoint(center, radius, angle) {
    return point(center.x + radius * Math.cos(angle), center.y + radius * Math.sin(angle));
}

export function reverseElement(element) {
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

// Build the parts from closed loops ('result.loops') and pieces to chain
// ('result.pieces'): '[{shape, holes, width, height, area}]', in reading
// order (top to bottom, then left to right). Moved so that their bounding
// box starts at (0, 0).
export function buildParts(result, warnings) {
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
    return parts.map(({x, y, ...part}) => part);
}
