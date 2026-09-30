"""Problem type 'boxstacks': 3D boxes stacked into same-footprint stacks."""

from ._packingsolver import (
    InstanceFormat,
    LinearProgrammingSolver,
    Objective,
    OptimizationMode,
)
# Shared with the 'rectangle' problem type.
from ._packingsolver.rectangle import (
    Defect,
    Direction,
    UnloadingConstraint,
)
from ._packingsolver.boxstacks import *  # noqa: F401,F403

from . import _visualize


def visualize(solution, **kwargs):
    """Return a plotly figure of 'solution', without writing any file.

    Requires plotly ('pip install packingsolver[visualization]'). The keyword
    arguments are those of 'packingsolver.visualize.boxstacks.figure'.
    """
    return _visualize.solution_figure("boxstacks", solution, **kwargs)
