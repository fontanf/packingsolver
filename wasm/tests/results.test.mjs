// Run with: node --test wasm/tests/*.test.js wasm/tests/*.test.mjs
// The results of an optimization in the web page ('results.js'): the key
// numbers of a solution and the chart of the progress.

import assert from "node:assert";
import test from "node:test";

import * as results from "../web/results.js";

const COLORS = {accent: "blue", muted: "gray", text: "black", grid: "silver"};

function output(solution, other = {}) {
    return {Time: 1.234, Solution: {NumberOfItems: 3, ...solution}, ...other};
}

const labels = (tiles) => tiles.map((tile) => tile.label);

test("results: values, bounds and gaps", () => {
    assert.strictEqual(results.objectiveValue(output({ItemProfit: 15}), "knapsack"), 15);
    assert.strictEqual(results.objectiveValue(output({XMax: 120}), "open-dimension-x"), 120);
    assert.strictEqual(results.objectiveValue(output({Width: 80}), "open-dimension-x"), 80);
    // No solution, no value.
    assert.strictEqual(results.objectiveValue(output({NumberOfItems: 0, NumberOfBins: 0}), "bin-packing"), null);
    assert.strictEqual(results.objectiveValue(output({}), "feasibility"), null);
    assert.strictEqual(results.boundValue(output({}, {KnapsackBound: 20}), "knapsack"), 20);
    assert.strictEqual(results.boundValue(output({}, {KnapsackBound: null}), "knapsack"), null);
    assert.strictEqual(results.boundValue(output({}), "bin-packing-with-leftovers"), null);
    assert.strictEqual(results.gap(15, 20), 0.25);
    assert.strictEqual(results.gap(4, 4), 0);
    assert.strictEqual(results.gap(0, 0), 0);
    assert.strictEqual(results.gap(15, null), null);
    assert.strictEqual(results.formatPercent(0.25), "25%");
    assert.strictEqual(results.formatPercent(0.0012345), "0.12%");
    assert.strictEqual(results.formatPercent(0), "0%");
    assert.strictEqual(results.totalItems({item_types: [{copies: 2}, {}]}), 3);
    assert.strictEqual(results.totalItems({item_types: [{copies: -1}]}), null);
    assert.strictEqual(results.formatValue(3570400), "3,570,400");
    assert.strictEqual(results.formatValue(1234.56789), "1,234.57");
});

test("results: tiles", () => {
    // Knapsack, with a bound: the gap.
    let tiles = results.summaryTiles({
        output: output({ItemProfit: 15}, {KnapsackBound: 20}), objective: "knapsack", totalItems: 5});
    assert.deepStrictEqual(labels(tiles), ["Items packed", "Profit", "Bound", "Gap", "Time"]);
    assert.strictEqual(tiles[0].value, "3 / 5");
    assert.strictEqual(tiles[3].value, "25%");
    assert.strictEqual(tiles[3].note, undefined);
    assert.strictEqual(tiles[4].value, "1.23 s");
    // Bin packing, optimal, with the density.
    tiles = results.summaryTiles({
        output: output({NumberOfBins: 2, ItemArea: 179, Waste: 21, WastePercentage: 0.105}, {BinPackingBound: 2}),
        objective: "bin-packing", totalItems: 3});
    assert.deepStrictEqual(labels(tiles), ["Items packed", "Bins", "Bound", "Gap", "Density", "Time"]);
    assert.strictEqual(tiles[3].value, "0%");
    assert.strictEqual(tiles[3].note, "Optimal");
    // Items over items plus waste.
    assert.strictEqual(tiles[4].value, "89.5%");
    // Irregular, open dimension: the density in the area used along the open
    // dimension, not in the bin.
    tiles = results.summaryTiles({
        output: output({XMax: 120, DensityX: 0.8, DensityY: 0.5, FullWastePercentage: 0.9}),
        objective: "open-dimension-x", totalItems: 3});
    assert.strictEqual(tiles.find((tile) => tile.label === "Density").value, "80%");
    tiles = results.summaryTiles({
        output: output({YMax: 60, DensityX: 0.8, DensityY: 0.5, FullWastePercentage: 0.9}),
        objective: "open-dimension-y", totalItems: 3});
    assert.strictEqual(tiles.find((tile) => tile.label === "Density").value, "50%");
    // Irregular, bin packing with leftovers: items over bins minus leftover.
    tiles = results.summaryTiles({
        output: output({ItemArea: 600, BinArea: 2000, LeftoverValue: 1200, DensityX: 0.8, FullWastePercentage: 0.7}),
        objective: "bin-packing-with-leftovers", totalItems: 3});
    assert.strictEqual(tiles.find((tile) => tile.label === "Density").value, "75%");
    // Rectangle, leftover along X (a length): items over items plus waste.
    tiles = results.summaryTiles({
        output: output({ItemArea: 600, BinArea: 2000, LeftoverValue: 30, Waste: 200}),
        objective: "bin-packing-with-leftovers", totalItems: 3});
    assert.strictEqual(tiles.find((tile) => tile.label === "Density").value, "75%");
    // Box: volumes; onedimensional: lengths.
    tiles = results.summaryTiles({output: output({NumberOfBins: 1, ItemVolume: 90, Waste: 10}),
        objective: "bin-packing", totalItems: 3});
    assert.strictEqual(tiles.find((tile) => tile.label === "Density").value, "90%");
    tiles = results.summaryTiles({output: output({NumberOfBins: 1, ItemLength: 95, Waste: 5}),
        objective: "bin-packing", totalItems: 3});
    assert.strictEqual(tiles.find((tile) => tile.label === "Density").value, "95%");
    // Onedimensional with nesting lengths: the items occupy less than their
    // length (8 items of length 70, nesting length 10, in a bin of 500).
    tiles = results.summaryTiles({output: output({NumberOfBins: 1, ItemLength: 560, ItemNestedLength: 490, Waste: 0}),
        objective: "bin-packing", totalItems: 8});
    assert.strictEqual(tiles.find((tile) => tile.label === "Density").value, "100%");
    tiles = results.summaryTiles({output: output({NumberOfBins: 1, ItemLength: 560, ItemNestedLength: 490, Waste: 10}),
        objective: "bin-packing", totalItems: 8});
    assert.strictEqual(tiles.find((tile) => tile.label === "Density").value, "98%");
    // Bin packing with leftovers: the number of bins, then the leftover if
    // the problem type has one (box, boxstacks, rectangle, irregular).
    tiles = results.summaryTiles({output: output({NumberOfBins: 2, ItemVolume: 90, Waste: 10, LeftoverValue: 40}),
        objective: "bin-packing-with-leftovers", totalItems: 3});
    assert.deepStrictEqual(labels(tiles), ["Items packed", "Bins", "Leftover", "Density", "Time"]);
    assert.strictEqual(tiles[1].value, "2");
    assert.strictEqual(tiles[2].value, "40");
    // Rectangleguillotine and onedimensional: no leftover value.
    tiles = results.summaryTiles({output: output({NumberOfBins: 2, ItemArea: 180, Waste: 20}),
        objective: "bin-packing-with-leftovers", totalItems: 3});
    assert.deepStrictEqual(labels(tiles), ["Items packed", "Bins", "Density", "Time"]);
    // Without bound; a solution stored without the number of items.
    tiles = results.summaryTiles({output: output({LeftoverValue: 7}), objective: "bin-packing-with-leftovers"});
    assert.deepStrictEqual(labels(tiles), ["Items packed", "Leftover", "Time"]);
    assert.strictEqual(tiles[0].value, "3");
});

test("results: chart of the progress", () => {
    const result = {output: output({ItemProfit: 15}, {Time: 5}), objective: "knapsack"};
    const progress = [
        {time: 0.5, value: 10, boundValue: 30},
        {time: 1, value: null, boundValue: 20},
        {time: 2, value: 15, boundValue: 20},
    ];
    const figure = results.progressFigure(progress, result, COLORS);
    assert.deepStrictEqual(figure.data.map((trace) => trace.name), ["Solution", "Bound"]);
    const [solution, bound] = figure.data;
    // Steps until the end of the optimization, a marker on the updates only.
    assert.deepStrictEqual(solution.x, [0.5, 2, 5]);
    assert.deepStrictEqual(solution.y, [10, 15, 15]);
    assert.deepStrictEqual(solution.marker.size, [8, 8, 0]);
    assert.strictEqual(solution.line.shape, "hv");
    assert.strictEqual(solution.line.color, "blue");
    assert.deepStrictEqual(bound.x, [0.5, 1, 2, 5]);
    // A marker where the bound changes only.
    assert.deepStrictEqual(bound.marker.size, [8, 8, 0, 0]);
    assert.strictEqual(bound.line.color, "gray");
    assert.strictEqual(figure.layout.showlegend, true);
    // The times span 0.5 to 5 s: a linear time axis.
    assert.strictEqual(figure.layout.xaxis.type, undefined);
    assert.strictEqual(figure.layout.yaxis.title.text, "Profit");
    // One series: no legend.
    const single = results.progressFigure(progress.map((line) => ({...line, boundValue: null})), result, COLORS);
    assert.strictEqual(single.data.length, 1);
    assert.strictEqual(single.layout.showlegend, false);
    // The times span 0.01 to 5 s: a logarithmic time axis.
    const early = results.progressFigure([{time: 0.01, value: 10, boundValue: null}], result, COLORS);
    assert.strictEqual(early.layout.xaxis.type, "log");
    // A progress stored without the values, an objective without value: no
    // chart.
    assert.strictEqual(results.progressFigure([{time: 1, solution: "3 items", bound: ""}], result, COLORS), null);
    assert.strictEqual(results.progressFigure(progress, {...result, objective: "feasibility"}, COLORS), null);
});
