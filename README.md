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

## Usage

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
===================================
           PackingSolver           
===================================

Problem type
------------
rectangleguillotine

Instance
--------
Objective:                Knapsack
Number of item types:     29
Number of items:          153
Number of bin types:      1
Number of bins:           1
Number of stacks:         29
Number of defects:        0
Cut type 1:               ThreeStagedGuillotine
Cut type 2:               NonExact
First stage orientation:  Horinzontal
min1cut:                  0
max1cut:                  -1
min2cut:                  0
max2cut:                  -1
Minimum waste:            1
one2cut:                  0
Cut through defects:      0

        Time        Profit   # items                         Comment
        ----        ------   -------                         -------
       0.000         68970         1              IBS (thread 1) q 1
       0.000         72000         1              IBS (thread 1) q 1
       0.000         76395         2              IBS (thread 1) q 1
       0.008         90705         2              IBS (thread 1) q 1
       0.008        132839         2              IBS (thread 1) q 1
       0.008        140970         2              IBS (thread 1) q 1
       0.009        148395         3              IBS (thread 1) q 1
       0.009        150705         4              IBS (thread 1) q 1
       0.010        153015         5              IBS (thread 1) q 1
       0.011        154087         6              IBS (thread 1) q 1
       0.011        196221         6              IBS (thread 1) q 1
       0.012        204352         6              IBS (thread 1) q 1
       0.012        206897         7              IBS (thread 1) q 1
       0.013        215028         7              IBS (thread 1) q 1
       0.013        216000         3              IBS (thread 4) q 1
       0.013        284970         4              IBS (thread 4) q 1
       0.013        292395         5              IBS (thread 4) q 1
       0.013        306705         5              IBS (thread 4) q 1
       0.013        348839         5              IBS (thread 4) q 1
       0.014        358042         6              IBS (thread 4) q 1
       0.014        372343         6              IBS (thread 4) q 1
       0.014        379768         7              IBS (thread 4) q 1
       0.014        388389         7              IBS (thread 4) q 1
       0.014        408379         7              IBS (thread 4) q 1
       0.014        415804         8              IBS (thread 4) q 1
       0.014        424425         8              IBS (thread 4) q 1
       0.015        444415         8              IBS (thread 4) q 1
       0.015        447517        14              IBS (thread 3) q 1
       0.015        449082        16              IBS (thread 3) q 1
       0.015        451840         9              IBS (thread 4) q 1
       0.015        460461         9              IBS (thread 4) q 1
       0.015        480451         9              IBS (thread 4) q 1
       0.016        496497        10              IBS (thread 4) q 1
       0.016        502186        10              IBS (thread 4) q 1
       0.016        523921        11              IBS (thread 4) q 1
       0.016        539967        12              IBS (thread 4) q 1
       0.016        577834        21              IBS (thread 3) q 1
       0.017        581548         9              IBS (thread 4) q 2
       0.017        588973        10              IBS (thread 2) q 2
       0.017        597058        10              IBS (thread 2) q 2
       0.017        599368        11              IBS (thread 2) q 2
       0.017        602118        14              IBS (thread 1) q 2
       0.020        605793        11              IBS (thread 4) q 9
       0.023        606147        13             IBS (thread 4) q 19
       0.029        606672        12             IBS (thread 4) q 42
       0.042        607062        14             IBS (thread 4) q 94
       0.068        609550        15            IBS (thread 4) q 211
       0.118        610101        31            IBS (thread 3) q 141
       0.118        610578        31            IBS (thread 3) q 141
       0.119        610787        32            IBS (thread 3) q 141
       0.158        611135        34            IBS (thread 1) q 156
       0.247        614725        31            IBS (thread 3) q 316
       0.257        614967        42            IBS (thread 3) q 316
       0.295        616880        16            IBS (thread 2) q 857
       0.751        619897        28            IBS (thread 1) q 857

Final statistics
----------------
Profit:            619897
Number of items:   28
Time:              1.00226
```

Visualize:
```
python3 packingsolver/scripts/visualize_rectangleguillotine.py ATP35_solution.csv
```

