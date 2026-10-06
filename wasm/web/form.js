// The form of the instance of the web page, without the page: the fields of
// each problem type, and the conversions between the form and the JSON
// instance format.

import * as irregularForm from "./irregular_form.js";

// Columns of the bin and item type tables, for each problem type. 'key' is
// the field of the JSON instance format.
export const DIMENSIONS = {
    rectangleguillotine: [["width", "Width"], ["height", "Height"]],
    rectangle: [["x", "Width"], ["y", "Height"]],
    box: [["x", "X"], ["y", "Y"], ["z", "Z"]],
    boxstacks: [["x", "X"], ["y", "Y"], ["z", "Z"]],
    onedimensional: [["length", "Length"]],
};

export function numberColumn([key, label], value, optional = false) {
    return {key, label, type: "number", value, optional};
}

// Fields of a row shown in its details line (opened with its "More" button)
// instead of a column. 'placeholder' is the value used if the field is empty;
// 'objectives', if given, the objectives for which the field is shown (it is
// ignored for the others).
export function detailsNumber(key, label, placeholder, options = {}) {
    return {key, label, type: "number", value: "", optional: true, details: true, placeholder, ...options};
}

// Copies of a bin or item type, which can be unlimited ('copies' -1, with a
// checkbox next to them): bin types except for the knapsack objective, item
// types only for the knapsack objective. And the minimum number of copies to
// use, in the details: for bin types only for the variable-sized bin packing
// objective, for item types only for the knapsack objective (all the items
// must be packed with the other objectives).
export function copiesColumns(isItem) {
    const copies = numberColumn(["copies", "Copies"], 1);
    // A single copy of the bin for the open dimension objectives.
    copies.singleForOpenDimension = !isItem;
    copies.unlimited = isItem?
        (objective) => objective === "knapsack":
        (objective) => objective !== "knapsack" && !openDimension(objective);
    const objectives = isItem? ["knapsack"]: ["variable-sized-bin-packing"];
    return [copies, detailsNumber("copies_min", "Minimum copies", "0", {objectives})];
}

// Type of the trims of each side of the bins of the rectangleguillotine
// problem type: hard (no item can be placed in the trim) or soft (the trim
// is only applied if it is not cut through).
export function trimTypeColumns() {
    return [["left", "hard"], ["right", "soft"], ["bottom", "hard"], ["top", "soft"]].map(([side, value]) => ({
        key: side + "_trim_type",
        label: side[0].toUpperCase() + side.slice(1) + " trim type",
        type: "select",
        options: [["hard", "Hard"], ["soft", "Soft"]],
        value,
        details: true,
    }));
}

// Whether an objective is an open dimension one: the instance then has a
// single bin (one bin type, one copy).
export function openDimension(objective) {
    return objective.startsWith("open-dimension");
}

// The semi-trailer truck of a bin type of the boxstacks problem type: a
// checkbox, and its data when it is checked (the details of the row,
// 'semi_trailer_truck' object of the JSON format). Empty fields keep their
// default value, except the two distances which must be > 0.
export function truckColumns() {
    const truck = (key, label, placeholder, required = false) => detailsNumber(key, label, placeholder, {
        truck: true, required, rowShown: (row) => row.semi_trailer_truck});
    return [
        {key: "semi_trailer_truck", label: "Semi-trailer truck", type: "checkbox", value: false,
            details: true, structural: true},
        truck("tractor_weight", "Tractor weight", "0"),
        truck("front_axle_middle_axle_distance", "Front axle - middle axle distance", "", true),
        truck("front_axle_tractor_gravity_center_distance", "Front axle - tractor gravity center distance", "0"),
        truck("front_axle_harness_distance", "Front axle - harness distance", "0"),
        truck("empty_trailer_weight", "Empty trailer weight", "0"),
        truck("harness_rear_axle_distance", "Harness - rear axle distance", "", true),
        truck("trailer_gravity_center_rear_axle_distance", "Trailer gravity center - rear axle distance", "0"),
        truck("trailer_start_harness_distance", "Trailer start - harness distance", "0"),
        truck("rear_axle_maximum_weight", "Rear axle maximum weight", "unlimited"),
        truck("middle_axle_maximum_weight", "Middle axle maximum weight", "unlimited"),
    ];
}

// Problem types with eligibility: ids of the bin types, id of the item types.
export const ELIGIBILITY = ["rectangle", "onedimensional"];

// Problem types with weights: the maximum weight of the bins and the weight
// of the items.
export const WEIGHTS = ["rectangle", "box", "boxstacks", "onedimensional"];

// Rotations of the box problem type (see 'box::Rotation'): the dimensions of
// the item along x, y and z.
export const BOX_ROTATIONS = ["XYZ", "YXZ", "ZYX", "YZX", "XZY", "ZXY"];

export function binColumns(problemType) {
    const columns = [
        ...DIMENSIONS[problemType].map((d) => numberColumn(d, 100)),
        ...copiesColumns(false),
        // The cost of the bins only matters for variable-sized bin packing.
        {...numberColumn(["cost", "Cost"], "", true), objectives: ["variable-sized-bin-packing"]},
    ];
    if (WEIGHTS.includes(problemType))
        columns.push({...numberColumn(["maximum_weight", "Maximum weight"], "", true), placeholder: "none"});
    if (problemType === "rectangleguillotine") {
        columns.push({key: "trims", label: "Trims", type: "trims", value: {}});
        columns.push(...trimTypeColumns());
    }
    // The item types with an eligibility id can only be packed in the bin types
    // which have it.
    if (ELIGIBILITY.includes(problemType)) {
        columns.push({key: "eligibility_ids", label: "Eligibility ids", type: "ids", value: "",
            details: true, placeholder: "none, e.g. 1, 2"});
    }
    if (problemType === "boxstacks") {
        // Maximum weight of a stack per unit of area of its footprint.
        columns.push(detailsNumber("maximum_stack_density", "Maximum stack density", "unlimited"));
        columns.push(...truckColumns());
    }
    // Resources consumed by the items packed in the bins.
    if (["rectangleguillotine", "rectangle", "box", "onedimensional"].includes(problemType))
        columns.push({key: "resources", label: "Resources", type: "resources", value: []});
    // The defects of the bins (of their floor for boxstacks).
    if (["rectangleguillotine", "rectangle", "boxstacks"].includes(problemType))
        columns.push({key: "defects", label: "Defects", type: "defects", value: []});
    // Columns of type "defects" are rendered on a line below their row.
    return columns;
}

export function itemColumns(problemType) {
    const columns = [
        ...DIMENSIONS[problemType].map((d) => numberColumn(d, 10)),
        ...copiesColumns(true),
        // The profit of the items only matters for the knapsack objective.
        {...numberColumn(["profit", "Profit"], "", true), objectives: ["knapsack"]},
    ];
    if (WEIGHTS.includes(problemType))
        columns.push({...numberColumn(["weight", "Weight"], "", true), placeholder: "0"});
    if (problemType === "rectangleguillotine" || problemType === "rectangle")
        columns.push({key: "oriented", label: "Oriented", type: "checkbox", value: false});
    if (problemType === "rectangle" || problemType === "boxstacks") {
        // The group of an item type, for the unloading constraint.
        columns.push(detailsNumber("group_id", "Group", "0", {integer: true,
            shown: (values) => values.unloading_constraint !== "none"}));
    }
    if (problemType === "boxstacks") {
        // Height removed when the item is stacked on another item.
        columns.push(detailsNumber("nesting_height", "Nesting height", "0"));
        // Maximum number of items in a stack containing this item type.
        columns.push(detailsNumber("maximum_stackability", "Maximum stackability", "unlimited", {integer: true}));
        // Maximum weight of the items stacked above the items of this type.
        columns.push(detailsNumber("maximum_weight_above", "Maximum weight above", "unlimited"));
    }
    if (ELIGIBILITY.includes(problemType))
        columns.push(detailsNumber("eligibility_id", "Eligibility id", "any bin", {integer: true}));
    if (problemType === "onedimensional") {
        // Length removed when the item is packed after another item.
        columns.push(detailsNumber("nesting_length", "Nesting length", "0"));
        // Maximum number of items in a bin containing this item type.
        columns.push(detailsNumber("maximum_stackability", "Maximum stackability", "unlimited", {integer: true}));
        // Maximum weight of the items packed after the items of this type.
        columns.push(detailsNumber("maximum_weight_after", "Maximum weight after", "unlimited"));
    }
    // Items with the same stack id are cut in their order (all the item types
    // have a stack id, or none).
    if (problemType === "rectangleguillotine")
        columns.push(detailsNumber("stack_id", "Stack id", "none", {integer: true}));
    if (problemType === "boxstacks")
        columns.push(numberColumn(["stackability_id", "Stackability id"], 0));
    if (problemType === "box")
        columns.push({key: "rotations", label: "Rotations", type: "rotations", value: ["XYZ"]});
    // The items of boxstacks stay on their base: only the rotation around the
    // vertical axis.
    if (problemType === "boxstacks") {
        columns.push({key: "rotations", label: "Rotations", type: "rotations", value: ["XYZ"],
            rotations: ["XYZ", "YXZ"]});
    }
    return columns;
}

// Parameters of the instance, for each problem type: 'key' is the field of
// the JSON instance format ('parameters' object if 'inParameters').
// Empty fields keep the default value of the library ('placeholder').
// Optional attributes:
// - 'unlimited': the value can be unlimited, with a checkbox next to it
//   (checked by default); it is then not written;
// - 'integer': the value is a positive integer;
// - 'wholeNumber': the value is an integer >= 0;
// - 'positive': the value is > 0;
// - 'required': the value can't be empty;
// - 'objectives': the objectives for which the parameter is shown;
// - 'shown(values)': whether the parameter is shown;
// - 'forced(values)': the value imposed by other parameters, if any (the
//   input is then disabled, and the parameter not written);
// - 'structural': the parameters are rendered again when it changes.
// The parameters which are not shown are not written.

// The parameters of the 2-cuts are not allowed with 2 stages.
export const TWO_CUTS = (values) => values.number_of_stages !== "2";
// With an unlimited number of stages, the cuts are exact, in any
// orientation.
export const UNLIMITED_STAGES = (values) => values.number_of_stages === "unlimited";
export const CUTTING_COST = ["bin-packing-cutting-cost"];

// The unloading constraints of the rectangle and boxstacks problem types:
// the order in which the groups of items are unloaded.
export const UNLOADING_CONSTRAINTS = [
    ["none", "None"],
    ["only-x-movements", "Only X movements"],
    ["only-y-movements", "Only Y movements"],
    ["increasing-x", "Increasing X"],
    ["increasing-y", "Increasing Y"],
];

// The parameters which depend on the objective come first, right below it.
export const INSTANCE_PARAMETERS = {
    boxstacks: [
        {key: "unloading_constraint", label: "Unloading constraint", type: "select", value: "none",
            options: UNLOADING_CONSTRAINTS, structural: true},
        // The weight constraints are not checked for the items of these
        // groups.
        {key: "no_check_weight_constraints", label: "Groups without weight constraints", type: "ids",
            placeholder: "none, e.g. 1, 2"},
    ],
    rectangle: [
        {key: "leftover_mode", label: "Leftover", type: "select", value: "area",
            options: [["area", "Area"], ["x", "Along X"], ["y", "Along Y"]],
            objectives: ["bin-packing-with-leftovers"]},
        {key: "unloading_constraint", label: "Unloading constraint", type: "select", value: "none",
            options: UNLOADING_CONSTRAINTS, structural: true},
    ],
    rectangleguillotine: [
        // The costs are integers in the library.
        {key: "waste_cost", label: "Waste cost", type: "number", placeholder: "0", wholeNumber: true,
            objectives: CUTTING_COST},
        {key: "cutting_costs", label: "Cutting costs", type: "cutting-costs", value: [], objectives: CUTTING_COST},
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
    ],
    irregular: [
        // The corner or side from which the leftover is measured.
        {key: "leftover_mode", label: "Leftover", type: "select", value: "bottom-left", inParameters: true,
            options: [["bottom-left", "Bottom left"], ["bottom-right", "Bottom right"], ["top-left", "Top left"],
                ["top-right", "Top right"], ["left", "Left"], ["right", "Right"], ["bottom", "Bottom"],
                ["top", "Top"]],
            objectives: ["bin-packing-with-leftovers"]},
        // Required: the objective isn't supported without it.
        {key: "open_dimension_xy_aspect_ratio", label: "Aspect ratio (height / width)",
            type: "number", value: "1", required: true, positive: true, inParameters: true,
            objectives: ["open-dimension-xy"]},
        {key: "item_item_minimum_spacing", label: "Minimum spacing between items",
            type: "number", placeholder: "0", inParameters: true},
    ],
};

export function defaultInstanceParameters(problemType) {
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

export function parameterShown(parameter, values, objective) {
    return (parameter.objectives === undefined || parameter.objectives.includes(objective))
        && (parameter.shown === undefined || parameter.shown(values));
}

export function parameterForced(parameter, values) {
    return (parameter.forced === undefined)? undefined: parameter.forced(values);
}

// Rows of the cutting costs: the bin, then the cuts of each stage, and the
// extra cut of the non-exact cut types.
export function cuttingCostRows(values) {
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

// Add the parameters of the instance to an instance in the JSON format.
export function addInstanceParameters(instanceObject, problemType, values, objective) {
    for (const parameter of INSTANCE_PARAMETERS[problemType] || []) {
        if (!parameterShown(parameter, values, objective) || parameterForced(parameter, values) !== undefined)
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
        } else if (parameter.type === "ids") {
            const ids = String(value).split(",").map((s) => s.trim()).filter((s) => s !== "");
            for (const id of ids) {
                if (!/^[0-9]+$/.test(id))
                    throw new Error(`invalid ${name}: "${id}".`);
            }
            if (ids.length === 0)
                continue;
            converted = ids.map(Number);
        } else if (parameter.type === "cutting-costs") {
            const rows = cuttingCostRows(values);
            converted = rows.map(([label], i) => {
                const cost = values.cutting_costs[i] || {fixed: "", variable: ""};
                const result = {};
                for (const key of ["fixed", "variable"]) {
                    const number = (cost[key] === "")? 0: Number(cost[key]);
                    if (!Number.isInteger(number) || number < 0)
                        throw new Error(`invalid ${key} cutting cost of the ${label.toLowerCase()}: "${cost[key]}" `
                            + "(the costs are integers).");
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
                if (parameter.required)
                    throw new Error(`${name}: enter a value.`);
                continue;
            }
            converted = Number(value);
            if (!Number.isFinite(converted) || converted < 0
                    || (parameter.positive && converted <= 0)
                    || (parameter.integer && (!Number.isInteger(converted) || converted < 1))
                    || (parameter.wholeNumber && !Number.isInteger(converted))) {
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
export const OBJECTIVES = [
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
    ["open-dimension-z", "Open dimension Z: minimize the height", ["box"]],
    ["open-dimension-xy", "Open dimension XY: minimize the area, with a given aspect ratio", ["irregular"]],
    ["feasibility", "Feasibility"],
];

// Default objective of each problem type.
export function defaultObjective(problemType) {
    return (problemType === "onedimensional")? "bin-packing": "bin-packing-with-leftovers";
}

// The examples of the README.
export const EXAMPLES = {
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

export function defaultRow(columns) {
    const row = {};
    for (const column of columns) {
        row[column.key] = structuredClone(column.value);
        if (column.unlimited)
            row["unlimited_" + column.key] = false;
    }
    return row;
}

// Whether a column is shown for the current objective.
// Whether a column is shown for the current objective and parameters of the
// instance ('shown(values)').
export function columnShown(column, objective, instanceParameters) {
    return (column.objectives === undefined || column.objectives.includes(objective))
        && (column.shown === undefined || column.shown(instanceParameters));
}

// Whether unlimited values are allowed for a column, for the current
// objective.
export function unlimitedAllowed(column, objective) {
    return column.unlimited !== undefined && column.unlimited(objective);
}

// Whether the value of a column of a row is unlimited (then its value is
// ignored).
export function unlimitedValue(row, column, objective) {
    return unlimitedAllowed(column, objective) && Boolean(row["unlimited_" + column.key]);
}

// A resource of a bin type: its capacity, whether exceeding it is allowed
// with a penalty, and the consumptions of the item types which use it. A
// consumption refers to the row of its item type (so that it follows the row
// when other rows are removed), and gives the consumption of the successive
// copies, as a list "1, 2, 3" (a single value: the same for all the copies;
// else the last value for the next copies).
export function defaultResource() {
    return {capacity: "", penalize: false, penalty: "", consumptions: []};
}

// The consumptions of a resource whose item type still exists (or which
// don't have a valid item type yet, 'itemRow' null).
export function resourceConsumptions(resource, itemRows) {
    return resource.consumptions.filter(
        (consumption) => consumption.itemRow === null || itemRows.includes(consumption.itemRow));
}

// Objectives for which the bin types created have unlimited copies.
export const UNLIMITED_BINS_OBJECTIVES = [
    "bin-packing", "bin-packing-with-leftovers", "variable-sized-bin-packing", "bin-packing-cutting-cost"];

export function newBinRow(type, objective) {
    const row = (type === "irregular")? irregularForm.defaultBinRow(): defaultRow(binColumns(type));
    if (UNLIMITED_BINS_OBJECTIVES.includes(objective))
        row.unlimited_copies = true;
    return row;
}

export function newItemRow(type) {
    return (type === "irregular")? irregularForm.defaultItemRow(): defaultRow(itemColumns(type));
}

// The instance in the JSON format, from the form of a problem type: 'form'
// is '{objective, instanceParameters, binTypes, itemTypes}'.
export function toInstance(problemType, form) {
    const type = problemType;
    if (type === "irregular") {
        return addInstanceParameters(
            irregularForm.instance(form.objective, form.binTypes, form.itemTypes),
            problemType, form.instanceParameters, form.objective);
    }
    const convert = (kind, columns, rows) => rows.map((row, i) => {
        try {
            return convertRow(columns, row);
        } catch (error) {
            throw new Error(`${kind} type ${i}: ${error.message}`);
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
            if (!columnShown(column, form.objective, form.instanceParameters))
                continue;
            // The semi-trailer truck: an object with its fields, if checked.
            if (column.key === "semi_trailer_truck") {
                if (value)
                    result.semi_trailer_truck = result.semi_trailer_truck || {};
                continue;
            }
            if (column.truck) {
                if (!row.semi_trailer_truck)
                    continue;
                result.semi_trailer_truck = result.semi_trailer_truck || {};
                if (value === "" || value === null || value === undefined) {
                    if (column.required)
                        throw new Error(`semi-trailer truck: missing ${column.label.toLowerCase()}.`);
                    continue;
                }
                const number = Number(value);
                if (!Number.isFinite(number) || number < 0 || (column.required && number <= 0))
                    throw new Error(`semi-trailer truck: invalid ${column.label.toLowerCase()}: "${value}".`);
                result.semi_trailer_truck[column.key] = number;
                continue;
            }
            if (unlimitedValue(row, column, form.objective)) {
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
            } else if (column.type === "resources") {
                if (value.length > 0) {
                    result.resources = value.map((resource, j) => {
                        const name = `resource ${j}`;
                        const converted = {capacity: convertNumber(resource.capacity, `${name} capacity`)};
                        if (resource.penalize) {
                            converted.penalize = true;
                            if (resource.penalty !== "")
                                converted.penalty = convertNumber(resource.penalty, `${name} penalty`);
                        }
                        const consumptions = [];
                        for (const consumption of resourceConsumptions(resource, form.itemTypes)) {
                            if (consumption.itemRow === null)
                                throw new Error(`${name}: invalid item type id of a consumption.`);
                            const k = form.itemTypes.indexOf(consumption.itemRow);
                            if (consumptions.some((c) => c.item_type_id === k))
                                throw new Error(`${name}: several consumptions for item type ${k}.`);
                            const values = String(consumption.values).split(",")
                                .map((v) => v.trim()).filter((v) => v !== "")
                                .map((v) => convertNumber(v, `${name} consumption of item type ${k}`));
                            if (values.length === 0)
                                throw new Error(`${name}: missing consumption of item type ${k}.`);
                            if (values.length === 1)
                                consumptions.push({item_type_id: k, consumption: values[0]});
                            else
                                consumptions.push({item_type_id: k, consumption_schedule: values});
                        }
                        if (consumptions.length > 0)
                            converted.consumptions = consumptions;
                        return converted;
                    });
                }
            } else if (column.type === "defects") {
                if (value.length > 0) {
                    result.defects = value.map((defect, j) => {
                        const converted = {};
                        for (const key of ["x", "y", "width", "height"])
                            converted[key] = convertNumber(defect[key], `defect ${j} ${key}`);
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
            } else if (column.type === "ids") {
                const ids = String(value).split(",").map((s) => s.trim()).filter((s) => s !== "");
                for (const id of ids) {
                    if (!/^[0-9]+$/.test(id))
                        throw new Error(`invalid ${column.label.toLowerCase()}: "${id}".`);
                }
                if (ids.length > 0)
                    result[column.key] = ids.map(Number);
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
    if (form.binTypes.length === 0)
        throw new Error("add at least one bin type.");
    if (form.itemTypes.length === 0)
        throw new Error("add at least one item type.");
    const itemTypes = convert("item", itemColumns(type), form.itemTypes);
    const binTypes = convert("bin", binColumns(type), form.binTypes);
    // An item type with an eligibility id must fit a bin type.
    itemTypes.forEach((itemType, i) => {
        if (itemType.eligibility_id === undefined)
            return;
        if (!binTypes.some((binType) => (binType.eligibility_ids || []).includes(itemType.eligibility_id))) {
            throw new Error(`item type ${i}: no bin type has the eligibility id ${itemType.eligibility_id}.`);
        }
    });
    // The stack ids: all the item types or none.
    const withStack = itemTypes.filter((itemType) => itemType.stack_id !== undefined).length;
    if (withStack > 0 && withStack < itemTypes.length) {
        const i = itemTypes.findIndex((itemType) => itemType.stack_id === undefined);
        throw new Error(`item type ${i}: missing stack id (if an item type has a stack id, they all must).`);
    }
    return addInstanceParameters({
        objective: form.objective,
        bin_types: binTypes,
        item_types: itemTypes,
    }, problemType, form.instanceParameters, form.objective);
}

/////////////////////////////////////////////////////////////////////////////
// From an instance
/////////////////////////////////////////////////////////////////////////////

// The spellings accepted by the solver, and the values of the form.
const OBJECTIVE_SPELLINGS = Object.fromEntries([
    ["feasibility", "Feasibility", "F"],
    ["bin-packing", "BinPacking", "BPP"],
    ["bin-packing-with-leftovers", "BinPackingWithLeftovers", "BPPL"],
    ["open-dimension-x", "OpenDimensionX", "ODX"],
    ["open-dimension-y", "OpenDimensionY", "ODY"],
    ["open-dimension-z", "OpenDimensionZ", "ODZ"],
    ["open-dimension-xy", "OpenDimensionXY", "ODXY"],
    ["knapsack", "Knapsack", "KP"],
    ["variable-sized-bin-packing", "VariableSizedBinPacking", "VBPP"],
    ["bin-packing-cutting-cost", "BinPackingCuttingCost", "BPPCC"],
].flatMap((spellings) => spellings.map((spelling) => [spelling, spellings[0]])));

// The values of the selects of the form, for the spellings of the solver (a
// word in lower case, in "CamelCase", in "UPPER_CASE"...).
const SELECT_SPELLINGS = {
    cut_type: {roadef2018: "roadef2018", nonexact: "non-exact", exact: "exact", homogenous: "homogenous"},
    first_stage_orientation: {horizontal: "horizontal", vertical: "vertical", any: "any"},
    unloading_constraint: {none: "none", onlyxmovements: "only-x-movements", onlyymovements: "only-y-movements",
        increasingx: "increasing-x", increasingy: "increasing-y"},
    leftover_mode: {area: "area", x: "x", y: "y",
        bottomleft: "bottom-left", bl: "bottom-left", bottomright: "bottom-right", br: "bottom-right",
        topleft: "top-left", tl: "top-left", topright: "top-right", tr: "top-right",
        left: "left", l: "left", right: "right", r: "right", bottom: "bottom", b: "bottom", top: "top", t: "top"},
    trim_type: {h: "hard", hard: "hard", 0: "hard", s: "soft", soft: "soft", 1: "soft"},
};

function selectValue(kind, value, path) {
    const normalized = String(value).toLowerCase().replace(/[-_]/g, "");
    const result = SELECT_SPELLINGS[kind][normalized];
    if (result === undefined)
        throw new Error(`${path}: unknown value "${value}".`);
    return result;
}

// The parameters of the instance of a problem type, from an instance of the
// JSON format.
function parametersFromInstance(problemType, json, ignored) {
    const values = defaultInstanceParameters(problemType);
    const parameters = INSTANCE_PARAMETERS[problemType] || [];
    for (const parameter of parameters) {
        const source = parameter.inParameters? (json.parameters || {}): json;
        const path = parameter.inParameters? `parameters.${parameter.key}`: parameter.key;
        if (!(parameter.key in source))
            continue;
        const value = source[parameter.key];
        if (parameter.key === "number_of_stages") {
            if (typeof value === "string")
                values.number_of_stages = "unlimited";
            else if (value === 2 || value === 3)
                values.number_of_stages = String(value);
            else
                throw new Error(`${path}: ${value} stages aren't supported by the form (2, 3 or unlimited).`);
        } else if (parameter.type === "select") {
            values[parameter.key] = selectValue(parameter.key, value, path);
        } else if (parameter.type === "checkbox") {
            values[parameter.key] = Boolean(value);
        } else if (parameter.type === "ids") {
            values[parameter.key] = value.join(", ");
        } else if (parameter.type === "cutting-costs") {
            values.cutting_costs = value.map((cost) => ({fixed: String(cost.fixed), variable: String(cost.variable)}));
            values.cutting_costs_stages = Math.max(1, value.length - 1);
        } else if (parameter.unlimited) {
            // -1: unlimited.
            const unlimited = (value === -1 || value === null);
            values["unlimited_" + parameter.key] = unlimited;
            values[parameter.key] = unlimited? "": String(value);
        } else {
            values[parameter.key] = String(value);
        }
    }
    // The fields which the form doesn't read.
    const known = parameters.filter((p) => !p.inParameters).map((p) => p.key);
    for (const key of Object.keys(json)) {
        if (!["objective", "bin_types", "item_types", "parameters"].includes(key) && !known.includes(key))
            ignored.add(key);
    }
    const inParameters = parameters.filter((p) => p.inParameters).map((p) => p.key);
    for (const key of Object.keys(json.parameters || {})) {
        if (!inParameters.includes(key))
            ignored.add(`parameters.${key}`);
    }
    if (inParameters.length === 0 && json.parameters !== undefined)
        ignored.add("parameters");
    return values;
}

// The rows of the bin or item types of a problem type ('columns'), from the
// types of an instance of the JSON format. 'itemRows' are the item rows (for
// the resources of the bins).
function rowsFromTypes(types, columns, kind, itemRows, ignored) {
    return (types || []).map((json, i) => {
        const path = `${kind}_types[${i}]`;
        const row = defaultRow(columns);
        const known = [];
        for (const column of columns) {
            const key = column.key;
            if (column.type === "trims") {
                for (const side of ["left", "right", "bottom", "top"]) {
                    known.push(side + "_trim");
                    if (json[side + "_trim"] !== undefined)
                        row.trims[side] = json[side + "_trim"];
                }
                continue;
            }
            if (column.truck) {
                if (json.semi_trailer_truck !== undefined && json.semi_trailer_truck[key] !== undefined
                        && json.semi_trailer_truck[key] !== null)
                    row[key] = json.semi_trailer_truck[key];
                continue;
            }
            if (key === "semi_trailer_truck") {
                known.push(key);
                row[key] = (json[key] !== undefined);
                continue;
            }
            known.push(key);
            if (json[key] === undefined || json[key] === null)
                continue;
            const value = json[key];
            if (column.unlimited && value === -1) {
                row["unlimited_" + key] = true;
            } else if (column.type === "checkbox") {
                row[key] = Boolean(value);
            } else if (column.type === "select") {
                row[key] = selectValue("trim_type", value, `${path}.${key}`);
            } else if (column.type === "ids") {
                row[key] = value.join(", ");
            } else if (column.type === "rotations") {
                row[key] = value.slice();
            } else if (column.type === "defects") {
                row[key] = value.map((defect) => ({x: defect.x, y: defect.y, width: defect.width, height: defect.height}));
            } else if (column.type === "resources") {
                row[key] = value.map((resource, j) => ({
                    capacity: resource.capacity,
                    penalize: Boolean(resource.penalize),
                    penalty: (resource.penalty === undefined)? "": resource.penalty,
                    consumptions: (resource.consumptions || []).map((consumption) => {
                        const itemRow = itemRows[consumption.item_type_id];
                        if (itemRow === undefined) {
                            throw new Error(`${path}.resources[${j}]: invalid item type id `
                                + `${consumption.item_type_id}.`);
                        }
                        const values = (consumption.consumption_schedule !== undefined)?
                            consumption.consumption_schedule: [consumption.consumption];
                        return {itemRow, values: values.join(", ")};
                    }),
                }));
            } else if (["cost", "copies_min", "eligibility_id", "stack_id"].includes(key) && value === -1) {
                // The default (any bin for 'eligibility_id', none for
                // 'stack_id').
            } else {
                row[key] = value;
            }
        }
        for (const key of Object.keys(json)) {
            if (!known.includes(key))
                ignored.add(`${path}.${key}`);
        }
        return row;
    });
}

// The form of a problem type ('{objective, instanceParameters, binTypes,
// itemTypes}', the arguments of 'toInstance'), from an instance of the JSON
// format, and 'ignored': the fields which the form doesn't read (nor the
// solver). Throws if the instance can't be represented in the form.
export function fromInstance(problemType, json) {
    if (typeof json !== "object" || json === null || Array.isArray(json))
        throw new Error("the instance must be a JSON object.");
    const objective = OBJECTIVE_SPELLINGS[json.objective];
    if (objective === undefined)
        throw new Error(`unknown objective "${json.objective}".`);
    if (!OBJECTIVES.some(([value, , types]) => value === objective && (types === undefined || types.includes(problemType))))
        throw new Error(`the objective "${json.objective}" isn't available for this problem type.`);
    const ignored = new Set();
    const instanceParameters = parametersFromInstance(problemType, json, ignored);
    let binTypes;
    let itemTypes;
    if (problemType === "irregular") {
        const rows = irregularForm.rowsFromInstance(json, ignored);
        binTypes = rows.binRows;
        itemTypes = rows.itemRows;
    } else {
        itemTypes = rowsFromTypes(json.item_types, itemColumns(problemType), "item", [], ignored);
        binTypes = rowsFromTypes(json.bin_types, binColumns(problemType), "bin", itemTypes, ignored);
    }
    return {objective, instanceParameters, binTypes, itemTypes, ignored: [...ignored]};
}
