.. _internals_boxstacks:

:code:`box-stacks` algorithms
=============================

See :ref:`boxstacks<boxstacks>` for the input/output format and CLI usage of this solver.

Sequential one-dimensional / rectangle
------------------------------------------

This algorithm applies to any objective, whenever the instance has a single bin.

Default algorithm for single-bin instances. Decomposes the problem in two phases:

1. Group items into stacks by solving a one-dimensional variable-sized bin packing problem (each stack is a "bin" whose length is limited by the stacking constraints).
2. Pack the resulting stacks into the bin's footprint by solving a rectangle knapsack problem.

This decomposition is much cheaper than the full tree search below, and is designed for instances where axle/weight constraints don't force a sparse packing. It is always run first.

.. image:: ../img/boxstacks_sequential_onedimensional_rectangle.png
   :scale: 100%
   :align: center

Tree search
------------

This algorithm applies to any objective, whenever the instance has a single bin.

Branch-and-bound style search over partial packings of items into stacked columns. Only run as a fallback, when the two-phase decomposition above looks insufficient: specifically, when it did not already pack every item, when it violated axle weight constraints at some point, and when there remains an unpacked item type light enough to still fit in the remaining weight budget.

.. image:: ../img/boxstacks_tree_search.png
   :scale: 100%
   :align: center

The tree search is otherwise more expensive than the two-phase decomposition, so it is skipped whenever the simpler algorithm's result already looks good enough.
