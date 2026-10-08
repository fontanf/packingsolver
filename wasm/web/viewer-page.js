// The solution viewer page ('viewer.html'): a certificate written by the
// solvers, opened, dropped on the page, or given by the URL
// ('?certificate=<URL>'), drawn by the visualizer ('viewer/').

import {PROBLEM_TYPES, detectProblemType, readSolution} from "./viewer/solution.js";
import {createViewer} from "./viewer/viewer.js";

const $ = (id) => document.getElementById(id);

// The names of the problem types, as in the solver page.
const PROBLEM_TYPE_NAMES = {
    rectangleguillotine: "Rectangles, guillotine cuts",
    rectangle: "Rectangles",
    box: "Boxes",
    boxstacks: "Box stacks",
    onedimensional: "One-dimensional",
    irregular: "Irregular shapes",
};

function showError(message) {
    $("error").textContent = (message === "")? "": "Error: " + message;
}

// Draw a certificate ('name': the name of its file).
async function show(text, name) {
    showError("");
    const problemType = detectProblemType(text);
    if (problemType === null) {
        showError(`${name} isn't a certificate of the solvers.`);
        return;
    }
    try {
        $("viewer").replaceChildren();
        $("viewer").hidden = true;
        if (!PROBLEM_TYPES.includes(problemType))
            throw new Error(`the problem type '${problemType}' isn't drawn by the viewer`);
        createViewer($("viewer"), readSolution(text, problemType));
        $("viewer").hidden = false;
    } catch (error) {
        console.error(error);
        showError(`can't read ${name}: ${error.message}`);
        return;
    }
    $("solution-title").textContent = `${name} (${PROBLEM_TYPE_NAMES[problemType]})`;
    document.title = `${name} - PackingSolver viewer`;
    $("solution").hidden = false;
    // The left and right keys change the bin shown.
    $("viewer").focus({preventScroll: true});
}

async function showFile(file) {
    await show(await file.text(), file.name);
}

function init() {
    $("open-file").addEventListener("click", () => $("file").click());
    $("file").addEventListener("change", async () => {
        const file = $("file").files[0];
        $("file").value = "";
        if (file !== undefined)
            await showFile(file);
    });
    // A file dropped on the page.
    document.addEventListener("dragover", (event) => event.preventDefault());
    document.addEventListener("drop", async (event) => {
        event.preventDefault();
        const file = event.dataTransfer.files[0];
        if (file !== undefined)
            await showFile(file);
    });
    // A certificate given by the URL.
    const url = new URLSearchParams(window.location.search).get("certificate");
    if (url !== null) {
        fetch(url)
            .then((response) => {
                if (!response.ok)
                    throw new Error(`${response.status} ${response.statusText}`);
                return response.text();
            })
            .then((text) => show(text, decodeURIComponent(url.split("/").pop())))
            .catch((error) => showError(`can't load ${url}: ${error.message}`));
    }
}

init();
