.. _internals_irregular_tree_search_maximal_spaces:

Tree search with maximal spaces
===============================

Objectives
----------

This algorithm only handles instances with a single bin. It solves the :code:`knapsack` and :code:`feasibility` objectives directly, and the :code:`bin-packing`, :code:`bin-packing-with-leftovers`, :code:`open-dimension-x`, :code:`open-dimension-y` and :code:`open-dimension-xy` objectives through the :ref:`sequential feasibility <internals_generic_algorithms_sequential_feasibility>` algorithm, which solves a sequence of :code:`feasibility` sub-problems with this algorithm.

Principle
---------

It lies between the :ref:`tree search <internals_irregular_tree_search>` and the :ref:`tree search with periodic packings <internals_irregular_tree_search_periodic_packing>`:

* like the latter, a node stores the *maximal empty spaces* of the bin, a *block* of items is packed at each stage, in the empty space closest to the bottom-left corner of the bin, and the nodes are evaluated by completing them greedily;
* like the former, the items are described by their actual (simplified) shapes, decomposed into trapezoids, and a block is inserted by resting it on a supporting part of the bin or of the blocks already packed and by sliding it to the right until it doesn't overlap them.

The algorithm thus works on two representations of the geometry at the same time:

* a **coarse one, made of rectangles**, which gives the maximal empty spaces. It is only used to decide *where* to try to insert a block: which empty space, which supporting parts, which blocks;
* an **exact one, made of trapezoids**, which decides *whether* a block can be inserted at a position. A position is valid if and only if the trapezoids of the block don't overlap the trapezoids of the items already packed and of the obstacles of the bin.

Unlike in the tree search, a block can be packed in any empty space, including the gaps left behind the front of the items already packed. Unlike in the tree search with periodic packings, the blocks are not reduced to their bounding boxes: a block doesn't have to fit inside the empty space it is inserted in, and may stick out of it, for example to slide down a slope of an item already packed, or into one of its concavities.

Frame
-----

As in the tree search, everything is computed in the frame of a *direction*, obtained by mirroring and transposing the instance: the bin and the items are converted into this frame, the algorithm packs the items towards the bottom-left corner of the frame, and the positions found are converted back at the end. Two directions are used, in parallel:

* from left to right then from bottom to top (the original frame);
* from bottom to top then from left to right (the frame where the x and y axes are exchanged).

In the following, "left", "bottom", etc. refer to the frame of the direction.

All the lengths are scaled (see :code:`scale_value` in the instance parameters).

Preprocessing
-------------

**Simplified shapes and orientations.** The shapes are simplified, and the item types are considered in a finite set of orientations, exactly as for the :ref:`tree search <internals_irregular_tree_search>`. The simplified shapes of the items, of the bin borders and of the defects contain the original ones, so that a packing of the simplified shapes is a packing of the original ones.

**Trapezoid sets.** For each item type and orientation, a *trapezoid set* is built as in the tree search:

* the simplified shape and the simplified shape inflated by the item-item spacing are both decomposed into generalized trapezoids (trapezoids whose parallel sides are horizontal);
* the *supporting parts*, chains of edges with the material below them, on which another item can rest, are computed on the inflated shape;
* the *supported parts*, chains of edges with the material above them, with which the item rests on another one, are computed on the non-inflated shape.

The bin is also decomposed into trapezoids: a strip below the bin, a strip above the bin, its borders (the parts of its bounding box outside of its shape), its defects and its fixed items, all inflated by their spacings. These are the *obstacles* of the bin. The bin has its own supporting parts: its floor, and the top of its borders, defects and fixed items.

**Blocks.** A block is a set of items with fixed relative positions. The blocks are:

* one block for each item type in each of its orientations, with a single item;
* the blocks of the :ref:`periodic packings <internals_irregular_tree_search_periodic_packing>` already computed by the instance builder, that is, only for the item types with many copies (no periodic packing is computed by this algorithm). The blocks of a single item, and the ones aligning the bounding boxes of the items on a grid, are not kept: packing these items one by one does at least as well on their actual shapes. If a periodic block uses an orientation which is not among the orientations of its item type, the trapezoid set of this orientation is built.

The trapezoids of a block are the trapezoids of its items, shifted by their positions in the block. A block is described by:

* its trapezoids, non-inflated and inflated;
* its supporting parts: the supporting parts of its items which are not covered by another item of the block. A supporting part is covered if a trapezoid of another item of the block overlaps it horizontally and has its bottom at or above the lowest point of the supporting part, that is, if another item of the block lies on top of it;
* its supported parts: the supported parts of its *lowest* item (the one whose bottom is the lowest, then the leftmost one) which are not covered by another item of the block. A supported part is covered if a trapezoid of another item of the block overlaps it horizontally and has its top at or below the highest point of the supported part. Only the lowest item is used, since resting the block on the bottom of its other items would mostly lead to the same positions, for a much larger number of slides;
* its rectangles: the rectangles covering its items (see below), and its *core rectangle*, the largest of them;
* its number of items, the area of its items and their profit.

**Rectangles.** Each item, in each orientation, is covered by rectangles: its inflated simplified shape, in the frame of the direction, is given to :code:`shape::outer_approximation_by_rectangles`, with a maximum waste (area of the rectangles minus the area of the shape) of 10% of the area of the shape. While this gives more than 8 rectangles, the maximum waste is doubled and the rectangles are computed again: the number of maximal empty spaces grows quickly with the number of rectangles, and so does the time needed to update them. For example, a frame whose hole represents 20% of its area is covered by 4 rectangles, which keeps its hole free, but it would be covered by its bounding box alone with a maximum waste of 30%.

The obstacles of the bin are covered by rectangles too, in the original frame and with a maximum waste of 10% of the smallest of the area of the bin and of the total area of the items, then converted into the frame of the direction.

Nodes
-----

A node stores:

* the blocks packed, with their positions;
* the trapezoids which a new block must not overlap: the obstacles of the bin and the inflated trapezoids of the items packed;
* the maximal empty spaces;
* for each item type, its number of copies packed, and the blocks which don't use more copies than available, called the *valid* blocks;
* the number of items packed, their area, their profit, and the profit of the greedy completion of the node (see below).

**Maximal empty spaces.** An empty space is an axis-aligned rectangle of the bin which doesn't overlap the rectangles covering the items packed and the obstacles. An empty space is *maximal* if it isn't contained in another empty space. The maximal empty spaces may overlap each other.

Since the rectangles contain the items, the empty spaces are really empty, but they don't cover all the free space: the parts of the rectangles outside of the items (their waste) are free but in no empty space. This is why the empty spaces are only used to select where to try to insert a block, and why the blocks are allowed to stick out of them.

The root node has a single empty space, the bounding box of the bin shrunk by the item-bin spacing, which is then cut by the rectangles of the obstacles.

When a block is packed, its rectangles, at its position, are removed from the empty spaces: each empty space overlapping at least one of them is cut by all of them, which replaces it by its maximal parts on the left, on the right, below and above each rectangle. Then, the new parts contained in another empty space are discarded.

**Unusable empty spaces.** A block *may fit* in an empty space if:

* its core rectangle fits in the empty space, up to 10%: its width is at most 1.1 times the width of the empty space, and its height at most 1.1 times its height;
* its bounding box fits in the bin.

This is only a rough necessary condition, the actual check being made on the trapezoids. After each insertion, the empty spaces in which no valid block may fit are discarded; and when cutting the empty spaces, the new parts narrower (or lower) than the narrowest (or lowest) core rectangle of the valid blocks, divided by 1.1, are discarded right away.

Insertions
----------

The insertions of a node are computed as follows.

**1. Selection of the empty space.** The empty space whose bottom-left corner is the closest to the bottom-left corner of the bin, in Manhattan distance, is selected; in case of a tie, the largest one.

**2. Candidate blocks.** The candidate blocks are the valid blocks which may fit in the selected empty space.

**3. Candidate supporting parts.** The candidate supporting parts are the ones which touch the bottom side of the empty space: they overlap it horizontally, and their lowest point is at or below it while their highest point is at or above it. They are searched among the supporting parts of the bin and of the blocks already packed (only for the blocks whose bounding box crosses the bottom side of the empty space). The bottom side of an empty space is usually the top of a rectangle covering an item, which is also the top of the item, so the supporting part of the item at the top of this rectangle touches it.

**4. Obstacles.** For each candidate supporting part, the trapezoids which a block resting on it may overlap are selected among the trapezoids of the node: the ones whose right is to the right of the left side of the empty space minus the width of the widest candidate block, and which overlap vertically the supporting part extended, above and below, by the height of the highest candidate block. The other trapezoids can't be reached by the slides below.

**5. Slides.** For each candidate block, for each candidate supporting part, and for each supported part of the block:

* the block is put on the supporting part: the first point of the supported part is put on the first point of the supporting part;
* it is slid to the right of a vertical *wall*, at the left side of the empty space minus the width of the block (and at least at the left side of the bin). The wall lets the block stick out of the empty space on the left by at most its own width, but prevents it from going back to the gaps further left, which belong to other empty spaces;
* it is slid until it doesn't overlap the obstacles of the supporting part.

A slide is the one of the tree search (see :code:`TrapezoidSetSlider`): the block moves to the right along the supporting part, its supported part staying in contact with it, following the larger of the slopes of the two current edges (the supporting edge and the supported edge), and changing edge when it reaches the end of one of them. When a trapezoid of the block overlaps an obstacle, the block moves by the smallest distance getting it out of the obstacle; this is repeated until it overlaps no obstacle. Each move is the smallest one, so the final position is the first position along the supporting part, at or after the initial one, without overlap.

The slide fails, and the position is discarded, if the end of the supporting part is reached, or if the block goes beyond the right side of the bin. The bottom and the top of the bin are obstacles, so they are respected by the slide too. Otherwise, the position is valid; it is kept if the bounding box of the block overlaps the empty space.

**6. Score.** Each position is scored by

.. math::

   V \times D^\beta \times N^{-\gamma}

where:

* :math:`V` is the profit of the block;
* :math:`N` is its number of items;
* :math:`D` is its *density*: the area of its items divided by the area of the rectangle going from the bottom-left corner of the empty space (or from the bottom-left corner of the bounding box of the block, if it is lower or more to the left) to the top-right corner of the bounding box of the block, capped at 1. It penalizes the blocks leaving free space between them and the corner of the empty space, and the blocks wasting space inside their own bounding box.

By default, :math:`\beta = 2` and :math:`\gamma = 0.4`. A larger :math:`\beta` favors the dense positions, a larger :math:`\gamma` the blocks of few items.

For each block, only its best position is kept. The insertions of the node are these positions, sorted by decreasing score.

**7. Failed empty spaces.** If no block has a valid position in the selected empty space, the empty space is removed from the node and the next one is selected, until an insertion is found or no empty space is left. This is valid since a block which can't be inserted in an empty space can't be inserted there later either: later, there are more items packed and fewer valid blocks.

Applying an insertion to a node adds the block to the node, adds its inflated trapezoids to the trapezoids of the node, updates the numbers of copies and the valid blocks, and cuts the empty spaces by its rectangles.

Example
-------

Consider a bin with an L-shaped item already packed in its bottom-left corner, its vertical arm on the left. The rectangles covering the L are the rectangles of its two arms. The empty space closest to the bottom-left corner of the bin is then the one above the horizontal arm and to the right of the vertical arm, in the concavity of the L.

The supporting part touching the bottom of this empty space is the top of the horizontal arm of the L, which starts at the inner corner of the L. A right triangle resting on it with one of its legs is put at the start of this supporting part, against the vertical arm of the L; the wall, at the left side of the empty space minus the width of the triangle, doesn't move it. If the triangle doesn't overlap anything there, this is its position: in the corner of the concavity. Otherwise, for example if another item already lies there, it is slid to the right until it doesn't overlap it.

If the top of the horizontal arm was sloped down to the right, the triangle would slide down along it, and its bounding box could end below the bottom of the empty space: this position is still kept since its bounding box overlaps the empty space.

Search
------

**Greedy completion.** A node is completed greedily by applying, at each stage, its insertion of best score, until it has no insertion left. Only the best insertion is computed at each stage, but this still requires sliding all the candidate blocks, so a greedy completion costs about as many slides as the number of stages times the number of candidate blocks. The greedy completion checks the time limit at each stage, and stops early when it is reached.

**Children.** The children of a node are generated one by one, in the order of the scores of its insertions. Each child is evaluated by its greedy completion: its *greedy value* is the profit of the greedy completion. The solution of a node is its greedy completion, converted back into the original frame.

**Beam search.** The tree is explored by an iterative beam search (:code:`treesearchsolver::iterative_beam_search`). At each depth, the beam keeps the nodes with the largest greedy values (then the largest profits); at most as many children as the size of the beam are generated for each node. The best solution found is the one of the node with the largest greedy value.

**Dominance.** Among the nodes with the same number of copies of each item type, only the one with the largest greedy value is kept (with a history shared by all the depths).

In anytime mode, the size of the beam grows at each restart. In the non-anytime modes, a beam search of size 1 then one of size :code:`not_anytime_tree_search_maximal_spaces_queue_size` (4 by default) are run.

Parameters
----------

The parameters of the branching scheme (:code:`BranchingSchemeMaximalSpaces::Parameters`) are:

* :code:`direction`: the direction;
* :code:`maximum_approximation_ratio` (default 5%): the maximum approximation ratio of the shape simplification, taken from :code:`not_anytime_maximum_approximation_ratio` when the algorithm is run from :code:`optimize`;
* :code:`rectangle_approximation_ratio` (default 0.1): the initial maximum waste of the rectangles covering an item, as a ratio of its area; also the tolerance of the test "may fit";
* :code:`maximum_number_of_rectangles` (default 8): the maximum number of rectangles covering an item;
* :code:`use_periodic_blocks` (default true): whether to use the periodic blocks, or only the single items;
* :code:`beta` (default 2) and :code:`gamma` (default 0.4): the exponents of the score.

Limitations
-----------

* The defects are always obstacles, whatever the quality rules of the items.
* The empty spaces are approximations of the free space: a block which only fits across several empty spaces, or whose core rectangle is too large for the empty spaces of a concavity, may be missed.
* Only the bottom-left corner of the bin is used as anchor (in each direction), unlike in the tree search with periodic packings, where the empty space closest to any corner of the bin is selected.
* The greedy completions dominate the running time: since each child is evaluated by a greedy completion, which slides all the candidate blocks at each stage, the algorithm gets slow on instances with many different item types.

Automatic selection
-------------------

The algorithm is never selected automatically. It can be forced with :code:`--use-tree-search-maximal-spaces 1` (CLI) or :code:`use_tree_search_maximal_spaces` (Python).

Source files
------------

* :code:`src/irregular/trapezoid_set.hpp`, :code:`src/irregular/trapezoid_set.cpp`: the directions, the trapezoid sets and the supports, shared with the tree search
* :code:`src/irregular/trapezoid_slider.hpp`, :code:`src/irregular/trapezoid_slider.cpp`: the slide of a trapezoid set along a supporting part, shared with the tree search
* :code:`src/irregular/empty_spaces.hpp`, :code:`src/irregular/empty_spaces.cpp`: the maximal empty spaces, shared with the tree search with periodic packings
* :code:`src/irregular/tree_search_maximal_spaces.hpp`, :code:`src/irregular/tree_search_maximal_spaces.cpp`: the blocks, the branching scheme and the search
