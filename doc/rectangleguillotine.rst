.. _rectangleguillotine:

:code:`rectangle-guillotine` solver
===================================

The :code:`rectangle-guillotine` solver solves problems where items are two-dimensional rectangles that must be cut from rectangular bins using **guillotine cuts** — cuts that go all the way from one side of the current plate to the other.

.. image:: ../img/rectangleguillotine.png
   :width: 512pt
   :align: center

These problems occur for example in glass cutting, wooden panel cutting, and paper cutting industries.

Features:

* Objectives:

  * Knapsack
  * Bin packing
  * Bin packing with leftovers
  * Open dimension X
  * Open dimension Y
  * Variable-sized bin packing

* Item types

  * Item rotation (90°)
  * Stacks (items that must stay grouped)

* Bin types

  * Trims (border offsets on each side)
  * Rectangular defects

* Cutting constraints

  * Number of cutting stages
  * Cut type (Roadef2018, NonExact, Exact, Homogenous)
  * First stage orientation (horizontal or vertical)
  * Minimum and maximum distances between cuts
  * Maximum number of 1-cuts per strip
  * Maximum number of 2-cuts per strip
  * Cut thickness

Guillotine vs non-guillotine patterns
-----------------------------------------

A cutting pattern is a **guillotine pattern** if it can be produced by a sequence of straight cuts, each going all the way from one edge of the current plate to the opposite edge (see `Cutting stages`_ below). A pattern that cannot be produced this way, however the items are arranged, is a **non-guillotine pattern**.

The :code:`rectangle-guillotine` solver only produces guillotine patterns. Problems where non-guillotine patterns are required (or simply allowed) should instead be modeled with the :ref:`rectangle<rectangle>` solver, which places items freely.

The following example solves the same instance — four items of clearly different shapes (8×3, 4×5, 5×4 and 7×6) arranged in a "pinwheel" around a tiny unused central rectangle — with both solvers (:code:`bin-packing` objective). The :code:`rectangle` solver, which is free to produce non-guillotine patterns, packs all 4 items into a single 12×9 bin using the pinwheel arrangement, leaving almost no waste. The :code:`rectangle-guillotine` solver cannot reproduce this pattern with straight cuts, however the items are arranged, so it needs a second bin.

.. |rectangleguillotine_gvng_rectangle| image:: img/rectangleguillotine_guillotine_vs_non_guillotine_rectangle.png
   :scale: 50%

.. |rectangleguillotine_gvng_rectangleguillotine| image:: img/rectangleguillotine_guillotine_vs_non_guillotine_rectangleguillotine.png
   :scale: 50%

.. literalinclude:: examples/rectangleguillotine/guillotine_vs_non_guillotine_rectangle/items.csv
   :caption: items.csv

.. literalinclude:: examples/rectangleguillotine/guillotine_vs_non_guillotine_rectangle/bins.csv
   :caption: bins.csv

.. literalinclude:: examples/rectangleguillotine/guillotine_vs_non_guillotine_rectangle/parameters.csv
   :caption: parameters.csv

.. list-table::
   :widths: 1 1
   :header-rows: 1
   :align: center

   * - ``rectangle``
     - ``rectangle-guillotine``
   * - .. code-block:: shell

            packingsolver_rectangle \
                    --items items.csv \
                    --bins bins.csv \
                    --parameters parameters.csv \
                    --certificate solution.csv
     - .. code-block:: shell

            packingsolver_rectangleguillotine \
                    --items items.csv \
                    --bins bins.csv \
                    --parameters parameters.csv \
                    --certificate solution.csv
   * - |rectangleguillotine_gvng_rectangle|
     - |rectangleguillotine_gvng_rectangleguillotine|

Basic usage
--------------

The :code:`rectangle-guillotine` solver takes as input:

* an item CSV file; option: ``--items items.csv``
* a bin CSV file; option: ``--bins bins.csv``
* optionally a defect CSV file; option: ``--defects defects.csv``
* optionally a parameter CSV file; option: ``--parameters parameters.csv``

It outputs:

* a solution CSV file; option: ``--certificate solution.csv``

The **item file** contains:

* The width of the item type (**mandatory**)

  * column ``WIDTH``
  * **Integer value**

* The height of the item type (**mandatory**)

  * column ``HEIGHT``
  * **Integer value**

* The number of copies of the item type

  * column ``COPIES``
  * default value: ``1``

* The profit of an item of this type (for a knapsack objective)

  * column ``PROFIT``
  * default value: item area (``WIDTH * HEIGHT``)

The **bin file** contains:

* The width of the bin type (**mandatory**)

  * column ``WIDTH``
  * **Integer value**

* The height of the bin type (**mandatory**)

  * column ``HEIGHT``
  * **Integer value**

* The number of copies of the bin type

  * column ``COPIES``
  * default value: ``1``

* The minimum number of copies that must be used

  * column ``COPIES_MIN``
  * default value: ``0``

* The cost of a bin of this type (for a variable-sized bin packing objective)

  * column ``COST``
  * default value: bin area

The **parameter file** has two columns: ``NAME`` and ``VALUE``. The possible entries are:

* The objective; name: ``objective``; possible values:

  * ``knapsack``
  * ``bin-packing``
  * ``bin-packing-with-leftovers``
  * ``open-dimension-x``
  * ``open-dimension-y``
  * ``variable-sized-bin-packing``

Inputs:

.. literalinclude:: examples/rectangleguillotine/items.csv
   :caption: items.csv

.. literalinclude:: examples/rectangleguillotine/bins.csv
   :caption: bins.csv

.. literalinclude:: examples/rectangleguillotine/parameters.csv
   :caption: parameters.csv

Solve:

.. code-block:: shell

    packingsolver_rectangleguillotine \
            --items items.csv \
            --bins bins.csv \
            --parameters parameters.csv \
            --certificate solution.csv

.. literalinclude:: examples/rectangleguillotine/output.txt

Visualize:

.. code-block:: shell

    python3 scripts/visualize_rectangleguillotine.py solution.csv

.. image:: img/rectangleguillotine_example_solution.png
   :width: 512pt
   :align: center

Cutting stages
--------------

A **guillotine cut** divides a plate into two sub-plates. Repeated cuts create a hierarchy of plates:

* **Stage 1**: cuts that divide the original bin into vertical (or horizontal) **strips**
* **Stage 2**: cuts inside each strip, perpendicular to stage-1 cuts
* **Stage 3**: cuts inside the resulting sub-strips (exact cutting only)

The number of stages controls how many levels of cuts are applied. Three-stage cutting is the most common in practice.

* The number of cutting stages; name: ``number_of_stages``; default value: ``3``

The following example packs 3 items (10×4, 6×6 and 4×6) into 10×10 bins (:code:`bin-packing` objective). With only 2 stages, the 6×6 and 4×6 items cannot be combined on the same shelf (that would require a stage-3 cut), so the 3 items no longer fit together and a second bin is needed. With 3 stages, a stage-3 cut splits the top shelf into the 6×6 and 4×6 items side by side, and all 3 items fit into a single bin.

.. |rectangleguillotine_stages_no| image:: img/rectangleguillotine_stages_no.png
   :scale: 50%

.. |rectangleguillotine_stages_yes| image:: img/rectangleguillotine_stages_yes.png
   :scale: 50%

.. list-table::
   :widths: 1 1
   :header-rows: 1
   :align: center

   * - 2 stages
     - 3 stages
   * - .. literalinclude:: examples/rectangleguillotine/stages_no/items.csv
          :caption: items.csv
     - .. literalinclude:: examples/rectangleguillotine/stages_yes/items.csv
          :caption: items.csv
   * - .. literalinclude:: examples/rectangleguillotine/stages_no/bins.csv
          :caption: bins.csv
     - .. literalinclude:: examples/rectangleguillotine/stages_yes/bins.csv
          :caption: bins.csv
   * - .. literalinclude:: examples/rectangleguillotine/stages_no/parameters.csv
          :caption: parameters.csv
     - .. literalinclude:: examples/rectangleguillotine/stages_yes/parameters.csv
          :caption: parameters.csv
   * - .. code-block:: shell

            packingsolver_rectangleguillotine \
                    --items items.csv \
                    --bins bins.csv \
                    --parameters parameters.csv \
                    --certificate solution.csv
     - .. code-block:: shell

            packingsolver_rectangleguillotine \
                    --items items.csv \
                    --bins bins.csv \
                    --parameters parameters.csv \
                    --certificate solution.csv
   * - |rectangleguillotine_stages_no|
     - |rectangleguillotine_stages_yes|

Cut types
---------

* ``roadef2018``: pattern from the 2018 ROADEF challenge; stage-2 cuts produce only items of identical height; trimming cuts are allowed
* ``non-exact``: more flexible; stage-3 cuts are not required; some waste is allowed in sub-plates
* ``exact``: items must fill their sub-plate exactly with no waste at stage 3
* ``homogenous``: all items in a strip have the same height

The cut type is set via the ``cut_type`` key in the parameters CSV file, using one of the values above.

The following example packs the same 10 items (one item type has 2 copies, so that ``homogenous`` — which allows several items to share a sub-plate only if they are all copies of the same item type — has a chance to place more than one item on a given sub-plate) into 10×10 bins with the :code:`bin-packing-with-leftovers` objective, which minimizes the number of bins first and, among solutions using that many bins, maximizes the leftover value of the last bin. Solving it with each of the 4 cut types gives 4 different results:

* ``roadef2018``: 3 bins, leftover value 30 — the deepest cutting level mixes a same-width 2-item pairing with a single-item-plus-waste pairing, a pattern only ``roadef2018`` treats as free (it does not count against the 3-stage budget)
* ``non-exact``: 3 bins, leftover value 12 — ties ``roadef2018`` on bin count (its own single-item-plus-waste pattern is also free), but cannot reproduce the same-width pairing, so it reaches the leftover bin through a different, less efficient cut
* ``exact``: 4 bins, leftover value 0 — never gets a free deepest level, so this instance's 4-level cuts count fully, pushing it one bin over ``non-exact``
* ``homogenous``: 4 bins, leftover value 12 — same bin count as ``exact``, but a different leftover value; the two copies of the same item type are placed together on a shared sub-plate (visible in the second bin below), which ``homogenous`` allows since they are the same item type, unlike the mismatched-type pairings used by the other cut types

.. |rectangleguillotine_cuttype_roadef2018| image:: img/rectangleguillotine_cuttype_roadef2018.png
   :scale: 50%

.. |rectangleguillotine_cuttype_nonexact| image:: img/rectangleguillotine_cuttype_nonexact.png
   :scale: 50%

.. |rectangleguillotine_cuttype_exact| image:: img/rectangleguillotine_cuttype_exact.png
   :scale: 50%

.. |rectangleguillotine_cuttype_homogenous| image:: img/rectangleguillotine_cuttype_homogenous.png
   :scale: 50%

.. literalinclude:: examples/rectangleguillotine/cuttype_roadef2018/items.csv
   :caption: items.csv

.. literalinclude:: examples/rectangleguillotine/cuttype_roadef2018/bins.csv
   :caption: bins.csv

.. list-table::
   :widths: 1 1
   :header-rows: 1
   :align: center

   * - ``roadef2018``
     - ``non-exact``
   * - .. literalinclude:: examples/rectangleguillotine/cuttype_roadef2018/parameters.csv
          :caption: parameters.csv
     - .. literalinclude:: examples/rectangleguillotine/cuttype_nonexact/parameters.csv
          :caption: parameters.csv
   * - .. code-block:: shell

            packingsolver_rectangleguillotine \
                    --items items.csv \
                    --bins bins.csv \
                    --parameters parameters.csv \
                    --certificate solution.csv
     - .. code-block:: shell

            packingsolver_rectangleguillotine \
                    --items items.csv \
                    --bins bins.csv \
                    --parameters parameters.csv \
                    --certificate solution.csv
   * - |rectangleguillotine_cuttype_roadef2018|
     - |rectangleguillotine_cuttype_nonexact|

.. list-table::
   :widths: 1 1
   :header-rows: 1
   :align: center

   * - ``exact``
     - ``homogenous``
   * - .. literalinclude:: examples/rectangleguillotine/cuttype_exact/parameters.csv
          :caption: parameters.csv
     - .. literalinclude:: examples/rectangleguillotine/cuttype_homogenous/parameters.csv
          :caption: parameters.csv
   * - .. code-block:: shell

            packingsolver_rectangleguillotine \
                    --items items.csv \
                    --bins bins.csv \
                    --parameters parameters.csv \
                    --certificate solution.csv
     - .. code-block:: shell

            packingsolver_rectangleguillotine \
                    --items items.csv \
                    --bins bins.csv \
                    --parameters parameters.csv \
                    --certificate solution.csv
   * - |rectangleguillotine_cuttype_exact|
     - |rectangleguillotine_cuttype_homogenous|

First stage orientation
---------------------------

The first stage orientation controls whether stage-1 cuts (see `Cutting stages`_ above) are vertical or horizontal.

* The first stage orientation; name: ``first_stage_orientation``; possible values: ``vertical`` or ``horizontal``

The following example packs 4 items (4×3, 4×7, 6×5 and 6×4) into 10×10 bins with only 2 cutting stages (:code:`bin-packing` objective). The two 4-wide items share a column, stacking to exactly fill a 4×10 vertical strip; the two 6-wide items share a second 6×10 vertical strip, stacking to a height of 9 with some waste. With ``first_stage_orientation`` set to ``vertical``, stage-1 cuts run along these two columns and all 4 items fit into a single bin. With ``horizontal``, stage-1 cuts run the other way instead: since no two items share the same height, each stage-1 row can only hold one item at that depth, and a second bin is needed.

.. |rectangleguillotine_orientation_no| image:: img/rectangleguillotine_orientation_no.png
   :scale: 50%

.. |rectangleguillotine_orientation_yes| image:: img/rectangleguillotine_orientation_yes.png
   :scale: 50%

.. list-table::
   :widths: 1 1
   :header-rows: 1
   :align: center

   * - ``horizontal``
     - ``vertical``
   * - .. literalinclude:: examples/rectangleguillotine/orientation_no/items.csv
          :caption: items.csv
     - .. literalinclude:: examples/rectangleguillotine/orientation_yes/items.csv
          :caption: items.csv
   * - .. literalinclude:: examples/rectangleguillotine/orientation_no/bins.csv
          :caption: bins.csv
     - .. literalinclude:: examples/rectangleguillotine/orientation_yes/bins.csv
          :caption: bins.csv
   * - .. literalinclude:: examples/rectangleguillotine/orientation_no/parameters.csv
          :caption: parameters.csv
     - .. literalinclude:: examples/rectangleguillotine/orientation_yes/parameters.csv
          :caption: parameters.csv
   * - .. code-block:: shell

            packingsolver_rectangleguillotine \
                    --items items.csv \
                    --bins bins.csv \
                    --parameters parameters.csv \
                    --certificate solution.csv
     - .. code-block:: shell

            packingsolver_rectangleguillotine \
                    --items items.csv \
                    --bins bins.csv \
                    --parameters parameters.csv \
                    --certificate solution.csv
   * - |rectangleguillotine_orientation_no|
     - |rectangleguillotine_orientation_yes|

Rotation
--------

Each item may individually be allowed to be rotated by 90° or have a fixed orientation.

By default, item rotation is allowed.

* Whether the item is fixed in its original orientation (cannot be rotated 90°)

  * column ``ORIENTED``
  * ``0``: rotation allowed (default)
  * ``1``: item is oriented; rotation not allowed

The following example packs 2 copies of a 6×10 item into 10×12 bins (:code:`bin-packing` objective). Without rotation, two items side by side need 12 of width (more than the bin's 10) and stacked need 20 of height (more than the bin's 12), so they cannot share a bin: 2 bins are needed. With rotation allowed, both items are turned on their side (10×6) and stacked exactly into a single 10×12 bin.

.. |rectangleguillotine_rotation_no| image:: img/rectangleguillotine_rotation_no.png
   :scale: 50%

.. |rectangleguillotine_rotation_yes| image:: img/rectangleguillotine_rotation_yes.png
   :scale: 50%

.. list-table::
   :widths: 1 1
   :header-rows: 1
   :align: center

   * - Without rotation
     - With rotation
   * - .. literalinclude:: examples/rectangleguillotine/rotation_no/items.csv
          :caption: items.csv
     - .. literalinclude:: examples/rectangleguillotine/rotation_yes/items.csv
          :caption: items.csv
   * - .. literalinclude:: examples/rectangleguillotine/rotation_no/bins.csv
          :caption: bins.csv
     - .. literalinclude:: examples/rectangleguillotine/rotation_yes/bins.csv
          :caption: bins.csv
   * - .. literalinclude:: examples/rectangleguillotine/rotation_no/parameters.csv
          :caption: parameters.csv
     - .. literalinclude:: examples/rectangleguillotine/rotation_yes/parameters.csv
          :caption: parameters.csv
   * - .. code-block:: shell

            packingsolver_rectangleguillotine \
                    --items items.csv \
                    --bins bins.csv \
                    --parameters parameters.csv \
                    --certificate solution.csv
     - .. code-block:: shell

            packingsolver_rectangleguillotine \
                    --items items.csv \
                    --bins bins.csv \
                    --parameters parameters.csv \
                    --certificate solution.csv
   * - |rectangleguillotine_rotation_no|
     - |rectangleguillotine_rotation_yes|

Cut thickness
-----------------

* The width of the saw blade; name: ``cut_thickness``; default value: ``0``

The following example packs 3 items (10×4, 10×3 and 10×3) into 10×10 bins (:code:`bin-packing` objective), stacked to exactly fill the bin height. With no cut thickness, all 3 items fit in a single bin. Each cut also consumes material equal to the saw blade width: with a cut thickness of 1, the two cuts separating the 3 items eat into the available height, so the third item no longer fits and a second bin is needed.

.. |rectangleguillotine_cutthickness_no| image:: img/rectangleguillotine_cutthickness_no.png
   :scale: 50%

.. |rectangleguillotine_cutthickness_yes| image:: img/rectangleguillotine_cutthickness_yes.png
   :scale: 50%

.. list-table::
   :widths: 1 1
   :header-rows: 1
   :align: center

   * - ``cut_thickness`` = 0
     - ``cut_thickness`` = 1
   * - .. literalinclude:: examples/rectangleguillotine/cutthickness_no/items.csv
          :caption: items.csv
     - .. literalinclude:: examples/rectangleguillotine/cutthickness_yes/items.csv
          :caption: items.csv
   * - .. literalinclude:: examples/rectangleguillotine/cutthickness_no/bins.csv
          :caption: bins.csv
     - .. literalinclude:: examples/rectangleguillotine/cutthickness_yes/bins.csv
          :caption: bins.csv
   * - .. literalinclude:: examples/rectangleguillotine/cutthickness_no/parameters.csv
          :caption: parameters.csv
     - .. literalinclude:: examples/rectangleguillotine/cutthickness_yes/parameters.csv
          :caption: parameters.csv
   * - .. code-block:: shell

            packingsolver_rectangleguillotine \
                    --items items.csv \
                    --bins bins.csv \
                    --parameters parameters.csv \
                    --certificate solution.csv
     - .. code-block:: shell

            packingsolver_rectangleguillotine \
                    --items items.csv \
                    --bins bins.csv \
                    --parameters parameters.csv \
                    --certificate solution.csv
   * - |rectangleguillotine_cutthickness_no|
     - |rectangleguillotine_cutthickness_yes|

Trims
-----

Trims model a reserved border around the bin (e.g., for clamping or edge defects). They prevent items from being placed within the specified distance of each edge.

* A **hard** trim is physically cut away: the trim strip is counted as waste.
* A **soft** trim is reserved but not cut: waste is only counted from the first actual cut inward.

* Border trims: distances from each edge before any item can be placed

  * columns ``BOTTOM_TRIM``, ``TOP_TRIM``, ``LEFT_TRIM``, ``RIGHT_TRIM`` in the bin CSV file
  * **Integer values**, default: ``0``

* Trim types: whether trimming strips are cut (hard) or just reserved (soft)

  * columns ``BOTTOM_TRIM_TYPE``, ``TOP_TRIM_TYPE``, ``LEFT_TRIM_TYPE``, ``RIGHT_TRIM_TYPE`` in the bin CSV file
  * ``Hard``: the trim is physically cut; the strip is waste
  * ``Soft``: the trim is reserved but not cut; default for top and right trims

The following example packs 3 items (4×8, 3×8 and 3×8) into 10×10 bins (:code:`bin-packing` objective), side by side to exactly fill the bin width. Without any trim, all 3 items fit in a single bin. Reserving a 1-unit trim on all four edges (left, right, bottom and top) leaves only 8 of usable width, which is no longer enough for the three items to sit side by side, so a second bin is needed.

.. |rectangleguillotine_trims_no| image:: img/rectangleguillotine_trims_no.png
   :scale: 50%

.. |rectangleguillotine_trims_yes| image:: img/rectangleguillotine_trims_yes.png
   :scale: 50%

.. list-table::
   :widths: 1 1
   :header-rows: 1
   :align: center

   * - Without trim
     - ``LEFT_TRIM`` = 2
   * - .. literalinclude:: examples/rectangleguillotine/trims_no/items.csv
          :caption: items.csv
     - .. literalinclude:: examples/rectangleguillotine/trims_yes/items.csv
          :caption: items.csv
   * - .. literalinclude:: examples/rectangleguillotine/trims_no/bins.csv
          :caption: bins.csv
     - .. literalinclude:: examples/rectangleguillotine/trims_yes/bins.csv
          :caption: bins.csv
   * - .. literalinclude:: examples/rectangleguillotine/trims_no/parameters.csv
          :caption: parameters.csv
     - .. literalinclude:: examples/rectangleguillotine/trims_yes/parameters.csv
          :caption: parameters.csv
   * - .. code-block:: shell

            packingsolver_rectangleguillotine \
                    --items items.csv \
                    --bins bins.csv \
                    --parameters parameters.csv \
                    --certificate solution.csv
     - .. code-block:: shell

            packingsolver_rectangleguillotine \
                    --items items.csv \
                    --bins bins.csv \
                    --parameters parameters.csv \
                    --certificate solution.csv
   * - |rectangleguillotine_trims_no|
     - |rectangleguillotine_trims_yes|

Defects
-------

Defects are rectangular regions inside a bin where items cannot be placed.

Defects are specified in the defects CSV file; option: ``--defects defects.csv``. The **defect file** contains the same columns as for the :ref:`rectangle<rectangle>` solver:

* The bin type that contains the defect (**mandatory**)

  * column ``BIN``
  * It refers to the bin type by its position (0-indexed) in the bins file

* The X coordinate of the bottom-left corner of the defect (**mandatory**)

  * column ``X``
  * **Integer value**

* The Y coordinate of the bottom-left corner of the defect (**mandatory**)

  * column ``Y``
  * **Integer value**

* The width of the defect (**mandatory**)

  * column ``WIDTH``
  * **Integer value**

* The height of the defect (**mandatory**)

  * column ``HEIGHT``
  * **Integer value**

Whether cuts may pass through defects is controlled via the ``cut_through_defects`` key in the parameters CSV file (``0`` by default; ``1`` to allow it).

The following example packs 2 copies of a 10×6 item into 10×12 bins (:code:`bin-packing` objective), stacked to fill each bin exactly with no waste. Without any defect, one bin is enough. Adding a small 2×2 defect in the middle of the join between the two items leaves no room to shift either item out of the way, since there is no slack left in the bin: one of the two items no longer fits, so a second bin is needed.

.. |rectangleguillotine_defects_no| image:: img/rectangleguillotine_defects_no.png
   :scale: 50%

.. |rectangleguillotine_defects_yes| image:: img/rectangleguillotine_defects_yes.png
   :scale: 50%

.. list-table::
   :widths: 1 1
   :header-rows: 1
   :align: center

   * - Without defects
     - With defects
   * - .. literalinclude:: examples/rectangleguillotine/defects_no/items.csv
          :caption: items.csv
     - .. literalinclude:: examples/rectangleguillotine/defects_yes/items.csv
          :caption: items.csv
   * - .. literalinclude:: examples/rectangleguillotine/defects_no/bins.csv
          :caption: bins.csv
     - .. literalinclude:: examples/rectangleguillotine/defects_yes/bins.csv
          :caption: bins.csv
   * -
     - .. literalinclude:: examples/rectangleguillotine/defects_yes/defects.csv
          :caption: defects.csv
   * - .. literalinclude:: examples/rectangleguillotine/defects_no/parameters.csv
          :caption: parameters.csv
     - .. literalinclude:: examples/rectangleguillotine/defects_yes/parameters.csv
          :caption: parameters.csv
   * - .. code-block:: shell

            packingsolver_rectangleguillotine \
                    --items items.csv \
                    --bins bins.csv \
                    --parameters parameters.csv \
                    --certificate solution.csv
     - .. code-block:: shell

            packingsolver_rectangleguillotine \
                    --items items.csv \
                    --bins bins.csv \
                    --defects defects.csv \
                    --parameters parameters.csv \
                    --certificate solution.csv
   * - |rectangleguillotine_defects_no|
     - |rectangleguillotine_defects_yes|

Stacks
------

Items with the same stack id must be produced contiguously, in the order they appear in the item file: all copies of an item type must be cut before moving on to the next item type of the same stack. This models a physical stack of items (e.g. glued or stapled together) that must be separated in a fixed sequence. By default, each item type forms its own single-item stack, so no ordering constraint applies.

* The stack identifier; items with the same stack id must remain contiguous in the solution

  * column ``STACK_ID``
  * default value: no stack grouping

The following example packs 4 items of height 5 into 10×10 bins (:code:`bin-packing` objective): two items of width 6 and 4 that together fill one 10×5 shelf, and two items of width 3 and 7 that together fill another 10×5 shelf. Without stacking constraints, the solver is free to group the items by shelf and packs everything into a single bin. Assigning all 4 items to the same stack, in an order that alternates between the two shelves (6, 3, 4, 7), forces the items to be produced in that exact sequence; since two items from different shelves can no longer be produced back-to-back, the shelves can no longer both be completed in a single bin, and a second bin is needed.

.. |rectangleguillotine_stacks_no| image:: img/rectangleguillotine_stacks_no.png
   :scale: 50%

.. |rectangleguillotine_stacks_yes| image:: img/rectangleguillotine_stacks_yes.png
   :scale: 50%

.. list-table::
   :widths: 1 1
   :header-rows: 1
   :align: center

   * - Without stacks
     - With stacks
   * - .. literalinclude:: examples/rectangleguillotine/stacks_no/items.csv
          :caption: items.csv
     - .. literalinclude:: examples/rectangleguillotine/stacks_yes/items.csv
          :caption: items.csv
   * - .. literalinclude:: examples/rectangleguillotine/stacks_no/bins.csv
          :caption: bins.csv
     - .. literalinclude:: examples/rectangleguillotine/stacks_yes/bins.csv
          :caption: bins.csv
   * - .. literalinclude:: examples/rectangleguillotine/stacks_no/parameters.csv
          :caption: parameters.csv
     - .. literalinclude:: examples/rectangleguillotine/stacks_yes/parameters.csv
          :caption: parameters.csv
   * - .. code-block:: shell

            packingsolver_rectangleguillotine \
                    --items items.csv \
                    --bins bins.csv \
                    --parameters parameters.csv \
                    --certificate solution.csv
     - .. code-block:: shell

            packingsolver_rectangleguillotine \
                    --items items.csv \
                    --bins bins.csv \
                    --parameters parameters.csv \
                    --certificate solution.csv
   * - |rectangleguillotine_stacks_no|
     - |rectangleguillotine_stacks_yes|

Minimum and maximum distances between cuts
-----------------------------------------------

* The minimum distance between first-level (stage-1) cuts; name: ``minimum_distance_1_cuts``; default value: ``0``
* The maximum distance between first-level cuts; name: ``maximum_distance_1_cuts``; ``-1`` for no limit
* The minimum distance between second-level cuts; name: ``minimum_distance_2_cuts``; default value: ``0``
* The maximum distance between second-level cuts; name: ``maximum_distance_2_cuts``; ``-1`` for no limit (default); not allowed when ``number_of_stages == 2``
* The minimum length for any waste piece; name: ``minimum_waste_length``; default value: ``0``

Maximum number of 1-cuts per strip
------------------------------------

* The maximum number of stage-1 cuts; name: ``maximum_number_1_cuts``; ``-1`` for no limit (default); must not be ``0``

The following example packs 3 copies of a 3×10 item into 10×10 bins with only 2 cutting stages (:code:`bin-packing` objective). Without a limit, the 3 items are placed side by side using 2 stage-1 cuts, all in a single bin. Setting ``maximum_number_1_cuts`` to 1 allows only 2 strips, so only 2 of the 3 items fit, and a second bin is needed for the third.

.. |rectangleguillotine_maxcuts_no| image:: img/rectangleguillotine_maxcuts_no.png
   :scale: 50%

.. |rectangleguillotine_maxcuts_yes| image:: img/rectangleguillotine_maxcuts_yes.png
   :scale: 50%

.. list-table::
   :widths: 1 1
   :header-rows: 1
   :align: center

   * - Without limit
     - ``maximum_number_1_cuts`` = 1
   * - .. literalinclude:: examples/rectangleguillotine/maxcuts_no/items.csv
          :caption: items.csv
     - .. literalinclude:: examples/rectangleguillotine/maxcuts_yes/items.csv
          :caption: items.csv
   * - .. literalinclude:: examples/rectangleguillotine/maxcuts_no/bins.csv
          :caption: bins.csv
     - .. literalinclude:: examples/rectangleguillotine/maxcuts_yes/bins.csv
          :caption: bins.csv
   * - .. literalinclude:: examples/rectangleguillotine/maxcuts_no/parameters.csv
          :caption: parameters.csv
     - .. literalinclude:: examples/rectangleguillotine/maxcuts_yes/parameters.csv
          :caption: parameters.csv
   * - .. code-block:: shell

            packingsolver_rectangleguillotine \
                    --items items.csv \
                    --bins bins.csv \
                    --parameters parameters.csv \
                    --certificate solution.csv
     - .. code-block:: shell

            packingsolver_rectangleguillotine \
                    --items items.csv \
                    --bins bins.csv \
                    --parameters parameters.csv \
                    --certificate solution.csv
   * - |rectangleguillotine_maxcuts_no|
     - |rectangleguillotine_maxcuts_yes|

Maximum number of 2-cuts per strip
------------------------------------

* The maximum number of stage-2 cuts per strip; name: ``maximum_number_2_cuts``; ``-1`` for no limit

The following example packs 4 items (10×3, 10×3, 10×4 and 10×10) into 20×10 bins with only 2 cutting stages (:code:`bin-packing` objective). A first stage-1 cut splits the bin into two 10-wide strips: one holds the 10×10 item alone, the other stacks the three shorter items using 2 stage-2 cuts. Without a limit, all 4 items fit into a single bin. Setting ``maximum_number_2_cuts`` to 1 allows only 2 stage-2 cuts' worth of shelves per strip, so only 2 of the 3 stacked items fit, and a second bin is needed for the third.

.. |rectangleguillotine_maxcuts2_no| image:: img/rectangleguillotine_maxcuts2_no.png
   :scale: 50%

.. |rectangleguillotine_maxcuts2_yes| image:: img/rectangleguillotine_maxcuts2_yes.png
   :scale: 50%

.. list-table::
   :widths: 1 1
   :header-rows: 1
   :align: center

   * - Without limit
     - ``maximum_number_2_cuts`` = 1
   * - .. literalinclude:: examples/rectangleguillotine/maxcuts2_no/items.csv
          :caption: items.csv
     - .. literalinclude:: examples/rectangleguillotine/maxcuts2_yes/items.csv
          :caption: items.csv
   * - .. literalinclude:: examples/rectangleguillotine/maxcuts2_no/bins.csv
          :caption: bins.csv
     - .. literalinclude:: examples/rectangleguillotine/maxcuts2_yes/bins.csv
          :caption: bins.csv
   * - .. literalinclude:: examples/rectangleguillotine/maxcuts2_no/parameters.csv
          :caption: parameters.csv
     - .. literalinclude:: examples/rectangleguillotine/maxcuts2_yes/parameters.csv
          :caption: parameters.csv
   * - .. code-block:: shell

            packingsolver_rectangleguillotine \
                    --items items.csv \
                    --bins bins.csv \
                    --parameters parameters.csv \
                    --certificate solution.csv
     - .. code-block:: shell

            packingsolver_rectangleguillotine \
                    --items items.csv \
                    --bins bins.csv \
                    --parameters parameters.csv \
                    --certificate solution.csv
   * - |rectangleguillotine_maxcuts2_no|
     - |rectangleguillotine_maxcuts2_yes|
