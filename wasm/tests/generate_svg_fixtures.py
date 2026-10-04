"""Generate the SVG files of the tests of the SVG reader ('wasm/web/svg.js').

Each file 'fixtures/svg/<name>.svg' comes with 'fixtures/svg/<name>.json':
either the parts the reader must find (for each part, in reading order: top
to bottom, then left to right, its area, number of holes and bounding box
size, computed analytically), the units and the number of warnings; or the
error it must raise ('error': a regular expression).

Usage:

    python3 wasm/tests/generate_svg_fixtures.py
"""

import json
import math
import os

DIRECTORY = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fixtures", "svg")

HEADER = '<?xml version="1.0" encoding="UTF-8"?>\n'


def save(name, svg, expected):
    os.makedirs(DIRECTORY, exist_ok=True)
    with open(os.path.join(DIRECTORY, name + ".svg"), "w") as file:
        file.write(HEADER + svg.strip() + "\n")
    with open(os.path.join(DIRECTORY, name + ".json"), "w") as file:
        json.dump(expected, file, indent=4)
        file.write("\n")
    print(name)


def part(area, holes, width, height):
    return {"area": area, "holes": holes, "width": width, "height": height}


def parts(parts, units="millimeters", warnings=0):
    return {"parts": parts, "units": units, "warnings": warnings}


def error(pattern):
    return {"error": pattern}


# As saved by Inkscape: size in millimeters, one user unit per millimeter.
save("inkscape_mm", '''
<svg xmlns="http://www.w3.org/2000/svg" xmlns:inkscape="http://www.inkscape.org/namespaces/inkscape"
     width="210mm" height="297mm" viewBox="0 0 210 297">
  <!-- A plate with a hole made of two subpaths of one path, and a circular
       hole drawn as a separate element. -->
  <g inkscape:label="Layer 1" inkscape:groupmode="layer" transform="translate(10,20)">
    <path d="M 0,0 H 60 V 40 H 0 Z M 10,10 h 10 v 10 h -10 z" style="fill:none;stroke:#000000"/>
    <circle cx="45" cy="20" r="5" style="fill:none;stroke:#ff0000"/>
  </g>
</svg>''', parts([part(60 * 40 - 100 - math.pi * 25, 2, 60, 40)]))

# Without a view box: user units are pixels (96 per inch).
save("pixels", '''
<svg xmlns="http://www.w3.org/2000/svg" width="400" height="300">
  <rect x="10" y="10" width="96" height="48"/>
</svg>''', parts([part(25.4 * 12.7, 0, 25.4, 12.7)]))

# Scaled view box: 2 user units per millimeter.
save("scaled_view_box", '''
<svg xmlns="http://www.w3.org/2000/svg" width="100mm" height="50mm" viewBox="0 0 200 100">
  <circle cx="50" cy="50" r="20"/>
</svg>''', parts([part(math.pi * 100, 0, 20, 20)]))

# Transformations: rotation, uniform scaling, matrix, nested groups, clones.
save("transforms", '''
<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink"
     width="200mm" height="200mm" viewBox="0 0 200 200">
  <defs>
    <polygon id="triangle" points="0,0 20,0 0,20"/>
  </defs>
  <g transform="translate(20 20)">
    <!-- Rotated by 90 degrees: 10 x 30. -->
    <rect x="0" y="0" width="30" height="10" transform="rotate(90 15 5)"/>
  </g>
  <g transform="translate(100,20) scale(2)">
    <!-- Rounded rectangle, scaled: 40 x 20, corner radius 6. -->
    <rect x="0" y="0" width="20" height="10" rx="3"/>
  </g>
  <!-- matrix(a b c d e f): rotation by 90 degrees and translation. -->
  <polygon points="0,0 30,0 30,10 0,10" transform="matrix(0 1 -1 0 60 100)"/>
  <!-- Clones of the triangle. -->
  <use xlink:href="#triangle" x="100" y="100"/>
  <use href="#triangle" transform="translate(150 100) scale(1.5)"/>
</svg>''', parts([
    part(300, 0, 10, 30),
    part(40 * 20 - (4 - math.pi) * 36, 0, 40, 20),
    part(300, 0, 10, 30),
    part(200, 0, 20, 20),
    part(450, 0, 30, 30),
]))

# Paths: relative and absolute commands, H and V, arcs with each flag
# combination, flags without separators.
save("arcs", '''
<svg xmlns="http://www.w3.org/2000/svg" width="200mm" height="100mm" viewBox="0 0 200 100">
  <!-- A slot: two half circles of radius 5, the flags without separators
       ("010 10": flags 0 and 1, then 0 and 10). -->
  <path d="m15 10h30a5 5 0 010 10H15a5 5 0 01 0-10z"/>
  <!-- Large arc: a disk of radius 10 with a 90-degree notch cut out. -->
  <path d="M 110,20 L 110,10 A 10,10 0 1 1 100,20 Z"/>
  <!-- Small arc bulging inward (sweep 0): a square of side 20 minus a
       circular segment. -->
  <path d="M 150,10 L 170,10 L 170,30 L 150,30 A 20,20 0 0 0 150,10 Z"/>
</svg>''', parts([
    part(300 + math.pi * 25, 0, 40, 10),
    part(math.pi * 100 * 3 / 4, 0, 20, 20),
    part(400 - (400 / 2) * (math.pi / 3 - math.sin(math.pi / 3)), 0, 20, 20),
]))

# Orientation: an "L" (its foot at the bottom right) and a "D" (bulging to
# the right), which must look the same as in SVG editors.
save("orientation", '''
<svg xmlns="http://www.w3.org/2000/svg" width="100mm" height="100mm" viewBox="0 0 100 100">
  <path d="M 0,0 H 10 V 20 H 30 V 30 H 0 Z"/>
  <path d="M 50,0 V 20 A 10,10 0 0 0 50,0 Z"/>
</svg>''', parts([part(500, 0, 30, 30), part(math.pi * 50, 0, 10, 20)]))

# Ignored: texts, images, unused definitions, hidden elements. An open
# contour gives a warning.
save("ignored", '''
<svg xmlns="http://www.w3.org/2000/svg" width="100mm" height="100mm" viewBox="0 0 100 100">
  <title>Parts</title>
  <defs><rect id="unused" width="5" height="5"/></defs>
  <text x="0" y="90">Part A</text>
  <image href="photo.png" x="0" y="0" width="10" height="10"/>
  <rect x="50" y="50" width="10" height="10" style="display: none"/>
  <g display="none"><circle cx="0" cy="0" r="3"/></g>
  <rect x="0" y="0" width="20" height="10"/>
  <polyline points="40,0 60,0 60,10"/>
</svg>''', parts([part(200, 0, 20, 10)], warnings=1))

# Unsupported: the file is rejected.
save("bezier", '''
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100">
  <path id="curve" d="M 0,0 C 10,20 30,20 40,0 Z"/>
</svg>''', error('Bézier curves are not supported \\(path "curve"\\)'))
save("quadratic_bezier", '''
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100">
  <path d="M 0,0 q 10,20 20,0 z"/>
</svg>''', error("Bézier curves are not supported \\(path\\)"))
save("ellipse", '''
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100">
  <ellipse cx="50" cy="50" rx="20" ry="10"/>
</svg>''', error("Ellipses are not supported"))
save("elliptical_arc", '''
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100">
  <path d="M 0,0 A 20,10 0 0 1 40,0 Z"/>
</svg>''', error("Elliptical arcs are not supported"))
save("elliptical_corners", '''
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100">
  <rect width="40" height="20" rx="5" ry="3"/>
</svg>''', error("Rounded corners with different radii"))
save("scaled_circle", '''
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100">
  <circle cx="10" cy="10" r="5" transform="scale(2 1)"/>
</svg>''', error("Arcs scaled non-uniformly"))
