PackingSolver's documentation
=============================

.. toctree::
   :maxdepth: 3
   :hidden:

   self
   objectives
   optimization_modes
   onedimensional
   rectangle
   rectangleguillotine
   box
   boxstacks
   irregular


Introduction
------------

`PackingSolver` is a software package dedicated to the practical resolution of cutting and packing problems.

`PackingSolver` takes as input:

* A set of pieces to cut/pack called **items**.
* A set of containers from which to cut / in which to pack these items, called **bins**.
* A set of parameters for the optimization

Then `PackingSolver` outputs the cutting/loading plans.

PackingSolver solves multiple problem types:

.. |rectangleguillotine| image:: ../img/rectangleguillotine.png
   :width: 512pt
.. |rectangle| image:: ../img/rectangle.png
   :width: 512pt
.. |box| image:: ../img/box.png
   :width: 512pt
.. |boxstacks| image:: ../img/boxstacks.png
   :width: 512pt
.. |onedimensional| image:: ../img/onedimensional.png
   :width: 512pt
.. |irregular| image:: ../img/irregular.png
   :width: 512pt


.. list-table::
   :widths: 1, 1
   :align: center

   * - :ref:`RectangleGuillotine<rectangleguillotine>`

       * Items: two-dimensional rectangles
       * Only edge-to-edge cuts are allowed
     - |rectangleguillotine|
   * - :ref:`Rectangle<rectangle>`

       * Items: two-dimensional rectangles
     - |rectangle|
   * - :ref:`Box<box>`

       * Items: three-dimensional rectangular parallelepipeds
     - |box|
   * - :ref:`BoxStacks<boxstacks>`

       * Items: three-dimensional rectangular parallelepipeds
       * Items are stacked; a stack contains items with the same width and length
     - |boxstacks|
   * - :ref:`OneDimensional<onedimensional>`

       * Items: one-dimensional items
     - |onedimensional|
   * - :ref:`Irregular<irregular>`

       * Items: two-dimensional polygons
     - |irregular|

Getting started
---------------

Let's see how to solve a simple rectangle packing problem.

In a first CSV file, we provide the width, height and number of copies of the items to pack.
Here we consider two items:

* The first one has a width of 300, a height of 200 and 10 copies.
* The second one has a width of 250, a height of 150 and 10 copies.

.. code-block:: none
   :caption: items.csv

   WIDTH,HEIGHT,COPIES
   300,200,10
   250,150,10

In a second CSV file, we provide the width, height and number of copies of the bins in which the items must be packed.
Here we consider a single container of width 1000 and of height 500 available in 10 copies.

.. code-block:: none
   :caption: bins.csv

   WIDTH,HEIGHT,COPIES
   1000,500,10

Finally, in a third CSV file, we provide the other optimizaton parameters. Here we just set the :code:`objective` parameter to :code:`bin-packing`, which means that we look to pack all the items using as few bins as possible. The :ref:`objectives<objectives>` page gives more details about the possible objectives.

.. code-block:: none
   :caption: parameters.csv

   NAME,VALUE
   objective,bin-packing

Now, we use the following command to launch the optimization:

.. code-block:: shell

    packingsolver_rectangle \
            --items items.csv \
            --bins bins.csv \
            --parameters parameters.csv \
            --certificate solution_rectangle.csv

The terminal output looks like:

.. literalinclude:: examples/rectangle/output.txt

From the terminal output, we see that the solver managed to pack all the items using two bins.
The loading plans are written in the :code:`solution_rectangle.csv` file. A script is available to visualize them:

.. code-block:: shell

    python3 scripts/visualize_rectangle.py solution_rectangle.csv

The script opens a page in a browser where the loading plans are displayed:

.. image:: img/getting_started_solution.png
   :width: 256pt
   :align: center

