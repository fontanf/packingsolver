.. _internals_irregular_local_search_linear_programming:

Local search: linear programming engine
=======================================

This page describes the separator of the linear programming engine of the :ref:`local search <internals_irregular_local_search>`.

Principle
---------

Instead of overlapping, the items may be *shrunk*: each item :math:`i` has a scale :math:`\lambda_i \in [0, 1]`, and an item with scale 1 has its full size. A layout is feasible when all the items have their full size, without overlap. A separation solves a sequence of linear programs, each one moving the items slightly and maximizing their weighted scales, while keeping all the (shrunk) items separated.

Shrunk items
------------

The items are decomposed into convex parts. In an item with scale :math:`\lambda`, each convex part is scaled by :math:`\lambda` around its *center*, the average of its vertices. Since the center is in the part, a scaled part is in the part, so a shrunk item is in the item. With a null scale, the parts are reduced to their centers.

The *loss* of a layout is the area lost by shrinking the items: :math:`\sum_i a_i (1 - \lambda_i)`, where :math:`a_i` is the area of item :math:`i`. In the rare case where the shrunk items still overlap beyond a small tolerance, the loss is the total area of the items plus the largest penetration.

Linear program of a step
------------------------

A step builds a linear program for each bin, around the current layout. The orientations of the items are fixed.

**Input**:

* the items :math:`i` of the bin; for each item :math:`i`: its current position :math:`\bar{p}_i`, the maximum move :math:`\delta_i` (a *movement ratio* times the largest side of its bounding box), its area :math:`a_i` and its guided local search weight :math:`g_i`
* the convex parts of the items; for each part :math:`P` of an item :math:`i(P)`: its vertices and its center :math:`c_P` (the average of its vertices), relative to the reference point of the item
* the obstacles of the bin (borders, defects and fixed items), also decomposed into convex parts, which don't move
* the pairs of parts which may get close to each other during the step (their bounding boxes, expanded by the maximum moves and the spacing, intersect); for each pair, the selected edge :math:`e` of one of the two parts, with its outward normal :math:`n`, and the vertex :math:`v` of the other part which is the closest to it, both relative to the reference points of their items
* the spacing :math:`s` of each pair, the bounding box of the bin and the item-bin spacing

The position in the bin of a vertex :math:`u` of a part :math:`P` of an item at position :math:`p` with scale :math:`\lambda` is :math:`p + c_P + \lambda (u - c_P)`. For an obstacle, it is the vertex itself.

**Variables**:

* :math:`p_i \in \mathbb{R}^2`: the position of the reference point of item :math:`i`, in the box :math:`\bar{p}_i \pm \delta_i`, and such that the centers of its parts are in the bin
* :math:`\lambda_i \in [0, 1]`: the scale of item :math:`i`

**Objective**: maximize the weighted scales of the items

.. math::

   \max \sum_i g_i \, a_i \, \lambda_i

**Constraints**:

* Separation: for each pair of parts :math:`(P, Q)` with the selected edge :math:`e` of :math:`P` and the vertex :math:`v` of :math:`Q`, the vertex is beyond the line through the edge, by at least the spacing

.. math::

   n \cdot \big( p_{i(Q)} + c_Q + \lambda_{i(Q)} (v - c_Q) \big)
   - n \cdot \big( p_{i(P)} + c_P + \lambda_{i(P)} (e - c_P) \big)
   \ge s

* Containment: for each part :math:`P`, the bounding box :math:`[x^{\min}_P, x^{\max}_P] \times [y^{\min}_P, y^{\max}_P]` of the scaled part is in the bin, minus the item-bin spacing; for the left side of the bin, for example:

.. math::

   x_{i(P)} + c_{P,x} + \lambda_{i(P)} (x^{\min}_P - c_{P,x}) \ge X^{\min} + s_{\text{bin}}

For a fixed edge, the closest vertex doesn't depend on the translations and the scales of the parts, so the constraint is linear in the positions and the scales. It proves that the two parts don't overlap: the line separates them.

**Always feasible.** The linear program always has a solution: the current positions with null scales. With null scales, the parts are reduced to their centers, which are in the bin. And the edge of each pair of parts is selected among the edges whose constraint is satisfied by the centers of the two parts (with the spacing): the one which separates the two parts the best in the current layout. When two parts don't overlap, the edge which separates them the best always separates their centers, since the centers are in the parts; so the restriction only matters for the parts which overlap. If no edge separates the centers (two centers closer than the spacing, or a center in an obstacle), the edge which separates the parts the best is selected, and the linear program may be infeasible.

This is the separation model of the :ref:`nonlinear programming engine <internals_irregular_local_search_nonlinear_programming>`, where the separating line of each pair of parts is a variable, with the direction of the line fixed during the step.

The linear programs are solved with HiGHS, without warm start.

Separation
----------

A separation solves linear programs as long as they improve the loss. The movement ratio acts as a trust region: it starts at 5%, and after a linear program which doesn't improve the loss, the layout goes back to the best one found and the movement ratio is halved, down to 0.01%. The separation stops after 4 linear programs in a row without improvement (2 while the loss is above the reference loss of the iterated local search, 24 for the initial layout).

After each step, the guided local search weight of each item which is still shrunk is increased by 1, so that the items which stay shrunk are favored by the next linear programs.

The separation succeeds when all the items have their full size and the layout, checked with the exact shapes at full scale, has no overlap.

Initial layout and disruption
-----------------------------

When a solution is loaded, its items keep their positions with their full size; the missing items are inserted at random positions, with a null scale. They are placed so that their bounding box at full size is in the bin, so that the centers of their parts are in the bin.

A layout is disrupted by taking the most shrunk item out and inserting it again in a random bin, at a random position, in a random allowed orientation, with a null scale. This is the only way for an item to change orientation.

The distance between two layouts, used by the population of the iterated local search, is the mean over the items of 1 if the item is in a different bin or orientation, and otherwise of its displacement divided by the square root of its area, capped at 1.

Strengths and limitations
-------------------------

Each linear program only moves the items slightly, so an item can't jump to a free space far away, except through the disruptions. On the literature strip packing benchmarks, the engine is clearly behind the :ref:`direct trigonometry engine <internals_irregular_local_search_direct_trigonometry>`, whose moves sample the whole bins. It is not part of the automatic selection.

It can be selected with :code:`--use-local-search-linear-programming 1` (CLI) or :code:`use_local_search_linear_programming` (Python).

Source files
------------

* :code:`src/irregular/linear_programming/separation_model.hpp`, :code:`src/irregular/linear_programming/separation_model.cpp`: the linear program of a step
* :code:`src/irregular/linear_programming/separator.hpp`, :code:`src/irregular/linear_programming/separator.cpp`: the separator
