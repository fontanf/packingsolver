.. _optimization_modes:

Optimization modes
==================

PackingSolver provides 3 optimization modes:

* Anytime
* Not-anytime
* Not-anytime sequential

Anytime mode
------------

In anytime mode, PackingSolver tries to quickly get a solution and looks for improving ones as long as a stopping criterion is not met. This mode is typically meant to be used **with a time limit**.

The anytime mode uses **multiple threads**, in general about 8 threads.

In anytime mode, PackingSolver may still stop before any stopping criterion is met if it proved it would not be able to improve the current solution further.

.. note::

    Anytime mode: let the solver find the best possible solution within X seconds.

**Example**

.. code-block:: none
   :caption: items.csv

   WIDTH,HEIGHT,COPIES
   100,50,1
   103,53,1
   106,56,1
   109,59,1
   112,62,1
   115,65,1
   118,68,1
   121,71,1
   124,74,1
   127,77,1
   130,80,1
   133,83,1
   136,86,1
   139,89,1
   142,92,1
   145,95,1
   148,98,1
   151,101,1
   154,104,1
   157,107,1
   160,110,1
   163,113,1
   166,116,1
   169,119,1
   172,122,1
   175,125,1
   178,128,1
   181,131,1
   184,134,1
   187,137,1
   190,140,1
   193,143,1
   196,146,1
   199,149,1
   202,152,1
   205,155,1
   208,158,1
   211,161,1
   214,164,1
   217,167,1

.. code-block:: none
   :caption: bins.csv

   WIDTH,HEIGHT,COPIES
   1000,500,20

.. code-block:: none
   :caption: parameters.csv

   NAME,VALUE
   objective,bin-packing-with-leftovers

.. code-block:: shell

    packingsolver_rectangle \
            --items items.csv \
            --bins bins.csv \
            --parameters parameters.csv \
            --certificate solution_rectangle.csv \
            --optimization-mode anytime \
            --time-limit 15

.. code-block:: none

   =================================
             PackingSolver          
   =================================
   
   Problem type
   ------------
   Rectangle
   
   Instance
   --------
   Objective:             BinPackingWithLeftovers
   Number of item types:  40
   Number of items:       40
   Number of bin types:   1
   Number of bins:        20
   Number of groups:      1
   Number of defects:     0
   Unloading constraint:  None
   Total item area:       735860
   Total item width:      6340
   Total item height:     4340
   Smallest item width:   50
   Smallest item height:  50
   Total item weight:     0
   Maximum item copies:   1
   Total bin area:        10000000
   Total bin weight:      0
   Maximum bin cost:      500000
   
           Time      # bins    Leftover                         Comment
           ----      ------    --------                         -------
          0.005           2      166300                  TS g 1 d X q 1
          0.005           2      189800                  TS g 1 d X q 1
          0.018           2      212864                         SSK q 1
          0.070           2      216608                  TS g 0 d X q 6
          0.229           2      218000                 TS g 0 d X q 19
          0.229           2      221384                 TS g 0 d X q 19
          0.232           2      223988                 TS g 0 d X q 19
          0.808           2      224000                 TS g 0 d X q 63
          0.808           2      225000                 TS g 0 d X q 63
          0.815           2      226500                 TS g 0 d X q 63
          1.666           2      230500                        SSK q 64
         12.251           2      241518                       SSK q 512
         14.221           2      243015               TS g 1 d X q 1066
   
   Final statistics
   ----------------
   Time (s):  15.0054
   
   Solution
   --------
   Number of items:  40 / 40 (100%)
   Item area:        735860 / 735860 (100%)
   Item weight:      0 / 0 (-nan%)
   Item profit:      735860 / 735860 (100%)
   Number of bins:   2 / 20 (10%)
   Bin area:         1000000 / 10000000 (10%)
   Bin weight:       0 / 0 (-nan%)
   Bin cost:         1e+06
   Waste:            21125
   Waste (%):        2.79068
   Full waste:       264140
   Full waste (%):   26.414
   Area load:        0.073586
   Weight load:      -nan
   X max:            515
   Y max:            499
   Leftover value:   243015

Non-anytime modes
-----------------

In non-anytime modes, PackingSolver stops by itself. These modes are meant to always return solutions of similar quality even with instances of different sizes. The resolution time will be smaller for smaller instances and larger for larger instances.

Non-anytime modes are more complex to use, since defining accurately the desired quality requires tuning the algorithms parameters (for now).

.. note::

    Non-anytime mode: let the solver find a solution of a given quality

There are 2 non-anytime modes. The `not-anytime` mode uses multiple threads, typically about 8 threads. The `not-anytime-sequential` mode uses a single thread.

The `not-anytime-sequential` mode is **deterministic**. The other optimization modes may not be deterministic even if they should be most of the time.

**Example**

Same input as above. The solver terminates after 35 seconds.

.. code-block:: shell

    packingsolver_rectangle \
            --items items.csv \
            --bins bins.csv \
            --parameters parameters.csv \
            --certificate solution_rectangle.csv \
            --optimization-mode not-anytime

.. code-block:: none

   =================================
             PackingSolver          
   =================================
   
   Problem type
   ------------
   Rectangle
   
   Instance
   --------
   Objective:             BinPackingWithLeftovers
   Number of item types:  40
   Number of items:       40
   Number of bin types:   1
   Number of bins:        20
   Number of groups:      1
   Number of defects:     0
   Unloading constraint:  None
   Total item area:       735860
   Total item width:      6340
   Total item height:     4340
   Smallest item width:   50
   Smallest item height:  50
   Total item weight:     0
   Maximum item copies:   1
   Total bin area:        10000000
   Total bin weight:      0
   Maximum bin cost:      500000
   
           Time      # bins    Leftover                         Comment
           ----      ------    --------                         -------
         17.920           2      234000                      TS g 0 d X
         17.921           2      241518                      TS g 1 d X
         28.537           2      246020                      SSK q 2048
   
   Final statistics
   ----------------
   Time (s):  34.1616
   
   Solution
   --------
   Number of items:  40 / 40 (100%)
   Item area:        735860 / 735860 (100%)
   Item weight:      0 / 0 (-nan%)
   Item profit:      735860 / 735860 (100%)
   Number of bins:   2 / 20 (10%)
   Bin area:         1000000 / 10000000 (10%)
   Bin weight:       0 / 0 (-nan%)
   Bin cost:         1e+06
   Waste:            18120
   Waste (%):        2.40325
   Full waste:       264140
   Full waste (%):   26.414
   Area load:        0.073586
   Weight load:      -nan
   X max:            510
   Y max:            498
   Leftover value:   246020

Now if we remove half of the items, the solver terminates after 22 seconds.

.. code-block:: none
   :caption: items.csv

   WIDTH,HEIGHT,COPIES
   100,50,1
   106,56,1
   112,62,1
   118,68,1
   124,74,1
   130,80,1
   136,86,1
   142,92,1
   148,98,1
   154,104,1
   160,110,1
   166,116,1
   172,122,1
   178,128,1
   184,134,1
   190,140,1
   196,146,1
   202,152,1
   208,158,1
   214,164,1

.. code-block:: none
   :caption: bins.csv

   WIDTH,HEIGHT,COPIES
   1000,500,20

.. code-block:: none
   :caption: parameters.csv

   NAME,VALUE
   objective,bin-packing-with-leftovers

.. code-block:: shell

    packingsolver_rectangle \
            --items items.csv \
            --bins bins.csv \
            --parameters parameters.csv \
            --certificate solution_rectangle.csv \
            --optimization-mode not-anytime

.. code-block:: none



   =================================
             PackingSolver          
   =================================
   
   Problem type
   ------------
   Rectangle
   
   Instance
   --------
   Objective:             BinPackingWithLeftovers
   Number of item types:  20
   Number of items:       20
   Number of bin types:   1
   Number of bins:        20
   Number of groups:      1
   Number of defects:     0
   Unloading constraint:  None
   Total item area:       359920
   Total item width:      3140
   Total item height:     2140
   Smallest item width:   50
   Smallest item height:  50
   Total item weight:     0
   Maximum item copies:   1
   Total bin area:        10000000
   Total bin weight:      0
   Maximum bin cost:      500000
   
           Time      # bins    Leftover                         Comment
           ----      ------    --------                         -------
          5.982           1      131000                      TS g 0 d X
   
   Final statistics
   ----------------
   Time (s):  22.0075
   
   Solution
   --------
   Number of items:  20 / 20 (100%)
   Item area:        359920 / 359920 (100%)
   Item weight:      0 / 0 (-nan%)
   Item profit:      359920 / 359920 (100%)
   Number of bins:   1 / 20 (5%)
   Bin area:         500000 / 10000000 (5%)
   Bin weight:       0 / 0 (-nan%)
   Bin cost:         500000
   Waste:            9080
   Waste (%):        2.4607
   Full waste:       140080
   Full waste (%):   28.016
   Area load:        0.035992
   Weight load:      -nan
   X max:            738
   Y max:            500
   Leftover value:   131000
