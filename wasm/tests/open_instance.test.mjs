// Run with: node --test wasm/tests/*.test.js wasm/tests/*.test.mjs
// Opening instances of the JSON format in the form of the web page: the
// instance built back from the form must be the same, for the solver (see
// 'canonical_instance.mjs'), for the instances of 'data' and for instances
// using all the fields of the form ('fixtures/instances').

import assert from "node:assert";
import fs from "node:fs";
import {createRequire} from "node:module";
import path from "node:path";
import test from "node:test";
import {fileURLToPath} from "node:url";

import * as form from "../web/form.js";
import {canonicalInstance} from "./canonical_instance.mjs";

const require = createRequire(import.meta.url);
const {loadModule} = require("./helpers.js");

const DIRECTORY = path.dirname(fileURLToPath(import.meta.url));
const DATA = path.join(DIRECTORY, "..", "..", "data");
const FIXTURES = path.join(DIRECTORY, "fixtures", "instances");
const PROBLEM_TYPES = ["rectangleguillotine", "rectangle", "box", "boxstacks", "onedimensional", "irregular"];

// The JSON files of a problem type in 'data', except the solutions and the
// largest instances (which take long to open, and don't use other fields).
const MAXIMUM_FILE_SIZE = 1000000;
function dataInstances(problemType) {
    const files = [];
    const walk = (directory) => {
        if (!fs.existsSync(directory))
            return;
        for (const name of fs.readdirSync(directory)) {
            const file = path.join(directory, name);
            if (fs.statSync(file).isDirectory())
                walk(file);
            else if (name.endsWith(".json") && !name.includes("solution")
                    && fs.statSync(file).size <= MAXIMUM_FILE_SIZE)
                files.push(file);
        }
    };
    walk(path.join(DATA, problemType));
    return files.sort();
}

// The instance of a file, or null if it isn't an instance.
function readInstance(file) {
    let json;
    try {
        json = JSON.parse(fs.readFileSync(file, "utf8"));
    } catch (error) {
        return null;
    }
    if (json === null || json.bin_types === undefined || json.item_types === undefined)
        return null;
    return json;
}

// Instances which the solver can't read either.
function unreadable(json) {
    if (json.objective === undefined)
        return "no objective";
    if (json.objective === "not_a_real_objective")
        return "invalid objective";
    if (json.bin_types.some((binType) => binType.type === "circle" && typeof binType.radius !== "number"))
        return "a circle without radius";
    return null;
}

function roundTrip(problemType, json) {
    const opened = form.fromInstance(problemType, json);
    const instance = form.toInstance(problemType, opened);
    assert.deepStrictEqual(canonicalInstance(problemType, instance), canonicalInstance(problemType, json));
    return {opened, instance};
}

// There are no instances of the JSON format for 'boxstacks' in 'data'.
for (const problemType of PROBLEM_TYPES.filter((type) => type !== "boxstacks")) {
    test(`open instance: the instances of data/${problemType}`, () => {
        let opened = 0;
        for (const absoluteFile of dataInstances(problemType)) {
            const json = readInstance(absoluteFile);
            if (json === null)
                continue;
            const file = path.relative(DATA, absoluteFile);
            if (unreadable(json) !== null)
                continue;
            try {
                roundTrip(problemType, json);
            } catch (error) {
                error.message = `${file}: ${error.message}`;
                throw error;
            }
            ++opened;
        }
        assert.ok(opened > 0);
    });
}

test("open instance: instances using all the fields of the form", async () => {
    const module = await loadModule();
    const files = fs.readdirSync(FIXTURES).filter((name) => name.endsWith(".json")).sort();
    assert.ok(files.length >= 2 * PROBLEM_TYPES.length);
    for (const name of files) {
        const problemType = PROBLEM_TYPES.find((type) => name.startsWith(type + "_"));
        const json = JSON.parse(fs.readFileSync(path.join(FIXTURES, name), "utf8"));
        const {opened, instance} = roundTrip(problemType, json);
        assert.deepStrictEqual(opened.ignored, [], name);
        // The solver reads both.
        for (const solved of [json, instance]) {
            const result = JSON.parse(module.solve(problemType, JSON.stringify(solved),
                JSON.stringify({optimization_mode: "not-anytime-sequential"})));
            assert.strictEqual(result.error, undefined, `${name}: ${result.error}`);
        }
    }
});

test("open instance: errors and ignored fields", () => {
    const rectangle = {objective: "bin-packing", bin_types: [{x: 10, y: 10}], item_types: [{x: 5, y: 5}]};
    assert.throws(() => form.fromInstance("rectangle", {...rectangle, objective: "maximize"}),
        /unknown objective "maximize"/);
    assert.throws(() => form.fromInstance("rectangle", {...rectangle, objective: "open-dimension-z"}),
        /isn't available for this problem type/);
    assert.throws(() => form.fromInstance("rectangle", [rectangle]), /must be a JSON object/);
    assert.throws(
        () => form.fromInstance("rectangleguillotine", {objective: "bin-packing", number_of_stages: 4,
            bin_types: [{width: 10, height: 10}], item_types: [{width: 5, height: 5}]}),
        /number_of_stages: 4 stages aren't supported by the form/);
    assert.throws(
        () => form.fromInstance("rectangle", {...rectangle, bin_types: [{x: 10, y: 10,
            resources: [{capacity: 1, consumptions: [{item_type_id: 3, consumption: 1}]}]}]}),
        /bin_types\[0\].resources\[0\]: invalid item type id 3/);
    assert.throws(
        () => form.fromInstance("rectangle", {...rectangle, unloading_constraint: "sideways"}),
        /unloading_constraint: unknown value "sideways"/);
    // The fields which the solver doesn't read are ignored, and listed.
    const opened = form.fromInstance("irregular", {
        objective: "bin-packing", parameters: {leftover_corner: "bl"}, comment: "a test",
        bin_types: [{type: "rectangle", width: 10, height: 10, id: 0}],
        item_types: [{type: "rectangle", width: 5, height: 5, priority: 1}],
    });
    assert.deepStrictEqual(opened.ignored.sort(),
        ["bin_types[0].id", "comment", "item_types[0].priority", "parameters.leftover_corner"]);
});
