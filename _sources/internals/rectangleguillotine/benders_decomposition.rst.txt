.. _internals_rectangleguillotine_benders_decomposition:

Benders decomposition
=====================

Objectives
----------

This algorithm solves the :code:`knapsack` objective, with one or several bins, and the :code:`bin-packing` and :code:`variable-sized-bin-packing` objectives. It supports the resources of the bins (the penalized ones only when they count the copies of item types up to a threshold: an integer capacity, and consumption schedules :math:`(1, \ldots, 1, 0)`), but not the stacks of several item types.

It is an exact method: when it converges, the solution found is optimal.

Principle
---------

It is a copy of the :ref:`Benders decomposition of the rectangle problem type <internals_rectangle_benders_decomposition>`, with guillotine feasibility subproblems.

The master problem and its cuts are built on the *relaxation* of the instance: the rectangle instance with the same item types and bin types, the bins without their hard trims, and the same resources. Every guillotine packing of a selection of items in a bin is a packing of this selection in the bin of the relaxation, so the cuts of the rectangle Benders decomposition (the dual feasible function cuts, the incompatible pairs and triplets, and the lifting of the no-good cuts with the bar relaxation) are valid for the guillotine problem; they are computed on the relaxation with the functions of the rectangle problem type. The other features of the instance (the guillotine cuts, the number of stages, the cut thickness, the soft trims and the defects) only restrict the packings: they are left to the subproblems.

For each bin of the solution of the master problem, the feasibility subproblem checks whether the items of the bin have a guillotine packing in it: it is a :code:`feasibility` instance with the parameters of the instance, solved by the rectangleguillotine algorithms, mostly the :ref:`tree search hypergraph <internals_rectangleguillotine_tree_search_hypergraph>`, which finds a packing of all the items or proves that there is none in a single search. With guillotine cuts, a selection of 3 items which fits without guillotine cuts may not fit, for example with 2 stages: the search of the minimal infeasible subsets also checks the selections of at most 3 items, and the bins with 3 items are not packed directly in a guillotine arrangement as in the rectangle problem type.

Search parameters
-----------------

They are the ones of the :ref:`rectangle Benders decomposition <internals_rectangle_benders_decomposition>`. In the :code:`exact` mode, its master problem and its feasibility subproblems are also solved in :code:`exact` mode, so that their results are proven.

Automatic selection
-------------------

It is not selected automatically. It can be selected with :code:`--use-benders-decomposition 1` (CLI) or :code:`use_benders_decomposition` (Python).

Source files
------------

* :code:`src/rectangleguillotine/benders_decomposition.hpp`, :code:`src/rectangleguillotine/benders_decomposition.cpp`: the decomposition
