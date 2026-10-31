.. _internals_generic_algorithms:

Generic algorithms
===================

The following algorithms are conceptually the same across all domains: each one solves a multi-bin problem by repeatedly solving single-bin (knapsack) sub-problems of the *same* domain.

Sequential single knapsack
----------------------------

This algorithm solves the :code:`knapsack`, :code:`bin-packing`, :code:`bin-packing-with-leftovers` and :code:`variable-sized-bin-packing` objectives.

It fills the bins one by one, each with a knapsack sub-problem on the remaining items; the content of a bin is repeated in as many bins as the remaining items allow. In anytime mode, after each pass, it restarts with more effort for the sub-problems.

.. image:: ../img/generic_algorithms_sequential_single_knapsack.svg
   :align: center

See :doc:`the detailed description <generic_algorithms/sequential_single_knapsack>`.

Sequential value correction
----------------------------

This algorithm solves the :code:`knapsack`, :code:`bin-packing`, :code:`bin-packing-with-leftovers` and :code:`variable-sized-bin-packing` objectives.

It is closely related to sequential single knapsack, but instead of restarting with a larger search effort, it restarts by increasing the profit of the items that were packed in high-waste bins, so that they get packed earlier in the next pass and hopefully generate less waste.

.. image:: ../img/generic_algorithms_sequential_value_correction.svg
   :align: center

See :doc:`the detailed description <generic_algorithms/sequential_value_correction>`.

Dichotomic search
------------------

This algorithm solves the :code:`variable-sized-bin-packing` objective.

It searches, by dichotomy, for the smallest achievable waste percentage: for a given waste estimate, it deduces which bins would be needed to reach that quantity of waste, and solves a bin packing sub-problem restricted to those bins.

.. image:: ../img/generic_algorithms_dichotomic_search.svg
   :align: center

See :doc:`the detailed description <generic_algorithms/dichotomic_search>`.

Column generation
-------------------

This algorithm solves the :code:`knapsack`, :code:`bin-packing` and :code:`variable-sized-bin-packing` objectives.

It solves the linear relaxation of a model whose variables are the possible contents of the bins (patterns), generated dynamically by column generation, the pricing problems being single-bin knapsack problems. Then a limited discrepancy search fixes the patterns of the relaxation one by one, solving the relaxation of the remaining problem at each node. It also gives a bound.

.. image:: ../img/generic_algorithms_column_generation_search.svg
   :align: center

See :doc:`the detailed description <generic_algorithms/column_generation>`.

Sequential feasibility
-------------------------

This algorithm solves the :code:`bin-packing`, :code:`bin-packing-with-leftovers`, :code:`open-dimension-x`, :code:`open-dimension-y`, :code:`open-dimension-z` and :code:`open-dimension-xy` objectives with an algorithm solving the :code:`feasibility` objective.

It repeatedly solves a :code:`feasibility` sub-problem asking whether all the items fit into a shrinking amount of space, and stops as soon as a sub-problem isn't solved.

.. image:: ../img/generic_algorithms_sequential_feasibility.svg
   :align: center

See :doc:`the detailed description <generic_algorithms/sequential_feasibility>`.

.. toctree::
   :maxdepth: 1
   :hidden:

   generic_algorithms/sequential_single_knapsack
   generic_algorithms/sequential_value_correction
   generic_algorithms/dichotomic_search
   generic_algorithms/column_generation
   generic_algorithms/sequential_feasibility
