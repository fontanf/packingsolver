.. _internals_rectangleguillotine_sequential_strips_onedimensional:

Sequential strips / one-dimensional
===================================

Objectives
----------

This algorithm solves the :code:`bin-packing` and :code:`bin-packing-with-leftovers` objectives, with a single bin type.

Principle
---------

It is a two-phase heuristic:

1. Generate a set of strips by solving a strip packing problem: pack all the items in a bin of the height of the bin and of infinite width, minimizing the width used. The width of a first-level sub-plate, a strip, is limited to the width of the bin. It is solved with the :ref:`column generation strips <internals_rectangleguillotine_column_generation_strips>` algorithm, with the :code:`open-dimension-x` objective.
2. Solve a :ref:`one-dimensional <internals_onedimensional>` problem, with the objective of the instance, where each strip becomes an item, its width being its length, and each bin becomes a one-dimensional bin, its width being its length. It decides how the strips are grouped into bins.

The second phase is run each time the first phase finds a new solution, and each complete solution is reported.

The solution is then built by placing, in each bin, the strips assigned to it side by side.

Contrary to the tree search with maximal spaces algorithm and to the column generation strips algorithm, the strips generated contain exactly all the items.

.. image:: ../../img/rectangleguillotine_sequential_strips_onedimensional.png
   :scale: 100%
   :align: center

The strips of the first phase are chosen to minimize the total width, not to be grouped well into bins, so the result is a heuristic solution.

The description above is for vertical first-level cuts; horizontal ones are handled the same way along y. When the orientation of the first stage is free, both are run in parallel.

Automatic selection
-------------------

It is not part of the automatic selection. It can be selected with :code:`--use-sequential-strips-onedimensional 1` (CLI) or :code:`use_sequential_strips_onedimensional` (Python).

Source files
------------

* :code:`src/rectangleguillotine/sequential_strips_onedimensional.hpp`, :code:`src/rectangleguillotine/sequential_strips_onedimensional.cpp`
