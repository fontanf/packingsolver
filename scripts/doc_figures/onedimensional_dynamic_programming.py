"""Figures of the detailed description of the dynamic programming of
onedimensional.

Usage, from the root of the repository:

    python3 scripts/doc_figures/onedimensional_dynamic_programming.py
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import figure
from figure import BLUE, GREEN, YELLOW, RED

SVG_DIR = "doc/img"
PREFIX = "onedimensional_dynamic_programming_"


def binary_decomposition(copies):
    chunks, total, chunk = [], 0, 1
    while total < copies:
        chunk = min(chunk, copies - total)
        chunks.append(chunk)
        total += chunk
        chunk *= 2
    return chunks


def figure_binary_decomposition():
    fig = figure.Figure(unit=22)
    copies = 13
    chunks = binary_decomposition(copies)
    styles = [BLUE, GREEN, YELLOW, RED]
    # The copies of the item type.
    for c in range(copies):
        fig.rect(c * 1.1, 2.5, 1, 1, "", BLUE)
    fig.text(copies * 1.1 / 2, 4.1, "13 copies of an item type", w=10, size=12)
    # The items of the 0-1 knapsack problem.
    x = 0
    for k, chunk in enumerate(chunks):
        fig.rect(x, 0, chunk * 1.1 - 0.1, 1, str(chunk), styles[k % len(styles)])
        x += chunk * 1.1 + 0.4
    fig.text(x / 2, -0.7, "0-1 knapsack items: " + " + ".join(str(c) for c in chunks)
             + " = {}".format(copies), w=14, size=12)
    fig.write(SVG_DIR + "/" + PREFIX + "binary_decomposition.svg")


if __name__ == "__main__":
    figure_binary_decomposition()
