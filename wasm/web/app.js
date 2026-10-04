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

// Rotations of the box problem type (see 'box::Rotation'): the dimensions of
// the item along x, y and z.
const BOX_ROTATIONS = ["XYZ", "YXZ", "ZYX", "YZX", "XZY", "ZXY"];

function binColumns(problemType) {
    const columns = [
        ...DIMENSIONS[problemType].map((d) => numberColumn(d, 100)),
        numberColumn(["copies", "Copies"], 1),
        numberColumn(["cost", "Cost"], "", true),
    ];
    if (problemType === "rectangleguillotine")
        columns.push({key: "trims", label: "Trims", type: "trims", value: {}});
    if (problemType === "rectangleguillotine" || problemType === "rectangle")
        columns.push({key: "defects", label: "Defects", type: "defects", value: []});
    // Columns of type "defects" are rendered on a line below their row.
    return columns;
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
    if (problemType === "box")
        columns.push({key: "rotations", label: "Rotations", type: "rotations", value: ["XYZ"]});
    return columns;
}

// Parameters of the instance, for each problem type: 'key' is the field of
// the JSON instance format ('parameters' object if 'inParameters').
// Empty fields keep the default value of the library ('placeholder').
const INSTANCE_PARAMETERS = {
    rectangleguillotine: [
        {key: "number_of_stages", label: "Number of stages", type: "select", value: "3",
            options: [["2", "2"], ["3", "3"], ["4", "4"], ["5", "5"], ["unlimited", "Unlimited"]]},
        {key: "cut_type", label: "Cut type", type: "select", value: "non-exact",
            options: [["non-exact", "Non-exact"], ["exact", "Exact"], ["homogenous", "Homogenous"],
                ["roadef2018", "ROADEF 2018"]]},
        {key: "first_stage_orientation", label: "First stage orientation", type: "select", value: "vertical",
            options: [["vertical", "Vertical"], ["horizontal", "Horizontal"], ["any", "Any"]]},
        {key: "cut_thickness", label: "Cut thickness", type: "number", placeholder: "0"},
        {key: "minimum_waste_length", label: "Minimum waste length", type: "number", placeholder: "0"},
        {key: "minimum_distance_1_cuts", label: "Minimum distance between 1-cuts", type: "number", placeholder: "0"},
        {key: "maximum_distance_1_cuts", label: "Maximum distance between 1-cuts", type: "number", placeholder: "none"},
        {key: "minimum_distance_2_cuts", label: "Minimum distance between 2-cuts", type: "number", placeholder: "0"},
        {key: "maximum_distance_2_cuts", label: "Maximum distance between 2-cuts", type: "number", placeholder: "none"},
        {key: "maximum_number_1_cuts", label: "Maximum number of 1-cuts", type: "number", placeholder: "none"},
        {key: "maximum_number_2_cuts", label: "Maximum number of 2-cuts", type: "number", placeholder: "none"},
        {key: "cut_through_defects", label: "Cut through defects", type: "checkbox", value: false},
    ],
    irregular: [
        {key: "item_item_minimum_spacing", label: "Minimum spacing between items",
            type: "number", placeholder: "0", inParameters: true},
    ],
};

function defaultInstanceParameters(problemType) {
    const values = {};
    for (const parameter of INSTANCE_PARAMETERS[problemType] || [])
        values[parameter.key] = (parameter.value !== undefined)? parameter.value: "";
    return values;
}

function renderInstanceParameters() {
    const container = $("instance-parameters");
    container.replaceChildren();
    const parameters = INSTANCE_PARAMETERS[problemType()] || [];
    container.hidden = (parameters.length === 0);
    const values = state.instanceParameters;
    // A label and an input for each parameter: the two columns of the grid
    // of the objective.
    for (const parameter of parameters) {
        const label = document.createElement("label");
        label.htmlFor = "instance-parameter-" + parameter.key;
        label.textContent = parameter.label;
        let input;
        if (parameter.type === "select") {
            input = document.createElement("select");
            for (const [value, text] of parameter.options) {
                const option = document.createElement("option");
                option.value = value;
                option.textContent = text;
                input.appendChild(option);
            }
            input.value = values[parameter.key];
            input.addEventListener("change", () => { values[parameter.key] = input.value; schedulePreview(); });
        } else if (parameter.type === "checkbox") {
            input = document.createElement("input");
            input.type = "checkbox";
            input.checked = values[parameter.key];
            input.addEventListener("change", () => { values[parameter.key] = input.checked; });
        } else {
            input = document.createElement("input");
            input.type = "number";
            input.min = "0";
            input.step = "any";
            input.placeholder = parameter.placeholder;
            input.value = values[parameter.key];
            input.addEventListener("input", () => { values[parameter.key] = input.value; schedulePreview(); });
        }
        input.id = label.htmlFor;
        container.append(label, input);
    }
}

// Add the parameters of the instance to an instance in the JSON format.
function addInstanceParameters(instanceObject) {
    for (const parameter of INSTANCE_PARAMETERS[problemType()] || []) {
        const value = state.instanceParameters[parameter.key];
        let converted;
        if (parameter.type === "checkbox") {
            if (!value)
                continue;
            converted = true;
        } else if (parameter.type === "select") {
            converted = (parameter.key === "number_of_stages" && value !== "unlimited")? Number(value): value;
        } else {
            if (value === "")
                continue;
            converted = Number(value);
            if (!Number.isFinite(converted) || converted < 0)
                throw new Error(`invalid ${parameter.label.toLowerCase()}: "${value}".`);
        }
        if (parameter.inParameters) {
            instanceObject.parameters = instanceObject.parameters || {};
            instanceObject.parameters[parameter.key] = converted;
        } else {
            instanceObject[parameter.key] = converted;
        }
    }
    return instanceObject;
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
    // Values of the parameters of the instance ('INSTANCE_PARAMETERS').
    instanceParameters: {},
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
        row[column.key] = structuredClone(column.value);
    return row;
}

function numberInput(value, label, onInput, placeholder = "") {
    const input = document.createElement("input");
    input.type = "number";
    input.min = "0";
    input.step = "any";
    input.value = value;
    input.placeholder = placeholder;
    input.setAttribute("aria-label", label);
    input.addEventListener("input", () => onInput(input.value));
    return input;
}

// Cell of a column of type "rotations": a checkbox for each rotation.
function rotationsCell(row, column) {
    const cell = document.createElement("div");
    cell.className = "checkboxes";
    for (const rotation of BOX_ROTATIONS) {
        const label = document.createElement("label");
        label.className = "inline";
        const input = document.createElement("input");
        input.type = "checkbox";
        input.checked = row[column.key].includes(rotation);
        input.addEventListener("change", () => {
            row[column.key] = BOX_ROTATIONS.filter((r) => (r === rotation)?
                input.checked: row[column.key].includes(r));
        });
        label.append(input, rotation);
        cell.appendChild(label);
    }
    return cell;
}

// Cell of a column of type "trims": the trim of each side.
function trimsCell(row, column) {
    const cell = document.createElement("div");
    cell.className = "dimensions";
    for (const [side, letter] of [["left", "L"], ["right", "R"], ["bottom", "B"], ["top", "T"]]) {
        const label = document.createElement("label");
        label.className = "inline";
        label.append(letter, numberInput(row[column.key][side] || "", `${side} trim`,
            (value) => { row[column.key][side] = value; }, "0"));
        cell.appendChild(label);
    }
    return cell;
}

// Cell of a column of type "defects": a list of rectangles.
function defectsCell(row, column) {
    const cell = document.createElement("div");
    cell.className = "defects";
    const defects = row[column.key];
    defects.forEach((defect, i) => {
        const line = document.createElement("div");
        line.className = "dimensions";
        for (const [key, letter] of [["x", "X"], ["y", "Y"], ["width", "W"], ["height", "H"]]) {
            const label = document.createElement("label");
            label.className = "inline";
            label.append(letter, numberInput(defect[key], `defect ${key}`,
                (value) => { defect[key] = value; }));
            line.appendChild(label);
        }
        const remove = document.createElement("button");
        remove.type = "button";
        remove.textContent = "×";
        remove.title = "Remove the defect";
        remove.addEventListener("click", () => { defects.splice(i, 1); renderForm(); });
        line.appendChild(remove);
        cell.appendChild(line);
    });
    const add = document.createElement("button");
    add.type = "button";
    add.textContent = "Add a defect";
    add.addEventListener("click", () => {
        defects.push({x: "", y: "", width: "", height: ""});
        renderForm();
    });
    cell.appendChild(add);
    return cell;
}

function renderTable(table, columns, rows) {
    table.replaceChildren();
    const header = table.createTHead().insertRow();
    // The defects are on a line below their row.
    const defectsColumn = columns.find((column) => column.type === "defects");
    columns = columns.filter((column) => column.type !== "defects");
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
            if (column.type === "rotations") {
                tr.insertCell().appendChild(rotationsCell(row, column));
                continue;
            }
            if (column.type === "trims") {
                tr.insertCell().appendChild(trimsCell(row, column));
                continue;
            }
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
        if (defectsColumn !== undefined)
            addDefectsLine(body, columns.length + 1, defectsCell(row, defectsColumn));
    });
}

// Line, below a row of a table, with the defects of the row.
function addDefectsLine(body, numberOfColumns, defects) {
    const tr = body.insertRow();
    tr.className = "defects-line";
    const cell = tr.insertCell();
    cell.colSpan = numberOfColumns;
    const label = document.createElement("span");
    label.className = "defects-label";
    label.textContent = "Defects";
    cell.append(label, defects);
}

function renderForm() {
    const type = problemType();
    const irregular = (type === "irregular");
    $("instance-preview-section").hidden = !irregular;
    $("file-items").hidden = !irregular;
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

// Add an item type for each part of a DXF or SVG file. The initial item type is
// replaced if it wasn't modified.
async function loadFileItems(file) {
    try {
        const {parts, units, warnings} = await irregularForm.readShapeFile(file);
        if (parts.length === 0)
            throw new Error(`no closed contour found in ${file.name}.`);
        const untouched = JSON.stringify(irregularForm.defaultItemRow());
        if (state.itemTypes.length === 1 && JSON.stringify(state.itemTypes[0]) === untouched)
            state.itemTypes = [];
        for (const part of parts)
            state.itemTypes.push({...irregularForm.defaultItemRow(), shape: "file", file: part});
        $("file-items-status").classList.remove("error-text");
        $("file-items-status").textContent =
            `Added ${parts.length} item type${(parts.length > 1)? "s": ""} from ${file.name}`
            + ((units !== null)? ` (${units})`: "") + "."
            + ((warnings.length > 0)? ` Warnings: ${warnings.join("; ")}.`: "");
        renderForm();
    } catch (error) {
        $("file-items-status").classList.add("error-text");
        $("file-items-status").textContent = "Error: " + error.message;
    }
}

// Highlight the example of the selected problem type.
function updateGallery() {
    for (const button of document.querySelectorAll("#gallery button"))
        button.setAttribute("aria-pressed", String(button.dataset.problemType === problemType()));
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
    state.instanceParameters = defaultInstanceParameters(type);
    renderInstanceParameters();
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
    if (type === "irregular") {
        return addInstanceParameters(
            irregularForm.instance($("objective").value, state.binTypes, state.itemTypes));
    }
    const convert = (kind, columns, rows) => rows.map((row, i) => {
        try {
            return convertRow(columns, row);
        } catch (error) {
            throw new Error(`${kind} type ${i + 1}: ${error.message}`);
        }
    });
    const convertNumber = (value, name) => {
        const number = Number(value);
        if (value === "" || value === null || value === undefined || !Number.isFinite(number))
            throw new Error(`invalid ${name}: "${value}".`);
        return number;
    };
    const convertRow = (columns, row) => {
        const result = {};
        for (const column of columns) {
            const value = row[column.key];
            if (column.type === "rotations") {
                if (value.length === 0)
                    throw new Error("select at least one rotation.");
                result.rotations = value;
            } else if (column.type === "trims") {
                for (const side of ["left", "right", "bottom", "top"]) {
                    if (value[side] !== undefined && value[side] !== "")
                        result[side + "_trim"] = convertNumber(value[side], side + " trim");
                }
            } else if (column.type === "defects") {
                if (value.length > 0) {
                    result.defects = value.map((defect, j) => {
                        const converted = {};
                        for (const key of ["x", "y", "width", "height"])
                            converted[key] = convertNumber(defect[key], `defect ${j + 1} ${key}`);
                        return converted;
                    });
                }
            } else if (column.type === "checkbox") {
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
    };
    if (state.binTypes.length === 0)
        throw new Error("add at least one bin type.");
    if (state.itemTypes.length === 0)
        throw new Error("add at least one item type.");
    return addInstanceParameters({
        objective: $("objective").value,
        bin_types: convert("bin", binColumns(type), state.binTypes),
        item_types: convert("item", itemColumns(type), state.itemTypes),
    });
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

    $("problem-type").addEventListener("change", () => {
        resetForm();
        updateGallery();
    });
    // The examples of solutions select their problem type.
    for (const button of document.querySelectorAll("#gallery button")) {
        button.addEventListener("click", () => {
            $("problem-type").value = button.dataset.problemType;
            resetForm();
            updateGallery();
            $("problem").scrollIntoView({behavior: "smooth"});
        });
    }
    updateGallery();
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
    $("load-file-items").addEventListener("click", () => $("file-items-file").click());
    $("file-items-file").addEventListener("change", async () => {
        const file = $("file-items-file").files[0];
        $("file-items-file").value = "";
        if (file !== undefined)
            await loadFileItems(file);
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
