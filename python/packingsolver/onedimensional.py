"""Problem type 'onedimensional': 1D items (e.g. bar/rod cutting)."""

from ._packingsolver import (
    InstanceFormat,
    LinearProgrammingSolver,
    Objective,
    OptimizationMode,
)
from ._packingsolver.onedimensional import *  # noqa: F401,F403

from . import _visualize


def visualize(solution, **kwargs):
    """Return a plotly figure of 'solution', without writing any file.

    Requires plotly ('pip install packingsolver[visualization]'). The keyword
    arguments are those of 'packingsolver.visualize.onedimensional.figure'.
    """
    return _visualize.solution_figure("onedimensional", solution, **kwargs)
