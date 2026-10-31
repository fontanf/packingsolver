"""Figure of the automatic selection of the algorithms
(doc/internals/algorithm_selection.rst).

Usage, from the root of the repository:

    python3 scripts/doc_figures/algorithm_selection.py
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import figure
from figure import WHITE

SVG_DIR = "doc/img"
PREFIX = "algorithm_selection_"

QUESTION = WHITE + "rounded=1;"
ANSWER = WHITE


def figure_variable_sized_bin_packing():
    """Selection for the variable-sized bin packing objective with several
    bin types (e.g. 'src/rectangle/optimize.cpp')."""
    fig = figure.Figure(unit=40)
    fig.text(5, 2.2, "Objective: variable-sized bin packing, several bin types", w=12)
    # Boxes: (x, y) of the center, width, label, style.
    boxes = {
        "items": (5, 0.5, 4, "How many items fit in a bin?", QUESTION),
        "few_items": (2, -1.5, 4, "Sequential value correction\nColumn generation", ANSWER),
        "copies": (8, -1.5, 5, "How many copies per item type?", QUESTION),
        "few_copies": (5, -3.5, 5, "Sequential single knapsack\nDichotomic search", ANSWER),
        "many_copies": (11, -3.5, 5, "Sequential single knapsack", ANSWER),
    }
    for x, y, w, label, style in boxes.values():
        fig.rect(x - w / 2, y - 0.5, w, 1, label, style)

    def connect(source, target, label):
        xs, ys = boxes[source][0], boxes[source][1] - 0.5
        xt, yt = boxes[target][0], boxes[target][1] + 0.5
        middle = (ys + yt) / 2
        fig.polyline([(xs, ys), (xs, middle), (xt, middle), (xt, yt)], arrow=True)
        fig.text((xs + xt) / 2, middle, label, w=1.2, h=0.4, size=11,
                 extra="labelBackgroundColor=#ffffff;")

    connect("items", "few_items", "few")
    connect("items", "copies", "many")
    connect("copies", "few_copies", "few")
    connect("copies", "many_copies", "many")
    fig.write(SVG_DIR + "/" + PREFIX + "variable_sized_bin_packing.svg")


if __name__ == "__main__":
    figure_variable_sized_bin_packing()
