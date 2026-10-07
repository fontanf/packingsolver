// PackingSolver web page: the projects of each problem type ('projects.js'),
// stored in the browser. The instance of a project is built in a form (or
// opened from a JSON file), solved in a Web Worker ('packingsolver_worker.js'),
// and its solutions are shown as they are found.

import * as irregularForm from "./irregular_form.js";
import * as form from "./form.js";
import * as projects from "./projects.js";
import {
    openDimension,
    BOX_ROTATIONS,
    binColumns,
    itemColumns,
    UNLIMITED_STAGES,
    INSTANCE_PARAMETERS,
    parameterForced,
    cuttingCostRows,
    OBJECTIVES,
    defaultResource,
    newItemRow,
} from "./form.js";

// Table of the cutting costs: fixed and variable costs of each row.
function cuttingCostsInput(values) {
    const container = document.createElement("div");
    container.className = "cutting-costs";
    const table = document.createElement("table");
    const header = table.createTHead().insertRow();
    for (const text of ["", "Fixed", "Variable"]) {
        const th = document.createElement("th");
        th.textContent = text;
        header.appendChild(th);
    }
    const body = table.createTBody();
    cuttingCostRows(values).forEach(([label, title], i) => {
        values.cutting_costs[i] = values.cutting_costs[i] || {fixed: "", variable: ""};
        const tr = body.insertRow();
        const th = document.createElement("th");
        th.textContent = label;
        th.title = title;
        tr.appendChild(th);
        for (const key of ["fixed", "variable"]) {
            const input = numberInput(values.cutting_costs[i][key], `${label} ${key} cost`,
                (value) => { values.cutting_costs[i][key] = value; }, "0");
            // The costs are integers in the library.
            input.step = "1";
            tr.insertCell().appendChild(input);
        }
    });
    container.appendChild(table);
    // With an unlimited number of stages, the stages with a cost.
    if (UNLIMITED_STAGES(values)) {
        const buttons = document.createElement("div");
        const add = document.createElement("button");
        add.type = "button";
        add.textContent = "Add a stage";
        add.addEventListener("click", () => {
            ++values.cutting_costs_stages;
            renderInstanceParameters();
        });
        const remove = document.createElement("button");
        remove.type = "button";
        remove.textContent = "Remove the last stage";
        remove.disabled = (values.cutting_costs_stages <= 1);
        remove.addEventListener("click", () => {
            --values.cutting_costs_stages;
            values.cutting_costs.length = values.cutting_costs_stages + 1;
            renderInstanceParameters();
        });
        buttons.append(add, remove);
        container.appendChild(buttons);
    }
    return container;
}

function renderInstanceParameters() {
    const container = $("instance-parameters");
    container.replaceChildren();
    const values = state.instanceParameters;
    const parameters = (INSTANCE_PARAMETERS[problemType()] || [])
        .filter((parameter) => parameterShown(parameter, values));
    container.hidden = (parameters.length === 0);
    // A label and an input for each parameter: the two columns of the grid
    // of the objective.
    for (const parameter of parameters) {
        const label = document.createElement("label");
        label.htmlFor = "instance-parameter-" + parameter.key;
        label.textContent = parameter.label;
        const onChange = () => {
            if (parameter.structural) {
                renderInstanceParameters();
                renderForm();
            }
            scheduleFormCheck();
        };
        let input;
        let cell;
        if (parameter.type === "select") {
            input = document.createElement("select");
            for (const [value, text] of parameter.options) {
                const option = document.createElement("option");
                option.value = value;
                option.textContent = text;
                input.appendChild(option);
            }
            const forced = parameterForced(parameter, values);
            input.value = (forced !== undefined)? forced: values[parameter.key];
            input.disabled = (forced !== undefined);
            input.addEventListener("change", () => { values[parameter.key] = input.value; onChange(); });
        } else if (parameter.type === "checkbox") {
            input = document.createElement("input");
            input.type = "checkbox";
            input.checked = values[parameter.key];
            input.addEventListener("change", () => { values[parameter.key] = input.checked; });
        } else if (parameter.type === "ids") {
            input = document.createElement("input");
            input.type = "text";
            input.placeholder = parameter.placeholder || "";
            input.value = values[parameter.key];
            input.addEventListener("input", () => { values[parameter.key] = input.value; scheduleFormCheck(); });
        } else if (parameter.type === "cutting-costs") {
            cell = cuttingCostsInput(values);
            label.removeAttribute("for");
        } else {
            input = document.createElement("input");
            input.type = "number";
            input.min = parameter.integer? "1": "0";
            input.step = (parameter.integer || parameter.wholeNumber)? "1": "any";
            input.placeholder = parameter.placeholder || "";
            input.value = values[parameter.key];
            input.addEventListener("input", () => { values[parameter.key] = input.value; onChange(); });
            if (parameter.unlimited) {
                // The number, and a checkbox which disables it.
                const key = "unlimited_" + parameter.key;
                if (values[key]) {
                    input.disabled = true;
                    input.value = "";
                    input.placeholder = "unlimited";
                }
                const checkbox = document.createElement("input");
                checkbox.type = "checkbox";
                checkbox.checked = values[key];
                checkbox.title = "Unlimited";
                checkbox.setAttribute("aria-label", "Unlimited " + parameter.label.toLowerCase());
                checkbox.addEventListener("change", () => {
                    values[key] = checkbox.checked;
                    renderInstanceParameters();
                    scheduleFormCheck();
                });
                cell = document.createElement("div");
                cell.className = "unlimited";
                cell.append(input, checkbox);
            }
        }
        if (input !== undefined)
            input.id = label.htmlFor;
        container.append(label, cell || input);
    }
}

// Fill the objectives of a problem type.
function renderObjectives(problemType) {
    const select = $("objective");
    select.replaceChildren();
    for (const [value, label, types] of OBJECTIVES) {
        if (types !== undefined && !types.includes(problemType))
            continue;
        const option = document.createElement("option");
        option.value = value;
        option.textContent = label;
        select.appendChild(option);
    }
}

// Bound of each objective in the output.
const BOUNDS = {
    "knapsack": "KnapsackBound",
    "bin-packing": "BinPackingBound",
    "variable-sized-bin-packing": "VariableSizedBinPackingBound",
    "open-dimension-x": "OpenDimensionXBound",
    "open-dimension-y": "OpenDimensionYBound",
    "open-dimension-z": "OpenDimensionZBound",
};

const $ = (id) => document.getElementById(id);

// The functions of the form which depend on the page: the objective, the
// parameters of the instance and the rows of the page.
const parameterShown = (parameter, values) => form.parameterShown(parameter, values, $("objective").value);
const addInstanceParameters = (instanceObject) => form.addInstanceParameters(
    instanceObject, problemType(), state.instanceParameters, $("objective").value);
const columnShown = (column) => form.columnShown(column, $("objective").value, state.instanceParameters);
const unlimitedAllowed = (column) => form.unlimitedAllowed(column, $("objective").value);
const unlimitedValue = (row, column) => form.unlimitedValue(row, column, $("objective").value);
const resourceConsumptions = (resource) => form.resourceConsumptions(resource, state.itemTypes);
const newBinRow = (type) => form.newBinRow(type, $("objective").value);
const formInstance = () => form.toInstance(problemType(), {
    objective: $("objective").value,
    instanceParameters: state.instanceParameters,
    binTypes: state.binTypes,
    itemTypes: state.itemTypes,
});

const state = {
    // Rows of the bin and item type tables: objects mapping column keys to
    // values.
    binTypes: [],
    itemTypes: [],
    worker: null,
    running: false,
    // Objective of the instance being solved.
    objective: null,
    // Solution shown ('{output, certificate, objective}').
    last: null,
    plotTimer: null,
    formCheckTimer: null,
    // Values of the parameters of the instance ('INSTANCE_PARAMETERS').
    instanceParameters: {},
    startTime: 0,
    // Time limit of the optimization, in seconds ('null' if none).
    timeLimit: null,
    // Description of the best solution of the optimization ('null' if none).
    best: null,
    // Timer updating the status while solving.
    statusTimer: null,
    // Whether the results were scrolled into view during the optimization.
    resultsRevealed: false,
    // The projects of all the problem types ('projects.js'), the project
    // opened, and the project being solved ('null' if none).
    projects: [],
    current: null,
    solvingProject: null,
    // Where the projects are stored ('projects.openStore').
    store: null,
    // Projects waiting to be saved: id -> '{project, timer}'.
    saveTimers: new Map(),
};

/////////////////////////////////////////////////////////////////////////////
// Instance form
/////////////////////////////////////////////////////////////////////////////

function problemType() {
    return $("problem-type").value;
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
    const rotations = column.rotations || BOX_ROTATIONS;
    // The 6 rotations of box on 2 lines, the 2 of boxstacks one above the
    // other.
    cell.className = (rotations.length <= 2)? "checkboxes single-column": "checkboxes";
    for (const rotation of rotations) {
        const label = document.createElement("label");
        label.className = "inline";
        const input = document.createElement("input");
        input.type = "checkbox";
        input.checked = row[column.key].includes(rotation);
        input.addEventListener("change", () => {
            row[column.key] = rotations.filter((r) => (r === rotation)?
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
    cell.className = "placed-shapes";
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
    return cell;
}


// Input of a number, checkbox or select field of a row.
function fieldInput(row, column) {
    if (column.type === "select") {
        const select = document.createElement("select");
        select.setAttribute("aria-label", column.label);
        for (const [value, text] of column.options) {
            const option = document.createElement("option");
            option.value = value;
            option.textContent = text;
            select.appendChild(option);
        }
        select.value = row[column.key];
        select.addEventListener("change", () => { row[column.key] = select.value; });
        return select;
    }
    const input = document.createElement("input");
    // A list of ids: "1, 2, 3".
    input.type = (column.type === "ids")? "text": column.type;
    input.setAttribute("aria-label", column.label);
    if (column.type === "checkbox") {
        input.checked = Boolean(row[column.key]);
        input.addEventListener("change", () => {
            row[column.key] = input.checked;
            if (column.structural)
                renderForm();
        });
    } else if (column.type === "ids") {
        input.value = row[column.key];
        input.placeholder = column.placeholder || "";
        input.className = "ids";
        input.addEventListener("input", () => { row[column.key] = input.value; });
    } else {
        input.min = "0";
        input.step = column.integer? "1": "any";
        input.value = row[column.key];
        if (column.optional)
            input.placeholder = column.placeholder || "default";
        input.addEventListener("input", () => { row[column.key] = input.value; });
    }
    return input;
}



// Cell of a number column which can be unlimited: the number, and a checkbox
// which disables it (its header says so).
function unlimitedCell(row, column) {
    const cell = document.createElement("div");
    cell.className = "unlimited";
    const input = fieldInput(row, column);
    const key = "unlimited_" + column.key;
    if (row[key]) {
        input.disabled = true;
        input.value = "";
        input.placeholder = "unlimited";
    }
    const checkbox = document.createElement("input");
    checkbox.type = "checkbox";
    checkbox.checked = Boolean(row[key]);
    checkbox.title = "Unlimited";
    checkbox.setAttribute("aria-label", `Unlimited ${column.label.toLowerCase()}`);
    checkbox.addEventListener("change", () => {
        row[key] = checkbox.checked;
        renderForm();
    });
    cell.append(input, checkbox);
    return cell;
}

// Line, below a row of a table, with the fields of its details.
function addDetailsLine(body, numberOfColumns, row, detailsColumns) {
    const tr = body.insertRow();
    tr.className = "details-line";
    const cell = tr.insertCell();
    cell.colSpan = numberOfColumns;
    const fields = document.createElement("div");
    fields.className = "details";
    for (const column of detailsColumns) {
        if (column.rowShown !== undefined && !column.rowShown(row))
            continue;
        const label = document.createElement("label");
        label.className = "inline";
        label.append(column.label, fieldInput(row, column));
        fields.appendChild(label);
    }
    cell.appendChild(fields);
}

// Rows whose details line is open.
const openDetails = new WeakSet();

function renderTable(table, columns, rows) {
    table.replaceChildren();
    const header = table.createTHead().insertRow();
    // The defects are on a line below their row, the details on another one.
    const defectsColumn = columns.find((column) => column.type === "defects");
    const resourcesColumn = columns.find((column) => column.type === "resources");
    const detailsColumns = columns.filter((column) => column.details && columnShown(column));
    columns = columns.filter(
        (column) => column.type !== "defects" && column.type !== "resources"
            && !column.details && columnShown(column));
    // The id of each row: its position, from 0, as in the C++ code.
    const idHeader = document.createElement("th");
    idHeader.textContent = "Id";
    header.appendChild(idHeader);
    for (const column of columns) {
        const th = document.createElement("th");
        th.textContent = column.label;
        if (unlimitedAllowed(column)) {
            const note = document.createElement("span");
            note.className = "header-note";
            note.textContent = "(or unlimited)";
            th.append(document.createElement("br"), note);
        }
        header.appendChild(th);
    }
    header.appendChild(document.createElement("th"));
    const body = table.createTBody();
    rows.forEach((row, rowIndex) => {
        const tr = body.insertRow();
        const id = tr.insertCell();
        id.className = "row-id";
        id.textContent = String(rowIndex);
        for (const column of columns) {
            if (column.type === "rotations") {
                tr.insertCell().appendChild(rotationsCell(row, column));
                continue;
            }
            if (column.type === "trims") {
                tr.insertCell().appendChild(trimsCell(row, column));
                continue;
            }
            if (unlimitedAllowed(column)) {
                tr.insertCell().appendChild(unlimitedCell(row, column));
                continue;
            }
            // A single copy of the bin for the open dimension objectives.
            if (column.singleForOpenDimension && openDimension($("objective").value)) {
                const input = fieldInput(row, column);
                input.disabled = true;
                input.title = "A single bin for the open dimension objectives";
                tr.insertCell().appendChild(input);
                continue;
            }
            tr.insertCell().appendChild(fieldInput(row, column));
        }
        const buttons = tr.insertCell();
        buttons.className = "row-buttons";
        // The defects are on a line below the row if it has some; the button
        // to add one is on the row.
        if (defectsColumn !== undefined) {
            const add = document.createElement("button");
            add.type = "button";
            add.textContent = "Add a defect";
            add.addEventListener("click", () => {
                row[defectsColumn.key].push({x: "", y: "", width: "", height: ""});
                renderForm();
            });
            buttons.appendChild(add);
        }
        // Same for the resources.
        if (resourcesColumn !== undefined) {
            const add = document.createElement("button");
            add.type = "button";
            add.textContent = "Add a resource";
            add.addEventListener("click", () => {
                row[resourcesColumn.key].push(defaultResource());
                renderForm();
            });
            buttons.appendChild(add);
        }
        if (detailsColumns.length > 0) {
            const more = document.createElement("button");
            more.type = "button";
            const open = openDetails.has(row);
            more.textContent = open? "Less": "More";
            more.setAttribute("aria-expanded", String(open));
            // A mark if a field of the details isn't at its default value.
            const modified = detailsColumns.filter(
                (column) => JSON.stringify(row[column.key]) !== JSON.stringify(column.value));
            if (modified.length > 0) {
                more.classList.add("modified");
                more.title = "Set: " + modified.map((column) => column.label.toLowerCase()).join(", ");
            }
            more.addEventListener("click", () => {
                if (openDetails.has(row))
                    openDetails.delete(row);
                else
                    openDetails.add(row);
                renderForm();
            });
            buttons.appendChild(more);
        }
        const remove = document.createElement("button");
        remove.type = "button";
        remove.textContent = "Remove";
        remove.addEventListener("click", () => {
            rows.splice(rowIndex, 1);
            renderForm();
        });
        buttons.appendChild(remove);
        if (openDetails.has(row) && detailsColumns.length > 0)
            addDetailsLine(body, columns.length + 2, row, detailsColumns);
        if (defectsColumn !== undefined && row[defectsColumn.key].length > 0)
            addDefectsLine(body, columns.length + 2, defectsCell(row, defectsColumn));
        if (resourcesColumn !== undefined && row[resourcesColumn.key].length > 0)
            addDefectsLine(body, columns.length + 2, resourcesCell(row, resourcesColumn), "Resources");
    });
}

// Line, below a row of a table, with the defects of the row.
function addDefectsLine(body, numberOfColumns, defects, text = "Defects") {
    const tr = body.insertRow();
    tr.className = "placed-shapes-line";
    const cell = tr.insertCell();
    cell.colSpan = numberOfColumns;
    const label = document.createElement("span");
    label.className = "placed-shapes-label";
    label.textContent = text;
    cell.append(label, defects);
}



// Cell of a column of type "resources": a line for each resource, then a line
// for each of its consumptions.
function resourcesCell(row, column) {
    const cell = document.createElement("div");
    cell.className = "placed-shapes";
    const resources = row[column.key];
    const labelled = (text, element) => {
        const label = document.createElement("label");
        label.className = "inline";
        label.append(text, element);
        return label;
    };
    const button = (text, title, onClick) => {
        const element = document.createElement("button");
        element.type = "button";
        element.textContent = text;
        if (title)
            element.title = title;
        element.addEventListener("click", onClick);
        return element;
    };
    resources.forEach((resource, i) => {
        resource.consumptions = resourceConsumptions(resource);
        const name = `resource ${i}`;
        const line = document.createElement("div");
        line.className = "dimensions resource";
        line.appendChild(labelled("Capacity", numberInput(resource.capacity, `${name} capacity`,
            (value) => { resource.capacity = value; })));
        const penalize = document.createElement("input");
        penalize.type = "checkbox";
        penalize.checked = resource.penalize;
        penalize.setAttribute("aria-label", `${name} penalize`);
        penalize.addEventListener("change", () => { resource.penalize = penalize.checked; renderForm(); });
        line.appendChild(labelled("Penalize", penalize));
        // Exceeding the capacity is allowed, at this cost.
        if (resource.penalize) {
            line.appendChild(labelled("Penalty", numberInput(resource.penalty, `${name} penalty`,
                (value) => { resource.penalty = value; }, "0")));
        }
        line.appendChild(button("Add a consumption", "", () => {
            // The first item type without a consumption.
            const used = resource.consumptions.map((consumption) => consumption.itemRow);
            const itemRow = state.itemTypes.find((r) => !used.includes(r)) || state.itemTypes[0];
            resource.consumptions.push({itemRow, values: ""});
            renderForm();
        }));
        line.appendChild(button("×", "Remove the resource", () => { resources.splice(i, 1); renderForm(); }));
        cell.appendChild(line);
        resource.consumptions.forEach((consumption, j) => {
            const consumptionLine = document.createElement("div");
            consumptionLine.className = "dimensions consumption-line";
            // The id of the item type (its position, from 0, as in the "Id"
            // column).
            const itemTypeId = document.createElement("input");
            itemTypeId.type = "number";
            itemTypeId.min = "0";
            itemTypeId.max = String(state.itemTypes.length - 1);
            itemTypeId.step = "1";
            itemTypeId.className = "item-type-id";
            itemTypeId.value = (consumption.itemRow !== null)? String(state.itemTypes.indexOf(consumption.itemRow)): "";
            itemTypeId.setAttribute("aria-label", `${name} consumption ${j} item type id`);
            itemTypeId.addEventListener("input", () => {
                const id = Number(itemTypeId.value);
                consumption.itemRow = (itemTypeId.value !== "" && Number.isInteger(id)
                        && id >= 0 && id < state.itemTypes.length)?
                    state.itemTypes[id]: null;
            });
            consumptionLine.appendChild(labelled("Item type", itemTypeId));
            const values = document.createElement("input");
            values.type = "text";
            values.className = "vertices";
            values.spellcheck = false;
            values.placeholder = "e.g. 2, or 1, 2, 3 for the successive copies";
            values.title = "The consumption of each copy, or of the successive copies "
                + "(the last value for the next copies)";
            values.value = consumption.values;
            values.setAttribute("aria-label", `${name} consumption ${j}`);
            values.addEventListener("input", () => { consumption.values = values.value; });
            consumptionLine.appendChild(labelled("Consumption", values));
            consumptionLine.appendChild(button("×", "Remove the consumption", () => {
                resource.consumptions.splice(j, 1);
                renderForm();
            }));
            cell.appendChild(consumptionLine);
        });
    });
    return cell;
}

function renderForm() {
    const type = problemType();
    const irregular = (type === "irregular");
    // A single bin for the open dimension objectives.
    $("add-bin-type").disabled = openDimension($("objective").value);
    $("form-error").hidden = !irregular;
    $("file-items").hidden = !irregular;
    if (irregular) {
        const objective = $("objective").value;
        irregularForm.renderTable(
            $("bin-types"), state.binTypes, false, objective, scheduleFormCheck, renderForm, state.itemTypes);
        irregularForm.renderTable(
            $("item-types"), state.itemTypes, true, objective, scheduleFormCheck, renderForm);
        scheduleFormCheck();
        return;
    }
    renderTable($("bin-types"), binColumns(type), state.binTypes);
    renderTable($("item-types"), itemColumns(type), state.itemTypes);
}

// Errors of the irregular form, shown as the values are typed (the shape
// of each row is drawn in its thumbnail).
function scheduleFormCheck() {
    clearTimeout(state.formCheckTimer);
    state.formCheckTimer = setTimeout(checkForm, 300);
}

function checkForm() {
    if (problemType() !== "irregular")
        return;
    try {
        formInstance();
        $("form-error").textContent = "";
    } catch (error) {
        $("form-error").textContent = error.message;
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



/////////////////////////////////////////////////////////////////////////////
// Projects
/////////////////////////////////////////////////////////////////////////////

const PROBLEM_TYPES = ["rectangleguillotine", "rectangle", "box", "boxstacks", "onedimensional", "irregular"];

// The last problem type and the last project of each problem type, to open
// them again when the page is reloaded or the problem type changed.
function remember(key, value) {
    try {
        localStorage.setItem("packingsolver." + key, value);
    } catch (error) {
        // Not remembered.
    }
}

function remembered(key) {
    try {
        return localStorage.getItem("packingsolver." + key);
    } catch (error) {
        return null;
    }
}

// The form and the parameters of the current project, from the page. The
// form of an example isn't modified.
function syncCurrent() {
    const project = state.current;
    if (project === null)
        return;
    if (!project.example) {
        project.form = {
            objective: $("objective").value,
            instanceParameters: state.instanceParameters,
            binTypes: state.binTypes,
            itemTypes: state.itemTypes,
        };
    }
    project.parameters = {
        optimizationMode: $("optimization-mode").value,
        timeLimit: $("time-limit").value,
    };
}

// Save a project at most every 500 ms.
function scheduleSave(project = state.current) {
    if (project === null || state.saveTimers.has(project.id))
        return;
    state.saveTimers.set(project.id, {
        project,
        timer: setTimeout(() => saveProject(project), 500),
    });
}

async function saveProject(project) {
    const pending = state.saveTimers.get(project.id);
    if (pending !== undefined) {
        clearTimeout(pending.timer);
        state.saveTimers.delete(project.id);
    }
    if (project === state.current)
        syncCurrent();
    // Removed in the meantime.
    if (!state.projects.includes(project))
        return;
    try {
        await state.store.save(project);
    } catch (error) {
        console.error(error);
        $("storage-note").textContent = "The projects couldn't be saved in this browser: " + error.message;
        $("storage-note").classList.add("error-text");
    }
}

// Save the projects waiting to be saved, when the page is left.
function flushSaves() {
    for (const {project} of [...state.saveTimers.values()])
        saveProject(project);
}

function projectNames(problemType, except = null) {
    return state.projects
        .filter((p) => p.problemType === problemType && p !== except)
        .map((p) => p.name);
}

function addProject(project) {
    state.projects.push(project);
    saveProject(project);
    openProject(project);
}

function openProject(project) {
    if (state.current !== null && state.current !== project)
        saveProject(state.current);
    state.current = project;
    remember("problemType", project.problemType);
    remember("project." + project.problemType, project.id);
    $("problem-type").value = project.problemType;
    updateGallery();
    renderObjectives(project.problemType);
    $("objective").value = project.form.objective;
    state.instanceParameters = project.form.instanceParameters;
    state.binTypes = project.form.binTypes;
    state.itemTypes = project.form.itemTypes;
    $("optimization-mode").value = project.parameters.optimizationMode;
    $("time-limit").value = project.parameters.timeLimit;
    for (const id of ["form-error", "file-items-status", "download-instance-error", "load-json-error"])
        $(id).textContent = "";
    $("form-ignored").hidden = true;
    // The examples can't be modified.
    $("form-fields").disabled = project.example;
    renderProjectList();
    renderProjectHeader();
    renderInstanceParameters();
    renderForm();
    renderResults();
}

// Open the last project opened of a problem type, its example otherwise.
function openProblemType(problemType) {
    const id = remembered("project." + problemType);
    const ofType = projects.projectsOfType(state.projects, problemType);
    openProject(ofType.find((p) => p.id === id) || ofType[0]);
}

function renderProjectList() {
    const list = $("project-list");
    list.replaceChildren();
    for (const project of projects.projectsOfType(state.projects, problemType())) {
        const button = document.createElement("button");
        button.type = "button";
        button.setAttribute("aria-current", String(project === state.current));
        const name = document.createElement("span");
        name.textContent = project.name;
        button.appendChild(name);
        const tag = (project === state.solvingProject)? "Solving": (project.example? "Example": "");
        if (tag !== "") {
            const span = document.createElement("span");
            span.className = "project-tag" + ((project === state.solvingProject)? " solving": "");
            span.textContent = tag;
            button.appendChild(span);
        }
        button.addEventListener("click", () => openProject(project));
        const item = document.createElement("li");
        item.appendChild(button);
        list.appendChild(item);
    }
}

function renderProjectHeader() {
    const project = state.current;
    $("project-name").textContent = project.name;
    $("project-name").hidden = false;
    $("project-name-input").hidden = true;
    $("rename-project").disabled = project.example;
    $("delete-project").disabled = project.example || project === state.solvingProject;
    $("delete-project").title = (project === state.solvingProject)? "Stop the optimization to delete the project": "";
    $("example-note").hidden = !project.example;
}

// Rename the current project, in an input in place of its name.
function startRename() {
    const input = $("project-name-input");
    input.value = state.current.name;
    $("project-name").hidden = true;
    input.hidden = false;
    input.focus();
    input.select();
}

function endRename(commit) {
    const input = $("project-name-input");
    if (input.hidden)
        return;
    const project = state.current;
    const name = input.value.trim();
    if (commit && name !== "" && name !== project.name) {
        project.name = projects.uniqueName(name, projectNames(project.problemType, project));
        saveProject(project);
        renderProjectList();
    }
    renderProjectHeader();
}

function duplicateProject() {
    syncCurrent();
    const project = state.current;
    addProject(projects.duplicateProject(project,
        projects.uniqueName(project.name + " (copy)", projectNames(project.problemType))));
}

async function deleteProject() {
    const project = state.current;
    if (project.example || project === state.solvingProject)
        return;
    if (!confirm(`Delete the project "${project.name}"?`))
        return;
    const pending = state.saveTimers.get(project.id);
    if (pending !== undefined) {
        clearTimeout(pending.timer);
        state.saveTimers.delete(project.id);
    }
    state.projects.splice(state.projects.indexOf(project), 1);
    state.current = null;
    openProject(projects.projectsOfType(state.projects, project.problemType)[0]);
    try {
        await state.store.remove(project.id);
    } catch (error) {
        console.error(error);
    }
}

function newProject() {
    const type = problemType();
    addProject(projects.newProject(type, projects.uniqueName("New project", projectNames(type))));
}

// A new project from an instance in the JSON format, named after the file,
// with the fields which the solver doesn't read listed.
async function loadJsonFile(file) {
    const type = problemType();
    let opened;
    try {
        let json;
        try {
            json = JSON.parse(await file.text());
        } catch (error) {
            throw new Error("invalid JSON: " + error.message);
        }
        opened = projects.projectFromInstance(type,
            projects.uniqueName(projects.nameFromFileName(file.name), projectNames(type)), json);
    } catch (error) {
        $("load-json-error").textContent = "Error: " + error.message;
        return;
    }
    addProject(opened.project);
    $("form-ignored").textContent = "Ignored fields (not read by the solver): " + opened.ignored.join(", ") + ".";
    $("form-ignored").hidden = (opened.ignored.length === 0);
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
    clearInterval(state.statusTimer);
    state.statusTimer = null;
    if (running) {
        updateSolvingStatus();
        state.statusTimer = setInterval(updateSolvingStatus, 200);
    }
}

// Status while solving: the project solved if it isn't the current one, the
// elapsed time, out of the time limit, and the best solution found so far.
function updateSolvingStatus() {
    const seconds = (performance.now() - state.startTime) / 1000;
    const project = state.solvingProject;
    let text = (project !== null && project !== state.current)?
        `Solving "${project.name}"...`: "Solving...";
    text += ` ${seconds.toFixed(1)} s`;
    if (state.timeLimit !== null)
        text += ` / ${format(state.timeLimit)} s`;
    if (state.best !== null)
        text += ` · best: ${state.best}`;
    setStatus(text);
}

// Scroll to the results when the first solution of the optimization is shown,
// if they aren't visible. Only once, not to fight the user scrolling back to
// the form.
function revealResults() {
    if (state.resultsRevealed)
        return;
    state.resultsRevealed = true;
    const top = $("results").getBoundingClientRect().top;
    // The bottom of the window is hidden by the buttons.
    const visibleBottom = window.innerHeight - $("actions").offsetHeight;
    if (top >= 0 && top < visibleBottom)
        return;
    const reducedMotion = window.matchMedia("(prefers-reduced-motion: reduce)").matches;
    $("results").scrollIntoView({behavior: reducedMotion? "auto": "smooth", block: "start"});
}

function solve() {
    syncCurrent();
    let instanceObject;
    try {
        instanceObject = formInstance();
    } catch (error) {
        setStatus("Error: " + error.message, true);
        return;
    }
    const parameters = {optimization_mode: $("optimization-mode").value};
    const timeLimit = $("time-limit").value;
    if (timeLimit !== "")
        parameters.time_limit = Number(timeLimit);

    // The solution and the progress of the optimization are those of the
    // project, even if another project is opened in the meantime.
    const project = state.current;
    project.result = null;
    project.progress = [];
    state.solvingProject = project;
    state.objective = instanceObject.objective;
    state.startTime = performance.now();
    state.timeLimit = (parameters.time_limit !== undefined)? parameters.time_limit: null;
    state.best = null;
    state.resultsRevealed = false;
    renderResults();
    renderProjectList();
    renderProjectHeader();
    saveProject(project);

    setRunning(true);
    state.worker.postMessage({
        type: "solve",
        problemType: project.problemType,
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
function describeSolution(output, objective) {
    const solution = output.Solution;
    const parts = [`${solution.NumberOfItems} items`];
    if (solution.NumberOfBins !== undefined)
        parts.push(`${solution.NumberOfBins} bins`);
    if (objective === "knapsack" && solution.ItemProfit !== undefined)
        parts.push(`profit ${format(solution.ItemProfit)}`);
    if (objective === "variable-sized-bin-packing" && solution.BinCost !== undefined)
        parts.push(`cost ${format(solution.BinCost)}`);
    if (objective === "bin-packing-with-leftovers" && solution.LeftoverValue !== undefined)
        parts.push(`leftover ${format(solution.LeftoverValue)}`);
    if (objective === "bin-packing-cutting-cost" && solution.CuttingCost !== undefined)
        parts.push(`cost ${format(solution.CuttingCost)}`);
    // The used length along the open dimension ('Width' and 'Height' for
    // rectangleguillotine).
    const lengths = {
        "open-dimension-x": [solution.XMax, solution.Width],
        "open-dimension-y": [solution.YMax, solution.Height],
        "open-dimension-z": [solution.ZMax],
    };
    if (objective in lengths) {
        const length = lengths[objective].find((value) => value !== undefined);
        if (length !== undefined)
            parts.push(`length ${format(length)}`);
    }
    if (objective === "open-dimension-xy" && solution.OpenDimensionXYArea !== undefined)
        parts.push(`area ${format(solution.OpenDimensionXYArea)}`);
    return parts.join(", ");
}

function describeBound(output, objective) {
    const key = BOUNDS[objective];
    if (key === undefined || output[key] === undefined || output[key] === null)
        return "";
    return format(output[key]);
}

function addProgressRow(line) {
    const row = $("progress").tBodies[0].insertRow();
    row.insertCell().textContent = line.time.toFixed(3);
    row.insertCell().textContent = line.solution;
    row.insertCell().textContent = line.bound;
}

// The solution and the progress of the current project.
function renderResults() {
    const project = state.current;
    $("progress").tBodies[0].replaceChildren();
    for (const line of project.progress)
        addProgressRow(line);
    $("summary").replaceChildren();
    Plotly.purge($("plot"));
    state.last = null;
    if (project.result === null) {
        $("results").hidden = true;
        return;
    }
    showResult(project.result);
}

// Show a solution of the current project ('{output, certificate,
// objective}').
function showResult(result) {
    state.last = result;
    $("results").hidden = false;
    $("download-certificate").textContent = (state.current.problemType === "irregular")?
        "Download the certificate (JSON)": "Download the certificate (CSV)";
    const output = result.output;
    const bound = describeBound(output, result.objective);
    $("summary").textContent = describeSolution(output, result.objective)
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
        const visualizer = await import(`./visualize/${state.current.problemType}.js`);
        const figure = visualizer.figure(result.certificate);
        // Another project opened in the meantime.
        if (result !== state.last)
            return;
        await Plotly.react($("plot"), figure.data, figure.layout, {responsive: true});
    } catch (error) {
        console.error(error);
        setStatus("Error while drawing the solution: " + error.message, true);
    }
}

function onWorkerMessage(event) {
    const message = event.data;
    const project = state.solvingProject;
    if (message.type === "ready") {
        setRunning(false);
        setStatus("Ready.");
    } else if (message.type === "solution") {
        if (project === null)
            return;
        const line = {
            time: message.output.Time,
            solution: describeSolution(message.output, state.objective),
            bound: describeBound(message.output, state.objective),
        };
        project.progress.push(line);
        if (project === state.current)
            addProgressRow(line);
        if (message.output.Solution.NumberOfItems > 0) {
            project.result = {output: message.output, certificate: message.certificate, objective: state.objective};
            state.best = line.solution;
            if (project === state.current) {
                showResult(project.result);
                revealResults();
            }
            if (state.running)
                updateSolvingStatus();
        }
        scheduleSave(project);
    } else if (message.type === "done") {
        setRunning(false);
        state.solvingProject = null;
        if (project !== null) {
            project.result = {output: message.output, certificate: message.certificate, objective: state.objective};
            saveProject(project);
            if (project === state.current) {
                showResult(project.result);
                revealResults();
            }
        }
        renderProjectList();
        renderProjectHeader();
        const seconds = (performance.now() - state.startTime) / 1000;
        setStatus(((project !== null && project !== state.current)? `"${project.name}": done`: "Done")
            + ` in ${seconds.toFixed(1)} s.`);
    } else if (message.type === "error") {
        setRunning(false);
        state.solvingProject = null;
        renderProjectList();
        renderProjectHeader();
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

// Download the instance of the form, in the JSON format, named after the
// project.
function downloadInstance() {
    let instanceObject;
    try {
        instanceObject = formInstance();
    } catch (error) {
        $("download-instance-error").textContent = "Error: " + error.message;
        return;
    }
    $("download-instance-error").textContent = "";
    download(`${state.current.name}.json`, "application/json",
        JSON.stringify(instanceObject, null, 4) + "\n");
}

/////////////////////////////////////////////////////////////////////////////
// Initialization
/////////////////////////////////////////////////////////////////////////////

async function init() {
    // Some fields are only shown for some objectives.
    $("objective").addEventListener("change", () => {
        // A single bin for the open dimension objectives.
        if (openDimension($("objective").value) && state.binTypes.length > 0) {
            state.binTypes.splice(1);
            state.binTypes[0].copies = 1;
            state.binTypes[0].unlimited_copies = false;
        }
        renderInstanceParameters();
        renderForm();
    });
    $("problem-type").addEventListener("change", () => openProblemType(problemType()));
    // The examples of solutions select their problem type.
    for (const button of document.querySelectorAll("#gallery button")) {
        button.addEventListener("click", () => {
            openProblemType(button.dataset.problemType);
            $("problem").scrollIntoView({behavior: "smooth"});
        });
    }
    $("add-bin-type").addEventListener("click", () => {
        state.binTypes.push(newBinRow(problemType()));
        renderForm();
    });
    $("add-item-type").addEventListener("click", () => {
        state.itemTypes.push(newItemRow(problemType()));
        renderForm();
    });
    $("load-file-items").addEventListener("click", () => $("file-items-file").click());
    $("file-items-file").addEventListener("change", async () => {
        const file = $("file-items-file").files[0];
        $("file-items-file").value = "";
        if (file !== undefined) {
            await loadFileItems(file);
            scheduleSave();
        }
    });
    $("new-project").addEventListener("click", newProject);
    $("load-json").addEventListener("click", () => $("json-file").click());
    $("json-file").addEventListener("change", async () => {
        const file = $("json-file").files[0];
        $("json-file").value = "";
        if (file !== undefined)
            await loadJsonFile(file);
    });
    $("rename-project").addEventListener("click", startRename);
    $("project-name-input").addEventListener("keydown", (event) => {
        if (event.key === "Enter")
            endRename(true);
        else if (event.key === "Escape")
            endRename(false);
    });
    $("project-name-input").addEventListener("blur", () => endRename(true));
    $("duplicate-project").addEventListener("click", duplicateProject);
    $("delete-project").addEventListener("click", deleteProject);
    // The changes of the form and of the parameters are saved.
    for (const section of [$("form-fields"), $("parameters")]) {
        for (const event of ["input", "change", "click"])
            section.addEventListener(event, () => scheduleSave());
    }
    window.addEventListener("pagehide", flushSaves);
    document.addEventListener("visibilitychange", () => {
        if (document.visibilityState === "hidden")
            flushSaves();
    });
    $("solve").addEventListener("click", solve);
    $("stop").addEventListener("click", () => state.worker.postMessage({type: "stop"}));
    $("download-certificate").addEventListener("click", () => {
        const json = state.current.problemType === "irregular";
        download(
            `${state.current.name}_solution.${json? "json": "csv"}`,
            json? "application/json": "text/csv",
            state.last.certificate);
    });
    $("download-instance").addEventListener("click", downloadInstance);
    // The error of a download is cleared when the instance changes.
    for (const event of ["input", "change", "click"]) {
        $("problem").addEventListener(event, (e) => {
            if (e.target !== $("download-instance"))
                $("download-instance-error").textContent = "";
        });
    }
    $("download-output").addEventListener("click", () => {
        download(`${state.current.name}_output.json`, "application/json",
            JSON.stringify(state.last.output, null, 4));
    });

    // The projects: the examples, and those stored in the browser.
    state.store = await projects.openStore();
    if (!state.store.persistent) {
        $("storage-note").textContent =
            "The projects can't be saved in this browser: they are lost when the page is closed.";
    }
    let stored = [];
    try {
        stored = await state.store.load();
    } catch (error) {
        console.error(error);
    }
    state.projects = projects.withExamples(PROBLEM_TYPES, stored);
    const type = remembered("problemType");
    openProblemType(PROBLEM_TYPES.includes(type)? type: problemType());

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
        state.solvingProject = null;
        renderProjectList();
        renderProjectHeader();
        setStatus("Error: " + error.message, true);
    };
}

init();
