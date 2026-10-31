"""Figures of the detailed description of the MILP assignment of
onedimensional.

Usage, from the root of the repository:

    python3 scripts/doc_figures/onedimensional_milp_assignment.py
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import figure
from figure import BLUE, GREEN, YELLOW, GRAY

SVG_DIR = "doc/img"
PREFIX = "onedimensional_milp_assignment_"


def figure_symmetry_breaking():
    """Labels of the items, and maximum number of copies of each item type in
    the bin of each rank."""
    fig = figure.Figure(unit=26)
    # Item types sorted by increasing number of copies.
    item_types = [("A", 1, BLUE), ("B", 2, GREEN), ("C", 3, YELLOW)]
    number_of_bins = 5
    # Labels.
    label, x = 1, 0
    cumulative = {}
    for name, copies, style in item_types:
        for c in range(copies):
            fig.rect(x, 0, 1, 1, "{}{}".format(name, label), style)
            x += 1.1
            label += 1
        cumulative[name] = label - 1
    fig.text(x / 2, 1.6, "labels of the items, item types by increasing number of copies", w=14, size=12)
    # Table of the bounds.
    oy = -2.5
    for j, (name, copies, style) in enumerate(item_types):
        fig.text(2.5 + j * 2, oy + 0.6, name, w=1.5, size=12, extra="fontStyle=1;")
    for k in range(number_of_bins):
        y = oy - (k + 1) * 1.1
        fig.text(0, y + 0.5, "bin {}".format(k), w=2, size=12)
        for j, (name, copies, style) in enumerate(item_types):
            bound = min(copies, max(0, cumulative[name] - k))
            fig.rect(1.9 + j * 2, y, 1.2, 1, str(bound), style if bound > 0 else GRAY)
    fig.text(4.5, oy - (number_of_bins + 1) * 1.1 - 0.1,
             "maximum number of copies in the bin of each rank", w=12, size=12)
    fig.write(SVG_DIR + "/" + PREFIX + "symmetry_breaking.svg")


if __name__ == "__main__":
    figure_symmetry_breaking()
