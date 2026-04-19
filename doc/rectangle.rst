.. _rectangle:

Rectangle solver
================

The Rectangle solver solves problems where items are two-dimensional rectangles that must be packed into rectangular bins without overlapping. Unlike the :ref:`RectangleGuillotine<rectangleguillotine>` solver, items can be placed in any position (guillotine cuts are not required).

.. image:: ../img/rectangle.png
   :width: 512pt
   :align: center

These problems occur for example in logistics (palletizing), sheet-metal cutting, and warehousing.

Features:

* Objectives:

  * Knapsack
  * Bin packing
  * Bin packing with leftovers
  * Open dimension X
  * Open dimension Y
  * Open dimension XY
  * Variable-sized bin packing

* Item types

  * Item rotation (90°)
  * Groups (for unloading constraints)
  * Weight

* Bin types

  * Rectangular defects
  * Maximum weight

* Unloading constraints

  * ``only-x-movements``
  * ``only-y-movements``
  * ``increasing-x``
  * ``increasing-y``

Basic usage
-----------

The Rectangle solver takes as input:

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

* Whether the item is fixed in its original orientation (cannot be rotated 90°)

  * column ``ORIENTED``
  * ``0``: rotation allowed (default)
  * ``1``: item is oriented; rotation not allowed

* The profit of an item of this type (for a knapsack objective)

  * column ``PROFIT``
  * default value: item area (``WIDTH * HEIGHT``)

* The group of the item (for unloading constraints)

  * column ``GROUP_ID``
  * default value: ``0``

* The weight of the item

  * column ``WEIGHT``
  * default value: ``0``

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

* The minimum number of copies of the bin type that must be used

  * column ``COPIES_MIN``
  * default value: ``0``

* The cost of a bin of this type (for a variable-sized bin packing objective)

  * column ``COST``
  * default value: bin area (``WIDTH * HEIGHT``)

* The maximum total weight that can be placed in a bin of this type

  * column ``MAXIMUM_WEIGHT``
  * default value: no limit

The **defect file** contains:

* The bin type that contains the defect (**mandatory**)

  * column ``BIN``

* The X coordinate of the bottom-left corner of the defect (**mandatory**)

  * column ``X``

* The Y coordinate of the bottom-left corner of the defect (**mandatory**)

  * column ``Y``

* The width of the defect (**mandatory**)

  * column ``WIDTH``

* The height of the defect (**mandatory**)

  * column ``HEIGHT``

The **parameter file** has two columns: ``NAME`` and ``VALUE``. The possible entries are:

* The objective; name: ``objective``; possible values:

  * ``knapsack``
  * ``bin-packing``
  * ``bin-packing-with-leftovers``
  * ``open-dimension-x``
  * ``open-dimension-y``
  * ``open-dimension-xy``
  * ``variable-sized-bin-packing``

* The unloading constraint; name: ``unloading_constraint``; possible values:

  * ``none`` (default)
  * ``only-x-movements``
  * ``only-y-movements``
  * ``increasing-x``
  * ``increasing-y``

**Example**

Inputs:

.. literalinclude:: examples/rectangle/items.csv
   :caption: items.csv

.. literalinclude:: examples/rectangle/bins.csv
   :caption: bins.csv

.. literalinclude:: examples/rectangle/parameters.csv
   :caption: parameters.csv

Solve:

.. code-block:: shell

    packingsolver_rectangle \
            --items items.csv \
            --bins bins.csv \
            --parameters parameters.csv \
            --certificate solution.csv

.. literalinclude:: examples/rectangle/output.txt

Visualize:

.. code-block:: shell

    python3 scripts/visualize_rectangle.py solution.csv

.. image:: img/rectangle_example_solution.png
   :width: 512pt
   :align: center

Command-line options
--------------------

.. code-block:: none

    packingsolver_rectangle --items items.csv [options]

Mandatory option:

* ``--items, -i``: path to the items CSV file (or path prefix when files follow the ``<prefix>items.csv`` naming convention)

Other input options:

* ``--bins, -b``: path to the bins CSV file
* ``--defects, -d``: path to the defects CSV file
* ``--parameters``: path to the parameters CSV file

Instance modifier options:

* ``--objective, -f``: objective (overrides the parameters file)
* ``--no-item-rotation``: fix all items in their original orientation
* ``--bin-infinite-x``: make all bins infinite in the X direction (for open-dimension-x)
* ``--bin-infinite-y``: make all bins infinite in the Y direction (for open-dimension-y)
* ``--bin-infinite-copies``: make all bin types available in unlimited copies
* ``--unweighted``: set all item profits to 1
* ``--bin-unweighted``: set all bin costs to 1

Output options:

* ``--certificate, -c``: path for the solution CSV file
* ``--output, -o``: path for a JSON file with detailed statistics
* ``--time-limit, -t``: time limit in seconds
* ``--verbosity-level, -v``: verbosity level (0–3)

Rotation
--------

By default, items may be rotated 90°. Set ``ORIENTED = 1`` in the items file to fix an item in its original orientation, or use ``--no-item-rotation`` on the command line to disable rotation for all items.

Defects
-------

Defects are rectangular regions inside a bin where items cannot be placed. They are specified in the defects CSV file. The ``BIN`` column refers to the bin type by its position (0-indexed) in the bins file.

Unloading constraints
---------------------

Unloading constraints model situations where items placed later in the bin must be reachable without moving items placed earlier (e.g., truck loading). The available constraints are:

* ``only-x-movements``: items can only be moved out along the X axis; therefore no item is blocked in the X direction by a later item
* ``only-y-movements``: same along the Y axis
* ``increasing-x``: items must be packed in non-decreasing order of their X coordinate; later items are always at a higher X position
* ``increasing-y``: same along the Y axis

Groups
------

Items can be assigned to groups via the ``GROUP_ID`` column. Groups are used together with unloading constraints to model deliveries: items in group 0 must be placed last (nearest the door) and unloaded first, items in group 1 are placed next, etc.
