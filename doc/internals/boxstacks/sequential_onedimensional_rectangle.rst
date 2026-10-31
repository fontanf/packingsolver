.. _internals_boxstacks_sequential_onedimensional_rectangle:

Sequential one-dimensional / rectangle
======================================

Objectives
----------

This algorithm solves the :code:`feasibility`, :code:`knapsack` and :code:`open-dimension-x` objectives with a single bin. With several bins, the generic algorithms (sequential single knapsack, sequential value correction) use it to solve their single-bin sub-problems.

Principle
---------

It decomposes the problem in two phases:

1. **Stacks.** The items are grouped into stacks by solving a :ref:`one-dimensional <internals_onedimensional>` variable-sized bin packing problem, for each group of items which can be stacked together (same stackability id, same unloading group). Each stack is a one-dimensional bin whose length is the height of the bin, and the one-dimensional instance carries the stacking constraints: maximum weight, maximum stackability, maximum weight above, nesting heights. The stacks generated contain exactly all the items.
2. **Footprints.** The stacks, described by their footprints, are packed into the floor of the bin by solving a :ref:`rectangle <internals_rectangle>` :code:`knapsack` problem (an :code:`open-dimension-x` problem for the :code:`open-dimension-x` objective), whose profits are the profits of the stacks. The rectangle problem carries the maximum weight of the bin and its axle weight constraints.

.. image:: ../../img/boxstacks_sequential_onedimensional_rectangle.png
   :scale: 100%
   :align: center

The rectangle problem is solved with the :ref:`rectangle tree search <internals_rectangle_tree_search>`, in a sequence of attempts:

* first trying to pack all the stacks, with guide 0, then guide 1, which give the best packings when the axle weight constraints are not critical;
* if the middle axle weight constraint is violated, with guide 8, which favors the nodes keeping the middle axle weight low (and the lighter items first in the symmetry breaking); if the rear axle weight constraint is violated, with guide 9 (and the heavier items first);
* if not all the stacks fit, maximizing the profit, with guides 4 and 5, then again guide 8 or 9 if an axle weight constraint is violated.

In all of them, the groups of items unloaded last are packed first (see the group guiding strategy of the rectangle tree search).

**Axle weights.** When the solution violates the middle axle weight constraint of a semi-trailer truck, a stack is split in two (among the stacks of at least two items, one of the latest unloading group, the lightest one) and the rectangle phase is repeated, up to 7 splits: smaller stacks give more freedom to distribute the weight along the bin. If the solution still violates the axle weight constraints, and some unpacked item is light enough to fit in the remaining weight capacity, the more expensive :ref:`boxstacks tree search <internals_boxstacks_tree_search>` is run afterwards.

The bin is filled along x; to fill it along y, the algorithm is run on the flipped instance.

This decomposition is much cheaper than the tree search, and is designed for the instances where the axle weight constraints don't force a sparse packing.

Automatic selection
-------------------

It is always run for the instances with a single bin.

Source files
------------

* :code:`src/boxstacks/sequential_onedimensional_rectangle.hpp`, :code:`src/boxstacks/sequential_onedimensional_rectangle.cpp`
