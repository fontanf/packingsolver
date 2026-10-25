.. _internals_rectangleguillotine_column_generation_strips:

Column generation strips
========================

Objectives
----------

This algorithm solves the :code:`knapsack`, :code:`open-dimension-x` and :code:`open-dimension-y` objectives, with a single bin.

It generates k-staged guillotine patterns. It is fast when k = 2. When k >= 4, it becomes impractical. It doesn't support the defects, the stacks, the minimum waste length and the maximum numbers of cuts.

Principle
---------

A k-staged pattern is made of first-level sub-plates, the *strips*, placed side by side, each one being a (k-1)-staged pattern. The algorithm chooses the strips by solving a Dantzig-Wolfe reformulation of the problem, where a variable is a strip, by column generation. Contrary to the tree search with maximal spaces algorithm, the strips are not generated beforehand, but dynamically, by the pricing problem.

Here is an illustration of the algorithm:

.. image:: ../../img/rectangleguillotine_column_generation_strips.png
   :scale: 100%
   :align: center

Model
-----

**Input**:

* a bin of width :math:`W`
* item types :math:`j = 1, \ldots, n`; for each item type :math:`j`: a profit :math:`p_j`, a width :math:`w_j` and a number of copies :math:`q_j`
* a set :math:`K` of feasible strips; for each strip :math:`k \in K`, :math:`x_j^k` is the number of copies of item type :math:`j` it contains

**Variables**:

* :math:`y^k \in \{0, \ldots, q^{\max}\}`, :math:`k \in K`: number of times strip :math:`k` is used

**Objective**: maximize the total profit of the packed items

.. math::

   \max \sum_{k} \Big( \sum_{j} p_j \, x_j^k \Big) y^k

**Constraints**:

* Bin width: the strips selected must fit side by side in the bin

.. math::

   \sum_{k} \Big( \max_{j} w_j x_j^k \Big) y^k \le W

* Item demand: each item type used at most :math:`q_j` times

.. math::

   \forall j \qquad \sum_{k} x_j^k \, y^k \le q_j

For the open dimension objectives, the width of the strips is minimized instead, all the items being packed.

Pricing problem
---------------

The pricing problem consists in finding a strip of negative reduced cost

.. math::

   rc(y^k) = \Big( \max_{j} w_j x_j^k \Big) u + \sum_{j} x_j^k v_j - \sum_{j} p_j x_j^k

where :math:`u` and :math:`v_j` are the dual variables of the bin width and item demand constraints. It is solved for every possible strip width, as a (k-1)-staged guillotine single knapsack problem:

* for 2-staged exact, 2-staged non-exact and 3-staged homogeneous patterns, the strips are solved as one-dimensional knapsack problems;
* otherwise, the same algorithm is applied recursively.

Search
------

The column generation gives the linear relaxation of the model, which is a bound. Integer solutions are found by a limited discrepancy search over the column generation: at each node, a strip of the linear solution is fixed, and the column generation is run again on the residual problem. In anytime mode, the search goes on until the time limit; in the non-anytime modes, it stops by itself.

Automatic selection
-------------------

With a single bin, it is run for the :code:`knapsack` and open dimension objectives with 2-staged patterns, or 3-staged homogeneous ones, and none of the features it doesn't support. It is also used by the :ref:`sequential strips / one-dimensional <internals_rectangleguillotine_sequential_strips_onedimensional>` algorithm. See :ref:`algorithm selection <internals_algorithm_selection>`.

It can be forced with :code:`--use-column-generation-strips 1` (CLI) or :code:`use_column_generation_strips` (Python).

Source files
------------

* :code:`src/rectangleguillotine/column_generation_strips.hpp`, :code:`src/rectangleguillotine/column_generation_strips.cpp`
