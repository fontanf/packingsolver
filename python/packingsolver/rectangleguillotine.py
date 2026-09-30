"""Problem type 'rectangleguillotine': 2D rectangles, guillotine cuts only."""

from ._packingsolver import (
    InstanceFormat,
    LinearProgrammingSolver,
    Objective,
    OptimizationMode,
)
from ._packingsolver.rectangleguillotine import *  # noqa: F401,F403

from . import _visualize


def visualize(solution, **kwargs):
    """Return a plotly figure of 'solution', without writing any file.

    Requires plotly ('pip install packingsolver[visualization]'). The keyword
    arguments are those of 'packingsolver.visualize.rectangleguillotine.figure'.
    """
    return _visualize.solution_figure("rectangleguillotine", solution, **kwargs)
