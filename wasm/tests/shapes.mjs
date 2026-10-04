// Checks of the parts read from drawings, shared by the tests of the DXF and
// SVG readers.

import assert from "node:assert";

// Exact signed area of a loop of line segments and circular arcs: the area
// of the polygon of their ends, plus the circular segments of the arcs.
export function exactArea(elements) {
    let area = 0;
    for (const e of elements) {
        area += (e.start.x * e.end.y - e.end.x * e.start.y) / 2;
        if (e.type !== "CircularArc")
            continue;
        const radius = Math.hypot(e.start.x - e.center.x, e.start.y - e.center.y);
        let sweep;
        if (e.orientation === "Full") {
            sweep = 2 * Math.PI;
        } else {
            const a0 = Math.atan2(e.start.y - e.center.y, e.start.x - e.center.x);
            const a1 = Math.atan2(e.end.y - e.center.y, e.end.x - e.center.x);
            sweep = a1 - a0;
            if (e.orientation === "Anticlockwise" && sweep <= 0)
                sweep += 2 * Math.PI;
            if (e.orientation === "Clockwise" && sweep >= 0)
                sweep -= 2 * Math.PI;
        }
        area += radius * radius / 2 * (sweep - Math.sin(sweep));
    }
    return area;
}

export function assertClosed(elements, where) {
    elements.forEach((e, i) => {
        const next = elements[(i + 1) % elements.length];
        assert.ok(
            Math.hypot(e.end.x - next.start.x, e.end.y - next.start.y) < 1e-9,
            `${where}: element ${i} isn't followed by the next one`);
    });
}

export function assertClose(actual, expected, tolerance, where) {
    assert.ok(
        Math.abs(actual - expected) <= tolerance * Math.max(1, Math.abs(expected)),
        `${where}: ${actual} != ${expected}`);
}

// Check parts against the expected ones ('{area, holes, width, height}').
export function assertParts(parts, expectedParts) {
    assert.strictEqual(parts.length, expectedParts.length, "number of parts");
    parts.forEach((part, i) => {
        const where = `part ${i}`;
        const e = expectedParts[i];
        assert.strictEqual(part.holes.length, e.holes, `${where}: number of holes`);
        const outerArea = exactArea(part.shape.elements);
        assert.ok(outerArea > 0, `${where}: not anticlockwise`);
        assertClosed(part.shape.elements, where);
        let area = outerArea;
        part.holes.forEach((hole, h) => {
            const holeArea = exactArea(hole.elements);
            assert.ok(holeArea > 0, `${where}, hole ${h}: not anticlockwise`);
            assertClosed(hole.elements, `${where}, hole ${h}`);
            area -= holeArea;
        });
        assertClose(area, e.area, 1e-9, `${where}: area`);
        // The bounding box is measured on approximated arcs.
        assertClose(part.width, e.width, 1e-3, `${where}: width`);
        assertClose(part.height, e.height, 1e-3, `${where}: height`);
    });
}

// Whether a point is inside the outline of a part (arcs approximated).
export function insidePart(part, x, y) {
    const points = [];
    for (const e of part.shape.elements) {
        if (e.type !== "CircularArc") {
            points.push(e.start);
            continue;
        }
        const r = Math.hypot(e.start.x - e.center.x, e.start.y - e.center.y);
        const a0 = Math.atan2(e.start.y - e.center.y, e.start.x - e.center.x);
        let sweep = (e.orientation === "Full")? 2 * Math.PI:
            Math.atan2(e.end.y - e.center.y, e.end.x - e.center.x) - a0;
        if (e.orientation === "Anticlockwise" && sweep <= 0)
            sweep += 2 * Math.PI;
        if (e.orientation === "Clockwise" && sweep >= 0)
            sweep -= 2 * Math.PI;
        for (let k = 0; k < 64; ++k) {
            const a = a0 + sweep * k / 64;
            points.push({x: e.center.x + r * Math.cos(a), y: e.center.y + r * Math.sin(a)});
        }
    }
    let inside = false;
    for (let i = 0, j = points.length - 1; i < points.length; j = i++) {
        const a = points[i];
        const b = points[j];
        if ((a.y > y) !== (b.y > y) && x < (b.x - a.x) * (y - a.y) / (b.y - a.y) + a.x)
            inside = !inside;
    }
    return inside;
}
