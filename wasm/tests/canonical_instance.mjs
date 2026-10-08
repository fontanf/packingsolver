// The canonical form of an instance of the JSON format: what the solver
// reads from it, to compare two instances (see 'open_instance.test.mjs').
// Shapes are lists of elements, anticlockwise, starting from their smallest
// point; default values and the fields which the objective doesn't use are
// removed; numbers are rounded.

const round = (v) => Number(Number(v).toPrecision(10));

const OBJECTIVES = Object.fromEntries([
    ["feasibility", "Feasibility", "F"], ["bin-packing", "BinPacking", "BPP"],
    ["bin-packing-with-leftovers", "BinPackingWithLeftovers", "BPPL"],
    ["open-dimension-x", "OpenDimensionX", "ODX"], ["open-dimension-y", "OpenDimensionY", "ODY"],
    ["open-dimension-z", "OpenDimensionZ", "ODZ"], ["open-dimension-xy", "OpenDimensionXY", "ODXY"],
    ["knapsack", "Knapsack", "KP"], ["variable-sized-bin-packing", "VariableSizedBinPacking", "VBPP"],
    ["bin-packing-cutting-cost", "BinPackingCuttingCost", "BPPCC"],
].flatMap((s) => s.map((x) => [x, s[0]])));

// A spelling of an enum of the solver, normalized ("BottomLeft",
// "bottom-left" -> "bottomleft"), and the abbreviations of the leftover
// modes.
const LEFTOVER = {bl: "bottomleft", br: "bottomright", tl: "topleft", tr: "topright",
    l: "left", r: "right", b: "bottom", t: "top"};
function normalize(value) {
    const normalized = String(value).toLowerCase().replace(/[-_]/g, "");
    return LEFTOVER[normalized] || normalized;
}

const LINE = ["LineSegment", "line_segment", "L", "l"];
const ORIENTATION = {Anticlockwise: "a", anticlockwise: "a", A: "a", a: "a",
    Clockwise: "c", clockwise: "c", C: "c", c: "c", Full: "f", full: "f", F: "f", f: "f"};

function shapeElements(shape) {
    const x = shape.x || 0;
    const y = shape.y || 0;
    if (shape.type === "rectangle") {
        const c = [[x, y], [x + shape.width, y], [x + shape.width, y + shape.height], [x, y + shape.height]];
        return c.map((p, i) => ({t: "L", s: p, e: c[(i + 1) % 4]}));
    }
    if (shape.type === "circle")
        return [{t: "A", s: [x + shape.radius, y], e: [x + shape.radius, y], c: [x, y], o: "f"}];
    if (shape.type === "polygon") {
        const v = shape.vertices.map((p) => [p.x, p.y]);
        return v.map((p, i) => ({t: "L", s: p, e: v[(i + 1) % v.length]}));
    }
    return shape.elements.map((e) => LINE.includes(e.type)?
        {t: "L", s: [e.start.x, e.start.y], e: [e.end.x, e.end.y]}:
        {t: "A", s: [e.start.x, e.start.y], e: [e.end.x, e.end.y], c: [e.center.x, e.center.y],
            o: ORIENTATION[e.orientation]});
}

// Twice the signed area of the polygon of the starts of the elements and of
// the middles of the arcs.
function signedArea(elements) {
    const points = [];
    for (const e of elements) {
        points.push(e.s);
        if (e.t === "A") {
            const r = Math.hypot(e.s[0] - e.c[0], e.s[1] - e.c[1]);
            let a0 = Math.atan2(e.s[1] - e.c[1], e.s[0] - e.c[0]);
            let a1 = Math.atan2(e.e[1] - e.c[1], e.e[0] - e.c[0]);
            if (e.o === "f") a1 = a0 + 2 * Math.PI;
            else if (e.o === "a" && a1 <= a0) a1 += 2 * Math.PI;
            else if (e.o === "c" && a1 >= a0) a1 -= 2 * Math.PI;
            for (const k of [1, 2, 3])
                points.push([e.c[0] + r * Math.cos(a0 + (a1 - a0) * k / 4), e.c[1] + r * Math.sin(a0 + (a1 - a0) * k / 4)]);
        }
    }
    let area = 0;
    points.forEach((p, i) => {
        const q = points[(i + 1) % points.length];
        area += p[0] * q[1] - q[0] * p[1];
    });
    return area;
}

function canonicalElements(elements) {
    let result = elements.map((e) => ({...e, s: e.s.map(round), e: e.e.map(round), ...(e.c? {c: e.c.map(round)}: {})}));
    if (signedArea(result) < 0) {
        result = result.slice().reverse().map((e) => ({...e, s: e.e, e: e.s,
            ...(e.t === "A"? {o: {a: "c", c: "a", f: "f"}[e.o]}: {})}));
    }
    // Start from the smallest start.
    let k = 0;
    result.forEach((e, i) => {
        const b = result[k].s;
        if (e.s[0] < b[0] || (e.s[0] === b[0] && e.s[1] < b[1]))
            k = i;
    });
    return [...result.slice(k), ...result.slice(0, k)];
}

function canonicalShape(shape, withHoles = true) {
    const result = {elements: canonicalElements(shapeElements(shape))};
    if (withHoles && (shape.holes || []).length > 0)
        result.holes = shape.holes.map((h) => canonicalShape(h, false));
    return result;
}

// A number, or its default if it isn't given (-1 and null are defaults).
function value(v, defaultValue) {
    return (v === undefined || v === null || v === -1)? defaultValue: round(v);
}

// The trim types: "hard" or "soft".
const TRIM_TYPES = {h: "hard", hard: "hard", 0: "hard", s: "soft", soft: "soft", 1: "soft"};

function canonicalResources(resources) {
    return (resources || []).map((r) => ({
        capacity: round(r.capacity),
        penalty: r.penalize? round(r.penalty || 0): null,
        consumptions: (r.consumptions || []).map((c) => ({
            id: c.item_type_id,
            schedule: (c.consumption_schedule || [c.consumption]).map(round),
        })).sort((a, b) => a.id - b.id),
    }));
}

// The other problem types: rectangleguillotine, rectangle, box, boxstacks,
// onedimensional.
function canonicalOther(problemType, json, objective) {
    const knapsack = (objective === "knapsack");
    const variableSized = (objective === "variable-sized-bin-packing");
    const result = {objective, parameters: {}};
    const p = result.parameters;
    if (problemType === "rectangleguillotine") {
        const unlimited = (typeof json.number_of_stages === "string");
        p.number_of_stages = unlimited? "unlimited": value(json.number_of_stages, 3);
        p.cut_type = unlimited? "exact": normalize(json.cut_type || "non-exact");
        p.first_stage_orientation = unlimited? "any": normalize(json.first_stage_orientation || "vertical");
        for (const [key, d] of [["cut_thickness", 0], ["minimum_waste_length", 0], ["minimum_distance_1_cuts", 0],
                ["maximum_distance_1_cuts", null], ["maximum_number_1_cuts", null]])
            p[key] = value(json[key], d);
        if (p.number_of_stages !== 2) {
            for (const [key, d] of [["minimum_distance_2_cuts", 0], ["maximum_distance_2_cuts", null],
                    ["maximum_number_2_cuts", null]])
                p[key] = value(json[key], d);
        }
        p.cut_through_defects = Boolean(json.cut_through_defects);
        if (objective === "bin-packing-cutting-cost") {
            p.waste_cost = value(json.waste_cost, 0);
            const costs = (json.cutting_costs || []).map((c) => [round(c.fixed), round(c.variable)]);
            while (costs.length > 0 && costs[costs.length - 1][0] === 0 && costs[costs.length - 1][1] === 0)
                costs.pop();
            p.cutting_costs = costs;
        }
    }
    if (problemType === "rectangle" || problemType === "boxstacks")
        p.unloading_constraint = normalize(json.unloading_constraint || "none");
    if (objective === "bin-packing-with-leftovers") {
        const defaultLeftoverMode = {rectangle: "area", box: "xyz", boxstacks: "xy"}[problemType];
        if (defaultLeftoverMode !== undefined)
            p.leftover_mode = normalize(json.leftover_mode || defaultLeftoverMode);
    }
    if (problemType === "boxstacks")
        p.no_check_weight_constraints = (json.no_check_weight_constraints || []).slice().sort((a, b) => a - b);
    const unloading = (p.unloading_constraint !== undefined && p.unloading_constraint !== "none");
    const dimensions = {
        rectangleguillotine: ["width", "height"], rectangle: ["x", "y"], box: ["x", "y", "z"],
        boxstacks: ["x", "y", "z"], onedimensional: ["length"],
    }[problemType];
    const weights = ["rectangle", "box", "boxstacks", "onedimensional"].includes(problemType);
    result.bin_types = json.bin_types.map((b) => {
        const r = {};
        for (const d of dimensions)
            r[d] = round(b[d]);
        r.copies = (b.copies === undefined)? 1: b.copies;
        if (variableSized) {
            r.copies_min = value(b.copies_min, 0);
            r.cost = value(b.cost, null);
        }
        if (weights)
            r.maximum_weight = value(b.maximum_weight, null);
        if (problemType === "rectangleguillotine") {
            for (const side of ["left", "right", "bottom", "top"]) {
                r[side + "_trim"] = value(b[side + "_trim"], 0);
                const d = (side === "left" || side === "bottom")? "hard": "soft";
                r[side + "_trim_type"] = TRIM_TYPES[String(b[side + "_trim_type"] || d).toLowerCase()];
            }
        }
        if (["rectangleguillotine", "rectangle", "boxstacks"].includes(problemType))
            r.defects = (b.defects || []).map((d) => [d.x, d.y, d.width, d.height].map(round));
        if (["rectangle", "onedimensional"].includes(problemType))
            r.eligibility_ids = b.eligibility_ids || [];
        if (["rectangleguillotine", "rectangle", "box", "onedimensional"].includes(problemType))
            r.resources = canonicalResources(b.resources);
        if (problemType === "boxstacks") {
            r.maximum_stack_density = value(b.maximum_stack_density, null);
            if (b.semi_trailer_truck !== undefined) {
                const t = b.semi_trailer_truck;
                r.truck = Object.fromEntries(["tractor_weight", "front_axle_middle_axle_distance",
                    "front_axle_tractor_gravity_center_distance", "front_axle_harness_distance", "empty_trailer_weight",
                    "harness_rear_axle_distance", "trailer_gravity_center_rear_axle_distance",
                    "trailer_start_harness_distance"].map((k) => [k, value(t[k], 0)])
                    .concat(["rear_axle_maximum_weight", "middle_axle_maximum_weight"].map((k) => [k, value(t[k], null)])));
            }
        }
        return r;
    });
    result.item_types = json.item_types.map((t) => {
        const r = {};
        for (const d of dimensions)
            r[d] = round(t[d]);
        r.copies = (t.copies === undefined)? 1: t.copies;
        if (knapsack) {
            r.copies_min = value(t.copies_min, null);
            r.profit = value(t.profit, null);
        }
        if (weights)
            r.weight = value(t.weight, 0);
        if (problemType === "rectangleguillotine") {
            r.oriented = Boolean(t.oriented);
            r.stack_id = value(t.stack_id, null);
        }
        if (problemType === "rectangle") {
            r.oriented = Boolean(t.oriented);
            if (unloading)
                r.group_id = value(t.group_id, 0);
        }
        if (["rectangle", "onedimensional"].includes(problemType))
            r.eligibility_id = value(t.eligibility_id, null);
        if (problemType === "box" || problemType === "boxstacks")
            r.rotations = ((t.rotations || []).length > 0)? t.rotations.slice().sort(): ["XYZ"];
        if (problemType === "boxstacks") {
            if (unloading)
                r.group_id = value(t.group_id, 0);
            r.stackability_id = value(t.stackability_id, 0);
            r.nesting_height = value(t.nesting_height, 0);
            r.maximum_stackability = value(t.maximum_stackability, null);
            r.maximum_weight_above = value(t.maximum_weight_above, null);
        }
        if (problemType === "onedimensional") {
            r.nesting_length = value(t.nesting_length, 0);
            r.maximum_stackability = value(t.maximum_stackability, null);
            r.maximum_weight_after = value(t.maximum_weight_after, null);
        }
        return r;
    });
    return result;
}

export function canonicalInstance(problemType, json) {
    const objective = OBJECTIVES[json.objective];
    if (problemType !== "irregular")
        return canonicalOther(problemType, json, objective);
    const parameters = json.parameters || {};
    const result = {objective, parameters: {
        spacing: round(parameters.item_item_minimum_spacing || 0),
        ...(objective === "bin-packing-with-leftovers"? {leftover: normalize(parameters.leftover_mode || "bottom-left")}: {}),
        ...(objective === "open-dimension-xy"? {ratio: round(parameters.open_dimension_xy_aspect_ratio)}: {}),
    }};
    result.bin_types = json.bin_types.map((b) => ({
        shape: canonicalShape(b, false),
        copies: (b.copies === undefined)? 1: b.copies,
        ...(objective === "variable-sized-bin-packing"? {
            copies_min: b.copies_min || 0,
            cost: (b.cost === undefined || b.cost === -1)? null: round(b.cost),
        }: {}),
        spacing: round(b.item_bin_minimum_spacing || 0),
        defects: (b.defects || []).map((d) => ({
            shape: canonicalShape(d),
            type: (d.defect_type === undefined)? -1: d.defect_type,
            spacing: round(d.item_defect_minimum_spacing || 0),
        })),
        fixed_items: (b.fixed_items || []).map((f) => ({
            id: f.item_type_id, x: round(f.bl_corner.x), y: round(f.bl_corner.y),
            angle: round(f.angle || 0), mirror: Boolean(f.mirror),
        })),
    }));
    result.item_types = json.item_types.map((t) => {
        let rotations = (t.allowed_rotations || []).map((r) => ({start: round(r.start), end: round(r.end), mirror: Boolean(r.mirror)}));
        if (rotations.length === 0)
            rotations = [{start: 0, end: 0, mirror: false}];
        if (t.allow_mirroring)
            rotations = [...rotations, ...rotations.filter((r) => !r.mirror).map((r) => ({...r, mirror: true}))];
        rotations = rotations.map((r) => JSON.stringify(r)).sort();
        return {
            shapes: (t.shapes || [t]).map((s) => canonicalShape(s)),
            copies: (t.copies === undefined)? 1: t.copies,
            ...(objective === "knapsack"? {
                copies_min: (t.copies_min === undefined || t.copies_min === -1)? null: t.copies_min,
                profit: (t.profit === undefined || t.profit === -1)? null: round(t.profit),
            }: {}),
            rotations: [...new Set(rotations)],
        };
    });
    return result;
}
