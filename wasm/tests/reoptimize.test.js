// Run with: node --test wasm/tests/*.test.js wasm/tests/*.test.mjs
// The re-optimization of the leftovers of a bin
// ('Session.startReoptimizeBinLeftovers').

const assert = require("node:assert");
const path = require("node:path");
const test = require("node:test");
const {setTimeout: sleep} = require("node:timers/promises");
const {Worker} = require("node:worker_threads");

const {MODULE_PATH, loadModule, INSTANCES} = require("./helpers.js");

const WORKER_PATH = path.join(__dirname, "..", "js", "packingsolver_worker.js");

const PARAMETERS = JSON.stringify({optimization_mode: "not-anytime-sequential"});

// Two 5x5 squares in a 10x10 bin, in opposite corners: the free space is in
// two regions. Packed again, the squares are side by side.
const SCATTERED_INSTANCE = {
    objective: "bin-packing",
    bin_types: [{x: 10, y: 10}],
    item_types: [{x: 5, y: 5, copies: 2}],
};
const SCATTERED_CERTIFICATE = [
    "TYPE,ID,COPIES,BIN,X,Y,LX,LY,GROUP_ID",
    "BIN,0,1,0,0,0,10,10,",
    "ITEM,0,1,0,0,0,5,5,0",
    "ITEM,0,1,0,5,5,5,5,0",
].join("\n") + "\n";

// Poll a session until it ends. Returns the result, with the updates in
// 'updates'.
async function pollUntilDone(session) {
    const updates = [];
    for (;;) {
        const pollResult = JSON.parse(session.poll());
        updates.push(...pollResult.solutions);
        if (pollResult.done)
            return {...pollResult.result, updates};
        await sleep(20);
    }
}

async function reoptimize(problemType, instance, certificate, binPos) {
    const module = await loadModule();
    const session = new module.Session();
    const startError = session.startReoptimizeBinLeftovers(
        problemType, JSON.stringify(instance), certificate, binPos, PARAMETERS);
    assert.strictEqual(startError, "");
    const result = await pollUntilDone(session);
    session.delete();
    return result;
}

async function solve(problemType, instance) {
    const module = await loadModule();
    const session = new module.Session();
    assert.strictEqual(session.start(problemType, JSON.stringify(instance), PARAMETERS), "");
    const result = await pollUntilDone(session);
    session.delete();
    assert.strictEqual(result.error, undefined);
    return result;
}

test("reoptimize: better leftovers", async () => {
    const result = await reoptimize("rectangle", SCATTERED_INSTANCE, SCATTERED_CERTIFICATE, 0);
    assert.strictEqual(result.error, undefined);
    assert.strictEqual(result.improved, true);
    assert.strictEqual(result.solution.NumberOfItems, 2);
    assert.strictEqual(result.solution.NumberOfBins, 1);
    assert.ok(result.certificate.startsWith("TYPE,"));
    assert.notStrictEqual(result.certificate, SCATTERED_CERTIFICATE);
    // The better solutions found during the re-optimization, the last one
    // being the result.
    assert.ok(result.updates.length >= 1);
    for (const update of result.updates) {
        assert.strictEqual(update.improved, true);
        assert.strictEqual(update.solution.NumberOfItems, 2);
    }
    assert.strictEqual(result.updates[result.updates.length - 1].certificate, result.certificate);
    // The given solution, for comparison.
    const given = await reoptimize("rectangle", SCATTERED_INSTANCE, result.certificate, 0);
    assert.strictEqual(given.improved, false);
    assert.strictEqual(given.updates.length, 0);
    assert.ok(result.solution.TotalLeftoverValue > 0);
});

// The leftovers of the solution found by the solver: the result is a full
// solution, whether they are better or not.
for (const problemType of ["rectangle", "box", "boxstacks", "irregular"]) {
    test(`reoptimize: ${problemType}`, async () => {
        const instance = INSTANCES[problemType];
        const solved = await solve(problemType, instance);
        const result = await reoptimize(problemType, instance, solved.certificate, 0);
        assert.strictEqual(result.error, undefined);
        assert.strictEqual(typeof result.improved, "boolean");
        assert.strictEqual(
            result.solution.NumberOfItems, solved.output.Solution.NumberOfItems);
        if (!result.improved)
            assert.strictEqual(result.certificate, solved.certificate);
    });
}

test("reoptimize: invalid bin", async () => {
    const result = await reoptimize("rectangle", SCATTERED_INSTANCE, SCATTERED_CERTIFICATE, 1);
    assert.match(result.error, /invalid bin/);
});

test("reoptimize: unsupported problem type", async () => {
    const result = await reoptimize(
        "onedimensional", INSTANCES.onedimensional, "TYPE,ID,COPIES,BIN,X,LX\n", 0);
    assert.match(result.error, /unsupported problem type/);
});

test("reoptimize: worker", async () => {
    const worker = new Worker(WORKER_PATH, {workerData: {modulePath: MODULE_PATH}});
    worker.postMessage({
        type: "reoptimize-bin-leftovers",
        problemType: "rectangle",
        instance: SCATTERED_INSTANCE,
        certificate: SCATTERED_CERTIFICATE,
        binPos: 0,
        parameters: {optimization_mode: "not-anytime-sequential"},
    });
    const received = [];
    const message = await new Promise((resolve, reject) => {
        worker.on("error", reject);
        worker.on("message", (message) => {
            received.push(message.type);
            if (message.type === "reoptimized" || message.type === "error")
                resolve(message);
        });
    });
    await worker.terminate();
    assert.strictEqual(message.type, "reoptimized");
    assert.ok(received.includes("reoptimization-solution"));
    assert.ok(!received.includes("solution"));
    assert.strictEqual(message.improved, true);
    assert.strictEqual(message.solution.NumberOfItems, 2);
});
