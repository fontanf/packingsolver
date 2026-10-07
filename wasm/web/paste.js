// Rows pasted in the bin and item type tables of the web page: cells copied
// from a spreadsheet (separated by tabs) or lines of a CSV file (separated by
// commas or semicolons). Without the page, to be tested.
//
// If the first line is a header (names of columns, as in the CSV files of the
// solvers or as in the tables), the columns are found by their names and the
// rows are added to the table. Otherwise, the cells fill the table from the
// cell where they are pasted, to the right and down, adding rows if needed.

import * as form from "./form.js";

// The cells of a pasted text ('{cells, delimiter}', 'cells' a list of rows,
// each a list of strings): separated by tabs if there are some (cells copied
// from a spreadsheet), else by semicolons or commas (the most frequent in the
// first line). Quoted cells ('"a, b"', '""' for a quote) are read as in CSV
// files.
export function parsePastedText(text) {
    text = text.replace(/\r\n?/g, "\n");
    const firstLine = text.split("\n")[0];
    const count = (character) => firstLine.split(character).length - 1;
    let delimiter = null;
    if (text.includes("\t"))
        delimiter = "\t";
    else if (count(";") > 0 && count(";") >= count(","))
        delimiter = ";";
    else if (count(",") > 0)
        delimiter = ",";
    const cells = [];
    let row = [];
    let cell = "";
    let quoted = false;
    for (let i = 0; i < text.length; ++i) {
        const character = text[i];
        if (quoted) {
            if (character === "\"" && text[i + 1] === "\"") {
                cell += "\"";
                ++i;
            } else if (character === "\"") {
                quoted = false;
            } else {
                cell += character;
            }
        } else if (character === "\"" && cell.trim() === "") {
            quoted = true;
            cell = "";
        } else if (character === delimiter) {
            row.push(cell.trim());
            cell = "";
        } else if (character === "\n") {
            row.push(cell.trim());
            cells.push(row);
            row = [];
            cell = "";
        } else {
            cell += character;
        }
    }
    row.push(cell.trim());
    cells.push(row);
    // Without the empty lines at the end.
    while (cells.length > 0 && cells[cells.length - 1].every((c) => c === ""))
        cells.pop();
    return {cells, delimiter};
}

// Whether a pasted text has several cells (otherwise, it is pasted in the
// field as usual).
export function severalCells(parsed) {
    return parsed.cells.length > 1 || (parsed.cells.length === 1 && parsed.cells[0].length > 1);
}

// A name of a column, to compare it: in lower case, without spaces,
// underscores and hyphens ("COPIES_MIN", "Copies min" -> "copiesmin").
export function normalizeName(name) {
    return name.toLowerCase().replace(/[\s_-]/g, "");
}

// The names of the columns of the CSV files of the solvers which are not the
// keys of the columns of the tables.
const CSV_NAMES = {
    onedimensional: {x: "length"},
};

// The columns of a table which can be pasted, by name: the name of each
// column (its key and its label) -> '{column}', '{column, side}' for the
// trims of rectangleguillotine ('BOTTOM_TRIM'...), '{column, rotation}' for
// the rotations of box and boxstacks ('ROTATION_XYZ'...).
function namedColumns(problemType, kind) {
    const columns = (kind === "bin")? form.binColumns(problemType): form.itemColumns(problemType);
    const names = new Map();
    for (const column of columns) {
        if (column.type === "defects" || column.type === "resources")
            continue;
        if (column.type === "trims") {
            for (const side of ["left", "right", "bottom", "top"])
                names.set(normalizeName(side + "_trim"), {column, side});
        } else if (column.type === "rotations") {
            for (const rotation of column.rotations || form.BOX_ROTATIONS)
                names.set(normalizeName("rotation_" + rotation), {column, rotation});
        } else {
            names.set(normalizeName(column.key), {column});
            names.set(normalizeName(column.label), {column});
        }
    }
    for (const [name, key] of Object.entries(CSV_NAMES[problemType] || {})) {
        const column = columns.find((c) => c.key === key);
        if (column !== undefined)
            names.set(normalizeName(name), {column});
    }
    return names;
}

// The columns of a table, in the order in which they are shown on its rows
// (as 'renderTable' in 'app.js': without the details, the defects and the
// resources).
export function tableColumns(problemType, kind, objective, instanceParameters) {
    const columns = (kind === "bin")? form.binColumns(problemType): form.itemColumns(problemType);
    return columns.filter((column) => column.type !== "defects" && column.type !== "resources"
        && !column.details && form.columnShown(column, objective, instanceParameters));
}

const TRUE_VALUES = ["1", "true", "yes", "x"];
const FALSE_VALUES = ["0", "false", "no", ""];

// Write a pasted cell in a row ('target' from 'namedColumns'). Returns an
// error message if the value isn't valid for the column (the value of the
// row is then unchanged), 'null' otherwise. The numbers are kept as typed (a
// number which isn't valid is highlighted by the checks of the form), except
// the decimal commas ("12,5"), if the cells aren't separated by commas.
function writeCell(row, target, value, objective, delimiter) {
    const {column} = target;
    const lower = value.toLowerCase();
    if (target.side !== undefined) {
        row[column.key][target.side] = number(value, delimiter);
        return null;
    }
    if (target.rotation !== undefined) {
        const rotations = column.rotations || form.BOX_ROTATIONS;
        let checked;
        if (TRUE_VALUES.includes(lower))
            checked = true;
        else if (FALSE_VALUES.includes(lower))
            checked = false;
        else
            return `invalid ${target.rotation} rotation: "${value}".`;
        const current = row[column.key];
        row[column.key] = rotations.filter((r) => (r === target.rotation)? checked: current.includes(r));
        return null;
    }
    if (column.type === "checkbox") {
        if (TRUE_VALUES.includes(lower))
            row[column.key] = true;
        else if (FALSE_VALUES.includes(lower))
            row[column.key] = false;
        else
            return `invalid ${column.label.toLowerCase()}: "${value}".`;
        return null;
    }
    if (column.type === "select") {
        const option = column.options.find(([optionValue, label]) =>
            normalizeName(optionValue) === normalizeName(value) || normalizeName(label) === normalizeName(value));
        if (option === undefined)
            return `invalid ${column.label.toLowerCase()}: "${value}".`;
        row[column.key] = option[0];
        return null;
    }
    if (column.type === "ids") {
        row[column.key] = value;
        return null;
    }
    // A number. -1 copies: unlimited, if allowed (otherwise, the value is
    // kept, and highlighted as invalid).
    if (column.unlimited !== undefined) {
        const unlimited = (value.trim() === "-1" && form.unlimitedAllowed(column, objective));
        row["unlimited_" + column.key] = unlimited;
        if (unlimited) {
            row[column.key] = "";
            return null;
        }
    }
    row[column.key] = number(value, delimiter);
    return null;
}

function number(value, delimiter) {
    return (delimiter !== "," && /^-?\d+,\d+$/.test(value))? value.replace(",", "."): value;
}

// Paste cells ('parsePastedText') in the rows of a table ('kind' "bin" or
// "item"; 'rows' modified in place). 'options':
// - 'objective', 'instanceParameters': of the form;
// - 'newRow()': a new row of the table;
// - 'startRow', 'startColumn': the index of the row and the key of the
//   column of the cell where the cells are pasted (for cells without
//   header; 'null' if not in a cell).
// Returns '{header, rows, added, replaced, ignoredColumns, ignoredCells,
// errors}', or 'null' if the cells have no header and aren't pasted in a
// cell.
export function pasteRows(problemType, kind, rows, parsed, options) {
    const {objective, newRow} = options;
    const names = namedColumns(problemType, kind);
    const first = parsed.cells[0];
    const isNumber = (cell) => /^-?\d+([.,]\d+)?$/.test(cell);
    const header = first.some((cell) => names.has(normalizeName(cell)))
        && first.every((cell) => !isNumber(cell));
    const result = {header, rows: 0, added: 0, replaced: false, ignoredColumns: [], ignoredCells: 0, errors: []};
    const write = (row, target, value, rowIndex) => {
        const error = writeCell(row, target, value, objective, parsed.delimiter);
        if (error !== null)
            result.errors.push(`${kind} type ${rowIndex}: ${error}`);
    };
    if (header) {
        const targets = first.map((cell) => (names.get(normalizeName(cell)) || null));
        result.ignoredColumns = first.filter((cell, j) => targets[j] === null && cell !== "");
        // A table with only its new row: replaced.
        if (rows.length === 1 && JSON.stringify(rows[0]) === JSON.stringify(newRow())) {
            rows.splice(0);
            result.replaced = true;
        }
        for (const cells of parsed.cells.slice(1)) {
            const row = newRow();
            rows.push(row);
            ++result.added;
            ++result.rows;
            cells.forEach((value, j) => {
                if (targets[j] !== null && targets[j] !== undefined)
                    write(row, targets[j], value, rows.length - 1);
            });
        }
        return result;
    }
    if (options.startRow === null || options.startColumn === null)
        return null;
    const columns = tableColumns(problemType, kind, objective, options.instanceParameters);
    const startColumn = columns.findIndex((column) => column.key === options.startColumn);
    if (startColumn === -1)
        return null;
    parsed.cells.forEach((cells, i) => {
        const rowIndex = options.startRow + i;
        while (rowIndex >= rows.length) {
            rows.push(newRow());
            ++result.added;
        }
        ++result.rows;
        cells.forEach((value, j) => {
            const column = columns[startColumn + j];
            // Beyond the last column, a column of several values (the
            // rotations of box, the trims), or the copies of the single bin
            // of the open dimension objectives: not pasted.
            if (column === undefined || column.type === "rotations" || column.type === "trims"
                    || (column.singleForOpenDimension && form.openDimension(objective))) {
                ++result.ignoredCells;
                return;
            }
            write(rows[rowIndex], {column}, value, rowIndex);
        });
    });
    return result;
}
