"""'OptimizeParameters' behaviours shared by every problem type."""

import gc
import importlib
import weakref

import pytest


PROBLEM_TYPES = [
    "rectangleguillotine",
    "rectangle",
    "box",
    "boxstacks",
    "onedimensional",
    "irregular",
]


@pytest.mark.parametrize("problem_type", PROBLEM_TYPES)
def test_new_solution_callback_reference_cycle_is_collected(problem_type):
    """A callback referencing its parameters (e.g. through a closure or the
    module globals) creates a reference cycle, which the garbage collector
    must be able to collect."""
    ps = importlib.import_module("packingsolver." + problem_type)

    class Holder:
        pass

    def make_cycle():
        holder = Holder()
        holder.parameters = ps.OptimizeParameters()
        holder.parameters.new_solution_callback = lambda output: holder
        return weakref.ref(holder)

    holder_ref = make_cycle()
    gc.collect()
    assert holder_ref() is None
