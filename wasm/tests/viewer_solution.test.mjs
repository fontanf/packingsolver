// Run with: node --test wasm/tests/*.test.js wasm/tests/*.test.mjs
// The certificates read by the solution viewer ('viewer/solution.js').

import assert from "node:assert";
import fs from "node:fs";
import path from "node:path";
import test from "node:test";
import {fileURLToPath} from "node:url";

import {parseCsv} from "../web/visualize/common.js";
import * as viewerSolution from "../web/viewer/solution.js";

const TESTS = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.join(TESTS, "..", "..");
const read = (...parts) => fs.readFileSync(path.join(ROOT, ...parts), "utf8");

test("viewer: problem types of the certificates", () => {
    const certificates = {
        rectangleguillotine: read("data", "rectangleguillotine", "tests", "bin_packing_3nvr", "solution.csv"),
        rectangle: read("data", "rectangle", "tests", "bin_packing_merge_identical_items_with_defects", "solution.csv"),
        onedimensional: read("data", "onedimensional", "tests", "bin_packing_perfect_pair", "solution.csv"),
        irregular: read("wasm", "tests", "fixtures", "visualize", "irregular", "inputs", "arcs_solution.json"),
        box: read("wasm", "tests", "fixtures", "visualize", "box", "defects.csv"),
        boxstacks: read("wasm", "tests", "fixtures", "visualize", "boxstacks", "copies.csv"),
    };
    for (const [problemType, text] of Object.entries(certificates))
        assert.strictEqual(viewerSolution.detectProblemType(text), problemType);
    assert.strictEqual(viewerSolution.detectProblemType("WIDTH,HEIGHT\n1,2"), null);
    assert.strictEqual(viewerSolution.detectProblemType('{"bin_types": []}'), null);
    assert.strictEqual(viewerSolution.detectProblemType("{"), null);
    // Box and boxstacks: plotly figures.
    assert.throws(() => viewerSolution.readSolution(certificates.box), /aren't drawn by this viewer/);
    assert.throws(() => viewerSolution.readSolution("a,b\n1,2"), /not a certificate/);
});

test("viewer: rectangle", () => {
    const text = read("data", "rectangle", "tests", "bin_packing_merge_identical_items_with_defects", "solution.csv");
    const solution = viewerSolution.readSolution(text);
    const rows = parseCsv(text);
    assert.strictEqual(solution.problemType, "rectangle");
    assert.strictEqual(solution.bins.length, rows.filter((r) => r.TYPE === "BIN").length);
    const bin = solution.bins[0];
    assert.deepStrictEqual(bin.box, {x0: 0, y0: 0, x1: 100, y1: 100});
    assert.strictEqual(bin.defects.length, rows.filter((r) => r.TYPE === "DEFECT" && r.BIN === "0").length);
    assert.strictEqual(bin.items.length, rows.filter((r) => r.TYPE === "ITEM" && r.BIN === "0").length);
    assert.deepStrictEqual(bin.items[0].box, {x0: 0, y0: 0, x1: 4, y1: 4});
    assert.strictEqual(bin.items[0].label, "Item type 0: 4 × 4 at (0, 0), group 0");
    // The copies of the bins.
    const copies = viewerSolution.readSolution(
        "TYPE,ID,COPIES,BIN,X,Y,LX,LY\nBIN,0,4,0,0,0,10,5\nITEM,1,4,0,0,0,2,3\nBIN,1,1,1,0,0,8,8\n");
    assert.deepStrictEqual(copies.bins.map((b) => [b.binTypeId, b.copies]), [[0, 4], [1, 1]]);
    assert.strictEqual(viewerSolution.numberOfBins(copies), 5);
});

test("viewer: rectangleguillotine", () => {
    // A certificate without COPIES column: a copy of each plate.
    const text = read("data", "rectangleguillotine", "tests", "knapsack_2evo_defects_1", "solution.csv");
    const rows = parseCsv(text);
    const solution = viewerSolution.readSolution(text);
    assert.strictEqual(solution.problemType, "rectangleguillotine");
    // The defects (TYPE -4) have no parent either.
    const plates = rows.filter((r) => !r.PARENT && r.TYPE !== "-4");
    assert.strictEqual(solution.bins.length, plates.length);
    assert.ok(solution.bins.every((bin) => bin.copies === 1 && bin.binTypeId === null));
    const bin = solution.bins[0];
    const ofPlate = rows.filter((r) => r.PLATE_ID === "0" && r.PARENT);
    assert.strictEqual(bin.items.length, ofPlate.filter((r) => Number(r.TYPE) >= 0).length);
    assert.strictEqual(bin.defects.length, rows.filter((r) => r.PLATE_ID === "0" && r.TYPE === "-4").length);
    assert.ok(bin.defects.length > 0);
    assert.strictEqual(bin.trims.length, ofPlate.filter((r) => r.CUT === "-1").length);
    assert.strictEqual(bin.wastes.length, ofPlate.filter((r) => r.TYPE === "-1" && r.CUT !== "-1").length);
    assert.strictEqual(bin.cuts.length,
        ofPlate.filter((r) => Number(r.TYPE) < 0 && r.TYPE !== "-4" && r.CUT !== "-1").length);
    // With the COPIES column.
    const copies = viewerSolution.readSolution(
        "PLATE_ID,COPIES,NODE_ID,X,Y,WIDTH,HEIGHT,TYPE,CUT,PARENT\n"
        + "0,3,0,0,0,100,50,-2,0,\n0,3,1,0,0,40,50,2,1,0\n0,3,2,40,0,60,50,-1,1,0\n");
    assert.strictEqual(copies.bins[0].copies, 3);
    assert.deepStrictEqual(copies.bins[0].items.map((item) => item.itemTypeId), [2]);
    assert.strictEqual(copies.bins[0].wastes.length, 1);
});

test("viewer: onedimensional", () => {
    // The copies of the bins.
    const text = read("data", "onedimensional", "tests", "bin_packing_dominant_set_all_fit", "solution.csv");
    const rows = parseCsv(text);
    const solution = viewerSolution.readSolution(text);
    assert.strictEqual(solution.problemType, "onedimensional");
    const bins = rows.filter((r) => r.TYPE === "BIN");
    assert.deepStrictEqual(solution.bins.map((bin) => bin.copies), bins.map((r) => Number(r.COPIES)));
    assert.strictEqual(viewerSolution.numberOfBins(solution), bins.reduce((n, r) => n + Number(r.COPIES), 0));
    assert.ok(viewerSolution.numberOfBins(solution) > solution.bins.length);
    const item = solution.bins[0].items[0];
    assert.strictEqual(item.box.y0, 0);
    assert.strictEqual(item.box.y1, 1);
});

test("viewer: irregular", () => {
    // Arcs: closed contours, the points of the arcs on their circles.
    const arcs = viewerSolution.readSolution(
        read("wasm", "tests", "fixtures", "visualize", "irregular", "inputs", "arcs_solution.json"));
    assert.strictEqual(arcs.problemType, "irregular");
    for (const bin of arcs.bins) {
        for (const item of bin.items) {
            for (const itemPath of item.paths) {
                for (const contour of itemPath)
                    assert.ok(contour.length >= 3);
            }
        }
    }
    // An item of several shapes: a path for each shape (overlapping shapes
    // aren't holes).
    assert.deepStrictEqual(arcs.bins[0].items.map((item) => item.paths.length), [1, 2]);
    // Its label in its largest shape, within its bounding box.
    const item = arcs.bins[0].items[1];
    const inside = (b, c) => b.x0 >= c.x0 && b.y0 >= c.y0 && b.x1 <= c.x1 && b.y1 <= c.y1;
    assert.ok(inside(item.labelBox, item.box));
    assert.notDeepStrictEqual(item.labelBox, item.box);
    // Holes: a path for each shape of an item, with a contour for the shape
    // and one for each hole.
    const json = JSON.parse(read("wasm", "tests", "fixtures", "visualize", "irregular", "inputs", "holes_solution.json"));
    const holes = viewerSolution.readSolution(JSON.stringify(json));
    json.bins.forEach((jsonBin, i) => {
        const bin = holes.bins[i];
        assert.strictEqual(bin.copies, jsonBin.copies);
        assert.strictEqual(bin.defects.length, (jsonBin.defects || []).length);
        jsonBin.items.forEach((jsonItem, j) => {
            assert.deepStrictEqual(bin.items[j].paths.map((p) => p.length),
                jsonItem.item_shapes.map((s) => 1 + (s.holes || []).length));
        });
    });
    // A full circle: points on the circle, all around it.
    const circle = viewerSolution.shapeContour([
        {type: "CircularArc", xs: 2, ys: 0, xe: 2, ye: 0, xc: 1, yc: 0, orientation: "Full"}]);
    assert.ok(circle.length >= 64);
    for (const [x, y] of circle)
        assert.ok(Math.abs(Math.hypot(x - 1, y) - 1) < 1e-9);
    const xs = circle.map(([x]) => x);
    assert.ok(Math.min(...xs) < 0.01 && Math.max(...xs) > 1.99);
    // A quarter circle, anticlockwise: from (1, 0) to (0, 1).
    const quarter = viewerSolution.shapeContour([
        {type: "CircularArc", xs: 1, ys: 0, xe: 0, ye: 1, xc: 0, yc: 0, orientation: "Anticlockwise"},
        {type: "LineSegment", xs: 0, ys: 1, xe: 0, ye: 0},
        {type: "LineSegment", xs: 0, ys: 0, xe: 1, ye: 0}]);
    assert.ok(quarter.every(([x, y]) => x >= -1e-9 && y >= -1e-9));
});
