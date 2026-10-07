// PackingSolver web page: builds an instance (form or JSON), solves it in a
// Web Worker ('packingsolver_worker.js') and shows the solutions as they are
// found.

import * as irregularForm from "./irregular_form.js";
import * as form from "./form.js";
import {
    openDimension,
    BOX_ROTATIONS,
    binColumns,
    itemColumns,
    UNLIMITED_STAGES,
    INSTANCE_PARAMETERS,
    defaultInstanceParameters,
    parameterForced,
    cuttingCostRows,
    OBJECTIVES,
    defaultObjective,
    EXAMPLES,
    defaultRow,
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
    problemType: null,
    objective: null,
    // Last update from the worker ('{output, certificate}').
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



function resetForm() {
    $("form-ignored").hidden = true;
    const type = problemType();
    renderObjectives(type);
    $("objective").value = defaultObjective(type);
    state.instanceParameters = defaultInstanceParameters(type);
    renderInstanceParameters();
    state.binTypes = [newBinRow(type)];
    state.itemTypes = [newItemRow(type)];
    renderForm();
}

function loadExample() {
    $("form-ignored").hidden = true;
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


// Load an instance in the JSON format in the form, with the fields which the
// solver doesn't read listed.
async function loadJsonFile(file) {
    let opened;
    try {
        let json;
        try {
            json = JSON.parse(await file.text());
        } catch (error) {
            throw new Error("invalid JSON: " + error.message);
        }
        opened = form.fromInstance(problemType(), json);
    } catch (error) {
        $("load-json-error").textContent = "Error: " + error.message;
        return;
    }
    $("load-json-error").textContent = "";
    renderObjectives(problemType());
    $("objective").value = opened.objective;
    state.instanceParameters = opened.instanceParameters;
    state.binTypes = opened.binTypes;
    state.itemTypes = opened.itemTypes;
    renderInstanceParameters();
    renderForm();
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

// Status while solving: the elapsed time, out of the time limit, and the best
// solution found so far.
function updateSolvingStatus() {
    const seconds = (performance.now() - state.startTime) / 1000;
    let text = `Solving... ${seconds.toFixed(1)} s`;
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

    state.problemType = problemType();
    state.objective = instanceObject.objective;
    state.last = null;
    state.startTime = performance.now();
    state.timeLimit = (parameters.time_limit !== undefined)? parameters.time_limit: null;
    state.best = null;
    state.resultsRevealed = false;
    $("progress").tBodies[0].replaceChildren();
    $("summary").replaceChildren();
    Plotly.purge($("plot"));
    $("results").hidden = true;

    setRunning(true);
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
    if (state.objective === "bin-packing-cutting-cost" && solution.CuttingCost !== undefined)
        parts.push(`cost ${format(solution.CuttingCost)}`);
    // The used length along the open dimension ('Width' and 'Height' for
    // rectangleguillotine).
    const lengths = {
        "open-dimension-x": [solution.XMax, solution.Width],
        "open-dimension-y": [solution.YMax, solution.Height],
        "open-dimension-z": [solution.ZMax],
    };
    if (state.objective in lengths) {
        const length = lengths[state.objective].find((value) => value !== undefined);
        if (length !== undefined)
            parts.push(`length ${format(length)}`);
    }
    if (state.objective === "open-dimension-xy" && solution.OpenDimensionXYArea !== undefined)
        parts.push(`area ${format(solution.OpenDimensionXYArea)}`);
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
    state.best = describeSolution(output);
    $("summary").textContent = state.best
        + (bound !== ""? `; bound ${bound}`: "")
        + `; ${output.Time.toFixed(2)} s`;
    revealResults();
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
        if (message.output.Solution.NumberOfItems > 0) {
            showResult(message);
            if (state.running)
                updateSolvingStatus();
        }
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

// Download the instance of the form, in the JSON format, named after its
// problem type.
function downloadInstance() {
    let instanceObject;
    try {
        instanceObject = formInstance();
    } catch (error) {
        $("download-instance-error").textContent = "Error: " + error.message;
        return;
    }
    $("download-instance-error").textContent = "";
    download(`instance_${problemType()}.json`, "application/json",
        JSON.stringify(instanceObject, null, 4) + "\n");
}

/////////////////////////////////////////////////////////////////////////////
// Initialization
/////////////////////////////////////////////////////////////////////////////

function init() {
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
    $("load-json").addEventListener("click", () => $("json-file").click());
    $("json-file").addEventListener("change", async () => {
        const file = $("json-file").files[0];
        $("json-file").value = "";
        if (file !== undefined)
            await loadJsonFile(file);
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
    $("download-instance").addEventListener("click", downloadInstance);
    // The error of a download is cleared when the instance changes.
    for (const event of ["input", "change", "click"]) {
        $("problem").addEventListener(event, (e) => {
            if (e.target !== $("download-instance"))
                $("download-instance-error").textContent = "";
        });
    }
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
