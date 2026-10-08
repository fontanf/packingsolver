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
import {canonicalInstance} from "./canonical_instance.mjs";

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

// The examples of the documentation, as written for the web page by
// 'scripts/web_examples.py'.
const DOC_EXAMPLES = path.join(DIRECTORY, "..", "..", "doc", "examples");
function docExamples() {
    const examples = {};
    for (const problemType of PROBLEM_TYPES) {
        const listPath = path.join(DOC_EXAMPLES, problemType, "examples.json");
        if (!fs.existsSync(listPath))
            continue;
        examples[problemType] = JSON.parse(fs.readFileSync(listPath, "utf8")).map((example) => ({
            ...example,
            instance: JSON.parse(fs.readFileSync(
                path.join(DOC_EXAMPLES, problemType, example.name, "instance.json"), "utf8")),
        }));
    }
    return examples;
}

test("projects: the examples of the documentation", () => {
    const examples = docExamples();
    assert.ok(examples.rectangle.length > 1);
    for (const [problemType, listed] of Object.entries(examples)) {
        for (const example of listed) {
            const name = `${problemType}/${example.name}`;
            // The form reads all their fields, and gives them back.
            const opened = form.fromInstance(problemType, example.instance);
            assert.deepStrictEqual(opened.ignored, [], name);
            assert.deepStrictEqual(
                canonicalInstance(problemType, form.toInstance(problemType, opened)),
                canonicalInstance(problemType, example.instance), name);
        }
    }
    // They replace the example of their problem type, in their order, and
    // keep the parameters and the solution stored for them.
    const stored = {...projects.docExampleProject("rectangle", examples.rectangle[1], 1),
        parameters: {optimizationMode: "anytime", timeLimit: "5"}};
    const all = projects.withExamples(PROBLEM_TYPES, [stored], examples);
    const rectangle = projects.projectsOfType(all, "rectangle");
    assert.deepStrictEqual(rectangle.map((p) => p.name), examples.rectangle.map((e) => e.title));
    assert.ok(rectangle.every((p) => p.example));
    assert.strictEqual(rectangle[1].parameters.timeLimit, "5");
    // Without examples of the documentation, the example of the problem
    // type.
    assert.deepStrictEqual(projects.examplesOfType("box", {}).map((p) => p.id), ["example-box"]);
    // The links of the documentation.
    const linked = projects.findExample(all, "rectangle/" + examples.rectangle[2].name);
    assert.strictEqual(linked, rectangle[2]);
    assert.strictEqual(projects.findExample(all, "rectangle/missing"), undefined);
    // An example of another problem type.
    assert.ok(examples.rectangle.some((e) => e.name === "defects_yes"));
    assert.strictEqual(projects.findExample(all, "box/defects_yes"), undefined);
    // An example which the form can't open is left out.
    const invalid = {rectangle: [{name: "invalid", title: "Invalid", instance: {objective: "maximize"}}]};
    assert.deepStrictEqual(projects.examplesOfType("rectangle", invalid).map((p) => p.id), ["example-rectangle"]);
});

test("projects: the links of the documentation to the examples", () => {
    const examples = docExamples();
    const docDirectory = path.join(DOC_EXAMPLES, "..");
    let links = 0;
    for (const name of fs.readdirSync(docDirectory).filter((n) => n.endsWith(".rst"))) {
        const text = fs.readFileSync(path.join(docDirectory, name), "utf8");
        // The links, and the examples of the 'example-tabs' directive
        // ('doc/_ext/example_tabs.py').
        const references = [
            ...[...text.matchAll(/\?example=([a-z]+\/[a-z0-9_]+)/g)].map((match) => match[1]),
            ...[...text.matchAll(/^\.\. example-tabs:: (.*)$/gm)].flatMap((match) => match[1].trim().split(/\s+/)),
        ];
        for (const reference of references) {
            const [problemType, exampleName] = reference.split("/");
            assert.ok((examples[problemType] || []).some((e) => e.name === exampleName),
                `${name}: no example ${reference}`);
            ++links;
        }
    }
    assert.ok(links > 0);
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

test("projects: export and import", () => {
    for (const fixture of Object.keys(REFERENCES)) {
        const original = openFixture(fixture);
        const problemType = original.problemType;
        original.name = "Mine";
        original.result = {output: {Time: 1, Solution: {NumberOfItems: 1}}, certificate: "a,b\n",
            objective: original.form.objective, totalItems: 4};
        original.progress = [{time: 1, solution: "1 items", bound: "", value: 1, boundValue: null}];
        const other = projects.newProject(problemType === "box"? "rectangle": "box", "Other type");
        const all = [...projects.withExamples(PROBLEM_TYPES, []), original, other];
        const {files, count} = projects.exportFiles(all, problemType);
        // The projects of the problem type, except the example.
        assert.strictEqual(count, 1);
        const certificate = (problemType === "irregular")? "solution.json": "solution.csv";
        assert.deepStrictEqual(Object.keys(files).sort(), [
            "Mine/form.json", "Mine/instance.json", "Mine/output.json", "Mine/progress.json",
            `Mine/${certificate}`, "projects.json"].sort(), fixture);
        // The instance, for the solvers.
        assert.deepStrictEqual(JSON.parse(files["Mine/instance.json"]), instance(original));

        // Imported: the same instance, the references between the rows
        // restored, the same solution.
        const [p] = projects.importFiles(files, projects.withExamples(PROBLEM_TYPES, []), PROBLEM_TYPES);
        assert.strictEqual(p.name, "Mine");
        assert.strictEqual(p.problemType, problemType);
        assert.notStrictEqual(p.id, original.id);
        assert.deepStrictEqual(instance(p), instance(original), fixture);
        assert.deepStrictEqual(p.result, original.result);
        assert.deepStrictEqual(p.progress, original.progress);
        assert.deepStrictEqual(p.parameters, original.parameters);

        // Imported again: a new name.
        const [again] = projects.importFiles(files, [...all, p], PROBLEM_TYPES);
        assert.strictEqual(again.name, "Mine (2)");
    }
    // The item rows referred to are the item rows of the project.
    const rectangle = openFixture("rectangle_knapsack.json");
    const [imported] = projects.importFiles(projects.exportFiles([rectangle], "rectangle").files, [], PROBLEM_TYPES);
    const consumption = imported.form.binTypes[0].resources[0].consumptions[0];
    assert.ok(imported.form.itemTypes.includes(consumption.itemRow));
    const irregular = openFixture("irregular_variable_sized.json");
    const [importedIrregular] = projects.importFiles(
        projects.exportFiles([irregular], "irregular").files, [], PROBLEM_TYPES);
    const fixedItem = importedIrregular.form.binTypes.flatMap((row) => row.fixed_items)[0];
    assert.ok(importedIrregular.form.itemTypes.includes(fixedItem.itemRow));
});

test("projects: export details", () => {
    // Only the example: nothing to export.
    assert.strictEqual(projects.exportFiles(projects.withExamples(PROBLEM_TYPES, []), "box").count, 0);
    // An invalid form: no instance; no solution: no solution files.
    const invalid = projects.newProject("box", "a/b");
    invalid.form.itemTypes[0].x = "";
    const same = projects.newProject("box", "A/B");
    const {files} = projects.exportFiles([invalid, same], "box");
    assert.deepStrictEqual(Object.keys(files).sort(),
        ["A_B (2)/form.json", "A_B (2)/instance.json", "a_b/form.json", "projects.json"]);
    // Extracted and compressed again: in a folder.
    const inFolder = Object.fromEntries(Object.entries(files).map(([path, content]) => ["export/" + path, content]));
    const imported = projects.importFiles(inFolder, [], PROBLEM_TYPES);
    assert.deepStrictEqual(imported.map((p) => p.name), ["a/b", "A/B"]);
    assert.strictEqual(imported[0].result, null);
    assert.deepStrictEqual(imported[0].progress, []);
    // Folder names.
    assert.strictEqual(projects.folderName("a:b?", []), "a_b_");
    assert.strictEqual(projects.folderName("Mine", ["mine"]), "Mine (2)");
    assert.strictEqual(projects.folderName(" . ", []), "Project");
});

test("projects: import errors", () => {
    const importFiles = (files) => projects.importFiles(files, [], PROBLEM_TYPES);
    const manifest = (json) => ({"projects.json": JSON.stringify(json)});
    const header = {format: projects.EXPORT_FORMAT, version: projects.EXPORT_VERSION};
    assert.throws(() => importFiles({"instance.json": "{}"}), /no projects\.json/);
    assert.throws(() => importFiles({"projects.json": "{"}), /invalid projects\.json/);
    assert.throws(() => importFiles(manifest({objective: "knapsack"})), /not an export of PackingSolver projects/);
    assert.throws(() => importFiles(manifest({...header, version: 99, projects: []})), /unsupported version/);
    assert.throws(() => importFiles(manifest({...header, projects: [{folder: "a", problemType: "cylinder"}]})),
        /unknown problem type/);
    assert.throws(() => importFiles(manifest({...header, projects: [{folder: "a", problemType: "box"}]})),
        /missing a\/form\.json/);
    assert.throws(() => importFiles({...manifest({...header, projects: [{folder: "a", problemType: "box"}]}),
        "a/form.json": JSON.stringify({objective: "knapsack"})}), /invalid form/);
});
