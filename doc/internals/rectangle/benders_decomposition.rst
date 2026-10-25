.. _internals_rectangle_benders_decomposition:

Benders decomposition
=====================

Objectives
----------

This algorithm solves the :code:`knapsack` objective, with one or several bins, and the :code:`bin-packing` and :code:`variable-sized-bin-packing` objectives.

It is an exact method: when it converges, the solution found is optimal.

Principle
---------

It is a combinatorial Benders decomposition, in the spirit of Côté, Haouari and Iori (2021). The problem is split in two:

* the *master problem* decides which items go in which bin, ignoring the geometry: it is a one-dimensional problem, where each item is reduced to its area and each bin to its area;
* the *subproblems* check, for each bin of the solution of the master problem, whether its items actually fit together in the bin.

When the items of a bin don't fit, a *cut* is added to the master problem, forbidding this selection of items (and, after strengthening, many others) in a bin of this type, and the master problem is solved again. The algorithm stops when the items of every bin fit: the solution of the master problem is then a feasible solution, and an optimal one since the master problem is a relaxation.

Master problem
--------------

The master problem is a :ref:`onedimensional <internals_onedimensional>` instance: the bins have their area as length, and the items their area as length. It is solved with the :ref:`MILP assignment <internals_onedimensional_milp_assignment>` algorithm (or with the onedimensional tree search).

The cuts, and the geometric information the master problem can't see by itself, are encoded as onedimensional *resources* on the bin types, so that the master problem never proposes a bin violating them:

* an item type which doesn't fit a bin type by its dimensions (e.g. a 1x100 item in a 10x10 bin, although its area is small enough) is not eligible for it;
* two items which fit neither side by side nor one above the other, in any orientation, are incompatible in a bin of this type;
* *precedences*: when an item type dominates another one (at least the same profit, and it fits the footprint of the other one in all its orientations), no copy of the dominated item type is packed unless all the copies of the dominating one are;
* the resources of the original instance are copied as is.

It is the :ref:`MILP assignment <internals_onedimensional_milp_assignment>` model, where the length of an item or a bin is its area, with the following constraints added on each bin instance :math:`(t, k)`. With the variables :math:`x_{i,t,k,c}` of this model (bin instance :math:`(t, k)` contains at least :math:`c` copies of item type :math:`i`), the number of copies of item type :math:`i` in a bin instance, counted up to :math:`n`, is simply :math:`\sum_{c \le n} x_{i,t,k,c}`, so all the cuts are linear.

**Input** (in addition to the ones of the MILP assignment model):

* for each bin type :math:`t`: the item types which don't fit it by their dimensions, the pairs of incompatible item types :math:`\mathcal{I}_t`, and the cuts found so far: the dual feasible function cuts :math:`\mathcal{D}_t`, each one with a coefficient :math:`f_i` for each item type and a right-hand side :math:`F`, and the no-good and triplet cuts :math:`\mathcal{C}_t`, each one forbidding :math:`n_i` copies of each item type :math:`i` of a set :math:`S`, with lifted item types :math:`L` of coefficients :math:`\alpha_j`

**Constraints** (in addition to the ones of the MILP assignment model):

* Eligibility: an item type which doesn't fit bin type :math:`t` by its dimensions has no variable :math:`x_{i,t,k,c}`
* Incompatibility: two incompatible item types (or two copies of a self-incompatible one) are not in the same bin

.. math::

   \forall (i, i') \in \mathcal{I}_t \qquad x_{i,t,k,1} + x_{i',t,k,1} \le 1

* Dual feasible function cuts

.. math::

   \forall D \in \mathcal{D}_t \qquad \sum_{i,c} f_i \, x_{i,t,k,c} \le F

* No-good and triplet cuts: the selection, and any selection containing it, is forbidden

.. math::

   \forall C \in \mathcal{C}_t \qquad \sum_{i \in S} \sum_{c \le n_i} x_{i,t,k,c} + \sum_{j \in L} \alpha_j \, x_{j,t,k,1} \le \sum_{i \in S} n_i - 1

In the implementation, these constraints are encoded as onedimensional resources on the bin types, with per-copy consumptions.

Iterations
----------

At each iteration, the master problem is solved, and the bins of its solution are checked, from the cheapest checks to the most expensive ones:

1. **Dual feasible functions.** For each bin, the most violated inequality derived from the :ref:`dual feasible functions <internals_rectangle_dual_feasible_functions>` is searched. Such an inequality is valid for any selection of items in a bin of this type, so it is added as a permanent cut. If one is found for some bin, the master problem is solved again.
2. **Incompatible triplets.** For each bin, the triplets of items which can never fit together in the bin are searched. Two rectangles in a rectangle can always be separated by a vertical or horizontal line, so two items fit together if and only if they fit side by side or one above the other. For three items, there are 8 guillotine arrangements, and if none of them fits, in any orientations, the three items can't fit together. If such a triplet is found for some bin, it is added as a permanent cut, and the master problem is solved again.
3. **Feasibility subproblems.** For each bin, a :code:`feasibility` instance with the items of the bin is solved with the rectangle algorithms, in non-anytime mode. If the items don't fit, no-good cuts are added.

**No-good cuts.** Instead of forbidding the whole selection of an infeasible bin, the algorithm looks for *minimal infeasible subsets*: subsets of the selection which don't fit, but from which removing any item gives a selection which fits. Forbidding a subset forbids every selection containing it, so these cuts are stronger. The subsets are found by removing the items one at a time (by increasing area) and checking the feasibility again, up to a maximum number of cuts per bin.

Each cut is then *lifted*, following the sequential lifting procedure of Balas (1975) and Wolsey (1975) as adapted by Côté, Haouari and Iori (2021): the item types not in the cut are visited one by one, and each one gets a coefficient in the cut equal to the number of items of the cut its presence in the bin excludes, bounded with the :ref:`bar relaxation <internals_rectangle_bar_relaxation>`.

**Proofs.** A feasibility subproblem may be inconclusive: it neither finds a packing nor proves that there is none within its budget. Its selection is still treated as infeasible, so that the master problem keeps converging, but the cut is marked as unproven. The bound of the master problem is only reported as a bound of the instance while all the cuts are proven.

Example
-------

In the example below, the bin has an area of 15 and items ``1``, ``2`` and ``3`` have an area of 6, 6 and 3 respectively, exactly matching the bin area when all three are selected. At iteration 1, with no cuts yet, the master problem selects all three items, the only selection reaching the maximum possible area, 15. This selection turns out to be infeasible: items ``1`` and ``2`` already leave no room for item ``3``. A no-good cut excluding this combination is then added, and at iteration 2 the master problem selects items ``1`` and ``2`` only (area 12), which is feasible:

.. image:: ../../img/rectangle_benders_decomposition.png
   :scale: 100%
   :align: center

Automatic selection
-------------------

It is not part of the automatic selection. The :ref:`column generation <internals_generic_algorithms_column_generation>` algorithm uses it to solve its pricing problems exactly when it needs to prove a bound.

It can be selected with :code:`--use-benders-decomposition 1` (CLI) or :code:`use_benders_decomposition` (Python).

Source files
------------

* :code:`src/rectangle/benders_decomposition.hpp`, :code:`src/rectangle/benders_decomposition.cpp`: the decomposition
* :code:`src/rectangle/item_subset_incompatibility.hpp`, :code:`src/rectangle/item_subset_incompatibility.cpp`: the incompatible triplets

References:

* "On the Two-Dimensional Knapsack Problem" (Caprara and Monaci, 2004)

  * https://doi.org/10.1016/S0167-6377(03)00057-9

* "A Cutting-Plane Approach for the Two-Dimensional Orthogonal Non-Guillotine Cutting Problem" (Baldacci and Boschetti, 2007)

  * https://doi.org/10.1016/j.ejor.2005.11.060

* "Combinatorial Benders Decomposition for the Two-Dimensional Bin Packing Problem" (Côté, Haouari and Iori, 2021)

  * https://doi.org/10.1287/ijoc.2020.1014

* "Optimisation de plans de palettisation hétérogène" (Le Jean, 2021, PhD thesis, Université Grenoble Alpes)

  * https://theses.fr/2021GRALM032
