.. _internals_box_tree_search_maximal_spaces:

Tree search with maximal spaces
===============================

Objectives
----------

This algorithm only handles instances with a single bin. It solves the :code:`feasibility` and :code:`knapsack` objectives directly, and the :code:`bin-packing`, :code:`bin-packing-with-leftovers`, :code:`open-dimension-x`, :code:`open-dimension-y` and :code:`open-dimension-z` objectives through the :ref:`sequential feasibility <internals_generic_algorithms_sequential_feasibility>` algorithm, which solves a sequence of :code:`feasibility` sub-problems with this algorithm.

Principle
---------

It is the algorithm of Araya, Moyano and Sanchez (2020) and of Guesser, Alves De Queiroz and Miyazawa (2026) for the container loading problem. The :ref:`tree search with maximal spaces for rectangle <internals_rectangle_tree_search_maximal_spaces>` is its two-dimensional version.

It works exactly as its two-dimensional version, which this page assumes known, with the following differences.

**Blocks.** The simple blocks are the grids of :math:`c_x \times c_y \times c_z` copies of an item type, in each of its allowed rotations, fitting in the bin. The combined blocks are two blocks side by side along x, y or z. They are generated in the same order (blocks of a single item first, then by decreasing profit per volume of their box), up to 10000 blocks. Unlike in two dimensions, the blocks with waste are kept even when there are few item types. The maximum weight of the bin is checked when a block is inserted.

**Maximal empty spaces.** They are boxes. When a block is placed, each maximal space it overlaps is replaced by up to six new spaces: the parts of the space before and after the block along x, along y and along z, each one extending over the whole space along the two other axes. The spaces contained in another one, and the spaces in which no remaining block fits, are removed.

**Selection of the space.** The distance of a space to the corners of the bin is the sum, over the three axes, of the distance between the space and the closest side of the bin along this axis. The space of smallest distance is selected, the largest one in case of ties, and the blocks are placed in its corner on the side of the closest corner of the bin.

**Fitness.** The fitness of a block placed in a space is :math:`V \times C^\alpha \times L^\beta \times N^{-\gamma}`, with the same parameters as in two dimensions, where :math:`C` is the area of the faces of the block in contact with the sides of the bin and with the blocks already placed, divided by its total surface, and :math:`L` is the fraction of the volume of the space which remains usable: in each direction, the length of the space not covered by the block is reduced to the largest length reachable with the remaining items.

**Search.** As in two dimensions: a node is evaluated by its greedy completion, which is also its guide, its solution and the criterion of its dominance (global dominance); the children are generated one at a time by decreasing fitness; a single iterative beam search is run, with a growth factor of 2.

Automatic selection
-------------------

With a single bin, it is run for the :code:`knapsack` and :code:`feasibility` objectives when many items fit in a section of the bin in every direction (more than 8 on average), where the :ref:`tree search <internals_box_tree_search>` would have too many children. It is also used to re-optimize the bins of the solution for the :code:`bin-packing-with-leftovers` objective when the leftovers of all the bins count. See :ref:`algorithm selection <internals_algorithm_selection>`.

It can be forced with :code:`--use-tree-search-maximal-spaces 1` (CLI) or :code:`use_tree_search_maximal_spaces` (Python).

Source files
------------

* :code:`src/box/block.hpp`, :code:`src/box/block.cpp`: the blocks
* :code:`src/box/tree_search_maximal_spaces.hpp`, :code:`src/box/tree_search_maximal_spaces.cpp`: the branching scheme and the search

References:

* "A beam search algorithm for the biobjective container loading problem" (Araya, Moyano and Sanchez, 2020)

  * https://doi.org/10.1016/j.ejor.2020.03.040

* "A tree search-based heuristic for the three-dimensional single container loading problem" (Guesser, Alves De Queiroz and Miyazawa, 2026)

  * https://doi.org/10.1016/j.ejor.2026.01.039
