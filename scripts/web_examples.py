"""Write the examples of the documentation for the web page.

The examples of a problem type are listed, with their titles, in
'doc/examples/<problem type>/examples.json'; each one is the instance
'doc/examples/<problem type>/<name>/instance.json'. They are written in a
single file, 'examples.json' of the web page:

    {"<problem type>": [{"name": ..., "title": ..., "instance": {...}}, ...]}

The page opens the example '<name>' of '<problem type>' at the URL
'?example=<problem type>/<name>', which the documentation links to.

Usage:

    python3 scripts/web_examples.py site/examples.json
"""

import argparse
import json
import os

EXAMPLES_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "doc", "examples")


def web_examples(examples_dir=EXAMPLES_DIR):
    examples = {}
    for problem_type in sorted(os.listdir(examples_dir)):
        list_path = os.path.join(examples_dir, problem_type, "examples.json")
        if not os.path.isfile(list_path):
            continue
        with open(list_path) as f:
            listed = json.load(f)
        examples[problem_type] = []
        for example in listed:
            with open(os.path.join(examples_dir, problem_type, example["name"], "instance.json")) as f:
                instance = json.load(f)
            examples[problem_type].append({
                "name": example["name"],
                "title": example["title"],
                "instance": instance,
            })
    return examples


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("output", help="path of the file to write")
    args = parser.parse_args()
    with open(args.output, "w") as f:
        json.dump(web_examples(), f, separators=(",", ":"))


if __name__ == "__main__":
    main()
