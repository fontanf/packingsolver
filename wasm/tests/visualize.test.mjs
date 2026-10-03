// Run with: node --test wasm/tests/*.test.js wasm/tests/*.test.mjs
// The JavaScript visualizers reproduce the figures of the Python ones (see
// 'generate_visualize_fixtures.py').

import assert from "node:assert";
import fs from "node:fs";
import path from "node:path";
import test from "node:test";
import {fileURLToPath} from "node:url";

const TESTS_DIR = path.dirname(fileURLToPath(import.meta.url));
const FIXTURES_DIR = path.join(TESTS_DIR, "fixtures", "visualize");
const VISUALIZE_DIR = path.join(TESTS_DIR, "..", "web", "visualize");

// Deep equality, numbers compared with a relative tolerance (JavaScript and
// numpy trigonometric functions may differ in the last bits).
function assertClose(actual, expected, where = "figure") {
    if (typeof expected === "number" && typeof actual === "number") {
        const tolerance = 1e-9 * Math.max(1, Math.abs(expected));
        assert.ok(
            Math.abs(actual - expected) <= tolerance,
            `${where}: ${actual} != ${expected}`);
        return;
    }
    if (Array.isArray(expected)) {
        assert.ok(Array.isArray(actual), `${where}: not an array`);
        assert.strictEqual(actual.length, expected.length, `${where}: length`);
        expected.forEach((value, i) => assertClose(actual[i], value, `${where}[${i}]`));
        return;
    }
    if (expected !== null && typeof expected === "object") {
        assert.ok(actual !== null && typeof actual === "object", `${where}: not an object`);
        // Properties set to 'undefined' are absent from the JSON figure.
        const actualKeys = Object.keys(actual).filter((key) => actual[key] !== undefined);
        assert.deepStrictEqual(
            actualKeys.sort(), Object.keys(expected).sort(), `${where}: keys`);
        for (const key of Object.keys(expected))
            assertClose(actual[key], expected[key], `${where}.${key}`);
        return;
    }
    assert.strictEqual(actual, expected, where);
}

for (const problemType of fs.readdirSync(FIXTURES_DIR).sort()) {
    const directory = path.join(FIXTURES_DIR, problemType);
    if (!fs.statSync(directory).isDirectory())
        continue;
    for (const name of fs.readdirSync(directory).sort()) {
        if (name === "cases.json" || !name.endsWith(".json"))
            continue;
        test(`visualize ${problemType}: ${name.slice(0, -5)}`, async () => {
            const fixture = JSON.parse(fs.readFileSync(path.join(directory, name), "utf8"));
            const visualizer = await import(path.join(VISUALIZE_DIR, problemType + ".js"));
            const figure = visualizer[toCamelCase(fixture.function)](
                fixture.certificate, fixture.options);
            assertClose(figure, fixture.figure);
        });
    }
}

// 'instance_figure' -> 'instanceFigure'.
function toCamelCase(name) {
    return name.replace(/_([a-z])/g, (_, letter) => letter.toUpperCase());
}
