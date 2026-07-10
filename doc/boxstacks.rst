.. _boxstacks:

:code:`box-stacks` solver
=========================

The :code:`box-stacks` solver solves three-dimensional bin packing problems where items are rectangular parallelepipeds (boxes) that must be packed into rectangular bins. Items can be stacked vertically: a **stack** is a column of items that all have the same footprint (X and Y dimensions) and the same stackability identifier.

.. image:: ../img/boxstacks.png
   :width: 512pt
   :align: center

These problems occur for example in pallet building, container loading, and warehouse management.

Features:

* Objectives:

  * Knapsack
  * Bin packing
  * Bin packing with leftovers
  * Open dimension X
  * Open dimension Y
  * Variable-sized bin packing

* Item types

  * 3D rotations (up to 6 orientations per item)
  * Groups (for unloading constraints)
  * Weight
  * Stacking constraints

    * Stackability identifier
    * Nesting height
    * Maximum number of items in a stack
    * Maximum weight above an item

* Bin types

  * Maximum weight
  * Maximum stack density (weight per unit floor area)
  * Maximum weight on the middle and rear axles (semi-trailer trucks)

* Unloading constraints (same as the :ref:`rectangle<rectangle>` solver)

Basic usage
--------------

The :code:`box-stacks` solver takes as input:

* an item CSV file; option: ``--items items.csv``
* a bin CSV file; option: ``--bins bins.csv``
* optionally a parameter CSV file; option: ``--parameters parameters.csv``

It outputs:

* a solution CSV file; option: ``--certificate solution.csv``

Items can be stacked on top of each other within a bin. An item can only be placed on top of another item if both items have exactly the same X and Y dimensions in their placed orientations, and both items have the same ``STACKABILITY_ID``.

The **item file** contains:

* The X dimension of the item type (**mandatory**)

  * column ``X``
  * **Integer value**

* The Y dimension of the item type (**mandatory**)

  * column ``Y``
  * **Integer value**

* The Z dimension of the item type (**mandatory**) — the vertical dimension in the default orientation

  * column ``Z``
  * **Integer value**

* The number of copies of the item type

  * column ``COPIES``
  * default value: ``1``

* The profit of an item of this type (for a knapsack objective)

  * column ``PROFIT``
  * default value: item volume (``X * Y * Z``)

* The stackability identifier; only items with the same X and Y dimensions **and** the same stackability identifier may be stacked on top of each other

  * column ``STACKABILITY_ID``
  * default value: ``0``

The **bin file** contains:

* The X dimension of the bin type (**mandatory**)

  * column ``X``
  * **Integer value**

* The Y dimension of the bin type (**mandatory**)

  * column ``Y``
  * **Integer value**

* The Z dimension of the bin type (**mandatory**) — the height of the bin

  * column ``Z``
  * **Integer value**

* The number of copies of the bin type

  * column ``COPIES``
  * default value: ``1``

* The minimum number of copies that must be used

  * column ``COPIES_MIN``
  * default value: ``0``

* The cost of a bin of this type

  * column ``COST``
  * default value: bin volume

* The maximum total weight allowed in a bin of this type

  * column ``MAXIMUM_WEIGHT``
  * default value: no limit

The **parameter file** has two columns: ``NAME`` and ``VALUE``. The possible entries are:

* The objective; name: ``objective``; possible values:

  * ``knapsack``
  * ``bin-packing``
  * ``bin-packing-with-leftovers``
  * ``open-dimension-x``
  * ``open-dimension-y``
  * ``variable-sized-bin-packing``

Inputs:

.. literalinclude:: examples/boxstacks/items.csv
   :caption: items.csv

.. literalinclude:: examples/boxstacks/bins.csv
   :caption: bins.csv

.. literalinclude:: examples/boxstacks/parameters.csv
   :caption: parameters.csv

Solve:

.. code-block:: shell

    packingsolver_boxstacks \
            --items items.csv \
            --bins bins.csv \
            --parameters parameters.csv \
            --certificate solution.csv

.. literalinclude:: examples/boxstacks/output.txt

Visualize:

.. code-block:: shell

    python3 scripts/visualize_boxstacks.py solution.csv

.. image:: img/boxstacks_example_solution.png
   :width: 512pt
   :align: center

Rotations
---------

* The allowed orientations

  * columns ``ROTATION_XYZ``, ``ROTATION_YXZ``, ``ROTATION_ZYX``, ``ROTATION_YZX``, ``ROTATION_XZY``, ``ROTATION_ZXY``
  * ``1``: this orientation is allowed; ``0`` or omitted: not allowed
  * default: if none of these columns is set to ``1``, only ``ROTATION_XYZ`` (the default orientation) is used

The six possible 3D orientations of a box are:

.. list-table::
   :header-rows: 1

   * - Column
     - X direction
     - Y direction
     - Z direction (vertical)
   * - ``ROTATION_XYZ``
     - x
     - y
     - z
   * - ``ROTATION_YXZ``
     - y
     - x
     - z
   * - ``ROTATION_ZYX``
     - z
     - y
     - x
   * - ``ROTATION_YZX``
     - y
     - z
     - x
   * - ``ROTATION_XZY``
     - x
     - z
     - y
   * - ``ROTATION_ZXY``
     - z
     - x
     - y

Each rotation is enabled independently via its own boolean column (``1`` to allow it, ``0`` or omitted to disallow it). If none of the ``ROTATION_*`` columns is set, only ``ROTATION_XYZ`` (the default orientation) is used. Common combinations:

* Only ``ROTATION_XYZ``: only the default orientation
* ``ROTATION_XYZ`` and ``ROTATION_YXZ``: Z face always on top; both XY rotations allowed
* ``ROTATION_XYZ``, ``ROTATION_YXZ``, ``ROTATION_ZYX`` and ``ROTATION_YZX``: Y face cannot be on top
* ``ROTATION_XYZ``, ``ROTATION_YXZ``, ``ROTATION_XZY`` and ``ROTATION_ZXY``: X face cannot be on top
* All six columns set to ``1``: all six orientations allowed

The following example packs a single 10×6×4 item into a 6×10×10 bin (:code:`bin-packing` objective). In its default orientation the item is 10 wide, which does not fit in the 6-wide bin, so without rotation the item cannot be packed at all. Allowing ``ROTATION_YXZ`` (swapping the X and Y directions) turns it into a 6×10×4 box, which fits, so the item packs into a single bin.

.. literalinclude:: examples/boxstacks/rotation_no/items.csv
   :caption: items.csv (without rotation)

.. literalinclude:: examples/boxstacks/rotation_yes/items.csv
   :caption: items.csv (with rotation)

.. literalinclude:: examples/boxstacks/rotation_no/bins.csv
   :caption: bins.csv

.. literalinclude:: examples/boxstacks/rotation_no/parameters.csv
   :caption: parameters.csv

.. code-block:: shell

    packingsolver_boxstacks \
            --items items.csv \
            --bins bins.csv \
            --parameters parameters.csv \
            --certificate solution.csv

Without rotation, no item fits into any bin:

.. literalinclude:: examples/boxstacks/rotation_no/output.txt
   :lines: 32-40

With rotation, the item fits into a single bin:

.. image:: img/boxstacks_rotation_yes.png
   :width: 384pt
   :align: center

Nesting height
--------------

* The nesting height; extra Z space saved when this item is placed on top of another item (e.g., for hollow boxes that nest inside each other)

  * column ``NESTING_HEIGHT``
  * default value: ``0``

When item B is placed on top of item A, the effective Z space consumed by A is reduced by ``A.NESTING_HEIGHT``: this models hollow items (e.g. crates, buckets) that partially nest into each other when stacked, instead of resting flat on top.

The following example packs 2 copies of a 6×6×6 item into 6×6×10 bins (:code:`bin-packing` objective). Without nesting height, stacking the 2 items requires 6+6=12 of Z space, which exceeds the bin height, so a second bin is needed. With a nesting height of 2, the second item sinks 2 units into the first, so the stack only needs 6+6-2=10 of Z space, exactly matching the bin height, and both items fit into a single bin.

.. |boxstacks_nesting_height_no| image:: img/boxstacks_nesting_height_no.png
   :scale: 50%

.. |boxstacks_nesting_height_yes| image:: img/boxstacks_nesting_height_yes.png
   :scale: 50%

.. list-table::
   :widths: 1 1
   :header-rows: 1
   :align: center

   * - Without nesting height
     - With nesting height
   * - .. literalinclude:: examples/boxstacks/nesting_height_no/items.csv
          :caption: items.csv
     - .. literalinclude:: examples/boxstacks/nesting_height_yes/items.csv
          :caption: items.csv
   * - .. literalinclude:: examples/boxstacks/nesting_height_no/bins.csv
          :caption: bins.csv
     - .. literalinclude:: examples/boxstacks/nesting_height_yes/bins.csv
          :caption: bins.csv
   * - .. literalinclude:: examples/boxstacks/nesting_height_no/parameters.csv
          :caption: parameters.csv
     - .. literalinclude:: examples/boxstacks/nesting_height_yes/parameters.csv
          :caption: parameters.csv
   * - .. code-block:: shell

            packingsolver_boxstacks \
                    --items items.csv \
                    --bins bins.csv \
                    --parameters parameters.csv \
                    --certificate solution.csv
     - .. code-block:: shell

            packingsolver_boxstacks \
                    --items items.csv \
                    --bins bins.csv \
                    --parameters parameters.csv \
                    --certificate solution.csv
   * - |boxstacks_nesting_height_no|
     - |boxstacks_nesting_height_yes|

Maximum number of items in a stack
-----------------------------------

* The maximum number of items that may be placed in a stack containing an item of this type

  * column ``MAXIMUM_STACKABILITY``
  * default value: no limit

The following example packs 3 copies of a 6×6×3 item into 6×6×10 bins (:code:`bin-packing` objective). Without a limit, all 3 items stack into a single bin (3×3=9 of Z space, within the bin height of 10). With ``MAXIMUM_STACKABILITY`` set to 2, at most 2 items may share a stack, and since the item's footprint already fills the whole bin floor, the third item cannot start a second stack in the same bin, so a second bin is needed.

.. |boxstacks_maximum_stackability_no| image:: img/boxstacks_maximum_stackability_no.png
   :scale: 50%

.. |boxstacks_maximum_stackability_yes| image:: img/boxstacks_maximum_stackability_yes.png
   :scale: 50%

.. list-table::
   :widths: 1 1
   :header-rows: 1
   :align: center

   * - Without limit
     - ``MAXIMUM_STACKABILITY`` = 2
   * - .. literalinclude:: examples/boxstacks/maximum_stackability_no/items.csv
          :caption: items.csv
     - .. literalinclude:: examples/boxstacks/maximum_stackability_yes/items.csv
          :caption: items.csv
   * - .. literalinclude:: examples/boxstacks/maximum_stackability_no/bins.csv
          :caption: bins.csv
     - .. literalinclude:: examples/boxstacks/maximum_stackability_yes/bins.csv
          :caption: bins.csv
   * - .. literalinclude:: examples/boxstacks/maximum_stackability_no/parameters.csv
          :caption: parameters.csv
     - .. literalinclude:: examples/boxstacks/maximum_stackability_yes/parameters.csv
          :caption: parameters.csv
   * - .. code-block:: shell

            packingsolver_boxstacks \
                    --items items.csv \
                    --bins bins.csv \
                    --parameters parameters.csv \
                    --certificate solution.csv
     - .. code-block:: shell

            packingsolver_boxstacks \
                    --items items.csv \
                    --bins bins.csv \
                    --parameters parameters.csv \
                    --certificate solution.csv
   * - |boxstacks_maximum_stackability_no|
     - |boxstacks_maximum_stackability_yes|

Maximum weight above an item
-----------------------------

* The weight of the item (used by this constraint, by the maximum stack density and maximum bin weight constraints, and by the semi-trailer truck axle weight constraints)

  * column ``WEIGHT``
  * default value: ``0``

* The maximum weight that may be placed above an item of this type

  * column ``MAXIMUM_WEIGHT_ABOVE``
  * default value: no limit

The following example packs 2 copies of a 6×6×3 item, each weighing 10, into 6×6×10 bins (:code:`bin-packing` objective). Without a limit, the 2 items stack into a single bin. With ``MAXIMUM_WEIGHT_ABOVE`` set to 5 on both items, neither item can support the other's weight (10 > 5) regardless of which one is on the bottom, so they cannot share a stack, and since their footprint fills the whole bin floor, a second bin is needed.

.. |boxstacks_maximum_weight_above_no| image:: img/boxstacks_maximum_weight_above_no.png
   :scale: 50%

.. |boxstacks_maximum_weight_above_yes| image:: img/boxstacks_maximum_weight_above_yes.png
   :scale: 50%

.. list-table::
   :widths: 1 1
   :header-rows: 1
   :align: center

   * - Without limit
     - ``MAXIMUM_WEIGHT_ABOVE`` = 5
   * - .. literalinclude:: examples/boxstacks/maximum_weight_above_no/items.csv
          :caption: items.csv
     - .. literalinclude:: examples/boxstacks/maximum_weight_above_yes/items.csv
          :caption: items.csv
   * - .. literalinclude:: examples/boxstacks/maximum_weight_above_no/bins.csv
          :caption: bins.csv
     - .. literalinclude:: examples/boxstacks/maximum_weight_above_yes/bins.csv
          :caption: bins.csv
   * - .. literalinclude:: examples/boxstacks/maximum_weight_above_no/parameters.csv
          :caption: parameters.csv
     - .. literalinclude:: examples/boxstacks/maximum_weight_above_yes/parameters.csv
          :caption: parameters.csv
   * - .. code-block:: shell

            packingsolver_boxstacks \
                    --items items.csv \
                    --bins bins.csv \
                    --parameters parameters.csv \
                    --certificate solution.csv
     - .. code-block:: shell

            packingsolver_boxstacks \
                    --items items.csv \
                    --bins bins.csv \
                    --parameters parameters.csv \
                    --certificate solution.csv
   * - |boxstacks_maximum_weight_above_no|
     - |boxstacks_maximum_weight_above_yes|

Maximum stack density
----------------------

* The maximum weight per unit floor area for any stack in the bin

  * column ``MAXIMUM_STACK_DENSITY`` in the bin CSV file
  * default value: no limit

The following example packs 2 copies of a 6×6×3 item, each weighing 10, into 6×6×10 bins (:code:`bin-packing` objective). Without a density limit, the 2 items stack into a single bin. The stack's floor area is 6×6=36, so its density is (10+10)/36≈0.56. Setting ``MAXIMUM_STACK_DENSITY`` to 0.5 on the bin means a stack may weigh at most 0.5×36=18, which the combined weight of 20 exceeds, so the items cannot share a stack; since their footprint fills the whole bin floor, a second bin is needed.

.. |boxstacks_maximum_stack_density_no| image:: img/boxstacks_maximum_stack_density_no.png
   :scale: 50%

.. |boxstacks_maximum_stack_density_yes| image:: img/boxstacks_maximum_stack_density_yes.png
   :scale: 50%

.. list-table::
   :widths: 1 1
   :header-rows: 1
   :align: center

   * - Without limit
     - ``MAXIMUM_STACK_DENSITY`` = 0.5
   * - .. literalinclude:: examples/boxstacks/maximum_stack_density_no/items.csv
          :caption: items.csv
     - .. literalinclude:: examples/boxstacks/maximum_stack_density_yes/items.csv
          :caption: items.csv
   * - .. literalinclude:: examples/boxstacks/maximum_stack_density_no/bins.csv
          :caption: bins.csv
     - .. literalinclude:: examples/boxstacks/maximum_stack_density_yes/bins.csv
          :caption: bins.csv
   * - .. literalinclude:: examples/boxstacks/maximum_stack_density_no/parameters.csv
          :caption: parameters.csv
     - .. literalinclude:: examples/boxstacks/maximum_stack_density_yes/parameters.csv
          :caption: parameters.csv
   * - .. code-block:: shell

            packingsolver_boxstacks \
                    --items items.csv \
                    --bins bins.csv \
                    --parameters parameters.csv \
                    --certificate solution.csv
     - .. code-block:: shell

            packingsolver_boxstacks \
                    --items items.csv \
                    --bins bins.csv \
                    --parameters parameters.csv \
                    --certificate solution.csv
   * - |boxstacks_maximum_stack_density_no|
     - |boxstacks_maximum_stack_density_yes|

Maximum weight on the middle and rear axles
---------------------------------------------

For bins that model the trailer of a semi-trailer truck, the solver can enforce legal weight limits on the tractor's middle (drive) axle and on the trailer's rear axle, computed from the position of the loaded stacks along the trailer and the truck's geometry.

* Whether the bin is a semi-trailer truck

  * column ``IS_SEMI_TRAILER_TRUCK`` in the bin CSV file
  * ``1``: bin is a semi-trailer truck; ``0`` or omitted: not a semi-trailer truck (no axle weight constraints)

* The weight of the tractor

  * column ``TRACTOR_WEIGHT``
  * default value: ``0``

* The distance between the tractor's front axle and its middle (drive) axle

  * column ``FRONT_AXLE_MIDDLE_AXLE_DISTANCE``
  * default value: ``0``

* The distance between the tractor's front axle and the tractor's center of gravity

  * column ``FRONT_AXLE_TRACTOR_GRAVITY_CENTER_DISTANCE``
  * default value: ``0``

* The distance between the tractor's front axle and the harness (fifth-wheel coupling point)

  * column ``FRONT_AXLE_HARNESS_DISTANCE``
  * default value: ``0``

* The weight of the empty trailer

  * column ``EMPTY_TRAILER_WEIGHT``
  * default value: ``0``

* The distance between the harness and the trailer's rear axle

  * column ``HARNESS_REAR_AXLE_DISTANCE``
  * default value: ``0``

* The distance between the empty trailer's center of gravity and its rear axle

  * column ``TRAILER_GRAVITY_CENTER_REAR_AXLE_DISTANCE``
  * default value: ``0``

* The distance between the start of the trailer's cargo area (X = 0 in the item file) and the harness

  * column ``TRAILER_START_HARNESS_DISTANCE``
  * default value: ``0``

* The maximum weight allowed on the trailer's rear axle

  * column ``REAR_AXLE_MAXIMUM_WEIGHT``
  * default value: no limit

* The maximum weight allowed on the tractor's middle (drive) axle

  * column ``MIDDLE_AXLE_MAXIMUM_WEIGHT``
  * default value: no limit

Loading cargo further toward the front of the trailer (closer to the harness) shifts more weight onto the tractor's middle axle and less onto the trailer's rear axle; loading further back does the opposite. The solver uses the combined weight and the X position of each stack's center to compute the resulting middle and rear axle weights, and only accepts solutions that stay within the configured limits.

Unloading constraints
-------------------------

When loading a truck that visits multiple loading/unloading locations, it might be necessary to be able to load/unload the items at a location without having to move the items which are already in the truck.
This is modeled in the solver with unloading constraints. Items are assigned to groups: items in group 0 are placed last (nearest the door) and unloaded first, items in group 1 are placed next, etc.

* The group of the item (for unloading constraints)

  * column ``GROUP_ID``
  * default value: ``0``

* The unloading constraint; name: ``unloading-constraint``; possible values:

  * ``none`` (default)
  * ``only-x-movements``
  * ``only-y-movements``
  * ``increasing-x``
  * ``increasing-y``

Note that unlike the :ref:`rectangle<rectangle>` solver, the parameter key is hyphenated: ``unloading-constraint`` rather than ``unloading_constraint``.

4 types of unloading constraints are available:

* ``only-x-movements``: items can only be moved out along the X axis; therefore, no item from a later group may be positioned to the right of an item from an earlier group
* ``only-y-movements``: same along the Y axis
* ``increasing-x``: all items from group 0 have a greater X coordinate than all items from group 1, which in turn have a greater X coordinate than all items from group 2, etc.
* ``increasing-y``: same along the Y axis

The following example packs 3 items (200×200, 400×400 and 150×150 footprints, groups 0, 1 and 2) into 700×400×100 bins (:code:`bin-packing-with-leftovers` objective). Without an unloading constraint, the items can be arranged freely and all 3 fit into a single bin. With ``only-x-movements``, the group-2 item cannot be positioned without ending up to the right of the group-1 item while itself needing to come out first, so it no longer fits alongside the others, and a second bin is needed.

.. |boxstacks_unloading_no| image:: img/boxstacks_unloading_no.png
   :scale: 50%

.. |boxstacks_unloading_yes| image:: img/boxstacks_unloading_yes.png
   :scale: 50%

.. list-table::
   :widths: 1 1
   :header-rows: 1
   :align: center

   * - No constraint
     - ``only-x-movements``
   * - .. literalinclude:: examples/boxstacks/unloading_no/items.csv
          :caption: items.csv
     - .. literalinclude:: examples/boxstacks/unloading_yes/items.csv
          :caption: items.csv
   * - .. literalinclude:: examples/boxstacks/unloading_no/bins.csv
          :caption: bins.csv
     - .. literalinclude:: examples/boxstacks/unloading_yes/bins.csv
          :caption: bins.csv
   * - .. literalinclude:: examples/boxstacks/unloading_no/parameters.csv
          :caption: parameters.csv
     - .. literalinclude:: examples/boxstacks/unloading_yes/parameters.csv
          :caption: parameters.csv
   * - .. code-block:: shell

            packingsolver_boxstacks \
                    --items items.csv \
                    --bins bins.csv \
                    --parameters parameters.csv \
                    --certificate solution.csv
     - .. code-block:: shell

            packingsolver_boxstacks \
                    --items items.csv \
                    --bins bins.csv \
                    --parameters parameters.csv \
                    --certificate solution.csv
   * - |boxstacks_unloading_no|
     - |boxstacks_unloading_yes|
