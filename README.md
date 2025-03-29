# PackingSolver

A state-of-the-art solver for (geometrical) packing problems.

PackingSolver solves the following problem types:

| Problem types            |  Examples |
:------------------------- |:-------------------------
[`rectangleguillotine`](#rectangleguillotine-solver)<ul><li>Items: two-dimensional rectangles</li><li>Only edge-to-edge cuts are allowed</li></ul>  |  <img src="https://github.com/fontanf/packingsolver/blob/master/img/rectangleguillotine.png" align=center width="512">
[`rectangle`](#rectangle-solver)<ul><li>Items: two-dimensional rectangles</li></ul>  |  <img src="https://github.com/fontanf/packingsolver/blob/master/img/rectangle.png" align=center width="512">
[`box`](#box-solver)<ul><li>Items: three-dimensional rectangular parallelepipeds</li></ul>  |  <img src="https://github.com/fontanf/packingsolver/blob/master/img/box.png" align=center width="512">
[`boxstacks`](#boxstacks-solver)<ul><li>Items: three-dimensional rectangular parallelepipeds</li><li>Items can be stacked; a stack contains items with the same width and length</li></ul>  |  <img src="https://github.com/fontanf/packingsolver/blob/master/img/boxstacks.png" align=center width="512">
[`onedimensional`](#onedimensional-solver)<ul><li>Items: one-dimensional items</li></ul>  |  <img src="https://github.com/fontanf/packingsolver/blob/master/img/onedimensional.png" align=center width="512">
[`irregular`](#irregular-solver)<ul><li>Items: two-dimensional polygons</li></ul>  |  <img src="https://github.com/fontanf/packingsolver/blob/master/img/irregular.png" align=center width="512">

## Compilation

```shell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel && cmake --install build --config Release --prefix install
```

## `rectangleguillotine` solver

Features:
* Objectives:
  * Knapsack
  * Open dimension X
  * Open dimension Y
  * Bin packing
  * Bin packing with leftovers
  * Variable-sized bin packing
* Item types:
  * With or without rotations
  * Stacks (precedence constraints on the order in which items are extracted)
* Bins types:
  * May contain defects
  * Allow or forbid cutting through a defect
* Two- and three-staged, exact, non-exact, roadef2018 and homogenous patterns
* First cut vertical, horizontal or any
* Trims
* Cut thickness
* Minimum distance between consecutive 1-cuts
* Maximum distance between consecutive 1-cuts
* Minimum distance between consecutive 2-cuts
* Minimum distance between cuts
* Maximum number of consecutive 2-cuts

Example:
```shell
./install/bin/packingsolver_rectangleguillotine \
        --verbosity-level 1 \
        --items data/rectangle/alvarez2002/ATP35_items.csv \
        --bins data/rectangle/alvarez2002/ATP35_bins.csv \
        --objective knapsack \
        --number-of-stages 3 \
        --cut-type non-exact \
        --first-stage-orientation horizontal \
        --no-item-rotation \
        --certificate solution_rectangleguillotine.csv \
        --time-limit 1
```

<details><p>

```
=================================
          PackingSolver          
=================================

Problem type
------------
RectangleGuillotine

Instance
--------
Objective:                Knapsack
Number of item types:     29
Number of items:          153
Number of bin types:      1
Number of bins:           1
Number of stacks:         29
Number of defects:        0
Number of stages:         3
Cut type:                 NonExact
First stage orientation:  Horizontal
min1cut:                  0
max1cut:                  -1
min2cut:                  0
max2cut:                  -1
Minimum waste:            1
one2cut:                  0
Cut through defects:      0
Cut thickness:            0

        Time        Profit   # items                         Comment
        ----        ------   -------                         -------
       0.001         68970         1         TS g 5 d Horizontal q 1
       0.002         72000         1         TS g 5 d Horizontal q 1
       0.009        140970         2         TS g 5 d Horizontal q 1
       0.010        144000         2         TS g 5 d Horizontal q 1
       0.011        212970         3         TS g 5 d Horizontal q 1
       0.012        216000         3         TS g 5 d Horizontal q 1
       0.013        284970         4         TS g 5 d Horizontal q 1
       0.014        292395         5         TS g 5 d Horizontal q 1
       0.015        306705         5         TS g 5 d Horizontal q 1
       0.016        348839         5         TS g 5 d Horizontal q 1
       0.017        358042         6         TS g 5 d Horizontal q 1
       0.018        372343         6         TS g 5 d Horizontal q 1
       0.019        379768         7         TS g 5 d Horizontal q 1
       0.020        388389         7         TS g 5 d Horizontal q 1
       0.021        408379         7         TS g 5 d Horizontal q 1
       0.022        415804         8         TS g 5 d Horizontal q 1
       0.023        424425         8         TS g 5 d Horizontal q 1
       0.024        444415         8         TS g 5 d Horizontal q 1
       0.025        451840         9         TS g 5 d Horizontal q 1
       0.026        460461         9         TS g 5 d Horizontal q 1
       0.027        480451         9         TS g 5 d Horizontal q 1
       0.029        496497        10         TS g 5 d Horizontal q 1
       0.030        502186        10         TS g 5 d Horizontal q 1
       0.031        523921        11         TS g 5 d Horizontal q 1
       0.032        539967        12         TS g 5 d Horizontal q 1
       0.033        547003         9         TS g 5 d Horizontal q 2
       0.034        561304         9         TS g 5 d Horizontal q 2
       0.035        581548         9         TS g 5 d Horizontal q 2
       0.036        588973        10         TS g 5 d Horizontal q 2
       0.036        597058        10         TS g 5 d Horizontal q 2
       0.037        599368        11         TS g 5 d Horizontal q 2
       0.039        602118        14         TS g 4 d Horizontal q 2
       0.043        605793        11         TS g 5 d Horizontal q 9
       0.049        606147        13        TS g 5 d Horizontal q 19
       0.059        606672        12        TS g 5 d Horizontal q 42
       0.074        607062        14        TS g 5 d Horizontal q 94
       0.104        609550        15       TS g 5 d Horizontal q 211
       0.154        610101        31       TS g 4 d Horizontal q 141
       0.155        610578        31       TS g 4 d Horizontal q 141
       0.156        610787        32       TS g 4 d Horizontal q 141
       0.212        611135        34       TS g 4 d Horizontal q 211
       0.294        614725        31       TS g 4 d Horizontal q 316
       0.304        614967        42       TS g 4 d Horizontal q 316
       0.453        616880        16      TS g 5 d Horizontal q 1139
       0.874        619897        28      TS g 4 d Horizontal q 1066

Final statistics
----------------
Time (s):  1.0037

Solution
--------
Number of items:  28 / 153 (18.3007%)
Item area:        619897 / 4322082 (14.3426%)
Item profit:      619897 / 4.32208e+06 (14.3426%)
Number of bins:   1 / 1 (100%)
Bin cost:         623040
Waste:            3143
Waste (%):        0.504462
Full waste:       3143
Full waste (%):   0.504462
```

</p></details>

Visualize solution:
```shell
python3 scripts/visualize_rectangleguillotine.py solution_rectangleguillotine.csv
```

## `rectangle` solver

Features:
* Objectives:
  * Knapsack
  * Open dimension X
  * Open dimension Y
  * Bin packing
  * Bin packing with leftovers
  * Variable-sized bin packing
* Item types:
  * With or without rotations
* Bin types:
  * May contain defects
  * Maximum weight
* Unloading constraints: only horizontal/vertical movements, increasing x/y

Example:
```shell
./install/bin/packingsolver_rectangle \
        --verbosity-level 1 \
        --items data/rectangle/afsharian2014/450-200.txt/C22M25R10N15_D4_items.csv \
        --bins data/rectangle/afsharian2014/450-200.txt/C22M25R10N15_D4_bins.csv \
        --defects data/rectangle/afsharian2014/450-200.txt/C22M25R10N15_D4_defects.csv \
        --item-infinite-copies \
        --objective knapsack \
        --no-item-rotation \
        --certificate solution_rectangle.csv \
        --time-limit 5
```

<details><p>

```
=================================
          PackingSolver          
=================================

Problem type
------------
Rectangle

Instance
--------
Objective:             Knapsack
Number of item types:  25
Number of items:       247
Number of bin types:   1
Number of bins:        1
Number of groups:      1
Number of defects:     4
Unloading constraint:  None
Total item area:       2576510
Total item width:      33005
Total item height:     17382
Smallest item width:   47
Smallest item height:  21
Total bin area:        90000
Total item weight:     0
Total bin weight:      0

        Time        Profit   # items                         Comment
        ----        ------   -------                         -------
       0.001         10773         1                  TS g 4 d X q 1
       0.002         17052         1                  TS g 4 d X q 1
       0.002         23765         1                  TS g 4 d X q 1
       0.003         27825         2                  TS g 4 d X q 1
       0.003         30429         2                  TS g 4 d X q 1
       0.004         34538         2                  TS g 4 d Y q 1
       0.004         39178         3                  TS g 4 d Y q 1
       0.005         40237         4                  TS g 4 d X q 1
       0.005         43421         2                  TS g 5 d Y q 1
       0.006         43818         4                  TS g 4 d Y q 1
       0.006         50405         5                  TS g 4 d X q 1
       0.007         52631         6                  TS g 4 d X q 1
       0.007         53985         5                  TS g 5 d Y q 1
       0.008         54875         7                  TS g 4 d X q 1
       0.008         57101         8                  TS g 4 d X q 1
       0.009         59327         9                  TS g 4 d X q 1
       0.009         61553        10                  TS g 4 d X q 1
       0.010         63797        11                  TS g 4 d X q 1
       0.010         66041        12                  TS g 4 d X q 1
       0.011         66125        13                  TS g 4 d X q 1
       0.011         67227        15                  TS g 4 d X q 1
       0.012         69471        16                  TS g 4 d X q 1
       0.014         69760        17                  TS g 4 d X q 3
       0.017         70866        10                 TS g 5 d Y q 19
       0.017         71638        11                 TS g 5 d Y q 19
       0.020         71674        12                 TS g 5 d Y q 28
       0.050         72296        11                TS g 5 d Y q 141
       0.162         72704        21                TS g 4 d X q 141
       0.282         72832        19                TS g 4 d Y q 316
       0.282         73344        19                TS g 4 d Y q 316
       0.286         73443        20                TS g 4 d Y q 316
       0.813         73980        20                TS g 4 d X q 711
       1.196         73997        22               TS g 4 d X q 1066
       1.794         74170        21               TS g 4 d X q 1599
       4.873         74986        22               TS g 4 d X q 3597

Final statistics
----------------
Time (s):  5.02934

Solution
--------
Number of items:  22 / 247 (8.90688%)
Item area:        74986 / 2576510 (2.91037%)
Item weight:      0 / 0 (-nan%)
Item profit:      74986 / 2.57651e+06 (2.91037%)
Number of bins:   1 / 1 (100%)
Bin area:         90000 / 90000 (100%)
Bin weight:       0 / 0 (-nan%)
Bin cost:         90000
Waste:            14166
Waste (%):        15.8897
Full waste:       15014
Full waste (%):   16.6822
Area load:        0.833178
Weight load:      -nan
X max:            448
Y max:            199
Leftover value:   848
```

</p></details>

Visualize solution:
```shell
python3 scripts/visualize_rectangle.py solution_rectangle.csv
```

## `box` solver

Features:
* Objectives:
  * Knapsack
  * Bin packing
  * Open dimension X
  * Open dimension Y
  * Open dimension Z
  * Variable-sized bin packing
* Item types:
  * Rotations (among the 6 possible rotations)
* Bin types:
  * Maximum weight

Example:
```shell
./install/bin/packingsolver_box \
        --verbosity-level 1 \
        --items ./data/box/bischoff1995/BR3.txt_1 \
        --objective knapsack \
        --certificate solution_box.csv \
        --output output.json \
        --time-limit 10
```

<details><p>

```
=================================
          PackingSolver          
=================================

Problem type
------------
Box

Instance
--------
Objective:             Knapsack
Number of item types:  8
Number of items:       94
Number of bin types:   1
Number of bins:        1
Number of defects:     0
Total item volume:     29989656
Total item profit:     2.99897e+07
Largest item profit:   867240
Total item weight:     0
Largest item copies:   24
Total bin volume:      30089620
Total bin weight:      inf
Largest bin cost:      136771

        Time        Profit   # items                         Comment
        ----        ------   -------                         -------
       0.000        246240         1                  TS g 4 d Y q 1
       0.001        409860         1                  TS g 4 d Y q 1
       0.008        867240         1                  TS g 4 d Y q 1
       0.008   1.11348e+06         2                  TS g 4 d Y q 1
       0.008    1.2771e+06         2                  TS g 5 d Y q 1
       0.008   1.73448e+06         2                  TS g 5 d Y q 1
       0.008   1.98072e+06         3                  TS g 5 d Y q 1
       0.009   2.14434e+06         3                  TS g 5 d Y q 1
       0.009   2.60172e+06         3                  TS g 5 d Y q 1
       0.009   2.84796e+06         4                  TS g 5 d Y q 1
       0.009   3.01158e+06         4                  TS g 5 d Y q 1
       0.009   3.46896e+06         4                  TS g 5 d Y q 1
       0.010    3.7152e+06         5                  TS g 5 d X q 1
       0.010   3.87882e+06         5                  TS g 5 d X q 1
       0.010    4.3362e+06         5                  TS g 5 d X q 1
       0.011   4.58244e+06         6                  TS g 5 d X q 1
       0.011   4.74606e+06         6                  TS g 5 d X q 1
       0.011   5.20344e+06         6                  TS g 5 d X q 1
       0.011   5.44968e+06         7                  TS g 5 d X q 1
       0.011    5.6133e+06         7                  TS g 5 d X q 1
       0.012   6.07068e+06         7                  TS g 5 d X q 1
       0.012   6.18893e+06         8                  TS g 5 d X q 1
       0.012   6.31692e+06         8                  TS g 5 d Y q 1
       0.013   6.48054e+06         8                  TS g 5 d Y q 1
       0.013   6.93792e+06         8                  TS g 5 d Y q 1
       0.013   7.05617e+06         9                  TS g 5 d X q 1
       0.013   7.18416e+06         9                  TS g 5 d Y q 1
       0.014   7.34778e+06         9                  TS g 5 d Y q 1
       0.014   7.80516e+06         9                  TS g 5 d Y q 1
       0.014   7.92341e+06        10                  TS g 5 d X q 1
       0.015    8.0514e+06        10                  TS g 5 d Y q 1
       0.015   8.21502e+06        10                  TS g 5 d Y q 1
       0.015    8.6724e+06        10                  TS g 5 d Y q 1
       0.015   8.79065e+06        11                  TS g 5 d X q 1
       0.016   8.91864e+06        11                  TS g 5 d Y q 1
       0.016   9.08226e+06        11                  TS g 5 d Y q 1
       0.016   9.53964e+06        11                  TS g 5 d Y q 1
       0.017   9.65789e+06        12                  TS g 5 d X q 1
       0.017   9.78588e+06        12                  TS g 5 d Y q 1
       0.017    9.9495e+06        12                  TS g 5 d Y q 1
       0.018   1.00678e+07        13                  TS g 5 d X q 1
       0.018   1.01957e+07        13                  TS g 5 d Y q 1
       0.018   1.03594e+07        13                  TS g 5 d Y q 1
       0.019   1.04776e+07        14                  TS g 5 d X q 1
       0.019   1.06056e+07        14                  TS g 5 d Y q 1
       0.019   1.07692e+07        14                  TS g 5 d Y q 1
       0.020   1.08506e+07        15                  TS g 5 d X q 1
       0.020   1.10155e+07        15                  TS g 5 d Y q 1
       0.021   1.11791e+07        15                  TS g 5 d Y q 1
       0.021   1.12151e+07        17                  TS g 5 d X q 1
       0.021   1.14253e+07        16                  TS g 5 d Y q 1
       0.022   1.15889e+07        16                  TS g 5 d Y q 1
       0.022   1.17516e+07        18                  TS g 5 d X q 1
       0.023   1.18352e+07        17                  TS g 5 d Y q 1
       0.023   1.19988e+07        17                  TS g 5 d Y q 1
       0.024   1.21615e+07        19                  TS g 5 d X q 1
       0.024   1.24077e+07        20                  TS g 5 d X q 1
       0.025   1.25714e+07        20                  TS g 5 d X q 1
       0.025   1.28176e+07        21                  TS g 5 d X q 1
       0.026   1.29812e+07        21                  TS g 5 d X q 1
       0.026   1.32275e+07        22                  TS g 5 d X q 1
       0.027   1.33911e+07        22                  TS g 5 d X q 1
       0.027   1.36373e+07        23                  TS g 5 d X q 1
       0.028   1.38009e+07        23                  TS g 5 d X q 1
       0.028   1.40472e+07        24                  TS g 5 d X q 1
       0.029   1.41739e+07        24                  TS g 5 d X q 1
       0.030   1.44201e+07        25                  TS g 5 d X q 1
       0.030   1.45469e+07        25                  TS g 5 d X q 1
       0.031   1.47931e+07        26                  TS g 5 d X q 1
       0.031   1.49198e+07        26                  TS g 5 d X q 1
       0.032   1.51661e+07        27                  TS g 5 d X q 1
       0.032   1.52928e+07        27                  TS g 5 d X q 1
       0.033   1.54123e+07        28                  TS g 5 d X q 1
       0.033    1.5539e+07        28                  TS g 5 d X q 1
       0.034   1.56657e+07        28                  TS g 5 d Y q 1
       0.035   1.57853e+07        29                  TS g 5 d X q 1
       0.035    1.5912e+07        29                  TS g 5 d X q 1
       0.036   1.60387e+07        29                  TS g 5 d Y q 1
       0.036   1.61582e+07        30                  TS g 5 d X q 1
       0.037   1.62849e+07        30                  TS g 5 d X q 1
       0.037   1.64117e+07        30                  TS g 5 d Y q 1
       0.038   1.65312e+07        31                  TS g 5 d X q 1
       0.039   1.66579e+07        31                  TS g 5 d X q 1
       0.039   1.67846e+07        31                  TS g 5 d Y q 1
       0.040   1.69041e+07        32                  TS g 5 d X q 1
       0.040   1.70309e+07        32                  TS g 5 d X q 1
       0.041   1.71092e+07        32                  TS g 5 d Y q 1
       0.042   1.72771e+07        33                  TS g 5 d X q 1
       0.042   1.73554e+07        33                  TS g 5 d X q 1
       0.043   1.74338e+07        33                  TS g 5 d Y q 1
       0.043   1.76017e+07        34                  TS g 5 d X q 1
       0.044     1.768e+07        34                  TS g 5 d X q 1
       0.045   1.77583e+07        34                  TS g 5 d Y q 1
       0.045   1.79263e+07        35                  TS g 5 d X q 1
       0.046   1.80046e+07        35                  TS g 5 d X q 1
       0.047   1.80829e+07        35                  TS g 5 d Y q 1
       0.047   1.81725e+07        36                  TS g 5 d X q 1
       0.048   1.82508e+07        36                  TS g 5 d X q 1
       0.049   1.84075e+07        36                  TS g 5 d Y q 1
       0.049   1.84971e+07        37                  TS g 5 d X q 1
       0.050   1.85754e+07        37                  TS g 5 d X q 1
       0.051   1.87321e+07        37                  TS g 5 d Y q 1
       0.051   1.88216e+07        38                  TS g 5 d X q 1
       0.052      1.89e+07        38                  TS g 5 d X q 1
       0.053   1.90567e+07        38                  TS g 5 d Y q 1
       0.053   1.91462e+07        39                  TS g 5 d X q 1
       0.054   1.92246e+07        39                  TS g 5 d X q 1
       0.055   1.93812e+07        39                  TS g 5 d Y q 1
       0.056   1.94708e+07        40                  TS g 5 d X q 1
       0.056   1.95491e+07        40                  TS g 5 d X q 1
       0.057   1.97058e+07        40                  TS g 5 d Y q 1
       0.058   1.97954e+07        41                  TS g 5 d X q 1
       0.059   1.98737e+07        41                  TS g 5 d X q 1
       0.059    1.9952e+07        41                  TS g 5 d Y q 1
       0.060   2.00416e+07        42                  TS g 5 d X q 1
       0.061   2.01199e+07        42                  TS g 5 d X q 1
       0.062   2.01599e+07        43                  TS g 5 d X q 1
       0.063   2.01885e+07        43                  TS g 5 d Y q 1
       0.063   2.02382e+07        43                  TS g 5 d X q 1
       0.064   2.03068e+07        44                  TS g 5 d Y q 1
       0.065    2.0313e+07        44                  TS g 5 d X q 1
       0.066   2.03816e+07        45                  TS g 5 d Y q 1
       0.067   2.05593e+07        45                  TS g 5 d X q 1
       0.067   2.06376e+07        45                  TS g 5 d X q 1
       0.069   2.08055e+07        46                  TS g 5 d X q 1
       0.069   2.08839e+07        46                  TS g 5 d X q 1
       0.070   2.10021e+07        47                  TS g 5 d X q 1
       0.071   2.12084e+07        47                  TS g 5 d X q 1
       0.072   2.13267e+07        48                  TS g 5 d X q 1
       0.073    2.1533e+07        48                  TS g 5 d X q 1
       0.074   2.16513e+07        49                  TS g 5 d X q 1
       0.075   2.18576e+07        49                  TS g 5 d X q 1
       0.076   2.19758e+07        50                  TS g 5 d X q 1
       0.077   2.21038e+07        50                  TS g 5 d X q 1
       0.078   2.22221e+07        51                  TS g 5 d X q 1
       0.079   2.23501e+07        51                  TS g 5 d X q 1
       0.080   2.24683e+07        52                  TS g 5 d X q 1
       0.081   2.25444e+07        52                  TS g 5 d X q 1
       0.081   2.25963e+07        52                  TS g 5 d X q 1
       0.083   2.27146e+07        53                  TS g 5 d X q 1
       0.083   2.27907e+07        53                  TS g 5 d X q 1
       0.084   2.28425e+07        53                  TS g 5 d X q 1
       0.085   2.29608e+07        54                  TS g 5 d X q 1
       0.086   2.29961e+07        70                  TS g 4 d X q 1
       0.087   2.31228e+07        70                  TS g 4 d X q 1
       0.088    2.3369e+07        71                  TS g 4 d X q 1
       0.089   2.34958e+07        71                  TS g 4 d X q 1
       0.091    2.3742e+07        72                  TS g 4 d X q 1
       0.092   2.38687e+07        72                  TS g 4 d X q 1
       0.093   2.42417e+07        73                  TS g 4 d X q 1
       0.094   2.44879e+07        74                  TS g 4 d X q 1
       0.095   2.45662e+07        74                  TS g 4 d X q 1
       0.096   2.48125e+07        75                  TS g 4 d X q 1
       0.100   2.48619e+07        66                  TS g 5 d X q 1
       0.101   2.49802e+07        67                  TS g 5 d X q 1
       0.103   2.50984e+07        68                  TS g 5 d X q 1
       0.104   2.51733e+07        69                  TS g 5 d X q 1
       0.105   2.52481e+07        70                  TS g 5 d X q 1
       0.106   2.53229e+07        71                  TS g 5 d X q 1
       0.107   2.53978e+07        72                  TS g 5 d X q 1
       0.158   2.54632e+07        79                  TS g 4 d X q 2
       0.255   2.56576e+07        80                  TS g 4 d X q 3
       0.399    2.5706e+07        80                  TS g 4 d X q 4
       0.956   2.57613e+07        73                  TS g 5 d X q 9
       0.970   2.58362e+07        74                  TS g 5 d X q 9
       1.625   2.59038e+07        74                 TS g 5 d X q 13
       1.641   2.59787e+07        75                 TS g 5 d X q 13
       2.772   2.59929e+07        80                 TS g 4 d X q 19
       2.921   2.59991e+07        74                 TS g 5 d X q 19
       2.953   2.60305e+07        75                 TS g 5 d X q 19
       2.954   2.60739e+07        75                 TS g 5 d X q 19
       2.993   2.61488e+07        76                 TS g 5 d X q 19
       7.892   2.62568e+07        80                 TS g 4 d X q 42
       7.901   2.63086e+07        80                 TS g 4 d X q 42
       7.932   2.63209e+07        81                 TS g 4 d X q 42
       7.936   2.63728e+07        81                 TS g 4 d X q 42
       7.954   2.65153e+07        82                 TS g 4 d X q 42
       7.956   2.65188e+07        82                 TS g 4 d X q 42
       7.958   2.65672e+07        82                 TS g 4 d X q 42
       7.972   2.67132e+07        83                 TS g 4 d X q 42

Final statistics
----------------
Time (s):  10.0127

Solution
--------
Number of items:   83 / 94 (88.2979%)
Item volume:       2.67132e+07 / 2.99897e+07 (89.0746%)
Item weight:       0 / 0 (-nan%)
Item profit:       2.67132e+07 / 2.99897e+07 (89.0746%)
Number of stacks:  0
Stack area:        0
Number of bins:    1 / 1 (100%)
Bin volume:        30089620 / 30089620 (100%)
Bin area:          136771 / 136771 (100%)
Bin weight:        inf / inf (-nan%)
Bin cost:          136771
Waste:             3325190
Waste (%):         11.0698
Full waste:        3376450
Full waste (%):    11.2213
Volume load:       0.887787
Area load:         0
Weight load:       0
X max:             586
Y max:             233
```

</p></details>

Visualize solution:
```shell
python3 scripts/visualize_box.py solution_box.csv
```

## `boxstacks` solver

Features:
* Objectives:
  * Knapsack
  * Bin packing
  * Variable-sized bin packing
* Item types:
  * Rotations (among the 6 possible rotations)
  * Nesting height
  * Maximum number of items in a stack containing an item of a given type
  * Maximum weight allowed above an item of a given type
* Bin types:
  * Maximum weight
  * Maximum stack density
  * Maximum weight on middle and rear axles
* Unloading constraints: only horizontal/vertical movements, increasing x/y

Example:
```shell
python3 scripts/download_data.py --data roadef2022_2024-04-25_bpp
./install/bin/packingsolver_boxstacks \
        --verbosity-level 1 \
        --items data/boxstacks/roadef2022_2024-04-25_bpp/C/AS/AS_149_items.csv \
        --bins data/boxstacks/roadef2022_2024-04-25_bpp/C/AS/AS_149_bins.csv \
        --parameters data/boxstacks/roadef2022_2024-04-25_bpp/C/AS/AS_149_parameters.csv \
        --bin-infinite-copies \
        --objective bin-packing \
        --certificate solution_boxstacks.csv \
        --time-limit 1
```

<details><p>

```
=================================
          PackingSolver          
=================================

Problem type
------------
BoxStacks

Instance
--------
Objective:             BinPacking
Number of item types:  13
Number of items:       118
Number of bin types:   1
Number of bins:        118
Number of groups:      1
Number of defects:     0
Unloading constraint:  IncreasingX
Item volume:           196704000000
Bin volume:            13001535000000
Item weight:           17323.9
Bin weight:            2.832e+06

        Time    Bins  Full waste (%)                         Comment
        ----    ----  --------------                         -------
       0.119       2           10.74                     iteration 0

Final statistics
----------------
Time (s):  0.119291

Solution
--------
Number of items:   118 / 118 (100%)
Item volume:       1.96704e+11 / 1.96704e+11 (100%)
Item weight:       17323.9 / 17323.9 (100%)
Item profit:       1.96704e+11 / 1.96704e+11 (100%)
Number of stacks:  37
Stack area:        69600000
Number of bins:    2 / 118 (1.69492%)
Bin volume:        220365000000 / 13001535000000 (1.69492%)
Bin area:          74700000 / 4407300000 (1.69492%)
Bin weight:        48000 / 2832000 (1.69492%)
Bin cost:          6
Waste:             19678500000
Waste (%):         9.09431
Full waste:        23661000000
Full waste (%):    10.7372
Volume load:       0.0151293
Area load:         0.015792
Weight load:       0.00611719
X max:             14400
Y max:             2400
```

</p></details>

Visualize solution:
```shell
python3 scripts/visualize_boxstacks.py solution_boxstacks.csv
```

## `onedimensional` solver

Features:
* Objectives:
  * Knapsack
  * Bin packing
  * Bin packing with leftovers
  * Variable-sized bin packing
* Item types:
  * Nesting length
  * Maximum number of items in a bin containing an item of a given type
  * Maximum weight allowed after an item of a given type
* Bin types:
  * Maximum weight
* Item type / bin type eligibility

Example:
```shell
./install/bin/packingsolver_onedimensional \
        --verbosity-level 2 \
        --items data/onedimensional/users/2024-04-21_items.csv \
        --bins data/onedimensional/users/2024-04-21_bins.csv \
        --parameters data/onedimensional/users/2024-04-21_parameters.csv \
        --time-limit 1 \
        --certificate solution_onedimensional.csv
```

<details><p>
 
 ```
=================================
          PackingSolver          
=================================

Problem type
------------
OneDimensional

Instance
--------
Objective:             VariableSizedBinPacking
Number of item types:  7
Number of items:       43554
Number of bin types:   1
Number of bins:        43554

    Bin type      Length    Max wght        Cost      Copies  Copies min
    --------      ------    --------        ----      ------  ----------
           0        6000         inf        6000       43554           0

    Bin type Eligibility
    -------- -----------

   Item type      Length      Weight   MaxWgtAft     MaxStck      Profit      Copies Eligibility
   ---------      ------      ------   ---------     -------      ------      ------ -----------
           0         837           0         inf  2147483647         837         820          -1
           1        1587           0         inf  2147483647        1587       26640          -1
           2        1987           0         inf  2147483647        1987         372          -1
           3        2487           0         inf  2147483647        2487       15602          -1
           4         727           0         inf  2147483647         727          40          -1
           5        1627           0         inf  2147483647        1627          40          -1
           6         747           0         inf  2147483647         747          40          -1

        Time          Cost  # bins  Full waste (%)                         Comment
        ----          ----  ------  --------------                         -------
       0.006    8.8338e+07   14723            6.46                        SVC it 0
       0.011     8.769e+07   14615            5.77                        SVC it 1
       0.023    8.7624e+07   14604            5.70                        SVC it 3
       0.027    8.7612e+07   14602            5.69                        SVC it 4
       0.070     8.757e+07   14595            5.64                          CG n 1

Final statistics
----------------
Time (s):  1.00035

Solution
--------
Number of items:  43554 / 43554 (100%)
Item length:      8.26294e+07 / 8.26294e+07 (100%)
Item profit:      8.26294e+07 / 8.26294e+07 (100%)
Number of bins:   14595 / 43554 (33.5101%)
Bin length:       87570000 / 261324000 (33.5101%)
Bin cost:         8.757e+07
Waste:            4940351
Waste (%):        5.64162
Full waste:       4940602
Full waste (%):   5.64189

         Bin        Type      Copies      Length      Weight     # items
         ---        ----      ------      ------      ------     -------
           0           0         124        5969           0           3
           1           0         231        4978           0           2
           2           0       13320        5669           0           3
           3           0         820        5819           0           3
           4           0          40        5709           0           3
           5           0          40        5729           0           3
           6           0          20        5749           0           3
```

</p></details>

Visualize:
```shell
python3 scripts/visualize_onedimensional.py solution_onedimensional.csv
```

## `irregular` solver

Features:
* Objectives:
  * Knapsack
  * Open dimension X
  * Open dimension Y
  * Bin packing
  * Bin packing with leftovers
  * Variable-sized bin packing
* Item types:
  * Polygonal shape (possibly non-convex, possibly with holes)
  * Discrete rotations
* Bin types:
  * Polygonal shape (possibly non-convex)
  * May contain different quality areas
* Minimum distance between each pair of items
* Minimum distance between each item and its container

Example:
```shell
./install/bin/packingsolver_irregular \
        --verbosity-level 1 \
        --input ./data/irregular/opencutlist/knight_armor.json \
        --time-limit 10 \
        --certificate solution_irregular.json
```

<details><p>

```
=================================
          PackingSolver          
=================================

Problem type
------------
Irregular

Instance
--------
Objective:                    BinPackingWithLeftovers
Number of item types:         45
Number of items:              100
Number of bin types:          1
Number of bins:               1
Number of defects:            0
Number of rectangular items:  5
Number of circular items:     0
Item area:                    2.91022e+06
Smallest item area:           3749.55
Largest item area:            90886.6
Bin area:                     5.796e+06
Item-bin minimum spacing:     0
Item-item minimum spacing:    0

        Time      # bins    Leftover                         Comment
        ----      ------    --------                         -------
       0.365           1 1.55619e+06                  TS g 0 d 5 q 1
       0.367           1 1.64045e+06                  TS g 0 d 5 q 1
       0.413           1 1.79437e+06                  TS g 0 d 0 q 1
       0.451           1 1.81666e+06                  TS g 0 d 4 q 1
       0.520           1 1.98572e+06                  TS g 1 d 5 q 1
       0.543           1 2.03254e+06                  TS g 1 d 4 q 1
       2.266           1 2.05049e+06                  TS g 1 d 5 q 3
       3.159           1 2.05693e+06                  TS g 1 d 0 q 4
       3.544           1 2.06829e+06                  TS g 1 d 1 q 4
       4.708           1 2.11687e+06                  TS g 1 d 0 q 6
       5.137           1 2.12846e+06                  TS g 1 d 1 q 6

Final statistics
----------------
Time (s):  10.0096

Solution
--------
Number of items:  100 / 100 (100%)
Item area:        2.91016e+06 / 2.91022e+06 (99.9981%)
Item profit:      2.91022e+06 / 2.91022e+06 (100%)
Number of bins:   1 / 1 (100%)
Bin area:         5796000 / 5796000 (100%)
Bin cost:         5.796e+06
Full waste:       2885838
Full waste (%):   49.7902
X max:            1771.76
Y max:            2070
Leftover value:   2.12846e+06
```

</p></details>

Visualize:
```shell
python3 scripts/visualize_irregular.py solution_irregular.json
```
