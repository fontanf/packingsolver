// Run with: node --test wasm/tests/*.test.js wasm/tests/*.test.mjs
// The instances built by the irregular form of the web page.

import assert from "node:assert";
import {createRequire} from "node:module";
import test from "node:test";

import * as irregularForm from "../web/irregular_form.js";

const require = createRequire(import.meta.url);
const {loadModule} = require("./helpers.js");

const example = irregularForm.EXAMPLE;
const binRows = example.binTypes.map((t) => ({...irregularForm.defaultBinRow(), ...t}));
const itemRows = example.itemTypes.map((t) => ({...irregularForm.defaultItemRow(), ...t}));

test("irregular form: rectangle", () => {
    const shape = irregularForm.rowShape({shape: "rectangle", width: "3", height: 2});
    assert.deepStrictEqual(shape.elements.map((e) => [e.start, e.end]), [
        [{x: 0, y: 0}, {x: 3, y: 0}],
        [{x: 3, y: 0}, {x: 3, y: 2}],
        [{x: 3, y: 2}, {x: 0, y: 2}],
        [{x: 0, y: 2}, {x: 0, y: 0}],
    ]);
});

test("irregular form: circle", () => {
    const shape = irregularForm.rowShape({shape: "circle", radius: 4});
    assert.deepStrictEqual(shape.elements, [{
        type: "CircularArc",
        start: {x: 4, y: 0},
        end: {x: 4, y: 0},
        center: {x: 0, y: 0},
        orientation: "Full",
    }]);
});

test("irregular form: clockwise polygons are reversed", () => {
    const anticlockwise = irregularForm.rowShape({shape: "polygon", vertices: "0 0, 4 0, 0 3"});
    const clockwise = irregularForm.rowShape({shape: "polygon", vertices: "0 0,0 3,  4 0"});
    const starts = (shape) => shape.elements.map((e) => [e.start.x, e.start.y]);
    assert.deepStrictEqual(starts(anticlockwise), [[0, 0], [4, 0], [0, 3]]);
    assert.deepStrictEqual(starts(clockwise), [[4, 0], [0, 3], [0, 0]]);
});

test("irregular form: invalid inputs", () => {
    const shape = (row) => () => irregularForm.rowShape(row);
    assert.throws(shape({shape: "polygon", vertices: "0 0, 1 0"}), /at least 3 vertices/);
    assert.throws(shape({shape: "polygon", vertices: "0 0, 1, 0 1"}), /invalid vertex "1"/);
    assert.throws(shape({shape: "polygon", vertices: "0 0, 1 1, 2 2"}), /no area/);
    assert.throws(shape({shape: "rectangle", width: "", height: 1}), /invalid width/);
    assert.throws(shape({shape: "circle", radius: -1}), /invalid radius/);
    // Errors mention the row.
    assert.throws(
        () => irregularForm.instance("bin-packing", binRows, [
            itemRows[0], {...itemRows[1], copies: 0}]),
        /item type 2: invalid number of copies/);
});

test("irregular form: rotations and mirroring", () => {
    const instance = irregularForm.instance("knapsack", binRows, [
        {...itemRows[0], rotations: "quarter", mirror: true, profit: "7"}]);
    const itemType = instance.item_types[0];
    assert.deepStrictEqual(
        itemType.allowed_rotations.map((r) => [r.start, r.end]),
        [[0, 0], [90, 90], [180, 180], [270, 270]]);
    assert.strictEqual(itemType.allow_mirroring, true);
    assert.strictEqual(itemType.profit, 7);
});

test("irregular form: the example solves", async () => {
    const module = await loadModule();
    const instance = irregularForm.instance(example.objective, binRows, itemRows);
    const result = JSON.parse(module.solve(
        "irregular", JSON.stringify(instance),
        JSON.stringify({optimization_mode: "not-anytime-sequential"})));
    assert.strictEqual(result.error, undefined);
    const numberOfItems = example.itemTypes.reduce((sum, t) => sum + t.copies, 0);
    assert.strictEqual(result.output.Solution.NumberOfItems, numberOfItems);
});

test("irregular form: arcs of the preview", () => {
    const center = {x: 1, y: 2};
    const close = (a, b) => Math.abs(a - b) < 1e-9;
    // Full circle: every point on the circle, closed.
    const full = irregularForm.shapePoints([{
        type: "CircularArc", start: {x: 4, y: 2}, end: {x: 4, y: 2}, center, orientation: "Full"}]);
    assert.ok(full.length > 32);
    assert.ok(full.every(([x, y]) => close(Math.hypot(x - 1, y - 2), 3)));
    assert.ok(close(full[0][0], full[full.length - 1][0]) && close(full[0][1], full[full.length - 1][1]));
    // Quarter arcs from (4, 2) to (1, 5): anticlockwise through the first
    // quadrant, clockwise through the three others.
    const arc = (orientation) => irregularForm.shapePoints([{
        type: "CircularArc", start: {x: 4, y: 2}, end: {x: 1, y: 5}, center, orientation}]);
    const anticlockwise = arc("Anticlockwise");
    const clockwise = arc("Clockwise");
    assert.ok(anticlockwise.every(([x, y]) => x >= 1 - 1e-9 && y >= 2 - 1e-9));
    assert.ok(clockwise.some(([x, y]) => y < 2 - 1));
    for (const points of [anticlockwise, clockwise]) {
        const last = points[points.length - 1];
        assert.ok(close(last[0], 1) && close(last[1], 5));
    }
});

test("irregular form: preview figure", () => {
    const instance = irregularForm.instance(example.objective, binRows, itemRows);
    const figure = irregularForm.previewFigure(instance);
    // One cell (trace and annotation) per bin and item type, 4 per row.
    assert.strictEqual(figure.data.length, 5);
    assert.strictEqual(figure.layout.annotations.length, 5);
    assert.strictEqual(figure.layout.height, 2 * 260 + 70 + 60);
    assert.strictEqual(figure.layout.annotations[2].text, "Item type 2 (×6)");
    assert.strictEqual(figure.data[4].xaxis, "x5");
});

test("irregular form: shapes from DXF files", () => {
    const hole = irregularForm.rowShape({shape: "circle", radius: 2});
    const part = {
        shape: irregularForm.rowShape({shape: "rectangle", width: 10, height: 10}),
        holes: [hole], width: 10, height: 10, name: "plate", warnings: [],
    };
    const item = {...irregularForm.defaultItemRow(), shape: "dxf", dxf: part};
    const instance = irregularForm.instance("knapsack", binRows, [item]);
    assert.deepStrictEqual(instance.item_types[0].shapes[0].holes, [hole]);
    // A bin can't have holes; a row needs its file.
    assert.throws(
        () => irregularForm.instance("knapsack", [{...binRows[0], shape: "dxf", dxf: part}], [item]),
        /bin type 1: a bin can't have holes/);
    assert.throws(
        () => irregularForm.instance("knapsack", binRows, [{...item, dxf: null}]),
        /item type 1: load a DXF file/);
});
