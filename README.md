# PackingSolver

A state-of-the-art tree search based solver for (geometrical) Packing Problems.

Only rectangle two- and three staged guillotine variants have been implemented yet.

![Example](example.png?raw=true "Example")

## Usage

Note: PackingSolver favours efficiency and flexibility over ease of use. Therefore, some knowledge about tree search algorithms and packing problems is required in order to use it.

Compile:
```shell
bazel build -- //...
# Or, to enable the column generation heuristic for VBPP:
bazel build --define cplex=true -- //...
```

Execute:
```shell
./bazel-bin/packingsolver/main  --verbose  --problem-type rectangleguillotine  --objective knapsack  --items data/rectangle/alvarez2002/ATP35_items.csv  --bins data/rectangle/alvarez2002/ATP35_bins.csv  --certificate ATP35_solution.csv  --output ATP35_output.json  --time-limit 1  -q "RG -p 3NHO -c 4" -a "IBS"  -q "RG -p 3NHO -c 5" -a "IBS"
```

Or in short:
```shell
./bazel-bin/packingsolver/main -v -p RG -f KP -i data/rectangle/alvarez2002/ATP35 -c ATP35_solution.csv -o ATP35_output.json -t 1  -q "RG -p 3NHO -c 4" -a "IBS"  -q "RG -p 3NHO -c 5" -a "IBS"
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

        Time        Profit   # items                         Comment
        ----        ------   -------                         -------
       0.001         68970         1              IBS (thread 1) q 1
       0.001         72000         1              IBS (thread 1) q 1
       0.007         76395         2              IBS (thread 1) q 1
       0.008         90705         2              IBS (thread 1) q 1
       0.008        132839         2              IBS (thread 1) q 1
       0.008        140970         2              IBS (thread 1) q 1
       0.008        148395         3              IBS (thread 1) q 1
       0.008        150705         4              IBS (thread 1) q 1
       0.008        153015         5              IBS (thread 1) q 1
       0.008        212970         3              IBS (thread 2) q 1
       0.008        216000         3              IBS (thread 2) q 1
       0.008        284970         4              IBS (thread 2) q 1
       0.009        292395         5              IBS (thread 2) q 1
       0.009        306705         5              IBS (thread 2) q 1
       0.009        348839         5              IBS (thread 2) q 1
       0.009        358042         6              IBS (thread 2) q 1
       0.009        375517        13              IBS (thread 1) q 1
       0.009        379768         7              IBS (thread 2) q 1
       0.009        383648        13              IBS (thread 1) q 1
       0.010        388389         7              IBS (thread 2) q 1
       0.010        391073        14              IBS (thread 1) q 1
       0.010        408379         7              IBS (thread 2) q 1
       0.010        415804         8              IBS (thread 2) q 1
       0.010        447517        14              IBS (thread 1) q 1
       0.010        449082        16              IBS (thread 1) q 1
       0.011        456337        16              IBS (thread 1) q 1
       0.011        459088        18              IBS (thread 1) q 1
       0.011        466142        20              IBS (thread 1) q 1
       0.011        513965        20              IBS (thread 1) q 1
       0.012        521390        21              IBS (thread 1) q 1
       0.012        530011        21              IBS (thread 1) q 1
       0.012        577834        21              IBS (thread 1) q 1
       0.012        581548         9              IBS (thread 2) q 2
       0.013        588973        10              IBS (thread 2) q 2
       0.013        597058        10              IBS (thread 2) q 2
       0.013        599368        11              IBS (thread 2) q 2
       0.013        602118        14              IBS (thread 1) q 2
       0.015        605793        11              IBS (thread 2) q 9
       0.019        606147        13             IBS (thread 2) q 19
       0.026        606672        12             IBS (thread 2) q 42
       0.036        607062        14             IBS (thread 2) q 94
       0.059        609550        15            IBS (thread 2) q 211
       0.095        610101        31            IBS (thread 1) q 141
       0.095        610578        31            IBS (thread 1) q 141
       0.096        610787        32            IBS (thread 1) q 141
       0.138        611135        34            IBS (thread 1) q 211
       0.199        614725        31            IBS (thread 1) q 316
       0.206        614967        42            IBS (thread 1) q 316
       0.328        616880        16           IBS (thread 2) q 1599
       0.618        619897        28           IBS (thread 1) q 1066

Final statistics
----------------
Profit:            619897
Number of items:   28
Time:              1.00226
```

A solution visualizer is available here: https://librallu.gitlab.io/packing-viz/

Problem types: `rectangleguillotine` (`RG`)

The problem type defines the certificate format.
Each problem type has a list of available objectives and a list of available branching schemes.

Each branching scheme has a list a compatible algorithms.

Each pair `-q`/`-a` respectively defines the branching scheme and the algorithm for a thread.

Options `--bin-infinite-copies`, `--bin-infinite-width`, `--bin-infinite-height`, `--bin-unweighted`, `--item-infinite-copies` and `--unweighted` are available to modify the instance properties.

### Problem type rectangleguillotine (RG)

Available objectives:
* `default`
* `bin-packing` (`BPP`)
* `knapsack` (`KP`)
* `strip-packing-width` (`SPPW`)
* `strip-packing-height` (`SPPH`)
* `bin-packing-with-leftovers` (`BPPL`)
* `variable-sized-bin-packing` (`VBPP`)

Algorithms:
* Branching scheme `rectangle-guillotine` (`RG`)
  (Objectives: `default`, `BPP`, `KP`, `SPPW`, `SPPH`, `BPPL`)
* Column generation heuristic + branching scheme `RG`
  (Objectives: `VBPP`)

### Branching scheme rectangle-guillotine (RG)

options:
* `--cut-type-1`: `three-staged-guillotine`, `two-staged-guillotine`
* `--cut-type-2`: `roadef2018`, `non-exact`, `exact`, `homogenous`
* `--first-stage-orientation`: `vertical`, `horizontal`, `any`
* `--min1cut`, `--max1cut`, `--min2cut`: positive integer
* `--min-waste`: positive integer
* `--one2cut`
* `--no-item-rotation`
* `--cut-through-defects`
* `-p`: predefined configurations. Examples:
  * `3RVR`: `--cut-type-1 three-staged-guillotine --cut-type-2 roadef2018 --first-stage-orientation vertical`
  * `2NHO`: `--cut-type-1 two-staged-guillotine --cut-type-2 non-exact --first-stage-orientation horizontal --no-item-rotation`
  * `3EAR`: `--cut-type-1 three-staged-guillotine --cut-type-2 exact --first-stage-orientation any`
  * `roadef2018`

## Benchmarks

The performances of PackingSolver have been compared to all published results from the scientific literature on corresponding Packing Problems.
Detailed results are available in `results_*.ods`.
`output.7z` contains all output files and solutions.

Do not hesitate to contact us if you are aware of any variant or article that we missed.

All experiments can be reproduced using the following scripts:
```shell
python3 packingsolver/scripts/bench.py "roadef2018_A" "roadef2018_B" "roadef2018_X" # ~50h
python3 packingsolver/scripts/bench.py "3NEGH-BPP-O" "3NEGH-BPP-R" "long2020_BPP" # ~10h
python3 packingsolver/scripts/bench.py "3GH-BPP-O" "3HG-BPP-O" "3HGV-BPP-O" # ~7h
python3 packingsolver/scripts/bench.py "2NEGH-BPP-O" "2NEGH-BPP-R" "2GH-BPP-O" # ~30h
python3 packingsolver/scripts/bench.py "3NEG-KP-O" "3NEG-KP-R" "3NEGV-KP-O" "3HG-KP-O" # ~10h
python3 packingsolver/scripts/bench.py "2NEG-KP-O" "2NEGH-KP-O" "2NEGV-KP-O" "2NEGH-KP-R"  # 1h
python3 packingsolver/scripts/bench.py "2G-KP-O" "2GH-KP-O" "2GV-KP-O" # 1m
python3 packingsolver/scripts/bench.py "3NEGH-SPP-O" "3NEGH-SPP-R" # ~20h
python3 packingsolver/scripts/bench.py "2NEGH-SPP-O" "2NEGH-SPP-R" # ~4h
python3 packingsolver/scripts/bench.py "3NEGH-CSP-O" "3NEGH-CSP-R" "long2020_CSP" # ~20h
python3 packingsolver/scripts/bench.py "3GH-CSP-O" "3HG-CSP-O" "3HGV-CSP-O" # ~15h
python3 packingsolver/scripts/bench.py "2NEGH-CSP-O" "2NEGH-CSP-R" "2GH-CSP-O" # ~30h
python3 packingsolver/scripts/bench.py "3NEG-VBPP-O" "3NEG-VBPP-R" "2GH-VBPP-O" "2GH-VBPP-R" # ~16h
```

