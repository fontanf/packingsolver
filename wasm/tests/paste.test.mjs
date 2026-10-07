// Run with: node --test wasm/tests/*.test.js wasm/tests/*.test.mjs
// Rows pasted in the tables of the web page ('paste.js'): cells copied from a
// spreadsheet or lines of a CSV file, with or without a header.

import assert from "node:assert";
import fs from "node:fs";
import path from "node:path";
import test from "node:test";
import {fileURLToPath} from "node:url";

import * as form from "../web/form.js";
import * as paste from "../web/paste.js";

const DATA = path.join(path.dirname(fileURLToPath(import.meta.url)), "..", "..", "data");

// The form of a new project of a problem type, and the function to paste in
// one of its tables.
function newForm(problemType, objective = form.defaultObjective(problemType)) {
    const f = {
        objective,
        instanceParameters: form.defaultInstanceParameters(problemType),
        binTypes: [form.newBinRow(problemType, objective)],
        itemTypes: [form.newItemRow(problemType)],
    };
    f.paste = (kind, text, startRow = null, startColumn = null) => paste.pasteRows(
        problemType, kind, (kind === "bin")? f.binTypes: f.itemTypes, paste.parsePastedText(text), {
            objective,
            instanceParameters: f.instanceParameters,
            newRow: () => (kind === "bin")? form.newBinRow(problemType, objective): form.newItemRow(problemType),
            startRow,
            startColumn,
        });
    return f;
}

test("paste: parse", () => {
    // Cells copied from a spreadsheet.
    assert.deepStrictEqual(paste.parsePastedText("10\t20\n30\t40\n").cells, [["10", "20"], ["30", "40"]]);
    // CSV, with quotes.
    assert.deepStrictEqual(paste.parsePastedText("a,\"b, c\",\"d \"\"e\"\"\"\r\n1,2,3").cells,
        [["a", "b, c", "d \"e\""], ["1", "2", "3"]]);
    // Semicolons (with decimal commas).
    const parsed = paste.parsePastedText("12,5;3\n1;2");
    assert.strictEqual(parsed.delimiter, ";");
    assert.deepStrictEqual(parsed.cells, [["12,5", "3"], ["1", "2"]]);
    // A single cell, a single column.
    assert.strictEqual(paste.severalCells(paste.parsePastedText("12")), false);
    assert.strictEqual(paste.severalCells(paste.parsePastedText("12\n")), false);
    assert.strictEqual(paste.severalCells(paste.parsePastedText("12\n13")), true);
    assert.strictEqual(paste.severalCells(paste.parsePastedText("12\t13")), true);
});

test("paste: a CSV file of the solvers, with its header", () => {
    // 'data/rectangle/tests/.../items.csv' pasted in a new project: the new
    // row replaced, the columns found by their names (the ids ignored).
    const directory = path.join(DATA, "rectangle", "tests", "bin_packing_with_leftovers_x");
    const text = fs.readFileSync(path.join(directory, "items.csv"), "utf8");
    const f = newForm("rectangle", "bin-packing-with-leftovers");
    const result = f.paste("item", text);
    assert.strictEqual(result.header, true);
    assert.strictEqual(result.replaced, true);
    const lines = text.trim().split("\n").slice(1);
    assert.strictEqual(f.itemTypes.length, lines.length);
    assert.deepStrictEqual(result.errors, []);
    // The same item types as the solvers read.
    const header = text.split("\n")[0].trim().split(",");
    const instance = form.toInstance("rectangle", f);
    lines.forEach((line, i) => {
        const values = Object.fromEntries(line.trim().split(",").map((v, j) => [header[j], Number(v)]));
        assert.strictEqual(instance.item_types[i].x, values.WIDTH);
        assert.strictEqual(instance.item_types[i].y, values.HEIGHT);
        if (values.COPIES !== undefined)
            assert.strictEqual(instance.item_types[i].copies, values.COPIES);
    });
    if (header.includes("ID"))
        assert.deepStrictEqual(result.ignoredColumns, ["ID"]);
});

test("paste: header", () => {
    // Names of the tables, in any case and order; columns unknown; rows
    // added after the rows which exist.
    let f = newForm("box", "knapsack");
    f.itemTypes[0].x = "5";
    let result = f.paste("item", "Name\tcopies\tZ\tX\tY\tProfit\nA\t-1\t3\t1\t2\t7\nB\t4\t30\t10\t20\t70");
    assert.strictEqual(result.replaced, false);
    assert.deepStrictEqual(result.ignoredColumns, ["Name"]);
    assert.strictEqual(f.itemTypes.length, 3);
    const instance = form.toInstance("box", f);
    // -1 copies: unlimited, for the knapsack objective.
    assert.deepStrictEqual(instance.item_types[1], {x: 1, y: 2, z: 3, copies: -1, profit: 7, rotations: ["XYZ"]});
    assert.deepStrictEqual(instance.item_types[2], {x: 10, y: 20, z: 30, copies: 4, profit: 70, rotations: ["XYZ"]});
    // The rotations of box.
    f = newForm("box", "knapsack");
    f.paste("item", "X,Y,Z,ROTATION_XYZ,ROTATION_YXZ,ROTATION_ZYX\n1,2,3,1,0,1");
    assert.deepStrictEqual(f.itemTypes[0].rotations, ["XYZ", "ZYX"]);
    // The x of the onedimensional CSV files.
    f = newForm("onedimensional");
    f.paste("item", "X,COPIES\n193,2\n197,1");
    assert.deepStrictEqual(form.toInstance("onedimensional", f).item_types,
        [{length: 193, copies: 2}, {length: 197, copies: 1}]);
    // The trims of rectangleguillotine, and their types.
    f = newForm("rectangleguillotine");
    result = f.paste("bin", "WIDTH;HEIGHT;LEFT_TRIM;BOTTOM_TRIM;LEFT_TRIM_TYPE\n6000;3210;20;12,5;SOFT");
    assert.deepStrictEqual(result.errors, []);
    const bin = form.toInstance("rectangleguillotine", f).bin_types[0];
    assert.strictEqual(bin.left_trim, 20);
    assert.strictEqual(bin.bottom_trim, 12.5);
    assert.strictEqual(bin.left_trim_type, "soft");
});

test("paste: cells without header", () => {
    // From the cell (1, width) of the items of rectangle: the width, the
    // height and the copies of the second row, then new rows.
    const f = newForm("rectangle", "bin-packing");
    f.itemTypes.push(form.newItemRow("rectangle"));
    const result = f.paste("item", "1\t2\t3\n4\t5\t6\t7\t8\t9\t10\n", 1, "x");
    assert.strictEqual(result.header, false);
    assert.strictEqual(result.added, 1);
    assert.strictEqual(result.rows, 2);
    assert.strictEqual(f.itemTypes.length, 3);
    // The visible columns: width, height, copies, weight, oriented.
    assert.deepStrictEqual(paste.tableColumns("rectangle", "item", "bin-packing", f.instanceParameters)
        .map((c) => c.key), ["x", "y", "copies", "weight", "oriented"]);
    assert.deepStrictEqual([f.itemTypes[1].x, f.itemTypes[1].y, f.itemTypes[1].copies], ["1", "2", "3"]);
    assert.deepStrictEqual([f.itemTypes[2].x, f.itemTypes[2].y, f.itemTypes[2].copies, f.itemTypes[2].weight],
        ["4", "5", "6", "7"]);
    // "8": not a value of a checkbox; "9", "10": beyond the last column.
    assert.strictEqual(result.errors.length, 1);
    assert.match(result.errors[0], /item type 2: invalid oriented: "8"/);
    assert.strictEqual(result.ignoredCells, 2);
    // The first row unchanged.
    assert.deepStrictEqual(f.itemTypes[0], form.newItemRow("rectangle"));
    // Not in a cell: not pasted.
    assert.strictEqual(f.paste("item", "1\t2"), null);
});

test("paste: copies", () => {
    // -1 copies of a bin type: unlimited for bin packing; for knapsack, kept,
    // and invalid.
    let f = newForm("rectangle", "bin-packing");
    f.paste("bin", "WIDTH,HEIGHT,COPIES\n100,50,-1");
    assert.strictEqual(f.binTypes[0].unlimited_copies, true);
    assert.strictEqual(form.toInstance("rectangle", f).bin_types[0].copies, -1);
    f = newForm("rectangle", "knapsack");
    f.paste("bin", "WIDTH,HEIGHT,COPIES\n100,50,-1");
    assert.strictEqual(f.binTypes[0].copies, "-1");
    assert.deepStrictEqual(form.rowErrors("rectangle", "bin", f.binTypes[0], f), {copies: "invalid copies: \"-1\"."});
    // A number pasted in the copies: not unlimited anymore.
    f = newForm("rectangle", "bin-packing");
    f.paste("bin", "100\t50\t3", 0, "x");
    assert.strictEqual(f.binTypes[0].unlimited_copies, false);
    assert.strictEqual(form.toInstance("rectangle", f).bin_types[0].copies, 3);
});
