.. _internals_generic_algorithms_sequential_feasibility:

Sequential feasibility
======================

Objectives
----------

This algorithm solves the :code:`bin-packing`, :code:`bin-packing-with-leftovers`, :code:`open-dimension-x`, :code:`open-dimension-y` and :code:`open-dimension-xy` objectives, with an algorithm solving the :code:`feasibility` objective. It is not selected by itself: it is used by the algorithms which only solve the :code:`feasibility` objective directly.

Principle
---------

It repeatedly solves a :code:`feasibility` sub-problem asking whether all the items fit into a shrinking amount of space:

* :code:`bin-packing`: a number of bins;
* :code:`bin-packing-with-leftovers`: a number of bins, the last one restricted to a length;
* :code:`open-dimension-x` and :code:`open-dimension-y`: a bin restricted to a length along the open dimension;
* :code:`open-dimension-xy`: a bin restricted to a length along both dimensions, with the given aspect ratio.

It starts from an estimate computed from the total space of the items. As long as the sub-problem is feasible, it tightens the space based on the solution just found (one bin fewer than it actually used, or slightly less than the length it actually reached) and solves it again. It stops as soon as a sub-problem turns out infeasible (or can't be solved within its budget), and returns the last feasible solution found.

**Warm start.** In the irregular version, each sub-problem, except the first one, receives the previous solution found as a warm start. It is generally not a feasible solution of the sub-problem: the space has been shrunk, so some items may stick out of their bin, and the bins which aren't available anymore are dropped together with their items. It is up to the inner algorithm to repair it, and the :ref:`irregular local search <internals_irregular_local_search>` does.

Uses
----

* irregular: the :ref:`local search <internals_irregular_local_search>` (shrinking the open dimension by 0.1% at each step), the :ref:`tree search with periodic packings <internals_irregular_tree_search_periodic_packing>`, the :ref:`tree search with maximal spaces <internals_irregular_tree_search_maximal_spaces>`, and the :ref:`tree search <internals_irregular_tree_search>` for the :code:`open-dimension-xy` objective;
* rectangle: the :ref:`tree search with maximal spaces <internals_rectangle_tree_search_maximal_spaces>`, for the :code:`open-dimension-x`, :code:`open-dimension-y` and :code:`bin-packing-with-leftovers` objectives with a single bin;
* rectangleguillotine: the :ref:`tree search with maximal spaces <internals_rectangleguillotine_tree_search_maximal_spaces>`, for the :code:`open-dimension-x`, :code:`open-dimension-y` and :code:`bin-packing-with-leftovers` objectives with a single bin.

The :ref:`MILP assignment <internals_onedimensional_milp_assignment>` and :ref:`column generation <internals_generic_algorithms_column_generation>` algorithms use a similar scheme for the :code:`bin-packing` objective, trying the numbers of bins in increasing order from a lower bound.

Source files
------------

* :code:`src/irregular/sequential_feasibility.hpp`, :code:`src/irregular/sequential_feasibility.cpp`
* :code:`src/rectangle/sequential_feasibility.hpp`, :code:`src/rectangle/sequential_feasibility.cpp`
* :code:`src/rectangleguillotine/sequential_feasibility.hpp`, :code:`src/rectangleguillotine/sequential_feasibility.cpp`
