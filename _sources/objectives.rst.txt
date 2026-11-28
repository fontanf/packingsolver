.. _objectives:

Objectives
==========

First, the objectives can be classified in two categories:

* **input minimization**: pack/cut all items using as few space as possible
* **output maximization**: pack/cut as many items as possible inside all the provided containers

There is a single output maximization objective called **knapsack**.

There is also a **feasibility** objective that simply checks whether a valid packing exists.

For input minimization, there are 3 objective types (each with a variant which values the leftovers, see :ref:`leftovers`):

* **Open-dimension**: there is a single bin and the objective is to minimize the space used in this bin
* **Bin packing**: there are multiple bins, the objective is to use as few bins as possible; bins must be used in the order they are provided
* **Variable-sized bin packing**: there are multiple bins, the objective is to use as few bins as possible; there is no constraint in the order bins are used

The objective of an instance is set as follows (here, bin packing; the examples below use the :ref:`rectangle<rectangle>` problem type):

.. tab-set::
   :sync-group: interface

   .. tab-item:: Online solver
      :sync: web

      Choose the **Objective** of the project.

   .. tab-item:: JSON
      :sync: json

      In the instance: ``"objective": "bin-packing"``.

   .. tab-item:: Python
      :sync: python

      ``instance_builder.set_objective(psr.Objective.BinPacking)``

   .. tab-item:: C++
      :sync: cpp

      ``instance_builder.set_objective(Objective::BinPacking);``

Bin packing
-----------

In bin packing problems, the objective is to **pack/cut all items** in **as few bins as possible**.
Bins must be used **in the order they are provided**.

PackingSolver supports two variants of this type of objective:

* :code:`bin-packing`: minimize the number of bins
* :code:`bin-packing-with-leftovers`: minimize the number of bins; and then maximize the value of the leftover of the last bin of the solution (see :ref:`leftovers`)

**Example**

In this example, there are two bin types. The first bin type has 10 copies. To use bins of the second bin type, the solution should already contain the first 10 copies of the first bin type. The solver only needs to use 2 copies of the first bin type to pack all items. Therefore, only the first bin type is used. Since the objective is bin packing with leftovers, the solver optimizes the leftover of the last bin of the solution.

.. image:: img/objective_bin_packing_with_leftovers_solution.png
   :align: center

.. example-tabs:: rectangle/objective_bin_packing_with_leftovers

Variable-sized bin packing
--------------------------

In variable-sized bin packing problems, the objective is to **pack/cut all items** in **the cheapest bins as possible**.
There is **no constraint in the order** bins are used.

Each bin has an associated cost. If no cost is provided, then the cost of a bin is its space (length, area, volume).

**Example**

The input is the same as the previous one. With the variable-sized bin packing objective, the solver can use the second bin type to minimize the overall waste.

.. image:: img/objective_variable_sized_bin_packing_solution.png
   :align: center

.. example-tabs:: rectangle/objective_variable_sized_bin_packing

Variable-sized bin packing with leftovers
-----------------------------------------

The :code:`variable-sized-bin-packing-with-leftovers` objective also accounts for what goes back into stock after packing the items: the leftovers of the bins which are used (see :ref:`leftovers`). It is available for all the problem types.

The objective is to minimize the **net cost** of the bins: the cost of the bins which are used, minus a **refund** for each of their leftovers. A leftover is refunded at the price of its bin per unit of space (the cost of the bin divided by its area, or its volume), for the space equivalent to its value: the value of the leftover times a factor which makes the largest bin type worth its own space. With the default ``squared-area`` value type, a leftover of area :math:`a` in a bin of cost :math:`c` and area :math:`A` is refunded :math:`c / A \times a^2 / A_{\max}`, where :math:`A_{\max}` is the area of the largest bin type.

Therefore:

* the cost of the bins and the value of their leftovers are traded off in a single quantity, whether costs are set for the bin types or not (if no cost is set, the cost of a bin is its space, and the net cost is the space of the bins which is not refunded);
* a leftover is refunded less than its space, and the smaller it is, the less it is refunded per unit of space: a large leftover as large as the largest bin type is refunded in full, and a single large leftover is worth more than several smaller ones of the same total space.

The solutions are found by the algorithms of the :code:`variable-sized-bin-packing` objective; only the comparison of the solutions differs. Since there is no bound on the net cost, the solver never proves a solution optimal for this objective. The net cost of a solution is given in the field ``NetCost`` of the output.

**Example**

The bin types are the same as in the previous examples, with fewer items, and no cost is set for the bin types. Both the variable-sized bin packing objective and the variable-sized bin packing with leftovers objective use a bin of each type, for a cost of 1000 × 500 + 700 × 400 = 780000. With the variable-sized bin packing objective, the space left is spread over the two bins. With the variable-sized bin packing with leftovers objective, the large bin is filled entirely, and the space left forms a single 250 × 400 leftover on the right of the small bin. It is refunded 100000² / 500000 = 20000, for a net cost of 760000.

.. image:: img/objective_variable_sized_bin_packing_with_leftovers_solution.png
   :align: center

.. example-tabs:: rectangle/objective_variable_sized_bin_packing_with_leftovers

Open-dimension
--------------

In open-dimension problems, the objective is to **pack/cut all items** in a single bin using **as few space as possible** on the bin. As few space as possible may for example mean:

* minimize the width used
* minimize the height used
* minimize the rectangular area used
* minimize the circular area used

PackingSolver currently supports the objectives:

* :code:`open-dimension-x`: minimize the width used
* :code:`open-dimension-y`: minimize the height used
* :code:`open-dimension-z`: minimize the height used, for the :ref:`box<box>` problem type
* :code:`open-dimension-xy`: minimize the area used, with a given aspect ratio, for the :ref:`irregular<irregular>` problem type

The open-dimension objectives are available for all the problem types except :ref:`one-dimensional<onedimensional>`.

**Example**

.. image:: img/objective_open_dimension_x_solution.png
   :align: center

.. example-tabs:: rectangle/objective_open_dimension_x

Knapsack
--------

In knapsack problems, the objective is to **maximize the value of the items packed/cut** inside **all the provided containers**.

Each item has an associated profit. If no profit is provided, then the profit of an item is its space (length, area, volume).

**Example**

The items are the same as in the previous examples. Only 2 copies of the small bin type are available. This is not enough to pack all the items.

.. image:: img/objective_knapsack_solution.png
   :align: center

.. example-tabs:: rectangle/objective_knapsack

Feasibility
-----------

In feasibility problems, the objective is simply to **determine whether a valid packing exists** — no optimization is performed.

The solver returns a solution as soon as it finds one valid placement of all items, or reports that no feasible packing was found.
