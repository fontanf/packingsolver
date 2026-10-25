.. _internals_irregular_milp_raster:

MILP raster
===========

Objectives
----------

This algorithm solves the :code:`feasibility` and :code:`knapsack` objectives, with one or several bins.

Principle
---------

The bins are discretized into a grid of square cells, and the items are only placed at positions of the grid. The problem then becomes a set packing problem: select placements of the items, such that no two placements cover the same cell, maximizing the profit of the items placed. It is solved as a mixed-integer linear program (MILP). The grid is then refined, and the program solved again.

.. image:: ../../img/irregular_milp_raster.png
   :scale: 100%
   :align: center

Rasterization
-------------

The rasterization is conservative, so that the solutions of the program are feasible packings:

* a cell is *covered* by an item as soon as the item covers a part of it, however small;
* a cell is *unavailable* in a bin as soon as a border of the bin, a defect or a fixed item covers a part of it, and the cells covered by fixed items are left to them.

For each item type and each of its orientations (the allowed angles, or a finite set of angles for the item types with continuous rotations), the item is rasterized once. Its *placements* are all the translations of this pattern by whole cells such that all its cells are available.

Model
-----

**Input**:

* the cells :math:`G` of the grids of the bins
* the placements :math:`P`; for each placement :math:`p`, its item type :math:`j(p)` and the set of cells :math:`C_p \subseteq G` it covers
* the item types :math:`j = 1, \ldots, n`; for each item type :math:`j`, its profit :math:`v_j`, and its minimum and maximum numbers of copies :math:`q^{\min}_j` and :math:`q_j` (without its fixed copies)

**Variables**:

* :math:`x_p \in \{0, 1\}`, :math:`p \in P`: :math:`x_p = 1` iff placement :math:`p` is selected

**Objective**: maximize the profit of the items placed

.. math::

   \max \sum_{p \in P} v_{j(p)} \, x_p

**Constraints**:

* Copies: each item type is placed between its minimum and its maximum number of copies. For the :code:`feasibility` objective, the minimum is the number of copies, so all the items must be placed.

.. math::

   \forall j \qquad q^{\min}_j \le \sum_{p \,:\, j(p) = j} x_p \le q_j

* Cell conflicts: each cell is covered by at most one placement.

.. math::

   \forall c \in G \qquad \sum_{p \,:\, c \, \in \, C_p} x_p \le 1

Grid refinement
---------------

The initial size of the cells is computed from the coordinates of the vertices of the shapes: if they are all multiples of a common value, the size of the cells is this value times a power of two, so that, once the grid has been refined down to this value, the shapes made of axis-aligned edges on this grid are rasterized exactly. The size of the cells is halved after each program, until the time limit.

The programs are solved with HiGHS. For the :code:`knapsack` objective, the solutions found by the solver are reported as they are found, and the solver stops as soon as its bound shows that it can't improve the best solution found.

Strengths and limitations
-------------------------

The rasterization wastes space along the boundaries of the items, so the algorithm is well suited to items made of axis-aligned edges on a common grid, like polyominoes and calendar puzzles, where it is exact. On general shapes, the number of cells, and so the size of the program, grows quickly as the grid is refined.

It is not part of the automatic selection. It can be selected with :code:`--use-milp-raster 1` (CLI) or :code:`use_milp_raster` (Python).

Source files
------------

* :code:`src/irregular/milp_raster.hpp`, :code:`src/irregular/milp_raster.cpp`
