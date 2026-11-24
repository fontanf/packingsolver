// Web Worker solving PackingSolver instances with the WebAssembly module,
// without blocking the page.
//
// Messages to the worker:
// - {type: "solve", problemType, instance, parameters}: solve an instance
//   ('instance' is a JSON string or an object in the JSON format of
//   PackingSolver, 'parameters' an object, see 'packingsolver_wasm.cpp');
// - {type: "reoptimize-bin-leftovers", problemType, instance, certificate,
//   binPos, parameters}: re-optimize the leftovers of the bin 'binPos' of a
//   solution, given by its certificate (problem types "rectangle", "box",
//   "boxstacks" and "irregular");
// - {type: "stop"}: stop the optimization; it then sends its best solution.
//
// Messages from the worker:
// - {type: "ready"}: the module is loaded;
// - {type: "solution", output, certificate}: the best solution or a bound has
//   improved (the solution may be empty, e.g. a bound found first);
// - {type: "done", output, certificate}: the optimization has ended;
// - {type: "reoptimization-solution", solution, certificate, improved}:
//   during a re-optimization of the leftovers of a bin, a solution with
//   better leftovers than the given one and than the previous ones was found
//   (same fields as "reoptimized");
// - {type: "reoptimized", solution, certificate, improved}: the
//   re-optimization of the leftovers of a bin has ended; 'solution' is the
//   'Solution' of the JSON output, the given solution if the leftovers
//   aren't better ('improved' is false);
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
    // The threads of the module run in workers started from its own script:
    // loaded with 'importScripts', it would take the URL of this one.
    modulePromise = PackingSolver({
        mainScriptUrlOrBlob: new URL("packingsolver.js", self.location.href).href,
    });
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
// Types of the messages sent for an update and at the end of the session:
// "solution" and "done", or "reoptimization-solution" and "reoptimized".
let solutionType = null;
let doneType = null;

async function onMessage(message) {
    if (message.type === "solve" || message.type === "reoptimize-bin-leftovers") {
        const module = await modulePromise;
        if (session !== null) {
            postToPage({type: "error", error: "an optimization is already running."});
            return;
        }
        const instance = (typeof message.instance === "string")?
            message.instance:
            JSON.stringify(message.instance);
        const parameters = JSON.stringify(message.parameters || {});
        session = new module.Session();
        let startError;
        if (message.type === "solve") {
            solutionType = "solution";
            doneType = "done";
            startError = session.start(message.problemType, instance, parameters);
        } else {
            solutionType = "reoptimization-solution";
            doneType = "reoptimized";
            startError = session.startReoptimizeBinLeftovers(
                message.problemType, instance, message.certificate, message.binPos, parameters);
        }
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
        postToPage({type: solutionType, ...solution});
    if (!pollResult.done)
        return;
    const result = pollResult.result;
    if (result.error !== undefined) {
        postToPage({type: "error", error: result.error});
    } else {
        postToPage({type: doneType, ...result});
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
