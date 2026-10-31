"""Figures of the description of the rectangleguillotine problem type
(doc/rectangleguillotine.rst).

Usage, from the root of the repository:

    python3 scripts/doc_figures/rectangleguillotine.py
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import figure
from figure import BLUE

SVG_DIR = "doc/img"
PREFIX = "rectangleguillotine_"


class Plate:
    """Plate of a cut tree: its rectangle, and either an item, or its
    sub-plates (cut vertically at odd stages, horizontally at even stages), or
    nothing (waste)."""

    def __init__(self, x, y, w, h, item=None, children=()):
        self.x, self.y, self.w, self.h = x, y, w, h
        self.item, self.children = item, list(children)

    def items(self):
        if self.item is not None:
            return [self]
        return [p for child in self.children for p in child.items()]

    def depth(self):
        return 1 + max((child.depth() for child in self.children), default=0)


# A 4-staged pattern in a 16 x 12 bin.
PATTERN = Plate(0, 0, 16, 12, children=[
    Plate(0, 0, 8, 12, children=[
        Plate(0, 0, 8, 6, item="0"),
        Plate(0, 6, 8, 5, children=[
            Plate(0, 6, 4, 5, item="1"),
            Plate(4, 6, 3, 5, children=[
                Plate(4, 6, 3, 3, item="2"),
                Plate(4, 9, 3, 2)]),
            Plate(7, 6, 1, 5)]),
        Plate(0, 11, 8, 1)]),
    Plate(8, 0, 6, 12, children=[
        Plate(8, 0, 6, 10, item="3"),
        Plate(8, 10, 6, 2)]),
    Plate(14, 0, 2, 12)])


def figure_number_of_stages():
    """The pattern, and its cut tree: each sub-plate is drawn with its items,
    one stage per level."""
    fig = figure.Figure(unit=10)
    gap = 2
    level = 16
    # x of each plate: the leaves side by side, a parent centered above its
    # children.
    positions = {}
    next_x = [0]

    def layout(plate, stage):
        if not plate.children or stage == PATTERN.depth() - 1:
            positions[id(plate)] = next_x[0]
            next_x[0] += plate.w + gap
            for child in plate.children:
                layout(child, stage + 1)
            return
        for child in plate.children:
            layout(child, stage + 1)
        first, last = plate.children[0], plate.children[-1]
        center = (positions[id(first)] + first.w / 2 + positions[id(last)] + last.w / 2) / 2
        positions[id(plate)] = center - plate.w / 2

    def draw_plate(plate, ox, oy):
        """The plate, its items and the edges of its sub-plates."""
        for p in plate.items():
            fig.rect(ox + p.x - plate.x, oy + p.y - plate.y, p.w, p.h, p.item, BLUE)

        def edges(q):
            fig.rect(ox + q.x - plate.x, oy + q.y - plate.y, q.w, q.h, "", "fillColor=none;strokeColor=#000000;")
            for child in q.children:
                edges(child)
        edges(plate)

    def draw(plate, stage):
        ox, oy = positions[id(plate)], -stage * level - plate.h
        draw_plate(plate, ox, oy)
        for child in plate.children:
            cx = positions[id(child)] + child.w / 2
            middle = -stage * level - plate.h - 2
            fig.polyline([(ox + plate.w / 2, oy), (ox + plate.w / 2, middle), (cx, middle),
                          (cx, -(stage + 1) * level)], arrow=True)
            draw(child, stage + 1)

    layout(PATTERN, 0)
    draw(PATTERN, 0)
    for stage in range(1, PATTERN.depth()):
        fig.text(-3, -stage * level - 1, "stage {}".format(stage), w=6, align="right")
    fig.write(SVG_DIR + "/" + PREFIX + "number_of_stages.svg")


if __name__ == "__main__":
    figure_number_of_stages()
