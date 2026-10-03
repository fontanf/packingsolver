// Run with: node --test wasm/tests/*.test.js wasm/tests/*.test.mjs
// The DXF reader of the web page, on the files of 'fixtures/dxf/'
// ('generate_dxf_fixtures.py').

import assert from "node:assert";
import fs from "node:fs";
import {createRequire} from "node:module";
import path from "node:path";
import test from "node:test";
import {fileURLToPath} from "node:url";

import {readParts} from "../web/dxf.js";

const require = createRequire(import.meta.url);
const {loadModule} = require("./helpers.js");

const DIRECTORY = path.join(path.dirname(fileURLToPath(import.meta.url)), "fixtures", "dxf");

function read(name) {
    return readParts(fs.readFileSync(path.join(DIRECTORY, name + ".dxf"), "utf8"));
}

// Exact signed area of a loop of line segments and circular arcs: the area
// of the polygon of their ends, plus the circular segments of the arcs.
function exactArea(elements) {
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

function assertClosed(elements, where) {
    elements.forEach((e, i) => {
        const next = elements[(i + 1) % elements.length];
        assert.ok(
            Math.hypot(e.end.x - next.start.x, e.end.y - next.start.y) < 1e-9,
            `${where}: element ${i} isn't followed by the next one`);
    });
}

function assertClose(actual, expected, tolerance, where) {
    assert.ok(
        Math.abs(actual - expected) <= tolerance * Math.max(1, Math.abs(expected)),
        `${where}: ${actual} != ${expected}`);
}

for (const name of fs.readdirSync(DIRECTORY).filter((f) => f.endsWith(".dxf")).sort()) {
    const fixture = name.slice(0, -4);
    test(`dxf: ${fixture}`, () => {
        const expected = JSON.parse(fs.readFileSync(path.join(DIRECTORY, fixture + ".json"), "utf8"));
        const {parts, warnings} = read(fixture);
        assert.strictEqual(parts.length, expected.parts.length, "number of parts");
        assert.strictEqual(warnings.length, expected.warnings, `warnings: ${warnings}`);
        parts.forEach((part, i) => {
            const where = `part ${i}`;
            const e = expected.parts[i];
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
    });
}

test("dxf: units", () => {
    assert.strictEqual(read("plate_with_hole").units, "millimeters");
    assert.strictEqual(read("r12_polyline").units, null);
});

test("dxf: warnings", () => {
    const {warnings} = read("multiple_parts");
    assert.ok(warnings.some((w) => /SPLINE/.test(w)));
    assert.ok(warnings.some((w) => /1 contour\(s\) not closed/.test(w)));
});

test("dxf: invalid files", () => {
    assert.throws(() => readParts("not a dxf file\n"), /group code expected/);
    assert.throws(() => readParts("0\nSECTION\n2\nHEADER\n0\nENDSEC\n0\nEOF\n"), /no ENTITIES section/);
});

test("dxf: the parts solve", async () => {
    const {parts} = read("multiple_parts");
    const instance = {
        objective: "knapsack",
        bin_types: [{
            type: "general",
            elements: [[0, 0], [100, 0], [100, 50], [0, 50]].map((p, i, a) => ({
                type: "LineSegment",
                start: {x: p[0], y: p[1]},
                end: {x: a[(i + 1) % 4][0], y: a[(i + 1) % 4][1]},
            })),
        }],
        item_types: parts.map((part) => ({shapes: [{...part.shape, holes: part.holes}]})),
    };
    const module = await loadModule();
    const result = JSON.parse(module.solve(
        "irregular", JSON.stringify(instance),
        JSON.stringify({optimization_mode: "not-anytime-sequential"})));
    assert.strictEqual(result.error, undefined);
    assert.strictEqual(result.output.Solution.NumberOfItems, parts.length);
});
