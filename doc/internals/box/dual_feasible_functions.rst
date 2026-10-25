.. _internals_box_dual_feasible_functions:

Dual feasible functions
=======================

Objectives
----------

This algorithm computes a bound for the :code:`bin-packing`, :code:`feasibility` and :code:`knapsack` objectives, for instances with a single bin type. It doesn't produce solutions.

Principle
---------

It is the generalization to three dimensions of the :ref:`dual feasible functions for rectangle <internals_rectangle_dual_feasible_functions>`, following the framework of Fekete and Schepers (2004): a dual feasible function is applied to each of the three dimensions of the items and of the bin, with its own breakpoint, and the volume bound of the transformed instance is a valid bound for the original instance.

* :code:`bin-packing` and :code:`feasibility`: the bound is the total transformed volume of the items divided by the transformed volume of the bin, rounded up, maximized over the breakpoints and the families of functions. For the :code:`feasibility` objective, the instance is proven infeasible if the bound exceeds the number of bins.
* :code:`knapsack`: the bound is the fractional (Dantzig) bound of the knapsack problem with the transformed volumes, minimized over the breakpoints and the families of functions.

When an item can be rotated, its transformed volume depends on its rotation; the smallest one over its allowed rotations (up to 6) is used, which keeps the bound valid whatever the rotation of the item in the packing.

Automatic selection
-------------------

The number of combinations of breakpoints grows quickly with the number of item types, so the bound is only computed automatically when there is a single bin type and at most 50 items. It can be forced with :code:`--use-dual-feasible-functions 1` (CLI) or :code:`use_dual_feasible_functions` (Python).

Source files
------------

* :code:`src/box/dual_feasible_functions.hpp`, :code:`src/box/dual_feasible_functions.cpp`

References:

* "New reduction procedures and lower bounds for the two-dimensional bin packing problem with fixed orientation" (Carlier, Clautiaux and Moukrim, 2007)

  * https://doi.org/10.1016/j.cor.2005.08.012

* "A general framework for bounds for higher-dimensional orthogonal packing problems" (Fekete and Schepers, 2004)

  * https://doi.org/10.1002/mcda.350
