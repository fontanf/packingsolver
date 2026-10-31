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

* :math:`f_0`: the items longer than :math:`C - k` are rounded up to :math:`C`, the items shorter than :math:`k` are rounded down to 0, and the other ones keep their length;
* :math:`f_1`: it counts the items of length between :math:`k` and :math:`C / 2` (the *medium* items), which count for 1, while the items shorter than :math:`k` count for 0. The bin counts for the maximum number of medium items fitting in it, and an item longer than :math:`C / 2` counts for the number of medium items it displaces: this maximum minus the maximum number of medium items fitting in the space it leaves;
* :math:`f_2`: the lengths are rounded down to multiples of :math:`k`, with a correction for the items longer than :math:`C / 2`.

The breakpoints tried are the lengths of the items (and the bin length minus them), and :math:`C / 2`.

For each breakpoint and each family:

* :code:`bin-packing` and :code:`feasibility`: the bound is the total transformed length of the items divided by the transformed length of the bin, rounded up. The best bound is kept. For the :code:`feasibility` objective, the instance is proven infeasible if the bound exceeds the number of bins.
* :code:`knapsack`: the bound is the fractional (Dantzig) bound of the knapsack problem with the transformed lengths: the items sorted by decreasing profit per transformed length, the last one taken fractionally. The smallest bound is kept.

An item packed right after another copy of the same item type only needs its length minus its nesting length, so the items are counted with this reduced length, which keeps the bounds valid. The resources are ignored.

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
