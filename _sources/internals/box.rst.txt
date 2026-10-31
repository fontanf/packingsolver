.. _internals_box:

:code:`box` algorithms
======================

See :ref:`box<box>` for the input/output format and CLI usage of this solver.

Tree search
------------

This algorithm solves the :code:`feasibility`, :code:`knapsack`, :code:`open-dimension-x`, :code:`open-dimension-y`, :code:`open-dimension-z`, :code:`bin-packing` and :code:`bin-packing-with-leftovers` objectives.

This is a direct generalization of the :ref:`tree search for rectangle <internals_rectangle_tree_search>` to three dimensions: a single item is packed at each stage, against the skyline of the items already packed.

See :doc:`the detailed description <box/tree_search>`.

Tree search with maximal spaces
----------------------------------

This algorithm solves the :code:`feasibility` and :code:`knapsack` objectives with a single bin.

It is a tree search algorithm where a block of items is packed at each stage, in a corner of the maximal empty spaces of the bin. The :ref:`tree search with maximal spaces for rectangle <internals_rectangle_tree_search_maximal_spaces>` is its two-dimensional version.

See :doc:`the detailed description <box/tree_search_maximal_spaces>`.

Dual feasible functions
-------------------------

This algorithm computes a bound for the :code:`bin-packing`, :code:`feasibility` and :code:`knapsack` objectives, for instances with a single bin type.

It is the generalization to three dimensions of the :ref:`dual feasible functions for rectangle <internals_rectangle_dual_feasible_functions>`.

See :doc:`the detailed description <box/dual_feasible_functions>`.

Trivial bounds
--------------

Closed-form bounds, computed for every instance: a volume-based fractional bound for the :code:`knapsack` objective, a volume-based number of bins for the :code:`bin-packing` objective, and bounds from the volume and the dimensions of the items for the open dimension objectives.

.. toctree::
   :maxdepth: 1
   :hidden:

   box/tree_search
   box/tree_search_maximal_spaces
   box/dual_feasible_functions
