.. _internals_boxstacks_tree_search:

Tree search
===========

Objectives
----------

This algorithm solves the :code:`feasibility` and :code:`knapsack` objectives with a single bin. It is run by the :ref:`sequential one-dimensional / rectangle <internals_boxstacks_sequential_onedimensional_rectangle>` algorithm when its solution violates the axle weight constraints.

Principle
---------

It is a constructive tree search where a single item is packed at each stage, generalizing the :ref:`tree search for rectangle <internals_rectangle_tree_search>`: the floor of the bin is filled with stacks of items along x, and a node stores the skyline of the stacks packed. The possible insertions for an unpacked item are:

* in a new stack, with its lower y touching either another packed stack or the side of the bin at y = 0, then pushed along x until it doesn't overlap the stacks packed;
* on top of a stack of the skyline.

.. image:: ../../img/boxstacks_tree_search.png
   :scale: 100%
   :align: center

An insertion above a stack must satisfy the stacking constraints: same stackability id, maximum stackability, maximum weight above, the height of the bin (using the nesting heights), the maximum stack density. Any insertion must satisfy the maximum weight of the bin and the unloading constraints.

**Axle weights.** The axle weights of a semi-trailer truck depend on the position of the center of gravity of the items, which is only known once the items are packed. So the axle weight constraints are checked when the last item of an unloading group is inserted, and the insertion is discarded if an axle is then overloaded.

Guides
------

The guides are the ones of the rectangle tree search, with volumes: by default, guides 4 and 5 for the :code:`knapsack` objective, guides 0 and 1 for the :code:`bin-packing-with-leftovers` objective, and guides 0 and 2 for the other objectives.

This algorithm is more expensive than the sequential one-dimensional / rectangle algorithm. It is useful when the axle weight constraints are active in the best solutions, which then need sparse packings.

Source files
------------

* :code:`src/boxstacks/tree_search.hpp`, :code:`src/boxstacks/tree_search.cpp`
