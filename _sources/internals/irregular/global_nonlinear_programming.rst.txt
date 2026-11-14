.. _internals_irregular_global_nonlinear_programming:

Global nonlinear programming
============================

Objectives
----------

This algorithm solves the :code:`feasibility` objective with a single bin: it finds a packing of all the items, or proves that there is none. Unlike the :ref:`tree search on relative positions <internals_irregular_tree_search_relative_positions>`, it handles the continuous rotations.

It requires SCIP: PackingSolver must be built with :code:`-DPACKINGSOLVER_USE_SCIP=ON`. In the :code:`exact` mode, it is selected automatically if an item type has continuous rotations.

Supported instances: a convex polygonal bin, polygonal items (with holes), any allowed rotation ranges (continuous ranges or single angles), the item-item and item-bin spacings. Not supported: several bins, non-convex bins, defects, fixed items, mirrored rotations, circular arcs.

Model
-----

The lengths are scaled so that the bin fits in the unit square.

**Variables**, for each item :math:`i`:

* :math:`(x_i, y_i)`: the position of its center, the average of the vertices of its shapes; it is in the convex hull of the item, so in the bin;
* :math:`(c_i, s_i) \in [-1, 1]^2`: the cosine and the sine of its angle.

A point :math:`v` of item :math:`i`, given relatively to its center, is at

.. math::

   p_i(v) = \big(x_i + c_i v_x - s_i v_y, \; y_i + s_i v_x + c_i v_y\big),

which is linear in the variables of the item.

**Rotations**: :math:`c_i^2 + s_i^2 = 1`. The angles of an allowed range of width :math:`w` and middle :math:`m` are the points of the unit circle such that

.. math::

   c_i \cos m + s_i \sin m \ge \cos(w / 2).

A range of width 360 degrees adds no constraint, and a single angle (:math:`w = 0`) fixes :math:`(c_i, s_i)`. With several ranges, a binary variable selects the range, and the constraints of the other ranges are relaxed.

**Containment**: for each vertex :math:`v` of the convex hull of each shape of item :math:`i`, and each edge of the bin, of outward normal :math:`n` and offset :math:`h` (shrunk by the item-bin spacing), :math:`n \cdot p_i(v) \le h`. These constraints are linear.

**Non-overlap**: the items are partitioned into convex parts. Two convex parts don't overlap iff a line separates them. For each pair of parts :math:`P` of item :math:`i` and :math:`Q` of item :math:`j \ne i`, the separating line, of normal :math:`(a, b) \in [-1, 1]^2` and offset :math:`d`, is a variable:

.. math::

   a \, p_{i,x}(v) + b \, p_{i,y}(v) \le d \quad \forall v \in P, \qquad
   a \, p_{j,x}(w) + b \, p_{j,y}(w) \ge d \quad \forall w \in Q.

These constraints are bilinear. A separating line remains one when its coefficients are scaled, so the normal only has to be nonzero: :math:`a^2 + b^2 \ge 1`. With an item-item spacing :math:`\delta`, the normal is a unit vector, :math:`a^2 + b^2 = 1`, and the constraints have :math:`d - \delta / 2` and :math:`d + \delta / 2` as right-hand sides.

**Symmetries**:

* the identical items are sorted by their x-coordinates;
* if the bin is centrally symmetric and all the items can be rotated by any angle, rotating a packing by 180 degrees around the center of the bin gives a packing, so the first item, which has the smallest x-coordinate among its identical items, is in the left half of the bin.

The program has no objective. It is solved to global optimality by the spatial branch-and-bound of SCIP, which branches on the continuous variables of the bilinear terms and on the binary variables.

Subsets
-------

If a subset of the items doesn't fit in the bin, neither do all of them. As in the tree search on relative positions, the program is first solved on subsets of the items: on each pair of item types, by decreasing area, then on the increasing subsets of the largest items. The algorithm stops at the first subset proven infeasible, which is the infeasible subset of the output. The solution of the program with all the items is converted into a packing, which is checked before being returned.

Small subsets are often proven infeasible quickly when two large items can't both fit in the bin. The program with three items or more is much harder: the relaxations of the bilinear terms are weak until the domains of most of the variables of each pair of parts are small.

Tolerances
----------

The results are exact up to the tolerances of SCIP: a packing in which the items overlap by less than its feasibility tolerance (:math:`10^{-6}` relatively to the size of the bin) may be missed, so the infeasibility proofs hold up to this tolerance. Conversely, the solutions are converted back to the original coordinates and checked, and a solution in which the items overlap isn't returned.

Performance
-----------

The algorithm is a baseline for the exact algorithms with continuous rotations, which are only able to solve problems with a few items. The same limit is reported in the literature: Cherri, Cherri and Soler (2018), with a model with separating edges selected by binary variables, and Jones (2014), with a relaxation in which circles inscribed in the items mustn't overlap, solve problems with two or three items, and sometimes four.

References
----------

* Cherri, L. H., Cherri, A. C., Soler, E. M. (2018). Mixed integer quadratically-constrained programming model to solve the irregular strip packing problem with continuous rotations. *Journal of Global Optimization*, 72(1), 89–107. `doi:10.1007/s10898-018-0638-x <https://doi.org/10.1007/s10898-018-0638-x>`_
* Jones, D. R. (2014). A fully general, exact algorithm for nesting irregular shapes. *Journal of Global Optimization*, 59(2–3), 367–404. `doi:10.1007/s10898-013-0129-z <https://doi.org/10.1007/s10898-013-0129-z>`_
