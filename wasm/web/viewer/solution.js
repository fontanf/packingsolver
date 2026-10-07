// Solutions of the visualizer: the certificates of the solvers, read in a
// model shared by the problem types (without the page, to be tested).
//
// A solution is '{problemType, bins}'. Each bin of 'bins' is a bin of the
// certificate, used 'copies' times:
//   {
//       index,           // its position in the certificate
//       binTypeId,
//       copies,
//       box: {x0, y0, x1, y1},   // its bounding box
//       outline,         // a path (see below)
//       items: [{itemTypeId, paths, box, labelBox, label}],
//       defects: [path],
//       // rectangleguillotine only:
//       trims: [box], wastes: [box], residuals: [box], cuts: [{depth, box}],
//   }
// A path is a list of closed contours, each a list of points [x, y]: a shape,
// then its holes (drawn with the even-odd rule). An item has a path for each
// of its shapes (several for some irregular items), so that its shapes which
// overlap aren't drawn as holes.
// 'labelBox': the bounding box of the largest shape of the item, where the id
// of its item type is written.
// For onedimensional, the boxes have a height of 1.

import {parseCsv} from "../visualize/common.js";

// The problem types which have a certificate in the CSV format, recognized
// by the columns of their header.
const CSV_COLUMNS = [
    ["rectangleguillotine", ["PLATE_ID", "NODE_ID", "CUT", "PARENT"]],
    ["boxstacks", ["TYPE", "BIN", "STACK", "Z"]],
    ["box", ["TYPE", "BIN", "Z", "LZ"]],
    ["rectangle", ["TYPE", "BIN", "Y", "LY"]],
    ["onedimensional", ["TYPE", "BIN", "X", "LX"]],
];

// The problem type of a certificate, from its content: a JSON certificate is
// irregular; a CSV certificate is recognized by the columns of its header.
// 'null' if it isn't a certificate.
export function detectProblemType(text) {
    const start = text.trimStart();
    if (start.startsWith("{")) {
        try {
            const json = JSON.parse(start);
            return (Array.isArray(json.bins))? "irregular": null;
        } catch (error) {
            return null;
        }
    }
    const header = start.split(/\r?\n/)[0].split(",").map((name) => name.trim());
    const found = CSV_COLUMNS.find(([, columns]) => columns.every((column) => header.includes(column)));
    return (found === undefined)? null: found[0];
}

// The problem types drawn by the visualizer (the other ones keep their
// plotly figures).
export const PROBLEM_TYPES = ["rectangleguillotine", "rectangle", "onedimensional", "irregular"];

function rectangle(x0, y0, x1, y1) {
    return [[[x0, y0], [x1, y0], [x1, y1], [x0, y1]]];
}

function boxOfPath(path) {
    const box = {x0: Infinity, y0: Infinity, x1: -Infinity, y1: -Infinity};
    for (const contour of path) {
        for (const [x, y] of contour) {
            box.x0 = Math.min(box.x0, x);
            box.y0 = Math.min(box.y0, y);
            box.x1 = Math.max(box.x1, x);
            box.y1 = Math.max(box.y1, y);
        }
    }
    return box;
}

function newBin(index, binTypeId, copies) {
    return {index, binTypeId, copies, box: null, outline: [], items: [], defects: [],
        trims: [], wastes: [], residuals: [], cuts: []};
}

const integer = (value) => parseInt(value, 10);
const number = (value) => Number(value);

// The bins of a certificate of the rectangle problem type.
function readRectangle(text) {
    const bins = [];
    for (const row of parseCsv(text)) {
        const i = integer(row.BIN);
        const x0 = number(row.X);
        const y0 = number(row.Y);
        const x1 = x0 + number(row.LX);
        const y1 = y0 + number(row.LY);
        if (row.TYPE === "BIN") {
            const bin = newBin(i, integer(row.ID), integer(row.COPIES));
            bin.box = {x0, y0, x1, y1};
            bin.outline = rectangle(x0, y0, x1, y1);
            bins[i] = bin;
        } else if (row.TYPE === "DEFECT") {
            bins[i].defects.push(rectangle(x0, y0, x1, y1));
        } else if (row.TYPE === "ITEM") {
            const item = {itemTypeId: integer(row.ID), paths: [rectangle(x0, y0, x1, y1)], box: {x0, y0, x1, y1},
                label: `Item type ${row.ID}: ${number(row.LX)} × ${number(row.LY)} at (${x0}, ${y0})`};
            if (row.GROUP_ID !== undefined && row.GROUP_ID !== null && row.GROUP_ID !== "")
                item.label += `, group ${row.GROUP_ID}`;
            bins[i].items.push(item);
        }
    }
    return bins;
}

// The bins of a certificate of the rectangleguillotine problem type: the
// nodes of the cutting tree of each plate.
function readRectangleGuillotine(text) {
    const bins = [];
    for (const row of parseCsv(text)) {
        const i = integer(row.PLATE_ID);
        const type = integer(row.TYPE);
        const depth = integer(row.CUT);
        const x0 = number(row.X);
        const y0 = number(row.Y);
        const x1 = x0 + number(row.WIDTH);
        const y1 = y0 + number(row.HEIGHT);
        const box = {x0, y0, x1, y1};
        if (type === -4) {  // Defect.
            bins[i].defects.push(rectangle(x0, y0, x1, y1));
        } else if (!row.PARENT) {  // The plate.
            // The plates have no bin type id in the certificate.
            const bin = newBin(i, null, integer(row.COPIES || "1"));
            bin.box = box;
            bin.outline = rectangle(x0, y0, x1, y1);
            bins[i] = bin;
        } else if (depth === -1) {  // Trims.
            bins[i].trims.push(box);
        } else if (type >= 0) {  // Item.
            bins[i].items.push({itemTypeId: type, paths: [rectangle(x0, y0, x1, y1)], box,
                label: `Item type ${type}: ${number(row.WIDTH)} × ${number(row.HEIGHT)} at (${x0}, ${y0})`});
        } else {
            bins[i].cuts.push({depth, box});
            if (type === -1)
                bins[i].wastes.push(box);
            else if (type === -3)
                bins[i].residuals.push(box);
        }
    }
    return bins;
}

// The bins of a certificate of the onedimensional problem type.
function readOneDimensional(text) {
    const bins = [];
    for (const row of parseCsv(text)) {
        const i = integer(row.BIN);
        const x0 = number(row.X);
        const x1 = x0 + number(row.LX);
        if (row.TYPE === "BIN") {
            const bin = newBin(i, integer(row.ID), integer(row.COPIES));
            bin.box = {x0, y0: 0, x1, y1: 1};
            bin.outline = rectangle(x0, 0, x1, 1);
            bins[i] = bin;
        } else if (row.TYPE === "ITEM") {
            bins[i].items.push({itemTypeId: integer(row.ID), paths: [rectangle(x0, 0, x1, 1)],
                box: {x0, y0: 0, x1, y1: 1}, label: `Item type ${row.ID}: ${number(row.LX)} at ${x0}`});
        }
    }
    return bins;
}

const ANTICLOCKWISE = ["Anticlockwise", "anticlockwise", "A", "a"];
const CLOCKWISE = ["Clockwise", "clockwise", "C", "c"];
const FULL = ["Full", "full", "F", "f"];

// The points of a shape of an irregular certificate (a list of line segments
// and circular arcs), as a closed contour. The arcs are approximated by
// segments of at most PI / 32.
export function shapeContour(shape) {
    const contour = [];
    for (const element of shape) {
        if (contour.length === 0)
            contour.push([element.xs, element.ys]);
        if (element.type !== "CircularArc") {
            contour.push([element.xe, element.ye]);
            continue;
        }
        const {xs, ys, xe, ye, xc, yc, orientation} = element;
        const radius = Math.hypot(xs - xc, ys - yc);
        const startAngle = Math.atan2(ys - yc, xs - xc);
        let endAngle = Math.atan2(ye - yc, xe - xc);
        if (FULL.includes(orientation))
            endAngle = startAngle + 2 * Math.PI;
        else if (ANTICLOCKWISE.includes(orientation) && endAngle <= startAngle)
            endAngle += 2 * Math.PI;
        else if (CLOCKWISE.includes(orientation) && endAngle >= startAngle)
            endAngle -= 2 * Math.PI;
        const steps = Math.max(2, Math.ceil(Math.abs(endAngle - startAngle) / (Math.PI / 32)));
        for (let k = 1; k <= steps; ++k) {
            const angle = startAngle + (endAngle - startAngle) * k / steps;
            contour.push([xc + radius * Math.cos(angle), yc + radius * Math.sin(angle)]);
        }
    }
    // Closed: without the last point if it is the first one.
    const [first, last] = [contour[0], contour[contour.length - 1]];
    if (contour.length > 1 && Math.abs(first[0] - last[0]) < 1e-9 && Math.abs(first[1] - last[1]) < 1e-9)
        contour.pop();
    return contour;
}

// A shape with its holes, as a path.
function shapePath(shape, holes = []) {
    return [shapeContour(shape), ...holes.map(shapeContour)];
}

// The bins of a certificate of the irregular problem type (JSON).
function readIrregular(text) {
    const json = JSON.parse(text);
    return json.bins.map((solutionBin, index) => {
        const bin = newBin(index, solutionBin.id, solutionBin.copies);
        bin.outline = shapePath(solutionBin.shape);
        bin.box = boxOfPath(bin.outline);
        for (const defect of solutionBin.defects || [])
            bin.defects.push(shapePath(defect.shape, defect.holes || []));
        for (const solutionItem of solutionBin.items) {
            const paths = solutionItem.item_shapes.map(
                (itemShape) => shapePath(itemShape.shape, itemShape.holes || []));
            let label = `Item type ${solutionItem.id} at (${solutionItem.x}, ${solutionItem.y})`;
            if (solutionItem.angle)
                label += `, rotated ${solutionItem.angle}°`;
            if (solutionItem.mirror)
                label += ", mirrored";
            const boxes = paths.map((p) => boxOfPath([p[0]]));
            const area = (b) => (b.x1 - b.x0) * (b.y1 - b.y0);
            const labelBox = boxes.reduce((largest, b) => (area(b) > area(largest))? b: largest);
            bin.items.push({itemTypeId: solutionItem.id, paths, box: boxOfPath(paths.flat()), labelBox, label});
        }
        return bin;
    });
}

// The solution of a certificate. 'problemType' is detected if not given.
// Throws an error if the certificate can't be read.
export function readSolution(text, problemType = null) {
    if (problemType === null)
        problemType = detectProblemType(text);
    if (problemType === null)
        throw new Error("not a certificate of PackingSolver.");
    const readers = {
        rectangleguillotine: readRectangleGuillotine,
        rectangle: readRectangle,
        onedimensional: readOneDimensional,
        irregular: readIrregular,
    };
    if (readers[problemType] === undefined)
        throw new Error(`the solutions of the problem type '${problemType}' aren't drawn by this viewer.`);
    const bins = readers[problemType](text).filter((bin) => bin !== undefined);
    return {problemType, bins};
}

// The number of bins of a solution, with their copies.
export function numberOfBins(solution) {
    return solution.bins.reduce((total, bin) => total + bin.copies, 0);
}
