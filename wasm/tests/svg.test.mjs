// Run with: node --test wasm/tests/*.test.js wasm/tests/*.test.mjs
// The SVG reader of the web page, on the files of 'fixtures/svg/'
// ('generate_svg_fixtures.py').

import assert from "node:assert";
import fs from "node:fs";
import {createRequire} from "node:module";
import path from "node:path";
import test from "node:test";
import {fileURLToPath} from "node:url";

import {readParts} from "../web/svg.js";
import {assertParts, insidePart} from "./shapes.mjs";

const require = createRequire(import.meta.url);
const {loadModule} = require("./helpers.js");

const DIRECTORY = path.join(path.dirname(fileURLToPath(import.meta.url)), "fixtures", "svg");

function read(name) {
    return readParts(fs.readFileSync(path.join(DIRECTORY, name + ".svg"), "utf8"));
}

for (const name of fs.readdirSync(DIRECTORY).filter((f) => f.endsWith(".svg")).sort()) {
    const fixture = name.slice(0, -4);
    test(`svg: ${fixture}`, () => {
        const expected = JSON.parse(fs.readFileSync(path.join(DIRECTORY, fixture + ".json"), "utf8"));
        if (expected.error !== undefined) {
            assert.throws(() => read(fixture), new RegExp(expected.error));
            return;
        }
        const {parts, units, warnings} = read(fixture);
        assert.strictEqual(units, expected.units);
        assert.strictEqual(warnings.length, expected.warnings, `warnings: ${warnings}`);
        assertParts(parts, expected.parts);
    });
}

test("svg: orientation as in SVG editors", () => {
    const [l, d] = read("orientation").parts;
    // The foot of the "L" is at the bottom right.
    assert.ok(insidePart(l, 25, 5));
    assert.ok(!insidePart(l, 25, 25));
    assert.ok(insidePart(l, 5, 25));
    // The "D" bulges to the right: its straight side is on the left.
    assert.ok(insidePart(d, 1, 1));
    assert.ok(!insidePart(d, 9.5, 1));
});

test("svg: invalid files", () => {
    assert.throws(() => readParts("<html></html>"), /no 'svg' element/);
    assert.throws(
        () => readParts('<svg xmlns="http://www.w3.org/2000/svg"><path d="M 0 0 L 10"/></svg>'),
        /invalid path data/);
});

test("svg: the parts solve", async () => {
    const {parts} = read("transforms");
    const bin = [[0, 0], [100, 0], [100, 60], [0, 60]];
    const instance = {
        objective: "knapsack",
        bin_types: [{
            type: "general",
            elements: bin.map((p, i) => ({
                type: "LineSegment",
                start: {x: p[0], y: p[1]},
                end: {x: bin[(i + 1) % 4][0], y: bin[(i + 1) % 4][1]},
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
