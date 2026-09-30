"""Problem type 'irregular': 2D polygons, circular arcs and shapes with holes."""

from ._packingsolver import (
    InstanceFormat,
    LinearProgrammingSolver,
    Objective,
    OptimizationMode,
)
from ._packingsolver.irregular import *  # noqa: F401,F403

from . import _visualize


def visualize(solution, **kwargs):
    """Return a plotly figure of 'solution', without writing any file.

    Requires plotly ('pip install packingsolver[visualization]'). The keyword
    arguments are those of 'packingsolver.visualize.irregular.figure'.
    """
    return _visualize.solution_figure("irregular", solution, **kwargs)
