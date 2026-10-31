"""Minimal writer of draw.io diagrams for the figures of the documentation.

The figures are written as uncompressed .drawio files, so that they can still
be edited with draw.io, and exported to PNG with the draw.io command-line
interface.

Coordinates are given in a mathematical frame (y going up) in arbitrary units;
a Figure converts them into draw.io pixels.
"""

import html
import os
import subprocess

# Colors of the existing figures of the documentation.
BLUE = "fillColor=#dae8fc;strokeColor=#6c8ebf;"
RED = "fillColor=#f8cecc;strokeColor=#b85450;"
GREEN = "fillColor=#d5e8d4;strokeColor=#82b366;"
YELLOW = "fillColor=#fff2cc;strokeColor=#d6b656;"
GRAY = "fillColor=#f5f5f5;strokeColor=#666666;fontColor=#333333;"
DEFECT = "fillColor=#999999;strokeColor=#333333;fontColor=#000000;fillStyle=hatch;labelBackgroundColor=#ffffff;"
WHITE = "fillColor=#ffffff;strokeColor=#000000;"
NONE = "fillColor=none;"


class Figure:

    def __init__(self, unit=40):
        self.unit = unit
        self.cells = []
        self.next_id = 2

    def _id(self):
        self.next_id += 1
        return "c" + str(self.next_id)

    def _x(self, x):
        return round(x * self.unit, 2)

    def _y(self, y):
        # draw.io's y axis goes down.
        return round(-y * self.unit, 2)

    def rect(self, x, y, w, h, label="", style=BLUE, extra=""):
        """Rectangle whose bottom-left corner is (x, y)."""
        self.cells.append(
                '<mxCell id="{}" value="{}" style="rounded=0;whiteSpace=wrap;html=1;{}{}" vertex="1" parent="1">'
                '<mxGeometry x="{}" y="{}" width="{}" height="{}" as="geometry"/></mxCell>'.format(
                    self._id(), html.escape(str(label), quote=True), style, extra,
                    self._x(x), self._y(y + h), round(w * self.unit, 2), round(h * self.unit, 2)))

    def bin(self, x, y, w, h):
        self.rect(x, y, w, h, "", WHITE + "strokeWidth=2;fillColor=none;")

    def text(self, x, y, label, w=4, h=0.6, align="center", size=12, extra=""):
        """Text box centered on (x, y), or starting at x if align is 'left'."""
        x0 = x - w / 2 if align == "center" else (x - w if align == "right" else x)
        self.cells.append(
                '<mxCell id="{}" value="{}" style="text;html=1;align={};verticalAlign=middle;'
                'whiteSpace=wrap;fontSize={};{}" vertex="1" parent="1">'
                '<mxGeometry x="{}" y="{}" width="{}" height="{}" as="geometry"/></mxCell>'.format(
                    self._id(), html.escape(str(label), quote=True), align, size, extra,
                    self._x(x0), self._y(y + h / 2), round(w * self.unit, 2), round(h * self.unit, 2)))

    def polyline(self, points, style="strokeColor=#000000;", arrow=False):
        """Polyline through 'points'; with an arrow at its end if 'arrow'."""
        (x1, y1), (x2, y2) = points[0], points[-1]
        waypoints = "".join(
                '<mxPoint x="{}" y="{}"/>'.format(self._x(x), self._y(y))
                for x, y in points[1:-1])
        end = "endArrow=classic;" if arrow else "endArrow=none;"
        self.cells.append(
                '<mxCell id="{}" style="html=1;rounded=0;{}{}" edge="1" parent="1">'
                '<mxGeometry relative="1" as="geometry">'
                '<mxPoint x="{}" y="{}" as="sourcePoint"/>'
                '<mxPoint x="{}" y="{}" as="targetPoint"/>'
                '<Array as="points">{}</Array>'
                '</mxGeometry></mxCell>'.format(
                    self._id(), end, style,
                    self._x(x1), self._y(y1), self._x(x2), self._y(y2), waypoints))

    def arrow(self, x1, y1, x2, y2, style="strokeColor=#000000;"):
        self.polyline([(x1, y1), (x2, y2)], style, arrow=True)

    def big_arrow(self, x, y, length=1.2, direction="down"):
        """Block arrow, as in the existing figures, starting at (x, y)."""
        if direction == "down":
            self.cells.append(
                    '<mxCell id="{}" value="" style="shape=flexArrow;endArrow=classic;html=1;" edge="1" parent="1">'
                    '<mxGeometry width="50" height="50" relative="1" as="geometry">'
                    '<mxPoint x="{}" y="{}" as="sourcePoint"/><mxPoint x="{}" y="{}" as="targetPoint"/>'
                    '</mxGeometry></mxCell>'.format(
                        self._id(), self._x(x), self._y(y), self._x(x), self._y(y - length)))
        else:
            self.cells.append(
                    '<mxCell id="{}" value="" style="shape=flexArrow;endArrow=classic;html=1;" edge="1" parent="1">'
                    '<mxGeometry width="50" height="50" relative="1" as="geometry">'
                    '<mxPoint x="{}" y="{}" as="sourcePoint"/><mxPoint x="{}" y="{}" as="targetPoint"/>'
                    '</mxGeometry></mxCell>'.format(
                        self._id(), self._x(x), self._y(y), self._x(x + length), self._y(y)))

    def write(self, drawio_path, png_path):
        content = (
                '<mxfile host="drawio">\n'
                '  <diagram name="Page-1" id="page">\n'
                '    <mxGraphModel grid="0" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" '
                'fold="1" page="0" pageScale="1" math="0" shadow="0">\n'
                '      <root>\n'
                '        <mxCell id="0"/>\n'
                '        <mxCell id="1" parent="0"/>\n'
                + "".join("        " + cell + "\n" for cell in self.cells)
                + '      </root>\n'
                '    </mxGraphModel>\n'
                '  </diagram>\n'
                '</mxfile>\n')
        with open(drawio_path, "w") as f:
            f.write(content)
        export(drawio_path, png_path)


def export(drawio_path, png_path):
    """Export a .drawio file to PNG with the draw.io command-line interface."""
    subprocess.run(
            ["drawio", "--no-sandbox", "-x", "-f", "png", "-s", "1",
             "-b", "10", "-o", png_path, drawio_path],
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL)
    if not os.path.exists(png_path):
        raise RuntimeError("draw.io didn't write " + png_path)
