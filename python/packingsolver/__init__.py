"""PackingSolver: a solver for geometrical packing and cutting-stock problems.

Each problem type is a submodule mirroring the C++ namespace of the same
name::

    import packingsolver.rectangle as psr

    instance_builder = psr.InstanceBuilder()
    instance_builder.set_objective(psr.Objective.BinPacking)
    ...
    instance = instance_builder.build()
    output = psr.optimize(instance, psr.OptimizeParameters())
"""

from ._version import __version__
from ._packingsolver import (
    InstanceFormat,
    LinearProgrammingSolver,
    Objective,
    OptimizationMode,
)

__all__ = [
    "__version__",
    "InstanceFormat",
    "LinearProgrammingSolver",
    "Objective",
    "OptimizationMode",
]
