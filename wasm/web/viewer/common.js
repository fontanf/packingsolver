// Shared by the modules of the viewer.

// The colors of the item types: 'plotly.express.colors.qualitative.Pastel',
// as the Python visualizers ('python/packingsolver/visualize/').
export const PASTEL_COLORS = [
    "rgb(102, 197, 204)", "rgb(246, 207, 113)", "rgb(248, 156, 116)",
    "rgb(220, 176, 242)", "rgb(135, 197, 95)", "rgb(158, 185, 243)",
    "rgb(254, 136, 177)", "rgb(201, 219, 116)", "rgb(139, 224, 164)",
    "rgb(180, 151, 231)", "rgb(179, 179, 179)",
];

// Parse a CSV text with a header line, like Python's 'csv.DictReader': an
// array of objects mapping the column names to the (string) values. The
// certificates have no quoted fields.
export function parseCsv(text) {
    const lines = text.split(/\r?\n/).filter((line) => line !== "");
    if (lines.length === 0)
        return [];
    const header = lines[0].split(",");
    return lines.slice(1).map((line) => {
        const values = line.split(",");
        const row = {};
        header.forEach((name, i) => {
            row[name] = (i < values.length)? values[i]: null;
        });
        return row;
    });
}
