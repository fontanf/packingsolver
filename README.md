# PackingSolver

A state-of-the-art solver for (geometrical) Packing Problems.

PackingSolver solves the following problem types:

* `rectangleguillotine`
  * Items: two-dimensional rectangles
  * Only edge-to-edge cuts are allowed

![Example](img/rectangleguillotine.png?raw=true "Example rectangleguillotine")

* `rectangle`
  * Items: two-dimensional rectangles

![Example](img/rectangle.png?raw=true "Example rectangle")

* `boxstacks`
  * Items: three-dimensional rectangular parallelepipeds
  * Items can be stacked; a stack contains items with the same width and length

![Example](img/boxstacks.png?raw=true "Example boxstacks")

* `onedimensional`
  * Items: one-dimensional items

![Example](img/onedimensional.png?raw=true "Example onedimensional")

* `irregular`
  * Items: two-dimensional polygons

![Example](img/irregular.png?raw=true "Example irregular")

## Compilation

```shell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel && cmake --install build --config Release --prefix install
```

## Problem type `rectangleguillotine`

Features:
* Objectives:
  * Knapsack
  * Open dimension X
  * Open dimension Y
  * Bin packing
  * Bin packing with leftovers
  * Variable-sized bin packing
* Item types
  * With or without rotations
  * Stacks (precedence constraints on the order in which items are extracted)
* Bins types
  * May contain defects
  * Allow or forbid cutting through a defect
* Two- and three-staged, exact, non-exact, roadef2018 and homogenous patterns
* First cut vertical, horizontal or any
* Trims
* Cut thickness
* Minimum and maximum distance between consecutive 1- and 2-cuts
* Minimum distance between cuts

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

## Problem type `rectangle`

Features:
* Objectives:
  * Knapsack
  * Open dimension X
  * Open dimension Y
  * Bin packing
  * Bin packing with leftovers
  * Variable-sized bin packing
* Item types
  * With or without rotations
* Bin types
  * May contain defects
  * Maximum weight
* Unloading constraints: only horizontal/vertical movements, increasing x/y

## Problem type `boxstacks`

Features:
* Objectives:
  * Knapsack
  * Bin packing
  * Variable-sized bin packing
* Item types
  * Rotations (among the 6 possible rotations)
  * Nesting height
  * Maximum number of items in a stack containing an item of a given type
  * Maximum weight allowed above an item of a given type
* Bin types
  * Maximum weight
  * Maximum stack density
  * Maximum weight on middle and rear axles
* Unloading constraints: only horizontal/vertical movements, increasing x/y

## Problem type `onedimensional`

Features:
* Objectives:
  * Knapsack
  * Variable-sized bin packing
* Item types
  * Nesting length
  * Maximum number of items in a bin containing an item of a given type
  * Maximum weight allowed after an item of a given type
* Bin types
  * Maximum weight
* Item type / Bin type eligibility

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

## Problem type `irregular`

Features:
* Objectives:
  * Knapsack
  * Open dimension X
  * Open dimension Y
  * Bin packing
  * Bin packing with leftovers
  * Variable-sized bin packing
* Item types
  * Polygonal shape (possibly non-convex, possibly with holes)
  * Discrete rotations
* Bin types
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
