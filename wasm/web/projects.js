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
