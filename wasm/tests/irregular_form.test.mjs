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

test("irregular form: arcs of the thumbnails", () => {
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

test("irregular form: thumbnails", () => {
    // The view box of an "L" (a polygon) in coordinates flipped vertically,
    // with a margin of 3% of its size.
    const l = irregularForm.rowThumbnail(itemRows[2], true);
    assert.deepStrictEqual(l.viewBox.map((v) => Number(v.toFixed(6))), [-0.6, -20.6, 21.2, 21.2]);
    assert.strictEqual(l.title, "20 × 20");
    assert.strictEqual(l.paths.length, 1);
    assert.strictEqual(l.paths[0].d, "M0 0 L20 0 L20 0 L20 10 L20 10 L10 10 L10 10 L10 20 L10 20 L0 20 L0 20 L0 0 Z");
    // A circle centered at the origin.
    const circle = irregularForm.rowThumbnail(itemRows[1], true);
    assert.strictEqual(circle.title, "16 × 16");
    assert.ok(Math.abs(circle.viewBox[0] + 8.48) < 1e-9);
    // An item with a hole: one path, both loops in it.
    const hole = irregularForm.rowShape({shape: "circle", radius: 2});
    const part = {
        shape: irregularForm.rowShape({shape: "rectangle", width: 10, height: 10}),
        holes: [hole], width: 10, height: 10, name: "plate", warnings: [],
    };
    const plate = irregularForm.rowThumbnail({...irregularForm.defaultItemRow(), shape: "file", file: part}, true);
    assert.strictEqual(plate.paths.length, 1);
    assert.strictEqual(plate.paths[0].d.split("M").length - 1, 2);
    // Invalid rows.
    assert.deepStrictEqual(
        irregularForm.rowThumbnail({...itemRows[0], width: ""}, true), {error: 'invalid width: "".'});
    assert.match(irregularForm.rowThumbnail({...binRows[0], defects: [{
        ...irregularForm.defaultDefect(), x: 1}]}, false).error, /invalid y/);
});

test("irregular form: shapes from files", () => {
    const hole = irregularForm.rowShape({shape: "circle", radius: 2});
    const part = {
        shape: irregularForm.rowShape({shape: "rectangle", width: 10, height: 10}),
        holes: [hole], width: 10, height: 10, name: "plate", warnings: [],
    };
    const item = {...irregularForm.defaultItemRow(), shape: "file", file: part};
    const instance = irregularForm.instance("knapsack", binRows, [item]);
    assert.deepStrictEqual(instance.item_types[0].shapes[0].holes, [hole]);
    // A bin can't have holes; a row needs its file.
    assert.throws(
        () => irregularForm.instance("knapsack", [{...binRows[0], shape: "file", file: part}], [item]),
        /bin type 1: a bin can't have holes/);
    assert.throws(
        () => irregularForm.instance("knapsack", binRows, [{...item, file: null}]),
        /item type 1: load a DXF or SVG file/);
});

test("irregular form: defects and spacings", async () => {
    const defects = [
        {...irregularForm.defaultDefect(), x: 10, y: 20, width: 5, height: 4, spacing: "1"},
        {...irregularForm.defaultDefect(), shape: "circle", x: 50, y: 25, radius: 3},
        {...irregularForm.defaultDefect(), shape: "polygon", vertices: "70 10, 80 10, 75 20"},
    ];
    const bin = {...binRows[0], spacing: "2", defects};
    const instance = irregularForm.instance(example.objective, [bin], itemRows);
    const binType = instance.bin_types[0];
    assert.strictEqual(binType.item_bin_minimum_spacing, 2);
    assert.strictEqual(binType.defects.length, 3);
    assert.deepStrictEqual(binType.defects[0].elements[0].start, {x: 10, y: 20});
    assert.deepStrictEqual(binType.defects[0].elements[1].start, {x: 15, y: 20});
    assert.strictEqual(binType.defects[0].item_defect_minimum_spacing, 1);
    assert.deepStrictEqual(binType.defects[1].elements[0].center, {x: 50, y: 25});
    assert.deepStrictEqual(binType.defects[1].elements[0].start, {x: 53, y: 25});
    assert.deepStrictEqual(binType.defects[2].elements[0].start, {x: 70, y: 10});
    assert.ok(!("item_defect_minimum_spacing" in binType.defects[1]));
    assert.throws(
        () => irregularForm.instance(example.objective,
            [{...bin, defects: [{...defects[0], x: ""}]}], itemRows),
        /bin type 1: defect 1: invalid x/);
    // The thumbnail of the bin draws the defects.
    const thumbnail = irregularForm.rowThumbnail(bin, false);
    assert.strictEqual(thumbnail.paths.filter((p) => p.fill === "#ef553b").length, 3);
    // The solver reads them.
    instance.parameters = {item_item_minimum_spacing: 0.5};
    const module = await loadModule();
    const result = JSON.parse(module.solve(
        "irregular", JSON.stringify(instance),
        JSON.stringify({optimization_mode: "not-anytime-sequential"})));
    assert.strictEqual(result.error, undefined);
    assert.ok(result.output.Solution.NumberOfItems > 0);
});

test("irregular form: holes", async () => {
    const holes = [
        {...irregularForm.defaultHole(), x: 2, y: 3, width: 4, height: 2},
        {...irregularForm.defaultHole(), shape: "circle", x: 15, y: 5, radius: 2},
        {...irregularForm.defaultHole(), shape: "polygon", vertices: "2 12, 6 12, 2 16"},
    ];
    const item = {...irregularForm.defaultItemRow(), width: 20, height: 20, holes};
    const instance = irregularForm.instance(example.objective, binRows, [item]);
    const shape = instance.item_types[0].shapes[0];
    assert.strictEqual(shape.holes.length, 3);
    assert.deepStrictEqual(shape.holes[0].elements[0].start, {x: 2, y: 3});
    assert.deepStrictEqual(shape.holes[1].elements[0].center, {x: 15, y: 5});
    assert.deepStrictEqual(shape.holes[2].elements[0].start, {x: 2, y: 12});
    // The holes are drawn in the thumbnail: one path with the four loops.
    const thumbnail = irregularForm.rowThumbnail(item, true);
    assert.strictEqual(thumbnail.paths.length, 1);
    assert.strictEqual(thumbnail.paths[0].d.split("M").length - 1, 4);
    // The holes of a file come first.
    const part = {
        shape: irregularForm.rowShape({shape: "rectangle", width: 20, height: 20}),
        holes: [irregularForm.rowShape({shape: "circle", radius: 1})], width: 20, height: 20,
        name: "plate", warnings: [],
    };
    const fileItem = {...item, shape: "file", file: part, holes: [holes[0]]};
    assert.deepStrictEqual(irregularForm.itemShape(fileItem).holes, [part.holes[0], shape.holes[0]]);
    // Invalid holes: outside the item, touching its border, invalid values.
    const error = (hole) => () => irregularForm.instance(
        example.objective, binRows, [{...item, holes: [holes[0], hole]}]);
    assert.throws(error({...holes[0], x: 18}), /item type 1: hole 2: it must be inside the item/);
    assert.throws(error({...holes[0], x: 0}), /hole 2: it must be inside the item/);
    assert.throws(error({...holes[1], radius: 6}), /hole 2: it must be inside the item/);
    assert.throws(error({...holes[0], y: ""}), /hole 2: invalid y/);
    // The solver reads them.
    const module = await loadModule();
    const result = JSON.parse(module.solve(
        "irregular", JSON.stringify(instance),
        JSON.stringify({optimization_mode: "not-anytime-sequential"})));
    assert.strictEqual(result.error, undefined);
    assert.strictEqual(result.output.Solution.NumberOfItems, 1);
});

test("irregular form: profits and costs only for their objectives", () => {
    const bins = [{...binRows[0], cost: "3"}];
    const items = [{...itemRows[0], profit: "7"}];
    const knapsack = irregularForm.instance("knapsack", bins, items);
    assert.strictEqual(knapsack.item_types[0].profit, 7);
    assert.ok(!("cost" in knapsack.bin_types[0]));
    const variableSized = irregularForm.instance("variable-sized-bin-packing", bins, items);
    assert.strictEqual(variableSized.bin_types[0].cost, 3);
    assert.ok(!("profit" in variableSized.item_types[0]));
    const binPacking = irregularForm.instance("bin-packing", bins, items);
    assert.ok(!("cost" in binPacking.bin_types[0]) && !("profit" in binPacking.item_types[0]));
});

test("irregular form: unlimited copies", async () => {
    const bins = [{...binRows[0], unlimited_copies: true}];
    const items = [{...itemRows[0], unlimited_copies: true}];
    // Bins except for the knapsack objective, items only for it.
    const binPacking = irregularForm.instance("bin-packing", bins, items);
    assert.strictEqual(binPacking.bin_types[0].copies, -1);
    assert.strictEqual(binPacking.item_types[0].copies, itemRows[0].copies);
    const knapsack = irregularForm.instance("knapsack", bins, items);
    assert.strictEqual(knapsack.bin_types[0].copies, binRows[0].copies);
    assert.strictEqual(knapsack.item_types[0].copies, -1);
    // The solver reads them.
    const module = await loadModule();
    for (const instance of [binPacking, knapsack]) {
        const result = JSON.parse(module.solve(
            "irregular", JSON.stringify(instance),
            JSON.stringify({optimization_mode: "not-anytime-sequential"})));
        assert.strictEqual(result.error, undefined);
        assert.ok(result.output.Solution.NumberOfItems > 0);
    }
});
