.. _internals_generic_algorithms:

Generic algorithms
===================

The following algorithms are conceptually the same across all six domains: each one solves a multi-bin problem by repeatedly solving single-bin (knapsack) sub-problems of the *same* domain. Each domain exposes an option to force it on or off (e.g. ``--use-sequential-value-correction 1``); when no algorithm is explicitly requested, the solver may pick one of them automatically (see :doc:`algorithm_selection`).

Sequential value correction
----------------------------

This algorithm solves the :code:`knapsack`, :code:`bin-packing`, :code:`bin-packing-with-leftovers` and :code:`variable-sized-bin-packing` objectives.

Algorithm for multiple knapsack, bin packing and variable-sized bin packing problems (option ``--use-sequential-value-correction``).

It solves a single knapsack sub-problem for each bin, in turn, until either all items are packed or there is no bin left. It then restarts, increasing the profit of items that were packed in high-waste bins, so that they get packed earlier in the next pass and hopefully generate less waste. It repeats this process, refining item profits each time, either for a fixed number of iterations (non-anytime modes) or until the time limit is reached (anytime mode).

It does not account for a minimum number of copies of each bin type in variable-sized bin packing.

For variable-sized bin packing, this algorithm typically requires less computational effort to reach a good solution than the other applicable algorithms. For multiple knapsack and bin packing, it is particularly useful for problems where the exact tree search cannot directly produce a good solution (e.g. ``boxstacks``).

A closely related variant, **sequential single knapsack** (option ``--use-sequential-single-knapsack``), runs only a single pass of the same process (no profit correction), but with a growing search effort for each knapsack sub-problem. It is preferred when there are many copies of few item types and many items fit per bin, since in that case a single good pass tends to already be close to optimal.

The search effort allotted to each knapsack sub-problem can be tuned via the ``*-subproblem-tree-search-queue-size`` family of options (e.g. ``--sequential-value-correction-subproblem-tree-search-queue-size``).

References:

* "Linear one-dimensional cutting-packing problems: numerical experiments with the sequential value correction method (SVC) and a modified branch-and-bound method (MBB)" (Mukhacheva et al., 2000)
* "Parallelized sequential value correction procedure for the one-dimensional cutting stock problem with multiple stock lengths" (Cui et Tang, 2014)

Dichotomic search
------------------

This algorithm solves the :code:`variable-sized-bin-packing` objective.

Algorithm for variable-sized bin packing problems (option ``--use-dichotomic-search``).

It searches, by dichotomy, for the smallest achievable waste percentage: for a given waste estimate, it deduces which bins would be needed to reach that quantity of waste, and solves a bin packing sub-problem restricted to those bins. If all items pack successfully, the waste estimate is decreased; otherwise, it is increased. The search continues until the estimate converges.

This algorithm is particularly effective for problems with many bin types, or with bins in which many items fit -- situations where column generation tends to struggle.

Column generation
-------------------

This algorithm solves the :code:`knapsack`, :code:`bin-packing`, :code:`bin-packing-with-leftovers` and :code:`variable-sized-bin-packing` objectives.

Algorithm for variable-sized bin packing, bin packing and (multiple) knapsack problems (option ``--use-column-generation``), based on a Dantzig-Wolfe decomposition of the natural set-covering formulation (one variable per bin type per feasible packing pattern, one constraint per item type and per bin type bound).

**Input**:

* bin types :math:`i = 1, \ldots, m`; for each bin type :math:`i`: a lower bound :math:`l_i` and an upper bound :math:`u_i` on its number of copies, and a cost :math:`c_i`
* item types :math:`j = 1, \ldots, n`; for each item type :math:`j`: a number of copies :math:`q_j`
* for each bin type :math:`i`, a set :math:`K_i` of feasible packing patterns; for each pattern :math:`k \in K_i`, :math:`x_{j,i}^k` is the number of copies of item type :math:`j` it contains

**Variables**:

* :math:`y_i^k \in \{0, \ldots, q^{\max}\}`, :math:`i = 1, \ldots, m`, :math:`k \in K_i`: number of times packing pattern :math:`k` of bin type :math:`i` is used

**Objective**: minimize the total cost of the bins used

.. math::

   \min \sum_{i} c_i \sum_{k} y_i^k

**Constraints**:

* Bin type bounds

.. math::

   \forall i \qquad l_i \le \sum_{k} y_i^k \le u_i

* Item demand: each item type is packed exactly :math:`q_j` times

.. math::

   \forall j \qquad \sum_{i} \sum_{k} x_{j,i}^k \, y_i^k = q_j

The pricing sub-problem -- finding a new, profitable packing pattern to add to the linear relaxation -- consists in finding, for some bin type :math:`i`, a column of negative reduced cost

.. math::

   rc(y_i^k) = c_i - u_i - \sum_{j} x_{j,i}^k \, v_j

where :math:`u_i` and :math:`v_j` are the dual variables of the bin type bound and item demand constraints. This reduces to solving a bounded knapsack problem with item profits :math:`v_j`, which is itself a single-bin knapsack instance, solved with the same tree search used elsewhere in the domain. The linear relaxation is explored with a limited discrepancy search, which also yields a bound on the objective (a knapsack bound, an open-dimension bound, or a bin-packing bound, depending on the objective).

The search effort allotted to the pricing sub-problem can be tuned via the ``--column-generation-subproblem-tree-search-queue-size`` family of options.
