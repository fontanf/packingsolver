.. _internals_box_tree_search:

Tree search
===========

Objectives
----------

This algorithm solves the :code:`feasibility`, :code:`knapsack`, :code:`open-dimension-x`, :code:`open-dimension-y`, :code:`open-dimension-z`, :code:`bin-packing` and :code:`bin-packing-with-leftovers` objectives. It supports the rotations of the items, the maximum weight of the bins and the resources.

Principle
---------

It is a direct generalization of the :ref:`tree search for rectangle <internals_rectangle_tree_search>` to three dimensions, which this page assumes known: a single item is packed at each stage, in the current (last) bin, or in a new bin if no item fits in the current one anymore, and the tree is explored by the :ref:`iterative beam search <internals_tree_search>`.

**Directions.** In the direction :code:`X`, the items are pushed towards the origin along the x-axis, and the free space of a bin is at the end of its x-axis. The directions :code:`Y` and :code:`Z` are the same with the axes exchanged. The directions used are the open dimension, or the axis of the leftover when there is one; otherwise, all three directions in separate searches when there is a single bin type, and, with several bin types, the longest dimension of each new bin.

**Order of the bins.** The bins are opened in the order in which they appear in the instance, or by increasing volume for the :code:`knapsack` objective.

Nodes
-----

A node stores the *skyline* of its current bin: the list of the *uncovered items*, the parts of the packed items visible from the end of the x-axis. Each uncovered item is a rectangle of the yz-plane, given by its ranges along y and z, with the x-coordinate of the end of its item. Together, they cover the whole yz-section of the bin; the parts where no item is visible are at :math:`x = 0`.

When an item is inserted, each uncovered item it overlaps in the yz-plane is replaced by the parts of it which the new item doesn't cover (up to four rectangles: below and above the new item along y, and before and after it along z), and the new item becomes an uncovered item.

**Effective skyline.** When the space between an item and a side of the bin along y (or z) is smaller than the smallest dimension of the items along y (or z), no item can be inserted there anymore: the uncovered items on this side are considered to reach the end of the item along x. This effective skyline is used for the dominance and for the volumes below.

**Volumes.** The *current volume* of a node is the volume of the bins before the current one plus the volume behind the effective skyline; its *waste* is the current volume minus the volume of the items packed. The *guide volume* is the volume of the bins before the current one plus the volume of the current bin behind the largest x-coordinate of the start of its items, plus the volume of the effective skyline beyond it.

Children
--------

An item, in each of its allowed rotations (up to 6), is inserted at a position given by two uncovered items, or by the sides of the bin:

* along y, it starts at the end of an uncovered item along y, or at 0;
* along z, it starts at the end of an uncovered item along z, or at 0;
* along x, it is pushed towards the origin until it doesn't overlap the skyline.

The insertion is discarded if the item sticks out of the bin, if the maximum weight of the bin or the capacity of a resource is exceeded (a *penalize* resource never discards an insertion; its penalty is subtracted from the profit instead), or if the item doesn't actually rest against the uncovered items which gave its position: it must overlap them along the other axis, and, once pushed along x, not start beyond their end. As in two dimensions, a new bin is only opened if no item fits in the current bin.

**Symmetry breaking.** An item type :math:`j` gets a *predecessor* :math:`k` if they have the same dimensions and allowed rotations, and if :math:`k` is at least as good: a profit at least as large and a weight at most as large (for two identical item types, the one of smaller index is the predecessor of the other). An item of type :math:`j` can only be inserted once all the copies of its predecessors are packed.

Guides
------

The guides are the ones of two dimensions, with volumes instead of areas. With :math:`V_g` the guide volume, :math:`V` the volume of the items packed, :math:`P` their profit and :math:`n` their number: guide 0 is :math:`V_g / V`, guide 1 is :math:`V_g / V / V \times n`, guide 4 is :math:`V_g / P`, and guide 5 is :math:`V_g / P / V \times n`. By default, guides 4 and 5 are used for the :code:`knapsack` objective, and guides 0 and 1 for the other objectives.

Solutions, bounds and dominance
-------------------------------

The solutions and the bounds are the ones of two dimensions, with volumes instead of areas:

* :code:`bin-packing`: the volume of all the items plus the waste of the node must fit in the bins;
* :code:`bin-packing-with-leftovers`: the leftover values of a node can only decrease in its descendants in the same bin;
* open dimension objectives: the length used is at least the end of the farthest item, and at least the volume of all the items plus the waste, divided by the section of the bin;
* :code:`knapsack` and :code:`feasibility`: the nodes are only discarded once a solution packing all the items is found.

**Dominance.** Two nodes with the same number of copies of each item type packed can be compared. Node 1 dominates node 2 if it uses fewer bins, or as many bins, leftover values which aren't smaller (for the :code:`bin-packing-with-leftovers` objective), a resource consumption in its current bin which isn't larger for any resource, and an effective skyline which is nowhere beyond the effective skyline of node 2: for each pair of their uncovered items which overlap in the yz-plane, the one of node 1 doesn't reach farther along x.

Search
------

In anytime mode, an iterative beam search is run for each pair (guide, direction), in parallel. When there are at most two pairs, each one is run twice, with the growth factors 1.33 and 1.5; otherwise, the growth factor is 1.5. In the non-anytime modes, each search is run with a queue of size 1, then with a queue of the size given by the parameter :code:`not_anytime_tree_search_queue_size`. The depth of a node is its number of items packed.

Automatic selection
-------------------

With a single bin and the :code:`knapsack` or :code:`feasibility` objective, the tree search is run in the directions in which few items fit in a section of the bin: at most 8 on average (parameter :code:`many_items_in_section_threshold`), the area of the section of the bin divided by the mean area of the section of the items; if no direction qualifies, the :ref:`tree search with maximal spaces <internals_box_tree_search_maximal_spaces>` is run instead. With a single bin and the other objectives, the tree search is run. With several bins, it is run when the item types have few copies compared to the number of items fitting in a bin, alongside the generic algorithms using it to solve their sub-problems. See :ref:`algorithm selection <internals_algorithm_selection>`.

It can be forced with :code:`--use-tree-search 1` (CLI) or :code:`use_tree_search` (Python).

Source files
------------

* :code:`src/box/tree_search.hpp`, :code:`src/box/tree_search.cpp`
