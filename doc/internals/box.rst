.. _internals_box:

:code:`box` algorithms
======================

See :ref:`box<box>` for the input/output format and CLI usage of this solver.

Tree search
------------

This algorithm solves the :code:`feasibility`, :code:`knapsack`, :code:`open-dimension-x`, :code:`open-dimension-y`, :code:`bin-packing` and :code:`bin-packing-with-leftovers` objectives.

This is a direct generalization of the algorithm for :code:`rectangle` problems.

See :ref:`rectangle <internals_rectangle_tree_search>`

Tree search with maximal spaces
----------------------------------

This algorithm solves the :code:`feasibility` and :code:`knapsack` objectives with a single bin.
It doesn't support unloading constraints.

This is a direct generalization of the algorithm for :code:`rectangle` problems.

See :ref:`rectangle <internals_rectangle_tree_search_maximal_spaces>`

References:

* "A beam search algorithm for the biobjective container loading problem" (Araya, Moyano and Sanchez, 2020)

  * https://doi.org/10.1016/j.ejor.2020.03.040

* "A tree search-based heuristic for the three-dimensional single container loading problem" (Guesser, Alves De Queiroz and Miyazawa, 2026)

  * https://doi.org/10.1016/j.ejor.2026.01.039
