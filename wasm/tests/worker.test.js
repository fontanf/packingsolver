// Run with: node --test wasm/tests/
// The Web Worker, run with 'worker_threads'.

const assert = require("node:assert");
const path = require("node:path");
const test = require("node:test");
const {Worker} = require("node:worker_threads");

const {MODULE_PATH, README_INSTANCE} = require("./helpers.js");

const WORKER_PATH = path.join(__dirname, "..", "js", "packingsolver_worker.js");

// Run the worker with a list of messages; 'onMessage(message, worker)' is
// called for each message from the worker. Resolves with all the messages,
// once a "done" or "error" message is received.
function runWorker(messages, onMessage = () => {}) {
    const worker = new Worker(WORKER_PATH, {workerData: {modulePath: MODULE_PATH}});
    for (const message of messages)
        worker.postMessage(message);
    const received = [];
    return new Promise((resolve, reject) => {
        worker.on("error", reject);
        worker.on("message", (message) => {
            received.push(message);
            onMessage(message, worker);
            if (message.type === "done" || message.type === "error") {
                worker.terminate().then(() => resolve(received));
            }
        });
    });
}

test("worker: solutions, stop and result", async () => {
    // No bound for this objective: in anytime mode, it only ends when
    // stopped.
    const received = await runWorker(
        [{type: "solve", problemType: "rectangle", instance: README_INSTANCE}],
        (message, worker) => {
            // The updates also report new bounds, possibly before any
            // solution is found.
            if (message.type === "solution"
                    && message.output.Solution.NumberOfItems > 0) {
                worker.postMessage({type: "stop"});
            }
        });
    const types = received.map((message) => message.type);
    assert.strictEqual(types[0], "ready");
    assert.ok(types.includes("solution"));
    const done = received[received.length - 1];
    assert.strictEqual(done.type, "done");
    assert.strictEqual(done.output.Solution.NumberOfItems, 20);
    assert.ok(done.certificate.startsWith("TYPE,"));
});

test("worker: error", async () => {
    const received = await runWorker(
        [{type: "solve", problemType: "unknown", instance: {}}]);
    const last = received[received.length - 1];
    assert.strictEqual(last.type, "error");
    assert.match(last.error, /unsupported problem type/);
});
