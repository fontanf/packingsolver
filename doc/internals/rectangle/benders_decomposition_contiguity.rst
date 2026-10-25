.. _internals_rectangle_benders_decomposition_contiguity:

Benders decomposition with contiguity
=====================================

Objectives
----------

This algorithm solves the :code:`knapsack` and :code:`feasibility` objectives, for a single bin. It doesn't model the defects, the weights and the eligibilities.

It is an exact method: when it converges, the solution found is optimal.

Principle
---------

It is a combinatorial Benders decomposition, following Côté, Dell'Amico and Iori (2014) for the strip packing problem, as adapted by Wang, Baldacci, Furini, Wei and Liu (2025) to a bin of fixed size.

Unlike the :ref:`Benders decomposition <internals_rectangle_benders_decomposition>`, whose master problem only sees the area of the items, the master problem here also decides the x-position of each item, and the subproblem only has to find y-positions consistent with them.

Master problem
--------------

The bin is cut into unit-width columns, and each item, in one of its orientations, into as many unit-width slices as its width. Each item selected is assigned to a run of consecutive columns, its x-position, and the total height of the items covering each column must not exceed the height of the bin. This is the *contiguous* relaxation of the problem (the "P|cont|Cmax" parallel processor scheduling relaxation): a solution of the master problem is not necessarily a packing, since the items don't have y-positions.

**Input**:

* a bin of width :math:`W` and height :math:`H`, cut into columns :math:`x = 0, \ldots, W - 1`
* the *units* :math:`u`, one per copy of each item type; for each unit :math:`u`: its profit :math:`p_u`, and its *candidate placements* :math:`(x, o)`: its left column :math:`x` and its orientation :math:`o`, with width :math:`w_{u,o}` and height :math:`h_{u,o}`
* the no-good cuts found so far :math:`\mathcal{C}`; each cut is given by a set of units :math:`S` and, for each unit :math:`u \in S`, a set of its candidate placements :math:`C_u`: the units of :math:`S` can't all be placed at placements of their sets (for the :code:`knapsack` objective, :math:`C_u` contains all the placements of :math:`u`)

The candidate left columns of each unit are the *meet-in-the-middle patterns* of Côté and Iori (2018), rather than all the integer positions, which keeps the model tractable.

**Variables**:

* :math:`\rho_{u,x,o} \in \{0, 1\}`: :math:`\rho_{u,x,o} = 1` iff unit :math:`u` is placed with its left side at column :math:`x`, in orientation :math:`o`

**Objective**: maximize the profit of the units placed (:code:`knapsack`; no objective for :code:`feasibility`)

.. math::

   \max \sum_{u,x,o} p_u \, \rho_{u,x,o}

**Constraints**:

* Units: each unit is placed at most once (exactly once for the :code:`feasibility` objective)

.. math::

   \forall u \qquad \sum_{x,o} \rho_{u,x,o} \le 1

* Column heights: the units covering a column fit in the height of the bin

.. math::

   \forall x' \qquad \sum_{(u,x,o) \,:\, x \le x' < x + w_{u,o}} h_{u,o} \, \rho_{u,x,o} \le H

* No-good cuts

.. math::

   \forall (S, C) \in \mathcal{C} \qquad \sum_{u \in S} \; \sum_{(x,o) \in C_u} \rho_{u,x,o} \le |S| - 1

The master problem is solved either by a dedicated branch-and-bound (the default), in the spirit of the numerically exact algorithm of Wang et al. (2025), or by this 0-1 MILP.

Subproblem
----------

The subproblem is the *y-check*: given the x-positions (and orientations) of the selected items, is there a set of y-positions making a valid packing? It is solved with the combinatorial branch-and-bound of Côté, Dell'Amico and Iori (2014), based on the skyline of the packed items.

If the y-check fails, for the :code:`knapsack` objective, the same set of items is first checked at other x-positions, by a *local Benders decomposition*: the algorithm itself is called on a :code:`feasibility` instance with exactly these items. If it finds a packing, it is optimal, since the profit only depends on the items selected. Otherwise, the set of items is infeasible at any position, and a no-good cut forbidding it is added to the master problem.

For the :code:`feasibility` objective, the no-good cut of a failed y-check is strengthened following Wang et al. (2025): the set of items is first shrunk (by binary splits along vertical lines, by sweeps, then by removing the items one at a time) to a smaller set which still fails, then the forbidden x-window of each remaining item is widened as far as it provably stays infeasible, with a linear program.

Automatic selection
-------------------

It is not part of the automatic selection. It can be selected with :code:`--use-benders-decomposition-contiguity 1` (CLI) or :code:`use_benders_decomposition_contiguity` (Python).

Source files
------------

* :code:`src/rectangle/benders_decomposition_contiguity.hpp`, :code:`src/rectangle/benders_decomposition_contiguity.cpp`: the decomposition and the y-check
* :code:`src/rectangle/onedimentional_contiguity/`: the master problem
* :code:`src/algorithms/meet_in_the_middle.hpp`: the meet-in-the-middle patterns

References:

* "Combinatorial Benders' Cuts for the Strip Packing Problem" (Côté, Dell'Amico and Iori, 2014)

  * https://doi.org/10.1287/opre.2013.1248

* "The Meet-in-the-Middle Principle for Cutting and Packing Problems" (Côté and Iori, 2018)

  * https://doi.org/10.1287/ijoc.2018.0806

* "Exact Algorithms for Two-Dimensional Knapsack Problems: A Unified Framework with New Benchmark Results" (Wang, Baldacci, Furini, Wei and Liu, 2025)

  * https://doi.org/10.1287/ijoc.2025.1423
