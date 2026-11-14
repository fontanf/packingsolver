.. _internals_box_static_stability:

Static stability
================

With the :ref:`static stability <box-static-stability>` constraint, the items of each bin must be in static mechanical equilibrium. This page describes how it is checked, and how the tree search algorithms, which don't model it, repair their solutions so that they satisfy it.

Feasibility criterion
---------------------

The criterion is the static mechanical equilibrium of Galrão Ramos, Oliveira, Gonçalves and Lopes (2016). The items are rigid bodies whose only external force is their weight, applied at their center. An item is supported through the contacts between its bottom face and the floor of the bin or the top faces of the items whose top is at the height of its bottom. Each contact is a rectangle, the intersection of the two faces. Lateral contacts are not taken into account: an item leaning against another one is not supported.

Any pressure on a contact rectangle which only pushes is equivalent to non-negative vertical forces at its four corners. The items are in equilibrium if and only if there exist such forces for which, for each item, the forces are balanced:

* the forces pushing it up (at the contacts below it) minus the forces pushing it down (at the contacts above it) equal its weight;
* their moments about the axes parallel to x and y through its center are zero.

This is a linear program with 4 variables per contact and 3 rows per item, without objective; the bin is stable if and only if it is feasible. The forces are divided by the largest weight of the items and the lever arms by the largest dimension of the bin, so that the coefficients are between -1 and 1. It is solved with HiGHS.

Unlike the checks on the support of each item alone (for example, a minimum supported fraction of its bottom face, or its center of gravity above the convex hull of its contacts), the forces that an item transmits to the items below it are taken into account: an item can be stable on its own support but be tipped by the weight of the items above it.

The linear program is not solved when all the items are on the floor of the bin (the bin is stable), or when an item with a weight has no support (the bin is not stable).

The criterion is checked by :code:`Solution` on each bin, for every solution, whichever algorithm built it. A solution with a bin which is not stable is not feasible.

The algorithms don't model it, so they only prove an infeasibility, and only compute a bound from the value of a solution, with the constraints they model: the copies, the minimum copies and the resources (:code:`Solution::feasible_without_unmodeled_constraints`).

Repair of the solutions
-----------------------

The tree search algorithms often generate solutions with floating items, especially the :ref:`tree search with maximal spaces <internals_box_tree_search_maximal_spaces>`, which places a block on the floor of a maximal space, while the space can extend beyond the items supporting it. Without repair, with this constraint, they may find no stable solution at all.

So, before passing a new solution to the algorithm formatter, :ref:`tree search <internals_box_tree_search>` and :ref:`tree search with maximal spaces <internals_box_tree_search_maximal_spaces>` repair it in two steps. Only the bins which are not stable are changed.

**Drop.** The items fall until they rest on the floor or on another item. They are processed in a single pass, from the lowest to the highest one: an item lands on the highest top face of the items already processed whose footprints intersect its footprint, or on the floor. The items which may stop an item are below it, so they are already at their final positions. The items only move down, and the items whose footprints intersect keep their vertical order, so the solution remains valid, with the same items in the same bins.

**Removal of the unbalanced items.** After the drop, no item floats, but an item may still tip, when its center of gravity is beyond its support, or because of the weight of the items above it. While a bin is not stable:

* the items which can't be balanced are found by solving the equilibrium linear program in which each row may be violated, at a cost equal to the violation (two non-negative slack variables per row, with a cost of 1); the items whose rows are violated in an optimal solution are the unbalanced items;
* the highest of them is removed, with the items which rest on it, directly or not;
* the other items keep their positions, so they remain supported.

In the example below, seen from the beginning of the y-axis (the items span the whole bin along y), the items C, E and F float in the solution of the search. After the drop, C rests on A, E on C and F on E. The center of gravity of E (dashed line) is beyond its contact with C, so E tips: it is removed, with F which rests on it:

.. image:: ../../img/box_static_stability_repair.svg
   :align: center

The removed items are not re-inserted. With the :code:`knapsack` objective, the repaired solution is feasible. With the objectives which require all the items to be packed, it often isn't.

The infeasibility proofs and the bounds of the tree searches are computed from the solution of the search, not from the repaired one, since the repair may remove items.

Source files
------------

* :code:`src/box/static_stability.hpp`, :code:`src/box/static_stability.cpp`: the equilibrium linear program, the drop and the removal of the unbalanced items
* :code:`src/box/solution.cpp`: the check of the criterion on each bin
* :code:`src/box/tree_search.cpp`, :code:`src/box/tree_search_maximal_spaces.cpp`: the calls to the repair
* :code:`scripts/doc_figures/box.py`: the figure of this page

References:

* "A container loading algorithm with static mechanical equilibrium stability constraints" (Galrão Ramos, Oliveira, Gonçalves and Lopes, 2016)

  * https://doi.org/10.1016/j.trb.2016.06.003

* "Standing on a common ground: a comparison of static stability approaches for pallet loading" (Mazur, Gamer, Ramos and Schoder, 2027)

  * https://doi.org/10.1111/itor.70059
