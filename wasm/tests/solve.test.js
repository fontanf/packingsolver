// Run with: node --test wasm/tests/

const assert = require("node:assert");
const test = require("node:test");

const {
    loadModule, INSTANCES, README_INSTANCE, PROVEN_OPTIMAL_INSTANCE,
} = require("./helpers.js");

function solve(module, problemType, instance, parameters) {
    return JSON.parse(module.solve(
        problemType, JSON.stringify(instance), JSON.stringify(parameters)));
}

for (const [problemType, instance] of Object.entries(INSTANCES)) {
    for (const optimizationMode of ["not-anytime", "not-anytime-sequential"]) {
        test(`${problemType}, ${optimizationMode}`, async () => {
            const module = await loadModule();
            const result = solve(module, problemType, instance, {
                optimization_mode: optimizationMode});
            assert.strictEqual(result.error, undefined);
            assert.ok(result.output.Solution.NumberOfItems > 0);
            assert.ok(result.certificate.length > 0);
        });
    }
}

test("rectangle: anytime mode (default)", async () => {
    const module = await loadModule();
    const result = solve(module, "rectangle", PROVEN_OPTIMAL_INSTANCE, {});
    assert.strictEqual(result.error, undefined);
    assert.strictEqual(result.output.Solution.NumberOfBins, 1);
    assert.strictEqual(result.output.BinPackingBound, 1);
});

for (const optimizationMode of [
        "not-anytime", "not-anytime-deterministic", "not-anytime-sequential"]) {
    test(`rectangle: README example, ${optimizationMode}`, async () => {
        const module = await loadModule();
        const result = solve(module, "rectangle", README_INSTANCE, {
            optimization_mode: optimizationMode});
        assert.strictEqual(result.error, undefined);
        assert.strictEqual(result.output.Solution.NumberOfItems, 20);
        // The certificate lists every bin and item.
        const lines = result.certificate.trim().split("\n");
        assert.strictEqual(lines[0], "TYPE,ID,COPIES,BIN,X,Y,LX,LY,GROUP_ID");
        assert.strictEqual(
            lines.filter((line) => line.startsWith("ITEM,")).length, 20);
    });
}

test("invalid optimization mode", async () => {
    const module = await loadModule();
    const result = solve(module, "rectangle", README_INSTANCE, {
        optimization_mode: "fast"});
    assert.match(result.error, /optimization mode/);
});

test("invalid instance", async () => {
    const module = await loadModule();
    const result = solve(module, "rectangle", {}, {});
    assert.match(result.error, /objective/);
});

test("unsupported problem type", async () => {
    const module = await loadModule();
    const result = solve(module, "unknown", {}, {});
    assert.match(result.error, /unsupported problem type/);
});
