.. _internals_irregular_local_search_direct_trigonometry:

Local search: direct trigonometry engine
========================================

This page describes the separator of the direct trigonometry engine of the :ref:`local search <internals_irregular_local_search>`. It follows the separator of Sparrow (Gardeyn and Vanden Berghe, 2025), which is itself built on the collision detection of jagua-rs.

Principle
---------

The items may overlap each other, overlap the defects, the fixed items and the borders of their bin, and stick out of their bin. Each collision is quantified by a *collision loss*, an approximation of the overlap area computed from circles inscribed in the shapes. A separation moves the colliding items one by one, each to the best of many sampled positions, to minimize the total collision loss weighted by guided local search weights.

The engine is called *direct trigonometry* because the overlaps are computed directly from the geometry of the shapes, without linear or nonlinear programs.

Shapes
------

For each item type and each orientation, the shapes are simplified into polygons with fewer vertices, with a small maximum approximation ratio (0.5%). The shapes inflated by the item-item minimum spacing are kept too.

Each polygon is described by *poles*: disjoint circles inscribed in it. The first one is the largest inscribed circle (the pole of inaccessibility); each next one is the largest circle inscribed in the polygon minus the previous circles. Poles are generated until they cover 90% of the area with at least 8 poles, or 80% with at least 16 poles, with at most 64 poles.

The *hazards* of a bin are the obstacles an item may collide with: its borders, its defects (inflated by the spacings), its fixed items, and its exterior.

Collision loss
--------------

Whether two shapes collide is decided exactly, from their polygons. The largest poles, covering half of the area of the item, are checked first, to detect most collisions quickly.

The collision loss of two colliding shapes is computed from their poles, following the Sparrow paper. For each pair of poles, the penetration depth (the sum of the radii minus the distance between the centers) is multiplied by the smallest radius. Penetration depths smaller than :math:`\varepsilon`, 1% of the largest diameter of the two shapes, decay smoothly towards 0 instead of being negative, so that the loss stays positive for colliding shapes whose poles don't overlap. With :math:`O` the sum over the pairs of poles, the loss is

.. math::

   \sqrt{O + \varepsilon^2} \times \sqrt{\sqrt{A_1} \sqrt{A_2}}

where :math:`A_1` and :math:`A_2` are the areas of the convex hulls of the shapes. As in Sparrow, the loss of an item sticking out of its bin is computed from the area :math:`O` of its bounding box outside the bin: :math:`2 \sqrt{O} \sqrt{A}`, where :math:`A` is the area of its convex hull.

Each pair of items, and each pair of an item and a hazard, has a guided local search weight. The *total loss* of a layout is the sum of the collision losses; the *weighted loss*, minimized by the moves, is the sum of the collision losses times their weights.

Moves
-----

At each iteration of a separation, each colliding item, in a random order, is removed from the layout and put back at the best placement found by the following search:

* the current placement, and 25 samples around it, in the same bin;
* 50 samples in random bins, in random allowed orientations;
* the 3 best distinct samples are refined by coordinate descent: the item is moved in small steps along the axes, the step growing (×1.1) after a successful move and shrinking (×0.5) after a failed one; then the best result is refined further with smaller steps.

A placement without collision is always preferred. The evaluation of a candidate placement stops as soon as its weighted loss exceeds the one of the worst of the best candidates.

With several workers (3 by default), each worker moves the colliding items from the current layout, in its own random order, in its own thread, and the layout of the worker with the smallest weighted loss is kept.

Guided local search weights
---------------------------

After each iteration, the weights of the colliding pairs are increased, by a factor between 1.2 and 2, proportional to their collision loss divided by the largest collision loss. The weights of the pairs which don't collide anymore decay by a factor 0.95, down to 1. So the collisions which persist become more and more expensive, and the items involved end up moved further away.

Separation
----------

A separation is a sequence of *strikes*. In a strike, iterations are performed until a counter of iterations without improvement reaches 200: the counter is reset when the smallest total loss found decreases by at least 2%, and incremented when it doesn't decrease. The strike is a failure if it hasn't decreased the total loss by at least 2%. Between strikes, the best layout found is restored, keeping the weights. The separation stops after 3 failed strikes in a row (1 while the loss is above the reference loss of the iterated local search), or as soon as the total loss is 0.

Initial layout and disruption
-----------------------------

When a solution is loaded, the items which stick out of their bin are moved back by removing a slice of the bin at a random position: the items after the slice are shifted towards the slice. The missing items are then inserted one by one, each at the best of 1000 sampled placements, the placements without collision being scored to favor the bottom left of the first bins.

A layout is disrupted by swapping two large items; the items covered by one of them after the swap are moved to the space it left.

The distance between two layouts, used by the population of the iterated local search, describes each item by its bin, its orientation, and the cell of a grid containing the center of its bounding box (the cells of an item type being as large as its smallest dimension). The copies of an item type being interchangeable, the distance is the number of items whose description doesn't match the description of an item of the other layout.

Differences with Sparrow
------------------------

* The rotations are discrete: an item changes orientation by sampling, never by coordinate descent. The items with continuous rotations are considered in a finite set of orientations.
* There is no spatial index: an evaluation goes through the items of the bin, filtered by their bounding boxes.

Automatic selection
-------------------

The engine is run by the automatic selection for the objectives supported by the local search, alongside the tree search. It can be forced with :code:`--use-local-search-direct-trigonometry 1` (CLI) or :code:`use_local_search_direct_trigonometry` (Python).

Source files
------------

* :code:`src/irregular/direct_trigonometry/separator.hpp`, :code:`src/irregular/direct_trigonometry/separator.cpp`: the separator
* :code:`src/irregular/direct_trigonometry/polygon.hpp`, :code:`src/irregular/direct_trigonometry/polygon.cpp`: the polygons, their intersection and their poles

References:

* "An open-source heuristic to reboot 2D nesting research" (Gardeyn and Vanden Berghe, 2025)

  * https://doi.org/10.48550/arXiv.2509.13329

* "Sparrow" (Gardeyn), and its collision detection engine "jagua-rs"

  * https://github.com/JeroenGar/sparrow
  * https://github.com/JeroenGar/jagua-rs
