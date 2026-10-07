// Run with: node --test wasm/tests/*.test.js wasm/tests/*.test.mjs
// The errors of the fields of the form of the web page ('form.rowErrors',
// 'form.parameterErrors'), and the copies of its rows ('form.duplicateRow').

import assert from "node:assert";
import test from "node:test";

import * as form from "../web/form.js";
import * as projects from "../web/projects.js";

function exampleForm(problemType) {
    return projects.exampleProject(problemType).form;
}

test("form errors: valid rows", () => {
    for (const problemType of ["rectangleguillotine", "rectangle", "box", "boxstacks", "onedimensional"]) {
        const f = exampleForm(problemType);
        for (const [kind, rows] of [["bin", f.binTypes], ["item", f.itemTypes]]) {
            for (const row of rows)
                assert.deepStrictEqual(form.rowErrors(problemType, kind, row, f), {}, problemType);
        }
        assert.deepStrictEqual(form.parameterErrors(problemType, f.instanceParameters, f.objective), {});
    }
});

test("form errors: invalid cells", () => {
    const f = exampleForm("rectangle");
    f.itemTypes[1].x = "";
    f.itemTypes[1].y = "abc";
    f.itemTypes[1].group_id = "1.5";
    // The errors of each cell, with the messages of 'toInstance'.
    assert.deepStrictEqual(form.rowErrors("rectangle", "item", f.itemTypes[1], f), {
        x: "missing width.",
        y: "invalid height: \"abc\".",
    });
    assert.deepStrictEqual(form.rowErrors("rectangle", "item", f.itemTypes[0], f), {});
    assert.throws(() => form.toInstance("rectangle", f), /item type 1: missing width\./);
    // A field which isn't shown isn't checked: the group, without unloading
    // constraint.
    f.instanceParameters.unloading_constraint = "increasing-x";
    assert.strictEqual(form.rowErrors("rectangle", "item", f.itemTypes[1], f).group_id,
        "invalid group: \"1.5\".");
    // Nested fields: the defects of a bin type.
    f.binTypes[0].defects.push({x: "1", y: "", width: "2", height: "2"});
    assert.deepStrictEqual(Object.keys(form.rowErrors("rectangle", "bin", f.binTypes[0], f)), ["defects"]);
});

test("form errors: invalid parameters", () => {
    const f = exampleForm("rectangleguillotine");
    f.instanceParameters.cut_thickness = "-1";
    f.instanceParameters.unlimited_maximum_number_1_cuts = false;
    f.instanceParameters.maximum_number_1_cuts = "";
    const errors = form.parameterErrors("rectangleguillotine", f.instanceParameters, f.objective);
    assert.deepStrictEqual(Object.keys(errors).sort(), ["cut_thickness", "maximum_number_1_cuts"]);
    assert.match(errors.cut_thickness, /invalid cut thickness/);
});

test("form: a copy of a row refers to the same item rows", () => {
    const f = exampleForm("rectangle");
    const resource = {capacity: "10", penalize: false, penalty: "",
        consumptions: [{itemRow: f.itemTypes[1], values: "2"}]};
    f.binTypes[0].resources.push(resource);
    const copy = form.duplicateRow(f.binTypes[0], f.itemTypes);
    assert.notStrictEqual(copy, f.binTypes[0]);
    assert.notStrictEqual(copy.resources[0], resource);
    assert.strictEqual(copy.resources[0].consumptions[0].itemRow, f.itemTypes[1]);
    f.binTypes.push(copy);
    const instance = form.toInstance("rectangle", f);
    assert.deepStrictEqual(instance.bin_types[1], instance.bin_types[0]);
    // A copy of an item row.
    const item = form.duplicateRow(f.itemTypes[0], f.itemTypes);
    assert.notStrictEqual(item, f.itemTypes[0]);
    assert.deepStrictEqual(item, f.itemTypes[0]);
});

test("form: a copy of an irregular bin row refers to the same item rows", () => {
    const f = exampleForm("irregular");
    f.binTypes[0].fixed_items.push({itemRow: f.itemTypes[2], x: "0", y: "0", angle: "", mirror: false});
    const copy = form.duplicateRow(f.binTypes[0], f.itemTypes);
    assert.strictEqual(copy.fixed_items[0].itemRow, f.itemTypes[2]);
    assert.notStrictEqual(copy.fixed_items[0], f.binTypes[0].fixed_items[0]);
});
