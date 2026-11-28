.. _internals_irregular_blocks:

Blocks
======

A *block* is a set of items with fixed relative positions. The :ref:`tree search with periodic packings <internals_irregular_tree_search_periodic_packing>` and the :ref:`tree search with maximal spaces <internals_irregular_tree_search_maximal_spaces>` pack a block at each stage. A block is described by its items, its numbers of copies of each item type, and its dimensions: the dimensions of the bounding box of its items, plus the item-item minimum spacing, so that two blocks placed side by side respect the spacing.

Kinds of blocks
---------------

The blocks are built for the bin of the instance, from the :ref:`periodic packings <internals_irregular_tree_search_periodic_packing>` of the item types:

* the tilings of :math:`n_x \times n_y` unit cells of a periodic packing, as long as they fit in the bin and don't use more copies than available;
* the *clipped lattice* blocks: the bin is filled with the lattice of a periodic packing, for several positions of the lattice, and a block is made of the items whose top and right are below given thresholds. Unlike the tilings, these blocks may contain partial unit cells, which is often denser along the sides of the bin;
* the grid blocks: the items of an item type aligned on a grid of their bounding boxes, in each of its orientations;
* the framed blocks (see below).

Framed blocks
-------------

A *frame item* is an item type in an orientation whose bounding box has a large empty rectangle touching one of its sides, at least half of the bounding box, like an L-shape or a U-shape. Without the framed blocks, such items are poorly packed: the space in their empty rectangle is only used by chance.

A block is framed by one frame item in an orientation :math:`r`, by two in the orientations :math:`r` and :math:`r + 180°`, or by four in the orientations :math:`r`, :math:`r + 90°`, :math:`r + 180°` and :math:`r + 270°`. The frame items are placed one after the other, with the block in their empty rectangle, and moved away from the block and from the previous frame items until they don't overlap them; the position giving the smallest bounding box is kept. The frame items are placed using only their bounding box, their empty rectangle and the bounding box of the block, so the framed blocks are valid without computing any no-fit polygon.

The *saving* of a framed block is the space taken by the block and by the frame items packed separately, minus the space taken by the framed block. The space taken by the frame items packed separately is their area divided by the best density of the blocks of their item type alone. A framed block is only kept if its saving is non-negative. The framed blocks are framed again once.

The number of framed blocks of each frame item type is limited. For a framed block, the maximum number of copies of the block, given the numbers of copies of its item types, times the number of copies of the frame item type in the block, is the number of copies of the frame item type that this block can use. The sum of this number over the framed blocks of a frame item type, at both depths, doesn't exceed 4 times the number of copies of the frame item type. Without this limit, an instance with many item types with a single copy has a framed block for almost every pair of an item and of an L-shape.

The framed blocks kept are the ones with the largest savings. To avoid framing all the blocks, the saving is first bounded: the framed block is at least as large, in each dimension, as the block and as each of its frame items, so its saving is at most the space taken by the block and by the frame items packed separately, minus the area of this smallest possible bounding box. For each frame item type, the blocks are framed by decreasing bound, until the bound is smaller than the saving of the framed blocks which already reach the limit.

Dominance
---------

A block dominates another one if it contains the same item types, at least as many copies of each, and takes no more space in both dimensions. Dominated blocks are discarded as they are generated, except the blocks of a single item, which are always kept: they may be the only way to pack the last copy of an item type.

Resources
---------

The resource consumption of a block is the consumption of each resource of the bin by its items alone in the bin, the consumption of the copies of an item type starting at the first entry of its consumption schedule. The blocks which exceed the capacity of a resource which is not *penalize* are discarded. The consumption of a node of the tree searches is the sum of the consumptions of its blocks: it is exact for the uniform consumption schedules, and overestimates the consumption for a non-increasing schedule when the copies of an item type are in several blocks.

Use by the algorithms
---------------------

* The :ref:`tree search with periodic packings <internals_irregular_tree_search_periodic_packing>` uses all the blocks, described by their bounding boxes.
* The :ref:`tree search with maximal spaces <internals_irregular_tree_search_maximal_spaces>` uses a block for each item type in each of its orientations, with a single item, and the blocks of more than one item built from the periodic packings: the tilings and the clipped lattice blocks. The grid blocks and the framed blocks are not kept: the tree search with maximal spaces packs the items on their actual shapes, and the items of these blocks are placed using their bounding boxes only.

Source files
------------

* :code:`src/irregular/periodic_packing.hpp`, :code:`src/irregular/periodic_packing.cpp`: the periodic packings
* :code:`src/irregular/tree_search_periodic_packing.hpp`, :code:`src/irregular/tree_search_periodic_packing.cpp`: the blocks (:code:`compute_periodic_blocks`)
