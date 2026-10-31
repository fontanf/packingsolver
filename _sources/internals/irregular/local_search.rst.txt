.. _internals_irregular_local_search:

Local search
============

Objectives
----------

The local search solves the :code:`feasibility` objective directly: it looks for a packing of all the items in the available bins. It solves the :code:`bin-packing`, :code:`bin-packing-with-leftovers`, :code:`open-dimension-x`, :code:`open-dimension-y` and :code:`open-dimension-xy` objectives through the :ref:`sequential feasibility <internals_generic_algorithms_sequential_feasibility>` algorithm: each time a packing is found, the space available is shrunk (one bin fewer, or the length of the last bin or the open dimension shrunk by 0.1%), and the local search is run again, starting from the previous packing.

It doesn't solve the :code:`knapsack` and :code:`variable-sized-bin-packing` objectives.

Principle
---------

Unlike the tree searches, the local search works on complete layouts: all the items are in a bin, but the layout is allowed to be infeasible. The goal is to make it feasible. How a layout may be infeasible, and how it is made feasible, depends on the *engine*:

* :ref:`direct trigonometry <internals_irregular_local_search_direct_trigonometry>`: the items may overlap; the overlap is minimized by moving the items one by one to the best of many sampled positions;
* :ref:`linear programming <internals_irregular_local_search_linear_programming>`: the items may be shrunk instead of overlapping; linear programs move them slightly and maximize their sizes;
* :ref:`nonlinear programming <internals_irregular_local_search_nonlinear_programming>`: the same with nonlinear programs, in which the items may also rotate.

Each engine provides a *separator*, which tries to make a layout feasible: a *separation*. A separation succeeds if the layout becomes feasible. Otherwise, it stops when it doesn't make progress anymore, and its layout is the best one found, with the smallest *loss*. The loss measures how far a layout is from being feasible: the overlap for the direct trigonometry engine, the area lost by shrinking the items for the other ones.

The engines share the search on top of the separator, described below.

Initial layout
--------------

The initial layout is built by the separator from a solution, possibly infeasible and partial: the solution found for the previous sub-problem of the sequential feasibility algorithm, or an empty solution. The items missing from it are inserted by the separator.

The solution of the previous sub-problem uses more space than available, since the space has been shrunk. Its bins are mapped to the bins of the sub-problem with the same bin types; the bins which don't fit anymore are dropped, and their items are inserted again. The items which stick out of their bin are repaired by the separator.

Iterated local search
---------------------

The initial layout is separated, with more effort than the next separations. While the separations fail, the search goes on as follows:

1. the layout of the failed separation is added to a *population* of failed layouts;
2. a layout of the population is drawn by binary tournament;
3. it is *disrupted*: the engine changes it a lot locally, e.g. by moving a large item somewhere else;
4. it is separated. This separation is given up early if its loss doesn't get below the loss of the drawn layout.

The population keeps layouts which have a small loss and which are different from each other, as in hybrid genetic search: each layout is ranked by its loss and by its contribution to the diversity, its distance to its closest neighbors in the population. The distance between two layouts is provided by the engine. The population holds between 4 and 8 layouts; when it is full, the layouts with the worst ranks are removed, except the 2 layouts with the smallest losses.

In anytime mode, the search goes on until the time limit. In the non-anytime modes, it stops after a given number of failed separations (and, for the direct trigonometry engine, after a given number of item moves); the instance is then considered infeasible.

Automatic selection
-------------------

The direct trigonometry engine is part of the automatic selection, for the objectives it supports, alongside the tree search. The linear and nonlinear programming engines are only run when they are selected explicitly. See :ref:`algorithm selection <internals_algorithm_selection>`.

Source files
------------

* :code:`src/irregular/local_search.hpp`, :code:`src/irregular/local_search.cpp`: the iterated local search and the conversion of the initial solutions
* :code:`src/irregular/sequential_feasibility.hpp`, :code:`src/irregular/sequential_feasibility.cpp`: the sequential feasibility algorithm

References:

* "A Hybrid Genetic Algorithm for Multidepot and Periodic Vehicle Routing Problems" (Vidal, Crainic, Gendreau, Lahrichi and Rei, 2012), for the management of the population

  * https://doi.org/10.1287/opre.1120.1048

.. toctree::
   :maxdepth: 1
   :hidden:

   local_search_direct_trigonometry
   local_search_linear_programming
   local_search_nonlinear_programming
