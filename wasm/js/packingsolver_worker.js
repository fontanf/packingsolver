// Web Worker solving PackingSolver instances with the WebAssembly module,
// without blocking the page.
//
// Messages to the worker:
// - {type: "solve", problemType, instance, parameters}: solve an instance
//   ('instance' is a JSON string or an object in the JSON format of
//   PackingSolver, 'parameters' an object, see 'packingsolver_wasm.cpp');
// - {type: "stop"}: stop the optimization; it then sends its best solution.
//
// Messages from the worker:
// - {type: "ready"}: the module is loaded;
// - {type: "solution", output, certificate}: the best solution or a bound has
//   improved (the solution may be empty, e.g. a bound found first);
// - {type: "done", output, certificate}: the optimization has ended;
// - {type: "error", error}: the optimization failed.
//
// In a browser, it is a classic worker: 'new Worker("packingsolver_worker.js")',
// next to 'packingsolver.js' and 'packingsolver.wasm'. In Node, it can be run
// with 'worker_threads', with the path of 'packingsolver.js' in
// 'workerData.modulePath'.

"use strict";

// Delay between two polls of the session, in milliseconds.
const POLL_INTERVAL = 100;

let postToPage;
let modulePromise;
if (typeof importScripts === "function") {
    // Browser.
    importScripts("packingsolver.js");
    postToPage = (message) => self.postMessage(message);
    self.onmessage = (event) => onMessage(event.data);
    modulePromise = PackingSolver();
} else {
    // Node.
    const {parentPort, workerData} = require("node:worker_threads");
    postToPage = (message) => parentPort.postMessage(message);
    parentPort.on("message", (message) => onMessage(message));
    modulePromise = require(workerData.modulePath)();
}

modulePromise.then(() => postToPage({type: "ready"}));

let session = null;
let pollTimer = null;

async function onMessage(message) {
    if (message.type === "solve") {
        const module = await modulePromise;
        if (session !== null) {
            postToPage({type: "error", error: "an optimization is already running."});
            return;
        }
        const instance = (typeof message.instance === "string")?
            message.instance:
            JSON.stringify(message.instance);
        session = new module.Session();
        const startError = session.start(
            message.problemType, instance, JSON.stringify(message.parameters || {}));
        if (startError !== "") {
            postToPage({type: "error", error: JSON.parse(startError).error});
            endSession();
            return;
        }
        pollTimer = setInterval(poll, POLL_INTERVAL);
    } else if (message.type === "stop") {
        if (session !== null)
            session.stop();
    }
}

function poll() {
    const pollResult = JSON.parse(session.poll());
    for (const solution of pollResult.solutions)
        postToPage({type: "solution", ...solution});
    if (!pollResult.done)
        return;
    const result = pollResult.result;
    if (result.error !== undefined) {
        postToPage({type: "error", error: result.error});
    } else {
        postToPage({type: "done", ...result});
    }
    endSession();
}

function endSession() {
    if (pollTimer !== null)
        clearInterval(pollTimer);
    pollTimer = null;
    session.delete();
    session = null;
}
