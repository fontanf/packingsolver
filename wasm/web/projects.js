// Projects of the web page: an instance of a problem type (its form), the
// parameters of the solver and the last solution found, stored in the
// browser (IndexedDB). The examples are projects which can't be modified nor
// removed; they can be duplicated.

import * as irregularForm from "./irregular_form.js";
import * as form from "./form.js";

// Default parameters of the solver.
export function defaultParameters() {
    return {optimizationMode: "anytime", timeLimit: "30"};
}

function project(problemType, name, projectForm) {
    return {
        id: newId(),
        problemType,
        name,
        example: false,
        // '{objective, instanceParameters, binTypes, itemTypes}': the form of
        // the instance (see 'form.toInstance').
        form: projectForm,
        parameters: defaultParameters(),
        // Last update of the solver ('{output, certificate}'), and a line for
        // each update of the optimization which found it ('{time, solution,
        // bound}').
        result: null,
        progress: [],
        created: Date.now(),
    };
}

function newId() {
    return (typeof crypto !== "undefined" && crypto.randomUUID !== undefined)?
        crypto.randomUUID():
        Date.now().toString(36) + Math.random().toString(36).slice(2);
}

// The example of a problem type. Its id doesn't change, so that its parameters
// and its solution can be stored.
export function exampleProject(problemType) {
    let binTypes;
    let itemTypes;
    let objective;
    if (problemType === "irregular") {
        const example = irregularForm.EXAMPLE;
        objective = example.objective;
        binTypes = example.binTypes.map((t) => ({...irregularForm.defaultBinRow(), ...t}));
        itemTypes = example.itemTypes.map((t) => ({...irregularForm.defaultItemRow(), ...t}));
    } else {
        const example = form.EXAMPLES[problemType];
        objective = example.objective;
        const fill = (columns, types) => types.map((t) => ({...form.defaultRow(columns), ...t}));
        binTypes = fill(form.binColumns(problemType), example.bin_types);
        itemTypes = fill(form.itemColumns(problemType), example.item_types);
    }
    return {
        ...project(problemType, "Example", {
            objective,
            instanceParameters: form.defaultInstanceParameters(problemType),
            binTypes,
            itemTypes,
        }),
        id: "example-" + problemType,
        example: true,
        created: 0,
    };
}

// A new project: a bin type and an item type with the default values.
export function newProject(problemType, name) {
    const objective = form.defaultObjective(problemType);
    return project(problemType, name, {
        objective,
        instanceParameters: form.defaultInstanceParameters(problemType),
        binTypes: [form.newBinRow(problemType, objective)],
        itemTypes: [form.newItemRow(problemType)],
    });
}

// A project from an instance in the JSON format, with the fields which the
// solver doesn't read ('{project, ignored}').
export function projectFromInstance(problemType, name, json) {
    const opened = form.fromInstance(problemType, json);
    const projectForm = {
        objective: opened.objective,
        instanceParameters: opened.instanceParameters,
        binTypes: opened.binTypes,
        itemTypes: opened.itemTypes,
    };
    return {project: project(problemType, name, projectForm), ignored: opened.ignored};
}

// A copy of a project, which can be modified. The rows of its form are copied
// with the references between them (e.g. the item type of a resource
// consumption).
export function duplicateProject(original, name) {
    const copy = structuredClone(original);
    copy.id = newId();
    copy.name = name;
    copy.example = false;
    copy.created = Date.now();
    return copy;
}

// The name of a project from the name of a JSON file: without its extension.
export function nameFromFileName(fileName) {
    const name = fileName.replace(/\.json$/i, "").trim();
    return (name !== "")? name: "Project";
}

// 'name', or 'name (2)', 'name (3)'... if it is already used.
export function uniqueName(name, usedNames) {
    const used = new Set(usedNames);
    if (!used.has(name))
        return name;
    for (let i = 2;; ++i) {
        const candidate = `${name} (${i})`;
        if (!used.has(candidate))
            return candidate;
    }
}

// The projects of a problem type: the example, then the others in the order
// in which they were created.
export function projectsOfType(projects, problemType) {
    return projects
        .filter((p) => p.problemType === problemType)
        .sort((p1, p2) => (p2.example - p1.example) || (p1.created - p2.created));
}

// The examples, with the parameters and the solution stored for them. The
// form of an example always comes from the code, so that it follows the
// changes of the examples.
export function withExamples(problemTypes, storedProjects) {
    const stored = new Map(storedProjects.map((p) => [p.id, p]));
    const projects = storedProjects.filter((p) => !p.example);
    for (const problemType of problemTypes) {
        const example = exampleProject(problemType);
        const record = stored.get(example.id);
        if (record !== undefined) {
            example.parameters = record.parameters;
            example.result = record.result;
            example.progress = record.progress;
        }
        projects.push(example);
    }
    return projects;
}

/////////////////////////////////////////////////////////////////////////////
// Export and import
/////////////////////////////////////////////////////////////////////////////

// The format of the files of the exported projects.
export const EXPORT_FORMAT = "packingsolver-projects";
export const EXPORT_VERSION = 1;

// In a file, a reference of a row of the form to an item row (e.g. the item
// type of a resource consumption, of a fixed item of an irregular bin type)
// is '{"$itemRow": <index of the item row>}', since JSON can't hold
// references.
const ITEM_ROW_REFERENCE = "$itemRow";

// Replace in 'value' (in place) the children for which 'replace' returns a
// value.
function replaceChildren(value, replace) {
    for (const key of Object.keys(value)) {
        const child = value[key];
        if (child === null || typeof child !== "object")
            continue;
        const replacement = replace(child);
        if (replacement !== undefined)
            value[key] = replacement;
        else
            replaceChildren(child, replace);
    }
}

// A copy of the form of a project which can be written in JSON.
function formToJson(projectForm) {
    const copy = structuredClone(projectForm);
    const indices = new Map(copy.itemTypes.map((itemRow, i) => [itemRow, i]));
    const reference = (child) => indices.has(child)? {[ITEM_ROW_REFERENCE]: indices.get(child)}: undefined;
    for (const itemRow of copy.itemTypes)
        replaceChildren(itemRow, reference);
    replaceChildren(copy.binTypes, reference);
    replaceChildren(copy.instanceParameters, reference);
    return copy;
}

// The form of a project from its JSON form ('formToJson'), in place.
function formFromJson(projectForm) {
    const itemRows = projectForm.itemTypes;
    const row = (child) => {
        if (!Object.prototype.hasOwnProperty.call(child, ITEM_ROW_REFERENCE))
            return undefined;
        const itemRow = itemRows[child[ITEM_ROW_REFERENCE]];
        // A reference to an item row which doesn't exist: no item row, as for
        // an invalid item type id in the form.
        return (itemRow !== undefined)? itemRow: null;
    };
    for (const itemRow of itemRows)
        replaceChildren(itemRow, row);
    replaceChildren(projectForm.binTypes, row);
    replaceChildren(projectForm.instanceParameters, row);
    return projectForm;
}

// The name of the folder of a project in an export: its name, without the
// characters which file systems don't accept, different from 'usedFolders'
// (in lower case: some file systems ignore the case).
export function folderName(name, usedFolders) {
    let folder = name.replace(/[\\/:*?"<>|\u0000-\u001f]/g, "_").replace(/^[\s.]+|[\s.]+$/g, "");
    if (folder === "")
        folder = "Project";
    const used = new Set([...usedFolders].map((f) => f.toLowerCase()));
    let candidate = folder;
    for (let i = 2; used.has(candidate.toLowerCase()); ++i)
        candidate = `${folder} (${i})`;
    return candidate;
}

// The name of the file of the certificate of a solution.
function certificateFile(problemType) {
    return (problemType === "irregular")? "solution.json": "solution.csv";
}

// The files of the export of the projects of a problem type, except its
// example ('{path: content}' in 'files', and the number of projects): a
// folder for each project with
// - 'form.json': its form (with the references between its rows),
// - 'instance.json': its instance in the JSON format, if its form is valid,
// - its last solution: the certificate ('solution.csv', or 'solution.json'
//   for irregular), the output ('output.json') and the progress
//   ('progress.json'),
// and 'projects.json': the list of the projects, with their name, their
// folder and the parameters of the solver.
export function exportFiles(projects, problemType) {
    const files = {};
    const entries = [];
    for (const p of projectsOfType(projects, problemType).filter((p) => !p.example)) {
        const folder = folderName(p.name, entries.map((entry) => entry.folder));
        files[`${folder}/form.json`] = JSON.stringify(formToJson(p.form)) + "\n";
        try {
            files[`${folder}/instance.json`] = JSON.stringify(form.toInstance(problemType, p.form), null, 4) + "\n";
        } catch (error) {
            // An invalid form: no instance.
        }
        if (p.result !== null) {
            files[`${folder}/${certificateFile(problemType)}`] = p.result.certificate;
            files[`${folder}/output.json`] = JSON.stringify(p.result.output, null, 4) + "\n";
        }
        if (p.progress.length > 0)
            files[`${folder}/progress.json`] = JSON.stringify(p.progress) + "\n";
        entries.push({
            folder,
            name: p.name,
            problemType: p.problemType,
            parameters: p.parameters,
            created: p.created,
            // What the solution files don't have.
            result: (p.result === null)? null: {objective: p.result.objective, totalItems: p.result.totalItems},
        });
    }
    files["projects.json"] = JSON.stringify({
        format: EXPORT_FORMAT,
        version: EXPORT_VERSION,
        exported: new Date().toISOString(),
        problemType,
        projects: entries,
    }, null, 4) + "\n";
    return {files, count: entries.length};
}

// The projects of the files of an export ('exportFiles'; '{path: content}'),
// added to 'projects': new projects (new ids), named as in the export, with
// " (2)", " (3)"... if the name is already used for their problem type. The
// files can be in a folder (e.g. extracted and compressed again). Throws an
// error if the files aren't an export.
export function importFiles(files, projects, problemTypes) {
    // 'projects.json', at the root of the export or in a folder.
    const manifest = Object.keys(files)
        .filter((path) => path === "projects.json" || path.endsWith("/projects.json"))
        .sort((path1, path2) => path1.length - path2.length)[0];
    if (manifest === undefined)
        throw new Error("not an export of PackingSolver projects (no projects.json).");
    const root = manifest.slice(0, manifest.length - "projects.json".length);
    const parse = (path) => {
        try {
            return JSON.parse(files[root + path]);
        } catch (error) {
            throw new Error(`invalid ${path}: ${error.message}`);
        }
    };
    const json = parse("projects.json");
    if (typeof json !== "object" || json === null || json.format !== EXPORT_FORMAT)
        throw new Error("not an export of PackingSolver projects.");
    if (json.version !== EXPORT_VERSION)
        throw new Error(`unsupported version of the export: ${json.version}.`);
    if (!Array.isArray(json.projects))
        throw new Error("the export has no projects.");
    const names = projects.map((p) => [p.problemType, p.name]);
    const now = Date.now();
    return json.projects.map((entry, i) => {
        const name = `project ${i} ("${entry && entry.name}")`;
        if (typeof entry !== "object" || entry === null || typeof entry.folder !== "string")
            throw new Error(`invalid ${name}.`);
        if (!problemTypes.includes(entry.problemType))
            throw new Error(`${name}: unknown problem type "${entry.problemType}".`);
        const folder = entry.folder + "/";
        if (files[root + folder + "form.json"] === undefined)
            throw new Error(`${name}: missing ${folder}form.json.`);
        const f = parse(folder + "form.json");
        if (typeof f !== "object" || f === null || typeof f.objective !== "string"
                || typeof f.instanceParameters !== "object" || f.instanceParameters === null
                || !Array.isArray(f.binTypes) || !Array.isArray(f.itemTypes)) {
            throw new Error(`${name}: invalid form.`);
        }
        // The last solution, if its files are there.
        let result = null;
        const certificate = files[root + folder + certificateFile(entry.problemType)];
        if (entry.result && certificate !== undefined && files[root + folder + "output.json"] !== undefined) {
            result = {output: parse(folder + "output.json"), certificate, objective: entry.result.objective};
            if (entry.result.totalItems !== undefined)
                result.totalItems = entry.result.totalItems;
        }
        const progress = (files[root + folder + "progress.json"] !== undefined)? parse(folder + "progress.json"): [];
        const projectName = uniqueName(
            (typeof entry.name === "string" && entry.name.trim() !== "")? entry.name.trim(): "Project",
            names.filter(([type]) => type === entry.problemType).map(([, n]) => n));
        names.push([entry.problemType, projectName]);
        return {
            id: newId(),
            problemType: entry.problemType,
            name: projectName,
            example: false,
            form: formFromJson(f),
            parameters: {...defaultParameters(), ...(entry.parameters || {})},
            result,
            progress: Array.isArray(progress)? progress: [],
            // After the projects which exist, in the order of the export.
            created: now + i,
        };
    });
}

/////////////////////////////////////////////////////////////////////////////
// Storage
/////////////////////////////////////////////////////////////////////////////

// Projects kept in memory, when they can't be stored in the browser.
export class MemoryStore {
    constructor() {
        this.projects = new Map();
        this.persistent = false;
    }

    async load() {
        return [...this.projects.values()].map((p) => structuredClone(p));
    }

    async save(p) {
        this.projects.set(p.id, structuredClone(p));
    }

    async remove(id) {
        this.projects.delete(id);
    }
}

// Projects stored in the browser, in IndexedDB. IndexedDB stores copies of the
// projects made like 'structuredClone', which keep the references between the
// rows of their forms.
export class IndexedDbStore {
    constructor(database) {
        this.database = database;
        this.persistent = true;
    }

    static open(name = "packingsolver") {
        return new Promise((resolve, reject) => {
            const request = indexedDB.open(name, 1);
            request.onupgradeneeded = () => request.result.createObjectStore("projects", {keyPath: "id"});
            request.onsuccess = () => resolve(new IndexedDbStore(request.result));
            request.onerror = () => reject(request.error);
            request.onblocked = () => reject(new Error("the database is blocked by another page."));
        });
    }

    transaction(mode, operation) {
        return new Promise((resolve, reject) => {
            const transaction = this.database.transaction("projects", mode);
            const request = operation(transaction.objectStore("projects"));
            transaction.oncomplete = () => resolve(request.result);
            transaction.onerror = () => reject(transaction.error);
            transaction.onabort = () => reject(transaction.error);
        });
    }

    load() {
        return this.transaction("readonly", (store) => store.getAll());
    }

    save(p) {
        return this.transaction("readwrite", (store) => store.put(p));
    }

    remove(id) {
        return this.transaction("readwrite", (store) => store.delete(id));
    }
}

// The store of the projects: IndexedDB if available, otherwise in memory. In
// memory too if IndexedDB doesn't answer within 'timeout' ms, not to leave
// the page without projects.
export async function openStore(timeout = 3000) {
    if (typeof indexedDB === "undefined")
        return new MemoryStore();
    let timer;
    const expired = new Promise((resolve, reject) => {
        timer = setTimeout(() => reject(new Error("IndexedDB didn't answer.")), timeout);
    });
    try {
        return await Promise.race([IndexedDbStore.open(), expired]);
    } catch (error) {
        console.error(error);
        return new MemoryStore();
    } finally {
        clearTimeout(timer);
    }
}
