# PackingSolver

A state-of-the-art solver for (geometrical) Packing Problems.

PackingSolver solves the following problem types:

| Problem types            |  Examples |
:------------------------- |:-------------------------
[`rectangleguillotine`](#rectangleguillotine-solver)<ul><li>Items: two-dimensional rectangles</li><li>Only edge-to-edge cuts are allowed</li></ul>  |  <img src="https://github.com/fontanf/packingsolver/blob/master/img/rectangleguillotine.png" align=center width="512">
[`rectangle`](#rectangle-solver)<ul><li>Items: two-dimensional rectangles</li></ul>  |  <img src="https://github.com/fontanf/packingsolver/blob/master/img/rectangle.png" align=center width="512">
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
