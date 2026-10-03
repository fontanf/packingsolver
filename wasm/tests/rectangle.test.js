// Run with: node --test wasm/tests/
// The module path can be set with PACKINGSOLVER_WASM (default:
// build_wasm/wasm/packingsolver.js).

const assert = require("node:assert");
const path = require("node:path");
const test = require("node:test");

const MODULE_PATH = process.env.PACKINGSOLVER_WASM
    || path.join(__dirname, "..", "..", "build_wasm", "wasm", "packingsolver.js");

let modulePromise = null;
function loadModule() {
    if (modulePromise === null)
        modulePromise = require(MODULE_PATH)();
    return modulePromise;
}

// The 'rectangle' example of the README.
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

function solve(module, instance, parameters) {
    const result = JSON.parse(module.solve(
        "rectangle", JSON.stringify(instance), JSON.stringify(parameters)));
    assert.strictEqual(result.error, undefined);
    return result;
}

test("rectangle: anytime mode (default)", async () => {
    const module = await loadModule();
    const result = solve(module, PROVEN_OPTIMAL_INSTANCE, {});
    assert.strictEqual(result.output.Solution.NumberOfBins, 1);
    assert.strictEqual(result.output.BinPackingBound, 1);
});

for (const optimizationMode of [
        "not-anytime", "not-anytime-deterministic", "not-anytime-sequential"]) {
    test(`rectangle: README example, ${optimizationMode}`, async () => {
        const module = await loadModule();
        const result = solve(module, README_INSTANCE, {
            optimization_mode: optimizationMode});
        assert.strictEqual(result.output.Solution.NumberOfItems, 20);
        assert.ok(result.output.Solution.NumberOfBins >= 1);
        // The certificate lists every bin and item.
        const lines = result.certificate.trim().split("\n");
        assert.strictEqual(lines[0], "TYPE,ID,COPIES,BIN,X,Y,LX,LY,GROUP_ID");
        assert.strictEqual(
            lines.filter((line) => line.startsWith("ITEM,")).length, 20);
    });
}

test("rectangle: invalid optimization mode", async () => {
    const module = await loadModule();
    const result = JSON.parse(module.solve(
        "rectangle", JSON.stringify(README_INSTANCE),
        JSON.stringify({optimization_mode: "fast"})));
    assert.match(result.error, /optimization mode/);
});

test("rectangle: invalid instance", async () => {
    const module = await loadModule();
    const result = JSON.parse(module.solve("rectangle", "{}", "{}"));
    assert.match(result.error, /objective/);
});

test("unsupported problem type", async () => {
    const module = await loadModule();
    const result = JSON.parse(module.solve("unknown", "{}", "{}"));
    assert.match(result.error, /unsupported problem type/);
});
