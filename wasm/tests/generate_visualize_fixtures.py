"""Generate the expected figures of the JavaScript visualizers.

For each problem type, 'fixtures/visualize/<type>/cases.json' lists cases:

    [{"name": ..., "certificate": <path from the repository root>,
      "options": {<keyword arguments>}, "function": "figure"}, ...]

('function' is optional, "figure" by default.) For each case, this script
calls the Python visualizer ('python/packingsolver/visualize/<type>.py') and
writes 'fixtures/visualize/<type>/<name>.json' with the certificate, the
options and the figure ('to_json()', without the template), which the
JavaScript visualizer must reproduce (see 'visualize.test.mjs').

Requires plotly (and numpy for irregular). Usage:

    python3 wasm/tests/generate_visualize_fixtures.py [type ...]
"""

import importlib.util
import io
import json
import os
import sys

TESTS_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.join(TESTS_DIR, "..", "..")
FIXTURES_DIR = os.path.join(TESTS_DIR, "fixtures", "visualize")
VISUALIZE_DIR = os.path.join(ROOT_DIR, "python", "packingsolver", "visualize")


def load_visualizer(problem_type):
    # Loaded by path: the 'packingsolver' package needs the compiled module.
    spec = importlib.util.spec_from_file_location(
            "visualize_" + problem_type,
            os.path.join(VISUALIZE_DIR, problem_type + ".py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def generate(problem_type):
    directory = os.path.join(FIXTURES_DIR, problem_type)
    with open(os.path.join(directory, "cases.json")) as file:
        cases = json.load(file)
    visualizer = load_visualizer(problem_type)
    for case in cases:
        with open(os.path.join(ROOT_DIR, case["certificate"])) as file:
            certificate = file.read()
        function = getattr(visualizer, case.get("function", "figure"))
        options = case.get("options", {})
        figure = json.loads(function(io.StringIO(certificate), **options).to_json())
        figure["layout"].pop("template", None)
        fixture = {
            "function": case.get("function", "figure"),
            "certificate": certificate,
            "options": options,
            "figure": figure,
        }
        with open(os.path.join(directory, case["name"] + ".json"), "w") as file:
            json.dump(fixture, file)
        print(problem_type, case["name"])


if __name__ == "__main__":
    problem_types = sys.argv[1:] or sorted(
            name for name in os.listdir(FIXTURES_DIR)
            if os.path.isdir(os.path.join(FIXTURES_DIR, name)))
    for problem_type in problem_types:
        generate(problem_type)
