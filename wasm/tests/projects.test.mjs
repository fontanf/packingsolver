// Run with: node --test wasm/tests/*.test.js wasm/tests/*.test.mjs
// Projects of the web page ('projects.js'): the examples, the new projects,
// the projects opened from instances of the JSON format, their copies and
// their storage.

import assert from "node:assert";
import fs from "node:fs";
import path from "node:path";
import test from "node:test";
import {fileURLToPath} from "node:url";

import * as form from "../web/form.js";
import * as projects from "../web/projects.js";

const DIRECTORY = path.dirname(fileURLToPath(import.meta.url));
const FIXTURES = path.join(DIRECTORY, "fixtures", "instances");
const PROBLEM_TYPES = ["rectangleguillotine", "rectangle", "box", "boxstacks", "onedimensional", "irregular"];

const instance = (project) => form.toInstance(project.problemType, project.form);

// The fixtures whose rows refer to other rows: the item types of the resource
// consumptions, and of the fixed items of the irregular bin types.
const REFERENCES = {
    "rectangle_knapsack.json": "rectangle",
    "box_knapsack.json": "box",
    "onedimensional_variable_sized.json": "onedimensional",
    "rectangleguillotine_cutting_cost.json": "rectangleguillotine",
    "irregular_variable_sized.json": "irregular",
};

function openFixture(name) {
    const json = JSON.parse(fs.readFileSync(path.join(FIXTURES, name), "utf8"));
    return projects.projectFromInstance(REFERENCES[name], "Fixture", json).project;
}

test("projects: the examples are valid instances", () => {
    for (const problemType of PROBLEM_TYPES) {
        const example = projects.exampleProject(problemType);
        assert.strictEqual(example.id, "example-" + problemType);
        assert.strictEqual(example.example, true);
        assert.strictEqual(instance(example).objective, example.form.objective, problemType);
    }
});

test("projects: the new projects are valid instances", () => {
    for (const problemType of PROBLEM_TYPES) {
        const project = projects.newProject(problemType, "New project");
        assert.strictEqual(project.example, false);
        assert.strictEqual(project.name, "New project");
        assert.strictEqual(project.result, null);
        assert.ok(instance(project), problemType);
    }
});

test("projects: names", () => {
    assert.strictEqual(projects.uniqueName("a", []), "a");
    assert.strictEqual(projects.uniqueName("a", ["a"]), "a (2)");
    assert.strictEqual(projects.uniqueName("a", ["a", "a (2)", "b"]), "a (3)");
    assert.strictEqual(projects.nameFromFileName("instance_rectangle.json"), "instance_rectangle");
    assert.strictEqual(projects.nameFromFileName("data.JSON"), "data");
    assert.strictEqual(projects.nameFromFileName(".json"), "Project");
});

test("projects: a copy keeps the references between its rows", () => {
    for (const name of Object.keys(REFERENCES)) {
        const original = openFixture(name);
        original.result = {output: {Time: 1}, certificate: "", objective: original.form.objective};
        const copy = projects.duplicateProject(original, "Copy");
        assert.notStrictEqual(copy.id, original.id);
        assert.strictEqual(copy.name, "Copy");
        assert.strictEqual(copy.example, false);
        assert.deepStrictEqual(copy.result, original.result);
        // The rows of the copy refer to the item rows of the copy.
        assert.notStrictEqual(copy.form.itemTypes[0], original.form.itemTypes[0]);
        assert.deepStrictEqual(instance(copy), instance(original), name);
    }
    // A copy of an example can be modified.
    const copy = projects.duplicateProject(projects.exampleProject("box"), "Example (copy)");
    assert.strictEqual(copy.example, false);
    assert.notStrictEqual(copy.id, "example-box");
});

test("projects: storage", async () => {
    const store = new projects.MemoryStore();
    const original = openFixture("rectangle_knapsack.json");
    await store.save(original);
    const [loaded] = await store.load();
    assert.notStrictEqual(loaded, original);
    assert.deepStrictEqual(instance(loaded), instance(original));
    await store.remove(original.id);
    assert.deepStrictEqual(await store.load(), []);
});

test("projects: examples and stored projects", () => {
    const stored = projects.newProject("box", "Mine");
    // The parameters and the solution of an example are stored, not its form.
    const storedExample = projects.exampleProject("box");
    storedExample.parameters = {optimizationMode: "not-anytime", timeLimit: "5"};
    storedExample.result = {output: {Time: 1}, certificate: "", objective: "knapsack"};
    storedExample.form.itemTypes = [];
    const all = projects.withExamples(PROBLEM_TYPES, [stored, storedExample]);
    assert.strictEqual(all.length, PROBLEM_TYPES.length + 1);
    const box = projects.projectsOfType(all, "box");
    assert.deepStrictEqual(box.map((p) => p.name), ["Example", "Mine"]);
    assert.deepStrictEqual(box[0].parameters, storedExample.parameters);
    assert.deepStrictEqual(box[0].result, storedExample.result);
    assert.deepStrictEqual(box[0].form, projects.exampleProject("box").form);
    assert.deepStrictEqual(projects.projectsOfType(all, "rectangle").map((p) => p.name), ["Example"]);
});
