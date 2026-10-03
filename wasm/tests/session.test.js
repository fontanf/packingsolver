// Run with: node --test wasm/tests/*.test.js wasm/tests/*.test.mjs

const assert = require("node:assert");
const test = require("node:test");
const {setTimeout: sleep} = require("node:timers/promises");

const {
    loadModule, INSTANCES, README_INSTANCE, PROVEN_OPTIMAL_INSTANCE,
} = require("./helpers.js");

// Poll a session until it ends; 'onSolutions' is called with each batch of
// new solutions. Returns the final result and all the solutions.
async function pollUntilDone(session, onSolutions = () => {}) {
    const solutions = [];
    for (;;) {
        const pollResult = JSON.parse(session.poll());
        solutions.push(...pollResult.solutions);
        if (pollResult.solutions.length > 0)
            onSolutions(pollResult.solutions);
        if (pollResult.done)
            return {result: pollResult.result, solutions};
        await sleep(20);
    }
}

test("session: anytime mode, proven optimal", async () => {
    const module = await loadModule();
    const session = new module.Session();
    assert.strictEqual(session.start(
        "rectangle", JSON.stringify(PROVEN_OPTIMAL_INSTANCE), "{}"), "");
    const {result, solutions} = await pollUntilDone(session);
    session.delete();
    assert.strictEqual(result.error, undefined);
    assert.strictEqual(result.output.Solution.NumberOfBins, 1);
    assert.ok(solutions.length >= 1);
    // The last new solution is the final one.
    assert.strictEqual(
        solutions[solutions.length - 1].output.Solution.NumberOfBins, 1);
});

test("session: stop", async () => {
    const module = await loadModule();
    const session = new module.Session();
    // No bound for this objective: in anytime mode, it only ends when
    // stopped.
    assert.strictEqual(session.start(
        "rectangle", JSON.stringify(README_INSTANCE), "{}"), "");
    const {result, solutions} = await pollUntilDone(
        session, () => session.stop());
    session.delete();
    assert.strictEqual(result.error, undefined);
    assert.ok(solutions.length >= 1);
    assert.strictEqual(result.output.Solution.NumberOfItems, 20);
});

for (const [problemType, instance] of Object.entries(INSTANCES)) {
    test(`session: ${problemType}, anytime mode, stopped`, async () => {
        const module = await loadModule();
        const session = new module.Session();
        assert.strictEqual(session.start(
            problemType, JSON.stringify(instance), "{}"), "");
        // Stopped at the first solution, unless it already ended (solution
        // proven optimal). The updates also report new bounds, possibly
        // before any solution is found.
        const {result} = await pollUntilDone(session, (solutions) => {
            if (solutions.some((s) => s.output.Solution.NumberOfItems > 0))
                session.stop();
        });
        session.delete();
        assert.strictEqual(result.error, undefined);
        assert.ok(result.output.Solution.NumberOfItems > 0);
    });
}

test("session: error", async () => {
    const module = await loadModule();
    const session = new module.Session();
    assert.strictEqual(session.start("rectangle", "{}", "{}"), "");
    const {result} = await pollUntilDone(session);
    session.delete();
    assert.match(result.error, /objective/);
});

test("session: started twice", async () => {
    const module = await loadModule();
    const session = new module.Session();
    assert.strictEqual(session.start(
        "rectangle", JSON.stringify(PROVEN_OPTIMAL_INSTANCE), "{}"), "");
    const error = JSON.parse(session.start(
        "rectangle", JSON.stringify(PROVEN_OPTIMAL_INSTANCE), "{}")).error;
    assert.match(error, /already started/);
    await pollUntilDone(session);
    session.delete();
});
