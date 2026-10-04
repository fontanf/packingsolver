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

// Fields of a row shown in its details line (opened with its "More" button)
// instead of a column. 'placeholder' is the value used if the field is empty;
// 'objectives', if given, the objectives for which the field is shown (it is
// ignored for the others).
function detailsNumber(key, label, placeholder, options = {}) {
    return {key, label, type: "number", value: "", optional: true, details: true, placeholder, ...options};
}

// Copies of a bin or item type, which can be unlimited ('copies' -1, with a
// checkbox next to them): bin types except for the knapsack objective, item
// types only for the knapsack objective. And the minimum number of copies to
// use, in the details: for bin types only for the variable-sized bin packing
// objective, for item types only for the knapsack objective (all the items
// must be packed with the other objectives).
function copiesColumns(isItem) {
    const copies = numberColumn(["copies", "Copies"], 1);
    copies.unlimited = isItem?
        (objective) => objective === "knapsack":
        (objective) => objective !== "knapsack";
    const objectives = isItem? ["knapsack"]: ["variable-sized-bin-packing"];
    return [copies, detailsNumber("copies_min", "Minimum copies", "0", {objectives})];
}

// Type of the trims of each side of the bins of the rectangleguillotine
// problem type: hard (no item can be placed in the trim) or soft (the trim
// is only applied if it is not cut through).
function trimTypeColumns() {
    return [["left", "hard"], ["right", "soft"], ["bottom", "hard"], ["top", "soft"]].map(([side, value]) => ({
        key: side + "_trim_type",
        label: side[0].toUpperCase() + side.slice(1) + " trim type",
        type: "select",
        options: [["hard", "Hard"], ["soft", "Soft"]],
        value,
        details: true,
    }));
}

// Problem types with weights: the maximum weight of the bins and the weight
// of the items.
const WEIGHTS = ["rectangle", "box", "boxstacks", "onedimensional"];

// Rotations of the box problem type (see 'box::Rotation'): the dimensions of
// the item along x, y and z.
const BOX_ROTATIONS = ["XYZ", "YXZ", "ZYX", "YZX", "XZY", "ZXY"];

function binColumns(problemType) {
    const columns = [
        ...DIMENSIONS[problemType].map((d) => numberColumn(d, 100)),
        ...copiesColumns(false),
        // The cost of the bins only matters for variable-sized bin packing.
        {...numberColumn(["cost", "Cost"], "", true), objectives: ["variable-sized-bin-packing"]},
    ];
    if (WEIGHTS.includes(problemType))
        columns.push(detailsNumber("maximum_weight", "Maximum weight", "none"));
    if (problemType === "rectangleguillotine") {
        columns.push({key: "trims", label: "Trims", type: "trims", value: {}});
        columns.push(...trimTypeColumns());
    }
    if (problemType === "rectangleguillotine" || problemType === "rectangle")
        columns.push({key: "defects", label: "Defects", type: "defects", value: []});
    // Columns of type "defects" are rendered on a line below their row.
    return columns;
}

function itemColumns(problemType) {
    const columns = [
        ...DIMENSIONS[problemType].map((d) => numberColumn(d, 10)),
        ...copiesColumns(true),
        // The profit of the items only matters for the knapsack objective.
        {...numberColumn(["profit", "Profit"], "", true), objectives: ["knapsack"]},
    ];
    if (WEIGHTS.includes(problemType))
        columns.push(detailsNumber("weight", "Weight", "0"));
    if (problemType === "rectangleguillotine" || problemType === "rectangle")
        columns.push({key: "oriented", label: "Oriented", type: "checkbox", value: false});
    // Items with the same stack id are cut in their order (all the item types
    // have a stack id, or none).
    if (problemType === "rectangleguillotine")
        columns.push(detailsNumber("stack_id", "Stack id", "none", {integer: true}));
    if (problemType === "boxstacks")
        columns.push(numberColumn(["stackability_id", "Stackability id"], 0));
    if (problemType === "box")
        columns.push({key: "rotations", label: "Rotations", type: "rotations", value: ["XYZ"]});
    return columns;
}

// Parameters of the instance, for each problem type: 'key' is the field of
// the JSON instance format ('parameters' object if 'inParameters').
// Empty fields keep the default value of the library ('placeholder').
// Optional attributes:
// - 'unlimited': the value can be unlimited, with a checkbox next to it
//   (checked by default); it is then not written;
// - 'integer': the value is a positive integer;
// - 'objectives': the objectives for which the parameter is shown;
// - 'shown(values)': whether the parameter is shown;
// - 'forced(values)': the value imposed by other parameters, if any (the
//   input is then disabled, and the parameter not written);
// - 'structural': the parameters are rendered again when it changes.
// The parameters which are not shown are not written.

// The parameters of the 2-cuts are not allowed with 2 stages.
const TWO_CUTS = (values) => values.number_of_stages !== "2";
// With an unlimited number of stages, the cuts are exact, in any
// orientation.
const UNLIMITED_STAGES = (values) => values.number_of_stages === "unlimited";
const CUTTING_COST = ["bin-packing-cutting-cost"];

const INSTANCE_PARAMETERS = {
    rectangleguillotine: [
        {key: "number_of_stages", label: "Number of stages", type: "select", value: "3",
            options: [["2", "2"], ["3", "3"], ["unlimited", "Unlimited"]], structural: true},
        {key: "cut_type", label: "Cut type", type: "select", value: "non-exact",
            options: [["non-exact", "Non-exact"], ["exact", "Exact"], ["homogenous", "Homogenous"],
                ["roadef2018", "ROADEF 2018"]],
            forced: (values) => UNLIMITED_STAGES(values)? "exact": undefined,
            // The number of cutting costs depends on it.
            structural: true},
        {key: "first_stage_orientation", label: "First stage orientation", type: "select", value: "vertical",
            options: [["vertical", "Vertical"], ["horizontal", "Horizontal"], ["any", "Any"]],
            forced: (values) => UNLIMITED_STAGES(values)? "any": undefined},
        {key: "cut_thickness", label: "Cut thickness", type: "number", placeholder: "0"},
        {key: "minimum_waste_length", label: "Minimum waste length", type: "number", placeholder: "0"},
        {key: "minimum_distance_1_cuts", label: "Minimum distance between 1-cuts", type: "number", placeholder: "0"},
        {key: "maximum_distance_1_cuts", label: "Maximum distance between 1-cuts", type: "number", unlimited: true},
        {key: "minimum_distance_2_cuts", label: "Minimum distance between 2-cuts", type: "number", placeholder: "0",
            shown: TWO_CUTS},
        {key: "maximum_distance_2_cuts", label: "Maximum distance between 2-cuts", type: "number", unlimited: true,
            shown: TWO_CUTS},
        {key: "maximum_number_1_cuts", label: "Maximum number of 1-cuts", type: "number", unlimited: true,
            integer: true},
        {key: "maximum_number_2_cuts", label: "Maximum number of 2-cuts", type: "number", unlimited: true,
            integer: true, shown: TWO_CUTS},
        {key: "cut_through_defects", label: "Cut through defects", type: "checkbox", value: false},
        {key: "waste_cost", label: "Waste cost", type: "number", placeholder: "0", objectives: CUTTING_COST},
        {key: "cutting_costs", label: "Cutting costs", type: "cutting-costs", value: [], objectives: CUTTING_COST},
    ],
    irregular: [
        {key: "item_item_minimum_spacing", label: "Minimum spacing between items",
            type: "number", placeholder: "0", inParameters: true},
    ],
};

function defaultInstanceParameters(problemType) {
    const values = {};
    for (const parameter of INSTANCE_PARAMETERS[problemType] || []) {
        values[parameter.key] = (parameter.value !== undefined)? structuredClone(parameter.value): "";
        if (parameter.unlimited)
            values["unlimited_" + parameter.key] = true;
    }
    // Number of stages of the cutting costs, with an unlimited number of
    // stages.
    values.cutting_costs_stages = 3;
    return values;
}

function parameterShown(parameter, values) {
    return (parameter.objectives === undefined || parameter.objectives.includes($("objective").value))
        && (parameter.shown === undefined || parameter.shown(values));
}

function parameterForced(parameter, values) {
    return (parameter.forced === undefined)? undefined: parameter.forced(values);
}

// Rows of the cutting costs: the bin, then the cuts of each stage, and the
// extra cut of the non-exact cut types.
function cuttingCostRows(values) {
    const rows = [["Bin", "Fixed cost per bin, variable cost per unit of area of the bin"]];
    const unlimited = UNLIMITED_STAGES(values);
    const stages = unlimited? values.cutting_costs_stages: Number(values.number_of_stages);
    for (let stage = 1; stage <= stages; ++stage)
        rows.push([`${stage}-cuts`, "Fixed cost per cut, variable cost per unit of length of the cut"]);
    if (!unlimited && (values.cut_type === "non-exact" || values.cut_type === "roadef2018")) {
        rows.push(["Extra cut", "The extra cut that non-exact cuts may need at the deepest stage, "
            + "fixed cost per cut, variable cost per unit of length of the cut"]);
    }
    return rows;
}

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
            tr.insertCell().appendChild(numberInput(values.cutting_costs[i][key], `${label} ${key} cost`,
                (value) => { values.cutting_costs[i][key] = value; }, "0"));
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
            if (parameter.structural)
                renderInstanceParameters();
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
        } else if (parameter.type === "cutting-costs") {
            cell = cuttingCostsInput(values);
            label.removeAttribute("for");
        } else {
            input = document.createElement("input");
            input.type = "number";
            input.min = parameter.integer? "1": "0";
            input.step = parameter.integer? "1": "any";
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

// Add the parameters of the instance to an instance in the JSON format.
function addInstanceParameters(instanceObject) {
    const values = state.instanceParameters;
    for (const parameter of INSTANCE_PARAMETERS[problemType()] || []) {
        if (!parameterShown(parameter, values) || parameterForced(parameter, values) !== undefined)
            continue;
        const value = values[parameter.key];
        const name = parameter.label.toLowerCase();
        let converted;
        if (parameter.type === "checkbox") {
            if (!value)
                continue;
            converted = true;
        } else if (parameter.type === "select") {
            converted = (parameter.key === "number_of_stages" && value !== "unlimited")? Number(value): value;
        } else if (parameter.type === "cutting-costs") {
            const rows = cuttingCostRows(values);
            converted = rows.map(([label], i) => {
                const cost = values.cutting_costs[i] || {fixed: "", variable: ""};
                const result = {};
                for (const key of ["fixed", "variable"]) {
                    const number = (cost[key] === "")? 0: Number(cost[key]);
                    if (!Number.isFinite(number) || number < 0)
                        throw new Error(`invalid ${key} cutting cost of the ${label.toLowerCase()}: "${cost[key]}".`);
                    result[key] = number;
                }
                return result;
            });
            // Without any cost, the default (all 0).
            if (converted.every((cost) => cost.fixed === 0 && cost.variable === 0))
                continue;
        } else {
            if (parameter.unlimited && values["unlimited_" + parameter.key])
                continue;
            if (value === "") {
                if (parameter.unlimited)
                    throw new Error(`${name}: enter a value, or check "unlimited".`);
                continue;
            }
            converted = Number(value);
            if (!Number.isFinite(converted) || converted < 0
                    || (parameter.integer && (!Number.isInteger(converted) || converted < 1))) {
                throw new Error(`invalid ${name}: "${value}".`);
            }
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

// Objectives, and the problem types for which they are available (all if
// not given).
const OBJECTIVES = [
    ["knapsack", "Knapsack: maximize the profit of the packed items"],
    ["bin-packing", "Bin packing: minimize the number of bins"],
    ["bin-packing-with-leftovers", "Bin packing with leftovers"],
    ["variable-sized-bin-packing", "Variable-sized bin packing: minimize the cost of the bins"],
    ["bin-packing-cutting-cost", "Bin packing with cutting costs: minimize the cost of the cuts",
        ["rectangleguillotine"]],
    ["open-dimension-x", "Open dimension X: minimize the length",
        ["rectangleguillotine", "rectangle", "box", "boxstacks", "irregular"]],
    ["open-dimension-y", "Open dimension Y: minimize the width",
        ["rectangleguillotine", "rectangle", "box", "boxstacks", "irregular"]],
    ["feasibility", "Feasibility"],
];

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
    "open-dimension-z": "OpenDimensionZBound",
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
    formCheckTimer: null,
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
    for (const column of columns) {
        row[column.key] = structuredClone(column.value);
        if (column.unlimited)
            row["unlimited_" + column.key] = false;
    }
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

// Whether a column is shown for the current objective.
function columnShown(column) {
    return column.objectives === undefined || column.objectives.includes($("objective").value);
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
    input.type = column.type;
    input.setAttribute("aria-label", column.label);
    if (column.type === "checkbox") {
        input.checked = Boolean(row[column.key]);
        input.addEventListener("change", () => { row[column.key] = input.checked; });
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

// Whether unlimited values are allowed for a column, for the current
// objective.
function unlimitedAllowed(column) {
    return column.unlimited !== undefined && column.unlimited($("objective").value);
}

// Whether the value of a column of a row is unlimited (then its value is
// ignored).
function unlimitedValue(row, column) {
    return unlimitedAllowed(column) && Boolean(row["unlimited_" + column.key]);
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
    const detailsColumns = columns.filter((column) => column.details && columnShown(column));
    columns = columns.filter(
        (column) => column.type !== "defects" && !column.details && columnShown(column));
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
            addDetailsLine(body, columns.length + 1, row, detailsColumns);
        if (defectsColumn !== undefined && row[defectsColumn.key].length > 0)
            addDefectsLine(body, columns.length + 1, defectsCell(row, defectsColumn));
    });
}

// Line, below a row of a table, with the defects of the row.
function addDefectsLine(body, numberOfColumns, defects) {
    const tr = body.insertRow();
    tr.className = "placed-shapes-line";
    const cell = tr.insertCell();
    cell.colSpan = numberOfColumns;
    const label = document.createElement("span");
    label.className = "placed-shapes-label";
    label.textContent = "Defects";
    cell.append(label, defects);
}

function renderForm() {
    const type = problemType();
    const irregular = (type === "irregular");
    $("form-error").hidden = !irregular;
    $("file-items").hidden = !irregular;
    if (irregular) {
        const objective = $("objective").value;
        irregularForm.renderTable(
            $("bin-types"), state.binTypes, false, objective, scheduleFormCheck, renderForm);
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

// Objectives for which the bin types created have unlimited copies.
const UNLIMITED_BINS_OBJECTIVES = [
    "bin-packing", "bin-packing-with-leftovers", "variable-sized-bin-packing", "bin-packing-cutting-cost"];

function newBinRow(type) {
    const row = (type === "irregular")? irregularForm.defaultBinRow(): defaultRow(binColumns(type));
    if (UNLIMITED_BINS_OBJECTIVES.includes($("objective").value))
        row.unlimited_copies = true;
    return row;
}

function newItemRow(type) {
    return (type === "irregular")? irregularForm.defaultItemRow(): defaultRow(itemColumns(type));
}

function resetForm() {
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
            // The fields which are not shown for the objective are ignored.
            if (!columnShown(column))
                continue;
            if (unlimitedValue(row, column)) {
                result[column.key] = -1;
            } else if (column.type === "rotations") {
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
            } else if (column.type === "select") {
                // The default value isn't written.
                if (value !== column.value)
                    result[column.key] = value;
            } else if (value !== "" && value !== null && value !== undefined) {
                const number = Number(value);
                if (!Number.isFinite(number))
                    throw new Error(`invalid ${column.label.toLowerCase()}: "${value}".`);
                if (column.integer && !Number.isInteger(number))
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
    const itemTypes = convert("item", itemColumns(type), state.itemTypes);
    // The stack ids: all the item types or none.
    const withStack = itemTypes.filter((itemType) => itemType.stack_id !== undefined).length;
    if (withStack > 0 && withStack < itemTypes.length) {
        const i = itemTypes.findIndex((itemType) => itemType.stack_id === undefined);
        throw new Error(`item type ${i + 1}: missing stack id (if an item type has a stack id, they all must).`);
    }
    return addInstanceParameters({
        objective: $("objective").value,
        bin_types: convert("bin", binColumns(type), state.binTypes),
        item_types: itemTypes,
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

// Download the instance of the form or of the JSON tab, in the JSON format,
// named after its problem type.
function downloadInstance() {
    let instanceObject;
    try {
        instanceObject = instance();
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
