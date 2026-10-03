// Shared by the tests of the WebAssembly module.
// The module path can be set with PACKINGSOLVER_WASM (default:
// build_wasm/wasm/packingsolver.js).

const fs = require("node:fs");
const path = require("node:path");

const MODULE_PATH = process.env.PACKINGSOLVER_WASM
    || path.join(__dirname, "..", "..", "build_wasm", "wasm", "packingsolver.js");

let modulePromise = null;
function loadModule() {
    if (modulePromise === null)
        modulePromise = require(MODULE_PATH)();
    return modulePromise;
}

const DATA_DIR = path.join(__dirname, "..", "..", "data");

function readData(...parts) {
    return JSON.parse(fs.readFileSync(path.join(DATA_DIR, ...parts), "utf8"));
}

// An instance of each problem type.
const INSTANCES = {
    rectangleguillotine: readData(
        "rectangleguillotine", "tests",
        "bin_packing_merge_identical_items_respects_resources", "instance.json"),
    rectangle: readData(
        "rectangle", "tests",
        "variable_sized_bin_packing_remove_dominated_bin_types_not_removed_with_negative_penalty_resource_on_b",
        "instance.json"),
    box: readData(
        "box", "tests",
        "bin_packing_merge_identical_items_respects_resources", "instance.json"),
    boxstacks: {
        objective: "bin-packing",
        bin_types: [{x: 100, y: 60, z: 40, copies: 2}],
        item_types: [{x: 40, y: 30, z: 20, copies: 3}],
    },
    onedimensional: readData(
        "onedimensional", "tests",
        "bin_packing_merge_identical_items_respects_resources", "instance.json"),
    irregular: readData(
        "irregular", "tests",
        "periodic_packing_single_item_exact_fit_rectangle.json"),
};

// The 'rectangle' example of the README. Its objective has no bound: in
// anytime mode, it only ends when stopped (or with a time limit).
const README_INSTANCE = {
    objective: "bin-packing-with-leftovers",
    bin_types: [{x: 1000, y: 500, copies: 10}],
    item_types: [
        {x: 300, y: 200, copies: 10},
        {x: 250, y: 150, copies: 10},
    ],
};

// Four 5x5 squares fill a 10x10 bin: the bound proves the solution optimal,
// so the anytime mode stops without a time limit.
const PROVEN_OPTIMAL_INSTANCE = {
    objective: "bin-packing",
    bin_types: [{x: 10, y: 10, copies: 4}],
    item_types: [{x: 5, y: 5, copies: 4}],
};

module.exports = {
    MODULE_PATH,
    loadModule,
    INSTANCES,
    README_INSTANCE,
    PROVEN_OPTIMAL_INSTANCE,
};
