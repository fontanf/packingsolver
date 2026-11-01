.. _internals_onedimensional_dual_feasible_functions:

Dual feasible functions
=======================

Objectives
----------

This algorithm computes a bound for the :code:`bin-packing`, :code:`feasibility` and :code:`knapsack` objectives, for instances with a single bin type. It doesn't produce solutions.

Principle
---------

A *dual feasible function* :math:`f` maps lengths to lengths such that, for any set of items fitting in a bin of length :math:`C`, the transformed lengths :math:`f(\ell_i)` fit in a bin of length :math:`f(C)`. Applying such a function to all the items and to the bin gives a new instance, which can't need fewer bins than the original one. Its trivial bound, the total transformed length of the items divided by the transformed length of the bin, is a valid bound for the original instance, and it is often stronger than the trivial bound of the original instance: the functions round the long items up to the full bin and the short items down to 0.

The functions used are the three families of Carlier, Clautiaux and Moukrim (2007), each one parametrized by a breakpoint :math:`k \in [1, C / 2]`:

* :math:`f_0` rounds the long lengths up to the capacity and the short ones down to 0:

.. math::

   f_0(x) = \begin{cases} C & \text{if } x > C - k \\ x & \text{if } k \le x \le C - k \\ 0 & \text{if } x < k \end{cases}

* :math:`f_1` counts the *medium* items, of length between :math:`k` and :math:`C / 2`. Let :math:`M(c)` be the maximum number of medium items fitting in a length :math:`c`, computed greedily from the shortest ones (which is optimal for a cardinality). A medium item counts for 1, a short item for 0, the bin for :math:`M(C)`, and a long item for the number of medium items it displaces:

.. math::

   f_1(x) = \begin{cases} M(C) - M(C - x) & \text{if } x > C / 2 \\ 1 & \text{if } k \le x \le C / 2 \\ 0 & \text{if } x < k \end{cases}

* :math:`f_2` rounds the lengths down to multiples of :math:`k`, counting the long lengths by the room they leave:

.. math::

   f_2(x) = \begin{cases} 2 \left( \lfloor C / k \rfloor - \lfloor (C - x) / k \rfloor \right) & \text{if } x > C / 2 \\ \lfloor C / k \rfloor & \text{if } x = C / 2 \\ 2 \lfloor x / k \rfloor & \text{if } x < C / 2 \end{cases}

The three functions are plotted on the :ref:`rectangle page <internals_rectangle_dual_feasible_functions>`.

The breakpoints tried are the lengths :math:`\ell` of the items not longer than :math:`C / 2`, the values :math:`C - \ell` for the longer ones (the lengths equal to :math:`C` are skipped), and :math:`C / 2`.

For each breakpoint and each family:

* :code:`bin-packing` and :code:`feasibility`: the bound is the total transformed length of the items divided by the transformed length of the bin, rounded up. The best bound is kept. For the :code:`feasibility` objective, the instance is proven infeasible if the bound exceeds the number of bins.
* :code:`knapsack`: the bound is the fractional (Dantzig) bound of the knapsack problem with the transformed lengths: the capacity is the transformed length of the bin times its number of copies, the items are sorted by decreasing profit per transformed length, and the last one is taken fractionally. An item whose transformed length is 0 is always taken; an item which doesn't fit in the bin is never taken. The smallest bound is kept.

An item packed right after another item only needs its length minus its nesting length, so the items are counted with this reduced length, :math:`\ell - \max(0, n)`, which keeps the bounds valid. The resources are ignored; for the :code:`knapsack` objective, the penalties of the *penalize* resources which increase the profit are added to the bound.

Automatic selection
-------------------

The bound is computed automatically when there is a single bin type and at most 100 items. It can be forced with :code:`--use-dual-feasible-functions 1` (CLI) or :code:`use_dual_feasible_functions` (Python).

Source files
------------

* :code:`src/onedimensional/dual_feasible_functions.hpp`, :code:`src/onedimensional/dual_feasible_functions.cpp`

References:

* "New reduction procedures and lower bounds for the two-dimensional bin packing problem with fixed orientation" (Carlier, Clautiaux and Moukrim, 2007)

  * https://doi.org/10.1016/j.cor.2005.08.012

* "A theoretical and experimental study of fast lower bounds for the two-dimensional bin packing problem" (Serairi and Haouari, 2018)

  * https://doi.org/10.1051/ro/2017019
