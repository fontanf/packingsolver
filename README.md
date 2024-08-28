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

## Usage

For macOS, CLP needs to be installed manually with [Homebrew](https://brew.sh/):
```sh
brew install clp
```
For Windows and Linux, CLP is downloaded automatically when building.

Compile:
```shell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
cmake --install build --config Release --prefix install
```

Execute:
```shell
./install/bin/packingsolver_rectangleguillotine  --verbosity-level 1 --objective knapsack  --predefined 3NHO  --items data/rectangle/alvarez2002/ATP35_items.csv  --bins data/rectangle/alvarez2002/ATP35_bins.csv  --certificate ATP35_solution.csv  --output ATP35_output.json  --time-limit 1
```

Or in short:
```shell
./install/bin/packingsolver_rectangleguillotine -v 1 -f KP -p 3NHO -i data/rectangle/alvarez2002/ATP35 -c ATP35_solution.csv -o ATP35_output.json -t 1
```
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

Visualize:
```
python3 scripts/visualize_rectangleguillotine.py ATP35_solution.csv
```
