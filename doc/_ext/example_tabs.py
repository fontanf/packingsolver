"""The 'example-tabs' directive: examples of 'doc/examples' in tabs.

    .. example-tabs:: rectangle/defects_no rectangle/defects_yes

shows the examples '<problem type>/<name>' (listed, with their titles, in
'doc/examples/<problem type>/examples.json') in a set of tabs, synchronized
with the other tabs of the documentation:

* Online solver: a link to each example in the online solver
  ('?example=<problem type>/<name>', see 'scripts/web_examples.py');
* JSON, Python and C++: the instance of each example ('instance.json', and
  'instance.py' and 'instance.cpp' written by
  'scripts/generate_doc_snippets.py').

With the ':solve:' option, the tabs also show how to solve the instance:
the command line in the JSON tab, the solve code in the Python and C++ tabs.
"""

import json
import os

from docutils import nodes
from docutils.parsers.rst import Directive, directives
from docutils.statemachine import StringList

ONLINE_SOLVER = "https://packingsolver.pages.dev/"

# The alias of the Python module of each problem type (see
# 'scripts/generate_doc_snippets.py').
ALIASES = {
    "rectangleguillotine": "psg",
    "rectangle": "psr",
    "box": "psb",
    "boxstacks": "psbs",
    "onedimensional": "pso",
    "irregular": "psi",
}


def solve_lines(problem_type, sync):
    """The lines which solve the instance, in the tab 'sync'."""
    certificate = "solution.json" if problem_type == "irregular" else "solution.csv"
    if sync == "json":
        return [
            "Solve it with the command-line solver:",
            "",
            ".. code-block:: shell",
            "",
            f"    packingsolver_{problem_type} \\",
            "            --input instance.json \\",
            f"            --certificate {certificate} \\",
            "            --time-limit 5",
        ]
    if sync == "python":
        alias = ALIASES[problem_type]
        return [
            "Solve it:",
            "",
            ".. code-block:: python",
            "",
            f"    parameters = {alias}.OptimizeParameters()",
            "    parameters.time_limit = 5",
            f"    output = {alias}.optimize(instance, parameters)",
            f'    output.solution.write("{certificate}")',
        ]
    return [
        "Solve it:",
        "",
        ".. code-block:: cpp",
        "",
        f'    #include "packingsolver/{problem_type}/optimize.hpp"',
        "",
        "    OptimizeParameters parameters;",
        "    parameters.timer.set_time_limit(5);",
        f"    {problem_type}::Output output = optimize(instance, parameters);",
        f'    output.solution_pool.best().write("{certificate}");',
        "",
        f"Link the target ``PackingSolver::{problem_type}`` of the CMake project.",
    ]


LANGUAGES = [
    ("JSON", "json", "instance.json", "json"),
    ("Python", "python", "instance.py", "python"),
    ("C++", "cpp", "instance.cpp", "cpp"),
]


def example_title(source_dir, reference):
    problem_type, name = reference.split("/")
    path = os.path.join(source_dir, "examples", problem_type, "examples.json")
    with open(path) as f:
        listed = json.load(f)
    for example in listed:
        if example["name"] == name:
            return example["title"]
    raise ValueError(f"example-tabs: no example {reference} in {path}")


class ExampleTabs(Directive):

    required_arguments = 1
    optional_arguments = 10
    option_spec = {"solve": directives.flag}

    def run(self):
        env = self.state.document.settings.env
        references = self.arguments
        titles = [example_title(env.srcdir, reference) for reference in references]
        for reference in references:
            problem_type, _ = reference.split("/")
            env.note_dependency(os.path.join(env.srcdir, "examples", problem_type, "examples.json"))

        links = ", ".join(
            f"`{title} <{ONLINE_SOLVER}?example={reference}>`__"
            for reference, title in zip(references, titles))
        solve = "solve" in self.options
        text = "Open the example in the online solver: " if len(references) == 1 \
            else "Open the examples in the online solver: "
        lines = [
            ".. tab-set::",
            "   :sync-group: interface",
            "",
            "   .. tab-item:: Online solver",
            "      :sync: web",
            "",
            f"      {text}{links}" + (", then click **Solve**." if solve else "."),
            "",
        ]
        for label, sync, file_name, language in LANGUAGES:
            lines += [
                f"   .. tab-item:: {label}",
                f"      :sync: {sync}",
                "",
            ]
            for reference, title in zip(references, titles):
                lines += [
                    f"      .. literalinclude:: /examples/{reference}/{file_name}",
                    f"         :language: {language}",
                    f"         :caption: {title}",
                    "",
                ]
            if solve:
                problem_type = references[0].split("/")[0]
                lines += ["      " + line if line else "" for line in solve_lines(problem_type, sync)]
                lines.append("")
        node_list = []
        content = StringList(lines, source=self.state.document.current_source)
        container = nodes.container()
        self.state.nested_parse(content, self.content_offset, container)
        node_list.extend(container.children)
        return node_list


def setup(app):
    app.add_directive("example-tabs", ExampleTabs)
    return {"parallel_read_safe": True, "parallel_write_safe": True}
