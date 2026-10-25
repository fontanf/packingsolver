.. _internals_box_tree_search:

Tree search
===========

Objectives
----------

This algorithm solves the :code:`feasibility`, :code:`knapsack`, :code:`open-dimension-x`, :code:`open-dimension-y`, :code:`open-dimension-z`, :code:`bin-packing` and :code:`bin-packing-with-leftovers` objectives.

Principle
---------

It is a direct generalization of the :ref:`tree search for rectangle <internals_rectangle_tree_search>` to three dimensions: a single item is packed at each stage, either in the current (last) bin or in a new bin, and the tree is explored by an iterative beam search.

In the direction :code:`X`, the items are packed along the x-axis. A node stores the *skyline* of its current bin: the list of the *uncovered items*, the parts of the packed items visible from the end of the x-axis, each described by its x-coordinate and its ranges along y and z. A new item, in one of its allowed rotations (up to 6), starts along y at the end of an uncovered item (or at 0), and along z at the end of an uncovered item (or at 0); it is then pushed along x until it doesn't overlap the skyline. The directions :code:`Y` and :code:`Z` are the same with the axes exchanged.

The directions used depend on the instance: the open dimension or the axis of the leftover when there is one, all three directions in separate searches when there is a single bin type; otherwise, each new bin is filled along its longest dimension.

Dominance
---------

As in two dimensions, two nodes with the same items packed are compared through their skylines. When the gap between an item and a side of the bin is too small for any remaining item, the skyline is considered as reaching the side there, which makes more nodes comparable.

Guides
------

The guides are the same as in two dimensions, with volumes instead of areas: by default, guides 4 and 5 (the volume lost divided by the profit) for the :code:`knapsack` objective, and guides 0 and 1 (the volume lost divided by the volume of the items packed) for the other objectives.

Automatic selection
-------------------

With a single bin, the tree search is run unless the objective is :code:`knapsack` or :code:`feasibility` and many items (more than 64 on average) fit in the bin, in which case the :ref:`tree search with maximal spaces <internals_box_tree_search_maximal_spaces>` is run instead. With several bins, it is run when the item types have few copies compared to the number of items fitting in a bin, alongside the generic algorithms using it to solve their sub-problems. See :ref:`algorithm selection <internals_algorithm_selection>`.

It can be forced with :code:`--use-tree-search 1` (CLI) or :code:`use_tree_search` (Python).

Source files
------------

* :code:`src/box/tree_search.hpp`, :code:`src/box/tree_search.cpp`
