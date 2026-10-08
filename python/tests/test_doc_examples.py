"""The Python versions of the examples of the documentation.

Each 'instance.py' of the examples listed in
'doc/examples/<problem type>/examples.json' (written by
'scripts/generate_doc_snippets.py') must build the same instance as the
'instance.json' next to it.
"""

import importlib
import json
import os

import pytest


EXAMPLES_DIR = os.path.join(os.path.dirname(__file__), "..", "..", "doc", "examples")


def examples():
    if not os.path.isdir(EXAMPLES_DIR):
        return []
    found = []
    for problem_type in sorted(os.listdir(EXAMPLES_DIR)):
        list_path = os.path.join(EXAMPLES_DIR, problem_type, "examples.json")
        if not os.path.isfile(list_path):
            continue
        with open(list_path) as f:
            for example in json.load(f):
                found.append((problem_type, example["name"]))
    return found


EXAMPLES = examples()


def written(module, instance, path):
    if module.__name__.endswith("irregular"):
        instance.write(str(path))
    else:
        instance.write(str(path), module.InstanceFormat.Json)
    with open(path) as f:
        return json.load(f)


@pytest.mark.skipif(not EXAMPLES, reason="no documentation examples")
@pytest.mark.parametrize(
    "problem_type,name", EXAMPLES,
    ids=[f"{problem_type}/{name}" for problem_type, name in EXAMPLES])
def test_doc_example(problem_type, name, tmp_path):
    module = importlib.import_module("packingsolver." + problem_type)
    directory = os.path.join(EXAMPLES_DIR, problem_type, name)
    with open(os.path.join(directory, "instance.py")) as f:
        namespace = {}
        exec(f.read(), namespace)
    instance_builder = module.InstanceBuilder()
    instance_builder.read(os.path.join(directory, "instance.json"))
    expected = instance_builder.build()
    assert written(module, namespace["instance"], tmp_path / "snippet.json") \
        == written(module, expected, tmp_path / "expected.json")
