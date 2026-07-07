.. _internals_overview:

Solver internals
=================

This section is not necessary to use the solver. It is aimed at people interested in understanding how the solver works under the hood. It assumes the reader is familiar with standard approaches to solving optimization problems (mathematical programming, tree search, local search, etc.).

To solve the wide range of problem types and features it supports, `PackingSolver` integrates multiple algorithms, some of which interact with each other. First, let's distinguish between the pattern-generating algorithms and the generic algorithms.

The **pattern-generating algorithms** are specific to a given problem type and generate an actual pattern. They may only solve the :code:`feasibility` objective, but usually also handle other objectives such as :code:`knapsack` or :code:`bin-packing` directly.

The **generic algorithms** are not specific to any problem type. They implement generic strategies that rely on the pattern-generating algorithms to solve sub-problems.

Even for a specific problem variant, different algorithms may work best depending on the characteristics of the instance. Therefore, `PackingSolver` includes multiple algorithms and **selects** the right ones to run based on the instance's characteristics.

The first sections of this part of the documentation describe the pattern-generating algorithms for each problem type. Then the generic algorithms are documented. Finally, the last section describes how algorithm selection works.

.. toctree::
   :maxdepth: 2

   onedimensional
   rectangle
   rectangleguillotine
   box
   boxstacks
   irregular
   generic_algorithms
   algorithm_selection
