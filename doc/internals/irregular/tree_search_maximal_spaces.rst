.. _internals_irregular_tree_search_maximal_spaces:

Tree search with maximal spaces
===============================

Objectives
----------

This algorithm only handles instances with a single bin. It solves the :code:`knapsack` and :code:`feasibility` objectives directly, and the :code:`bin-packing`, :code:`bin-packing-with-leftovers`, :code:`open-dimension-x`, :code:`open-dimension-y` and :code:`open-dimension-xy` objectives through the :ref:`sequential feasibility <internals_generic_algorithms_sequential_feasibility>` algorithm.

Principle
---------

It lies between the :ref:`tree search <internals_irregular_tree_search>` and the :ref:`tree search with periodic packings <internals_irregular_tree_search_periodic_packing>`:

* like the latter, a node stores the *maximal empty spaces* of the bin, a block of items is packed at each stage, in the empty space closest to the bottom-left corner of the bin, and the nodes are evaluated by completing them greedily;
* like the former, the items are described by their actual (simplified) shapes, decomposed into trapezoids, and a block is inserted by resting it on a supporting part of the bin or of the blocks already packed and by sliding it to the right until it doesn't overlap them.

Unlike the tree search, a block can be packed in any empty space, including the gaps left behind the front of the items already packed. Unlike the tree search with periodic packings, the blocks are not reduced to their bounding boxes: a block may stick out of the empty space it is packed in, for example to fit in a concavity of an item already packed.

Preprocessing
-------------

The shapes are simplified, and the item types are considered in the same orientations, as for the :ref:`tree search <internals_irregular_tree_search>`, whose trapezoids, supporting parts and supported parts are reused.

**Blocks.** The blocks are:

* each item type in each of its orientations;
* the blocks of the :ref:`periodic packings <internals_irregular_tree_search_periodic_packing>` already computed by the instance builder (that is, for the item types with many copies), except the ones aligning the bounding boxes of the items on a grid: packing these items one by one does at least as well on their actual shapes.

The supporting parts of a block are the supporting parts of its items which are not covered by another item of the block. A block rests on the supported parts of its lowest item which are not covered by another item of the block.

**Rectangles.** Each item is covered by at most 8 rectangles (see :code:`shape::outer_approximation_by_rectangles`), whose waste is at most 10% of the area of the item if possible. The obstacles of the bin (its borders, its defects and the fixed items) are covered by rectangles too.

Nodes
-----

A node stores:

* the blocks packed, and the trapezoids of their items, inflated by the item-item spacing;
* the maximal empty spaces: the empty rectangles which aren't contained in another empty rectangle, where the occupied space is given by the rectangles covering the items packed and the obstacles. The empty spaces too small for all the blocks still available are discarded.

The empty spaces are only used to select where to pack a block; the overlaps are checked on the trapezoids.

Insertions
----------

The empty space whose bottom-left corner is the closest to the bottom-left corner of the bin is selected. The supporting parts touching its bottom side are the ones of the bin and of the blocks already packed whose bounding box crosses it.

For each block whose largest rectangle fits in the empty space, and for each pair (supporting part, supported part of the block), the block is put on the supporting part, slid to the right of the left side of the empty space minus the width of the block, then slid until it doesn't overlap the trapezoids of the items packed and of the obstacles. The position is kept if the block is then inside the bin and overlaps the empty space. Each position is scored by

.. math::

   V \times D^\beta \times N^{-\gamma}

where :math:`V` is the profit of the block, :math:`N` its number of items, and :math:`D` its density: the area of its items divided by the area of the rectangle from the bottom-left corner of the empty space (or of the block, if lower or more to the left) to the top-right corner of the block. By default, :math:`\beta = 2` and :math:`\gamma = 0.4`. For each block, only its best position is kept.

If no block can be inserted in the selected empty space, the empty space is removed, and the next one is selected.

Search
------

The nodes are evaluated by completing them greedily, inserting the best scored block at each stage; the beam search keeps the nodes with the best completions. Among the nodes with the same number of copies of each item type, only the one with the best completion is kept. The final solutions are completed greedily too.

The tree is explored by an iterative beam search, for two directions in parallel: from left to right then from bottom to top, and from bottom to top then from left to right. In anytime mode, the size of the beam grows at each restart. In the non-anytime modes, a beam search of size 1 then one of size :code:`not_anytime_tree_search_maximal_spaces_queue_size` (4 by default) are run.

Limitations
-----------

* The defects are always obstacles, whatever the quality rules of the items.
* The empty spaces are approximations of the free space: a block which only fits across several empty spaces, or in a concavity smaller than its largest rectangle, may be missed.

Automatic selection
-------------------

The algorithm is never selected automatically. It can be forced with :code:`--use-tree-search-maximal-spaces 1` (CLI) or :code:`use_tree_search_maximal_spaces` (Python).

Source files
------------

* :code:`src/irregular/trapezoid_set.hpp`, :code:`src/irregular/trapezoid_set.cpp`: the trapezoids and the supports, shared with the tree search
* :code:`src/irregular/trapezoid_slider.hpp`, :code:`src/irregular/trapezoid_slider.cpp`: the slide of a trapezoid set along a supporting part, shared with the tree search
* :code:`src/irregular/empty_spaces.hpp`, :code:`src/irregular/empty_spaces.cpp`: the maximal empty spaces, shared with the tree search with periodic packings
* :code:`src/irregular/tree_search_maximal_spaces.hpp`, :code:`src/irregular/tree_search_maximal_spaces.cpp`: the blocks, the branching scheme and the search
