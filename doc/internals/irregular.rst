.. _internals_irregular:

:code:`irregular` algorithms
============================

See :ref:`irregular<irregular>` for the input/output format and CLI usage of this solver.

Trivial single item
-----------------------

This algorithm solves the :code:`feasibility` objective.

This is not an algorithm aimed at generating real patterns. It is aimed at providing a fast debug routine to check if an item fits in at least one bin.

It tries to place the item by aligning the center of its bounding box with the center of the bin's bounding box, then validates (and, if needed, adjusts) the placement against the bin's actual shape.

Tree search
------------

This algorithm solves the :code:`feasibility`, :code:`knapsack`, :code:`bin-packing`, :code:`bin-packing-with-leftovers`, :code:`open-dimension-x`, :code:`open-dimension-y` objectives.

Branch-and-bound style search over partial packings (option ``--use-tree-search``).

Items are approximated by unions of trapezoids to make overlap and containment checks tractable, and placed one at a time, guided by one or more configurable guides. Supports item rotations. Can run as an anytime improvement process, or with a fixed exploration budget and a maximum approximation ratio in non-anytime modes.

MILP raster
--------------

This algorithm solves the :code:`feasibility` and :code:`knapsack` objectives.

Approximate algorithm (option ``--use-milp-raster``).

Rasterizes the bin (and items) onto a grid and solves the resulting placement problem as a mixed-integer linear program. Trades exactness (the raster is an approximation of the true continuous shapes) for the ability to use a general-purpose MILP solver.

Local search
--------------

This algorithm solves the :code:`feasibility`, :code:`bin-packing`, :code:`bin-packing-with-leftovers`, :code:`open-dimension-x`, :code:`open-dimension-y` and :code:`open-dimension-xy` objectives.

Improvement heuristic (option ``--use-local-search``).

Starting from an existing solution, iteratively applies local moves (e.g. small translations/rotations of items, swaps) to try to improve it further.

One-dimensional bound
------------------------

This algorithm computes a bound for the :code:`bin-packing`, :code:`knapsack` and :code:`variable-sized-bin-packing` objectives.

Bound-only method, run unconditionally (not user-selectable).

Relaxes the instance to a one-dimensional problem (each bin and item type keeps only its area, rounded to an integer length) and solves it, which is much cheaper than solving the irregular problem itself. The resulting bin-packing / knapsack / variable-sized-bin-packing bound is a valid bound for the original instance, since no irregular packing can use less total area than this relaxation requires.
