// PackingSolver web page: builds an instance (form or JSON), solves it in a
// Web Worker ('packingsolver_worker.js') and shows the solutions as they are
// found.

import * as irregularForm from "./irregular_form.js";

// Columns of the bin and item type tables, for each problem type. 'key' is
// the field of the JSON instance format.
const DIMENSIONS = {
    rectangleguillotine: [["width", "Width"], ["height", "Height"]],
    rectangle: [["x", "Width"], ["y", "Height"]],
    box: [["x", "X"], ["y", "Y"], ["z", "Z"]],
    boxstacks: [["x", "X"], ["y", "Y"], ["z", "Z"]],
    onedimensional: [["length", "Length"]],
};

function numberColumn([key, label], value, optional = false) {
    return {key, label, type: "number", value, optional};
}

function binColumns(problemType) {
    return [
        ...DIMENSIONS[problemType].map((d) => numberColumn(d, 100)),
        numberColumn(["copies", "Copies"], 1),
        numberColumn(["cost", "Cost"], "", true),
    ];
}

function itemColumns(problemType) {
    const columns = [
        ...DIMENSIONS[problemType].map((d) => numberColumn(d, 10)),
        numberColumn(["copies", "Copies"], 1),
        numberColumn(["profit", "Profit"], "", true),
    ];
    if (problemType === "rectangleguillotine" || problemType === "rectangle")
        columns.push({key: "oriented", label: "Oriented", type: "checkbox", value: false});
    if (problemType === "boxstacks")
        columns.push(numberColumn(["stackability_id", "Stackability id"], 0));
    return columns;
}

const OBJECTIVES = [
    ["knapsack", "Knapsack: maximize the profit of the packed items"],
    ["bin-packing", "Bin packing: minimize the number of bins"],
    ["bin-packing-with-leftovers", "Bin packing with leftovers"],
    ["variable-sized-bin-packing", "Variable-sized bin packing: minimize the cost of the bins"],
    ["open-dimension-x", "Open dimension X: minimize the length"],
    ["feasibility", "Feasibility"],
];

// Default objective of each problem type.
function defaultObjective(problemType) {
    return (problemType === "onedimensional")? "bin-packing": "bin-packing-with-leftovers";
}

// The examples of the README.
const EXAMPLES = {
    rectangleguillotine: {
        objective: "bin-packing-with-leftovers",
        bin_types: [{width: 1000, height: 700, copies: 5}],
        item_types: [
            {width: 250, height: 200, copies: 2},
            {width: 150, height: 300, copies: 2},
            {width: 200, height: 150, copies: 3},
        ],
    },
    rectangle: {
        objective: "bin-packing-with-leftovers",
        bin_types: [{x: 1000, y: 500, copies: 10}],
        item_types: [{x: 300, y: 200, copies: 10}, {x: 250, y: 150, copies: 10}],
    },
    box: {
        objective: "knapsack",
        bin_types: [{x: 216, y: 173, z: 110}],
        item_types: [
            {x: 108, y: 76, z: 30, copies: 20},
            {x: 110, y: 43, z: 25, copies: 20},
            {x: 92, y: 81, z: 55, copies: 20},
        ],
    },
    boxstacks: {
        objective: "knapsack",
        bin_types: [{x: 7500, y: 2400, z: 3000}],
        item_types: [
            {x: 2500, y: 800, z: 750, stackability_id: 0, copies: 10},
            {x: 2500, y: 800, z: 1000, stackability_id: 1, copies: 10},
            {x: 2500, y: 800, z: 1250, stackability_id: 2, copies: 10},
        ],
    },
    onedimensional: {
        objective: "bin-packing",
        bin_types: [{length: 1000, copies: 100}],
        item_types: [
            193, 197, 199, 211, 223, 227, 229, 233, 239, 241, 251, 257, 263,
            269, 271, 277, 281, 283, 293, 307, 311, 313, 317, 331, 337, 347,
        ].map((length) => ({length})),
    },
};

// Bound of each objective in the output.
const BOUNDS = {
    "knapsack": "KnapsackBound",
    "bin-packing": "BinPackingBound",
    "variable-sized-bin-packing": "VariableSizedBinPackingBound",
    "open-dimension-x": "OpenDimensionXBound",
    "open-dimension-y": "OpenDimensionYBound",
};

const $ = (id) => document.getElementById(id);

const state = {
    // Rows of the bin and item type tables: objects mapping column keys to
    // values.
    binTypes: [],
    itemTypes: [],
    worker: null,
    running: false,
    problemType: null,
    objective: null,
    // Last update from the worker ('{output, certificate}').
    last: null,
    plotTimer: null,
    previewTimer: null,
    startTime: 0,
};

/////////////////////////////////////////////////////////////////////////////
// Instance form
/////////////////////////////////////////////////////////////////////////////

function problemType() {
    return $("problem-type").value;
}

function defaultRow(columns) {
    const row = {};
    for (const column of columns)
        row[column.key] = column.value;
    return row;
}

function renderTable(table, columns, rows) {
    table.replaceChildren();
    const header = table.createTHead().insertRow();
    for (const column of columns) {
        const th = document.createElement("th");
        th.textContent = column.label;
        header.appendChild(th);
    }
    header.appendChild(document.createElement("th"));
    const body = table.createTBody();
    rows.forEach((row, rowIndex) => {
        const tr = body.insertRow();
        for (const column of columns) {
            const input = document.createElement("input");
            input.type = column.type;
            input.setAttribute("aria-label", column.label);
            if (column.type === "checkbox") {
                input.checked = Boolean(row[column.key]);
                input.addEventListener("change", () => { row[column.key] = input.checked; });
            } else {
                input.min = "0";
                input.step = "any";
                input.value = row[column.key];
                if (column.optional)
                    input.placeholder = "default";
                input.addEventListener("input", () => { row[column.key] = input.value; });
            }
            tr.insertCell().appendChild(input);
        }
        const remove = document.createElement("button");
        remove.type = "button";
        remove.textContent = "Remove";
        remove.addEventListener("click", () => {
            rows.splice(rowIndex, 1);
            renderForm();
        });
        tr.insertCell().appendChild(remove);
    });
}

function renderForm() {
    const type = problemType();
    const irregular = (type === "irregular");
    $("instance-preview-section").hidden = !irregular;
    $("dxf-items").hidden = !irregular;
    if (irregular) {
        irregularForm.renderTable(
            $("bin-types"), state.binTypes, false, schedulePreview, renderForm);
        irregularForm.renderTable(
            $("item-types"), state.itemTypes, true, schedulePreview, renderForm);
        schedulePreview();
        return;
    }
    renderTable($("bin-types"), binColumns(type), state.binTypes);
    renderTable($("item-types"), itemColumns(type), state.itemTypes);
}

// Preview of the shapes of the irregular form, drawn as they are typed.
function schedulePreview() {
    clearTimeout(state.previewTimer);
    state.previewTimer = setTimeout(preview, 300);
}

async function preview() {
    if (problemType() !== "irregular")
        return;
    let instanceObject;
    try {
        instanceObject = formInstance();
    } catch (error) {
        $("preview-error").textContent = error.message;
        return;
    }
    $("preview-error").textContent = "";
    try {
        const figure = irregularForm.previewFigure(instanceObject);
        await Plotly.react($("instance-preview"), figure.data, figure.layout, {
            responsive: true,
            displayModeBar: false,
        });
    } catch (error) {
        console.error(error);
        $("preview-error").textContent = "Error while drawing the preview: " + error.message;
    }
}

// Add an item type for each part of a DXF file. The initial item type is
// replaced if it wasn't modified.
async function loadDxfItems(file) {
    try {
        const {parts, units, warnings} = await irregularForm.readDxfFile(file);
        if (parts.length === 0)
            throw new Error(`no closed contour found in ${file.name}.`);
        const untouched = JSON.stringify(irregularForm.defaultItemRow());
        if (state.itemTypes.length === 1 && JSON.stringify(state.itemTypes[0]) === untouched)
            state.itemTypes = [];
        for (const part of parts)
            state.itemTypes.push({...irregularForm.defaultItemRow(), shape: "dxf", dxf: part});
        $("dxf-items-status").classList.remove("error-text");
        $("dxf-items-status").textContent =
            `Added ${parts.length} item type${(parts.length > 1)? "s": ""} from ${file.name}`
            + ((units !== null)? ` (${units})`: "") + "."
            + ((warnings.length > 0)? ` Warnings: ${warnings.join("; ")}.`: "");
        renderForm();
    } catch (error) {
        $("dxf-items-status").classList.add("error-text");
        $("dxf-items-status").textContent = "Error: " + error.message;
    }
}

function newBinRow(type) {
    return (type === "irregular")? irregularForm.defaultBinRow(): defaultRow(binColumns(type));
}

function newItemRow(type) {
    return (type === "irregular")? irregularForm.defaultItemRow(): defaultRow(itemColumns(type));
}

function resetForm() {
    const type = problemType();
    $("objective").value = defaultObjective(type);
    state.binTypes = [newBinRow(type)];
    state.itemTypes = [newItemRow(type)];
    renderForm();
}

function loadExample() {
    const type = problemType();
    if (type === "irregular") {
        const example = irregularForm.EXAMPLE;
        $("objective").value = example.objective;
        state.binTypes = example.binTypes.map((t) => ({...irregularForm.defaultBinRow(), ...t}));
        state.itemTypes = example.itemTypes.map((t) => ({...irregularForm.defaultItemRow(), ...t}));
        renderForm();
        return;
    }
    const example = EXAMPLES[type];
    $("objective").value = example.objective;
    const fill = (columns, types) => types.map((t) => ({...defaultRow(columns), ...t}));
    state.binTypes = fill(binColumns(type), example.bin_types);
    state.itemTypes = fill(itemColumns(type), example.item_types);
    renderForm();
}

// Instance in the JSON format, from the form.
function formInstance() {
    const type = problemType();
    if (type === "irregular")
        return irregularForm.instance($("objective").value, state.binTypes, state.itemTypes);
    const convert = (columns, rows) => rows.map((row) => {
        const result = {};
        for (const column of columns) {
            const value = row[column.key];
            if (column.type === "checkbox") {
                if (value)
                    result[column.key] = true;
            } else if (value !== "" && value !== null && value !== undefined) {
                const number = Number(value);
                if (!Number.isFinite(number))
                    throw new Error(`invalid ${column.label.toLowerCase()}: "${value}".`);
                result[column.key] = number;
            } else if (!column.optional) {
                throw new Error(`missing ${column.label.toLowerCase()}.`);
            }
        }
        return result;
    });
    if (state.binTypes.length === 0)
        throw new Error("add at least one bin type.");
    if (state.itemTypes.length === 0)
        throw new Error("add at least one item type.");
    return {
        objective: $("objective").value,
        bin_types: convert(binColumns(type), state.binTypes),
        item_types: convert(itemColumns(type), state.itemTypes),
    };
}

function instance() {
    if ($("panel-json").hidden)
        return formInstance();
    const text = $("json-text").value;
    try {
        return JSON.parse(text);
    } catch (error) {
        throw new Error("invalid JSON: " + error.message);
    }
}

function selectTab(name) {
    for (const tab of ["form", "json"]) {
        $("tab-" + tab).setAttribute("aria-selected", String(tab === name));
        $("panel-" + tab).hidden = (tab !== name);
    }
}

/////////////////////////////////////////////////////////////////////////////
// Solving
/////////////////////////////////////////////////////////////////////////////

function setStatus(text, isError = false) {
    $("status").textContent = text;
    $("status").classList.toggle("error", isError);
}

function setRunning(running) {
    state.running = running;
    $("solve").disabled = running || state.worker === null;
    $("stop").disabled = !running;
}

function solve() {
    let instanceObject;
    try {
        instanceObject = instance();
    } catch (error) {
        setStatus("Error: " + error.message, true);
        return;
    }
    const parameters = {optimization_mode: $("optimization-mode").value};
    const timeLimit = $("time-limit").value;
    if (timeLimit !== "")
        parameters.time_limit = Number(timeLimit);

    state.problemType = problemType();
    state.objective = instanceObject.objective;
    state.last = null;
    state.startTime = performance.now();
    $("progress").tBodies[0].replaceChildren();
    $("summary").replaceChildren();
    Plotly.purge($("plot"));
    $("results").hidden = true;

    setRunning(true);
    setStatus("Solving...");
    state.worker.postMessage({
        type: "solve",
        problemType: state.problemType,
        instance: instanceObject,
        parameters,
    });
}

function format(value) {
    if (typeof value !== "number")
        return String(value);
    // At most 6 significant digits, without trailing zeros.
    return Number.isInteger(value)? String(value): String(Number(value.toPrecision(6)));
}

// Short description of a solution, from its JSON output.
function describeSolution(output) {
    const solution = output.Solution;
    const parts = [`${solution.NumberOfItems} items`];
    if (solution.NumberOfBins !== undefined)
        parts.push(`${solution.NumberOfBins} bins`);
    if (state.objective === "knapsack" && solution.ItemProfit !== undefined)
        parts.push(`profit ${format(solution.ItemProfit)}`);
    if (state.objective === "variable-sized-bin-packing" && solution.BinCost !== undefined)
        parts.push(`cost ${format(solution.BinCost)}`);
    if (state.objective === "bin-packing-with-leftovers" && solution.LeftoverValue !== undefined)
        parts.push(`leftover ${format(solution.LeftoverValue)}`);
    if (state.objective === "open-dimension-x" && solution.XMax !== undefined)
        parts.push(`length ${format(solution.XMax)}`);
    return parts.join(", ");
}

function describeBound(output) {
    const key = BOUNDS[state.objective];
    if (key === undefined || output[key] === undefined || output[key] === null)
        return "";
    return format(output[key]);
}

function addProgressRow(output) {
    const row = $("progress").tBodies[0].insertRow();
    row.insertCell().textContent = output.Time.toFixed(3);
    row.insertCell().textContent = describeSolution(output);
    row.insertCell().textContent = describeBound(output);
}

function showResult(result) {
    state.last = result;
    $("results").hidden = false;
    const output = result.output;
    const bound = describeBound(output);
    $("summary").textContent = describeSolution(output)
        + (bound !== ""? `; bound ${bound}`: "")
        + `; ${output.Time.toFixed(2)} s`;
    schedulePlot();
}

// Plotting a solution can take a while: at most once every 500 ms.
function schedulePlot() {
    if (state.plotTimer !== null)
        return;
    state.plotTimer = setTimeout(async () => {
        state.plotTimer = null;
        await plot();
    }, 500);
}

async function plot() {
    const result = state.last;
    if (result === null || result.output.Solution.NumberOfItems === 0)
        return;
    try {
        const visualizer = await import(`./visualize/${state.problemType}.js`);
        const figure = visualizer.figure(result.certificate);
        await Plotly.react($("plot"), figure.data, figure.layout, {responsive: true});
    } catch (error) {
        console.error(error);
        setStatus("Error while drawing the solution: " + error.message, true);
    }
}

function onWorkerMessage(event) {
    const message = event.data;
    if (message.type === "ready") {
        setRunning(false);
        setStatus("Ready.");
    } else if (message.type === "solution") {
        addProgressRow(message.output);
        if (message.output.Solution.NumberOfItems > 0)
            showResult(message);
    } else if (message.type === "done") {
        setRunning(false);
        showResult(message);
        const seconds = (performance.now() - state.startTime) / 1000;
        setStatus(`Done in ${seconds.toFixed(1)} s.`);
    } else if (message.type === "error") {
        setRunning(false);
        setStatus("Error: " + message.error, true);
    }
}

function download(name, type, content) {
    const url = URL.createObjectURL(new Blob([content], {type}));
    const link = document.createElement("a");
    link.href = url;
    link.download = name;
    link.click();
    URL.revokeObjectURL(url);
}

/////////////////////////////////////////////////////////////////////////////
// Initialization
/////////////////////////////////////////////////////////////////////////////

function init() {
    for (const [value, label] of OBJECTIVES) {
        const option = document.createElement("option");
        option.value = value;
        option.textContent = label;
        $("objective").appendChild(option);
    }

    $("problem-type").addEventListener("change", resetForm);
    $("tab-form").addEventListener("click", () => selectTab("form"));
    $("tab-json").addEventListener("click", () => selectTab("json"));
    $("add-bin-type").addEventListener("click", () => {
        state.binTypes.push(newBinRow(problemType()));
        renderForm();
    });
    $("add-item-type").addEventListener("click", () => {
        state.itemTypes.push(newItemRow(problemType()));
        renderForm();
    });
    $("load-example").addEventListener("click", loadExample);
    $("load-dxf-items").addEventListener("click", () => $("dxf-items-file").click());
    $("dxf-items-file").addEventListener("change", async () => {
        const file = $("dxf-items-file").files[0];
        $("dxf-items-file").value = "";
        if (file !== undefined)
            await loadDxfItems(file);
    });
    $("json-file").addEventListener("change", async () => {
        const file = $("json-file").files[0];
        if (file !== undefined)
            $("json-text").value = await file.text();
    });
    $("solve").addEventListener("click", solve);
    $("stop").addEventListener("click", () => state.worker.postMessage({type: "stop"}));
    $("download-certificate").addEventListener("click", () => {
        const json = state.problemType === "irregular";
        download(
            json? "solution.json": "solution.csv",
            json? "application/json": "text/csv",
            state.last.certificate);
    });
    $("download-output").addEventListener("click", () => {
        download("output.json", "application/json", JSON.stringify(state.last.output, null, 4));
    });

    resetForm();

    // The threads of the solver need 'SharedArrayBuffer'.
    if (!window.crossOriginIsolated) {
        $("isolation-error").hidden = false;
        setStatus("The solver can't run on this page.", true);
        return;
    }
    state.worker = new Worker("packingsolver_worker.js");
    state.worker.onmessage = onWorkerMessage;
    state.worker.onerror = (error) => {
        setRunning(false);
        setStatus("Error: " + error.message, true);
    };
}

init();
