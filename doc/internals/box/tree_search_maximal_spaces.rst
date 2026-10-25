.. _internals_box_tree_search_maximal_spaces:

Tree search with maximal spaces
===============================

Objectives
----------

This algorithm solves the :code:`feasibility` and :code:`knapsack` objectives with a single bin.

Principle
---------

It is the algorithm of Araya, Moyano and Sanchez (2020) and of Guesser, Alves De Queiroz and Miyazawa (2026) for the container loading problem. The :ref:`tree search with maximal spaces for rectangle <internals_rectangle_tree_search_maximal_spaces>` is its two-dimensional version.

A block of items is packed at each stage. The blocks are boxes filled with items in a guillotine pattern: grids of copies of an item type in one of its rotations, and their combinations, generated before the search. A node stores the *maximal empty spaces* of the bin: the empty boxes which aren't contained in another empty box. To generate the children of a node:

1. the empty space whose corner is the closest to a corner of the bin is selected;
2. one child is generated for each remaining block fitting in it, by decreasing fitness;
3. the block is placed in that corner, and the maximal empty spaces are updated.

The fitness of a block placed in a space combines its profit, its contact with the sides of the space and the blocks around it, the volume which remains usable after placing it, and its number of items, as in two dimensions. A node is evaluated by completing it greedily; the beam search keeps the nodes with the best completions.

Automatic selection
-------------------

With a single bin, it is run for the :code:`knapsack` and :code:`feasibility` objectives when many items (more than 64 on average) fit in the bin. See :ref:`algorithm selection <internals_algorithm_selection>`.

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
