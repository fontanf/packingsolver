.. _internals_box:

:code:`box` algorithms
======================

See :ref:`box<box>` for the input/output format and CLI usage of this solver.

Tree search
------------

This algorithm solves the :code:`feasibility`, :code:`knapsack`, :code:`open-dimension-x`, :code:`open-dimension-y`, :code:`open-dimension-z`, :code:`bin-packing`, :code:`bin-packing-with-leftovers` and :code:`variable-sized-bin-packing` objectives.

Branch-and-bound style search over partial packings (option ``--use-tree-search``). Places boxes one at a time in three dimensions, guided by one or more configurable guides. Can run as an anytime improvement process, or with a fixed exploration budget in non-anytime modes.

Tree search with maximal spaces
----------------------------------

This algorithm solves the :code:`feasibility` and :code:`knapsack` objectives.

Alternative branching scheme (option ``--use-tree-search-maximal-spaces``), selected automatically when many items fit per bin. Tracks the maximal empty rectangular parallelepipeds (boxes) still available, rather than reasoning about item placement directly.

References:

* "A beam search algorithm for the biobjective container loading problem" (Araya, Moyano and Sanchez, 2020)

  * https://doi.org/10.1016/j.ejor.2020.03.040

* "A tree search-based heuristic for the three-dimensional single container loading problem" (Guesser, Alves De Queiroz and Miyazawa, 2026)

  * https://doi.org/10.1016/j.ejor.2026.01.039
