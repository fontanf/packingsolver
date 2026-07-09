.. _internals_algorithm_selection:

Algorithm selection
=====================

For a given instance, each domain solver usually has several applicable algorithms. All of them can be enabled or disabled individually with a ``--use-<algorithm>`` option (e.g. ``--use-tree-search 1``, ``--use-column-generation 0``); several can be enabled at once, in which case they run concurrently (in anytime mode) and the best solution found by any of them is kept, along with the best bound found by any of them.

If none of the ``--use-<algorithm>`` options is set, the solver picks a default combination automatically, based on simple structural properties of the instance:

* **The objective** (knapsack, bin packing, variable-sized bin packing, open-dimension, feasibility, ...): each objective supports a different subset of algorithms; see the :ref:`objectives<objectives>` page.
* **The number of bins available**: with a single bin, the problem reduces to a single knapsack/open-dimension/feasibility instance, and only single-bin algorithms (tree search and its variants) apply. With multiple bins, decomposition algorithms (sequential value correction, dichotomic search, column generation) become relevant.
* **How many items fit in a bin on average**: instances where many items fit per bin behave differently from instances where a bin holds only a handful of items.
* **How many copies of each item type there are on average**: instances with few item types and many copies of each (e.g. cutting stock problems) behave differently from instances with many distinct item types and few copies of each.

The general shape of this heuristic, applied consistently across domains, is:

* **Many copies per item type, and many items fit per bin** → sequential single knapsack: a single, high-effort tree search pass per bin is enough, and repeating it with corrected profits would add little.
* **Many copies per item type, but few items fit per bin** → sequential value correction combined with column generation.
* **Few copies per item type** (many distinct item types) → tree search, combined with column generation for knapsack-like objectives.
* For variable-sized bin packing with several bin types, **dichotomic search** is preferred over column generation, which tends to perform poorly in that case.

The :ref:`optimization mode<optimization_modes>` also affects how these algorithms behave, not just when they stop:

* In **anytime** mode, decomposition algorithms restart in a loop with a growing search effort for their knapsack sub-problems each time, so solution quality keeps improving the longer the solver runs.
* In **non-anytime** modes, each algorithm instead uses a single, fixed search effort (configurable via the corresponding ``--not-anytime-*`` option) and is run deterministically rather than restarted.
