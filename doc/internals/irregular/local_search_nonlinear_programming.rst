.. _internals_irregular_local_search_nonlinear_programming:

Local search: nonlinear programming engine
==========================================

This page describes the separator of the nonlinear programming engine of the :ref:`local search <internals_irregular_local_search>`. It requires Knitro: PackingSolver must be built with :code:`-DPACKINGSOLVER_USE_KNITRO=ON`.

Principle
---------

As in the :ref:`linear programming engine <internals_irregular_local_search_linear_programming>`, the items may be shrunk instead of overlapping, and a separation solves a sequence of programs, each one moving the items slightly and maximizing their weighted scales. The programs are nonlinear, so that the items may also rotate within their allowed rotation ranges, which is useful for the items with continuous rotations.

The shrunk items, the loss, the separation, the guided local search weights, the initial layout and the disruption are the same as in the linear programming engine. The distance between two layouts also counts the rotations of the items.

Nonlinear program of a step
---------------------------

A step builds a nonlinear program for each bin, around the current layout.

**Input**:

* the items :math:`i` of the bin; for each item :math:`i`: its current position :math:`\bar{p}_i`, the maximum move :math:`\delta_i`, its rotation range :math:`[\theta^{\min}_i, \theta^{\max}_i]` relative to its current angle (at most 5° in each direction, for the item types with continuous rotations, decreasing with the movement ratio; :math:`\{0\}` otherwise), its area :math:`a_i` and its guided local search weight :math:`g_i`
* the convex parts of the items; for each part :math:`P` of an item :math:`i(P)`: its vertices and its center :math:`c_P`, relative to the reference point of the item, at its current angle
* the obstacles of the bin, decomposed into convex parts, which don't move
* the pairs of parts which may get close to each other during the step, with their spacing :math:`s`
* the bounding box of the bin and the item-bin spacing

The position in the bin of a vertex :math:`u` of a part :math:`P` of an item at position :math:`p`, rotated by :math:`\theta`, with scale :math:`\lambda` is

.. math::

   U(u) = p + R(\theta) \big( c_P + \lambda (u - c_P) \big)

where :math:`R(\theta)` is the rotation of angle :math:`\theta`. For an obstacle, it is the vertex itself.

**Variables**:

* :math:`p_i \in \mathbb{R}^2`: the position of the reference point of item :math:`i`, in the box :math:`\bar{p}_i \pm \delta_i`, and such that the centers of its parts are in the bin
* :math:`\theta_i \in [\theta^{\min}_i, \theta^{\max}_i]`: the rotation of item :math:`i` around its reference point
* :math:`\lambda_i \in [0, 1]`: the scale of item :math:`i`
* :math:`\alpha_{PQ} \in \mathbb{R}` and :math:`\beta_{PQ} \in \mathbb{R}`, for each pair of parts :math:`(P, Q)`: the angle of the normal :math:`n(\alpha_{PQ}) = (\cos \alpha_{PQ}, \sin \alpha_{PQ})` and the offset of a line separating the two parts

**Objective**: maximize the weighted scales of the items

.. math::

   \max \sum_i g_i \, a_i \, \lambda_i

**Constraints**:

* Separation: for each pair of parts :math:`(P, Q)`, the vertices of :math:`P` are on one side of the separating line, and the vertices of :math:`Q` on the other side, at least at the spacing

.. math::

   \forall u \in P \qquad \beta_{PQ} - n(\alpha_{PQ}) \cdot U(u) \ge 0

.. math::

   \forall u \in Q \qquad n(\alpha_{PQ}) \cdot U(u) - \beta_{PQ} \ge s

* Containment: the vertices of the parts which may reach the borders of the bounding box of the bin stay inside it, minus the item-bin spacing; for the left side of the bin, for example:

.. math::

   U(u)_x \ge X^{\min} + s_{\text{bin}}

Two convex parts don't overlap if and only if such a line exists, so, unlike in the linear programming engine, no separating edge needs to be selected: the optimization chooses the lines. These are the quasi-phi-functions of Stoyan, Pankratov and Romanova (2016).

The lines are initialized with the lines separating the parts with the largest gap in the current layout (or with the smallest penetration if they overlap), computed exactly from the Minkowski difference of the two parts.

**Always feasible.** As in the linear programming engine, the current positions with null scales are a solution: the parts are then reduced to their centers, which stay in the bin, and two distinct centers can always be separated by a line. The exceptions are a center inside an obstacle, and two centers closer than the spacing.

The programs are solved with the interior point method of Knitro, with tight feasibility tolerances so that the solutions pass the exact overlap checks. The active set and SQP methods of Knitro don't move away from the warm start.

Strengths and limitations
-------------------------

The nonlinear programs are much more expensive than the linear ones: a step can take from a tenth of a second to several seconds, so on large instances the engine barely moves from its initial layout within typical time limits. It is not part of the automatic selection.

It can be selected with :code:`--use-local-search-nonlinear-programming 1` (CLI) or :code:`use_local_search_nonlinear_programming` (Python).

Source files
------------

* :code:`src/irregular/nonlinear_programming/separation_model.hpp`, :code:`src/irregular/nonlinear_programming/separation_model.cpp`: the nonlinear program of a step
* :code:`src/irregular/nonlinear_programming/separating_line.hpp`, :code:`src/irregular/nonlinear_programming/separating_line.cpp`: the initial separating lines
* :code:`src/irregular/nonlinear_programming/separator.hpp`, :code:`src/irregular/nonlinear_programming/separator.cpp`: the separator

References:

* "Quasi-phi-functions and optimal packing of ellipses" (Stoyan, Pankratov and Romanova, 2016)

  * https://doi.org/10.1007/s10898-015-0331-2

* "Cutting and packing problems for irregular objects with continuous rotations: mathematical modelling and non-linear optimization" (Stoyan, Pankratov and Romanova, 2016)

  * https://doi.org/10.1057/jors.2015.94
