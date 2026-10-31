.. _internals_generic_algorithms_sequential_value_correction:

Sequential value correction
===========================

Objectives
----------

This algorithm solves the :code:`knapsack`, :code:`bin-packing`, :code:`bin-packing-with-leftovers` and :code:`variable-sized-bin-packing` objectives, with several bins. It is available for all the problem types. For the :code:`variable-sized-bin-packing` objective, it doesn't handle the minimum numbers of copies of the bin types.

Principle
---------

Like the :ref:`sequential single knapsack <internals_generic_algorithms_sequential_single_knapsack>` algorithm, it builds a solution bin by bin, solving a single knapsack sub-problem for each bin with the remaining items, until all the items are packed or there is no bin left. But the profits of the items in the sub-problems are *corrected* from one pass to the next: the items which were packed in bins with a lot of waste get a higher profit, so that they get packed earlier in the next pass, together with other items, and hopefully generate less waste.

In the example below, items :code:`1` (3 copies) and :code:`2` (6 copies) are packed. At iteration 1, item :code:`2` fills two bins perfectly, but item :code:`1` is packed alone in three separate bins, each mostly empty. Since item :code:`1` was packed in high-waste bins, its profit is increased: at iteration 2, it now gets packed earlier and combines with item :code:`2` to fill bins completely, reducing the total number of bins from 5 to 4:

.. image:: ../../img/generic_algorithms_sequential_value_correction.png
   :scale: 100%
   :align: center

Profit correction
-----------------

Initially, the profit of an item is its own profit for the :code:`knapsack` objective, and its space (length, area or volume) raised to the power 1.1 for the other objectives, which breaks the ties between solutions of the same space.

During a pass, each bin spreads its waste over its items, in proportion to their space: an item of space :math:`s` in a bin whose items have a total space :math:`S` and whose waste is :math:`w` gets an *adjusted space* :math:`s + w \, s / S`. After the pass, for each item type :math:`j`:

* for the :code:`knapsack` objective, the new profit is its original profit per unit of space times the mean adjusted space of its packed copies;
* for the other objectives, the new profit is the total adjusted space of its copies divided by its number of copies, each copy left unpacked counting for 100 times the space of the largest bin;

and the profit used in the next pass is the mean of the previous profit and of the new one.

For the :code:`variable-sized-bin-packing` objective, the sub-problem is solved for each bin type, and the bin type with the best ratio between the profit of its solution and its cost is kept. The solutions of the previous bins types are reused when they are still valid.

Search
------

In anytime mode, the algorithm performs passes with corrected profits until the time limit, with a fixed effort for the sub-problems (a beam of size 128 for the tree searches); for the irregular problem type, it first runs with an increasing effort, up to this one, with 3 passes for each effort (parameter :code:`anytime_sequential_value_correction_growing_queue_maximum_number_of_iterations`). In the non-anytime modes, it performs a given number of passes.

For the :code:`knapsack` objective, it stops when all the items are packed. For the :code:`bin-packing` objective, it stops when all the items are packed in at most 2 bins: if all the items didn't fit in a single bin in the first pass, the next passes are very unlikely to manage it.

Automatic selection
-------------------

With several bins, it is run when the item types have many copies compared to the number of items fitting in a bin, but few items fit in a bin, and in some other cases depending on the problem type. See :ref:`algorithm selection <internals_algorithm_selection>`.

It can be forced with :code:`--use-sequential-value-correction 1` (CLI) or :code:`use_sequential_value_correction` (Python).

Source files
------------

* :code:`src/algorithms/sequential_value_correction.hpp`

References:

* "Linear one-dimensional cutting-packing problems: numerical experiments with the sequential value correction method (SVC) and a modified branch-and-bound method (MBB)" (Mukhacheva et al., 2000)

  * https://doi.org/10.1590/S0101-74382000000200002

* "Parallelized sequential value correction procedure for the one-dimensional cutting stock problem with multiple stock lengths" (Cui and Tang, 2014)

  * https://doi.org/10.1080/0305215X.2013.841903

* "Sequential value correction heuristic for the two-dimensional cutting stock problem with three-staged homogenous patterns" (Chen et al., 2016)

  * https://doi.org/10.1080/10556788.2015.1048860
