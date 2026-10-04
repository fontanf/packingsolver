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
import {assertParts} from "./shapes.mjs";

const require = createRequire(import.meta.url);
const {loadModule} = require("./helpers.js");

const DIRECTORY = path.join(path.dirname(fileURLToPath(import.meta.url)), "fixtures", "dxf");

function read(name) {
    return readParts(fs.readFileSync(path.join(DIRECTORY, name + ".dxf"), "utf8"));
}

for (const name of fs.readdirSync(DIRECTORY).filter((f) => f.endsWith(".dxf")).sort()) {
    const fixture = name.slice(0, -4);
    test(`dxf: ${fixture}`, () => {
        const expected = JSON.parse(fs.readFileSync(path.join(DIRECTORY, fixture + ".json"), "utf8"));
        if (expected.error !== undefined) {
            assert.throws(() => read(fixture), new RegExp(expected.error));
            return;
        }
        const {parts, warnings} = read(fixture);
        assert.strictEqual(warnings.length, expected.warnings, `warnings: ${warnings}`);
        assertParts(parts, expected.parts);
    });
}

test("dxf: units", () => {
    assert.strictEqual(read("plate_with_hole").units, "millimeters");
    assert.strictEqual(read("r12_polyline").units, null);
});

test("dxf: warnings", () => {
    const {warnings} = read("multiple_parts");
    assert.deepStrictEqual(warnings, ["1 contour(s) not closed were ignored"]);
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
