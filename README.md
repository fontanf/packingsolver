# PackingSolver

A state-of-the-art tree search based solver for (geometrical) Packing Problems.

Only rectangle two- and three staged guillotine variants have been implemented yet.

![Example](example.png?raw=true "Example")

* Code author: Florian Fontan
* Algorithm design: [Luc Libralesso](https://github.com/librallu), Florian Fontan

For commercial support, custom developments or industrial collaboration, please do not hesitate to contact me (Florian Fontan).
For academic collaboration, please do not hesitate to contact Luc Libralesso.

## Usage

Note: PackingSolver favours efficiency and flexibility over ease of use. Therefore, some knowledge about tree search algorithms and packing problems is required in order to use it.

Compile:
```shell
bazel build -- //...
```

Execute:
```shell
./bazel-bin/packingsolver/main  --verbose  --problem-type rectangleguillotine  --objective knapsack  --items data/rectangle/alvarez2002/ATP35_items.csv  --bins data/rectangle/alvarez2002/ATP35_bins.csv  --certificate ATP35_solution.csv  --output ATP35_output.json  --time-limit 1  -q "RG -p 3NHO" -a "IMBA* -c 4"  -q "RG -p 3NHO" -a "IMBA* -c 5"
```

Or in short:
```shell
./bazel-bin/packingsolver/main -v -p RG -f KP -i data/rectangle/alvarez2002/ATP35 -c ATP35_solution.csv -o ATP35_output.json -t 1  -q "RG -p 3NHO" -a "IMBA* -c 4"  -q "RG -p 3NHO" -a "IMBA* -c 5"
```

A solution visualizer is available here: https://librallu.gitlab.io/packing-viz/

Problem types: `rectangleguillotine` (`RG`)

The problem type defines the certificate format.
Each problem type has a list of available objectives and a list of available branching schemes.

Each branching scheme has a list a compatible algorithms.

Each pair `-q`/`-a` respectively defines the branching scheme and the algorithm for a thread.

Options `--bin-infinite-copies`, `--bin-infinite-width`, `--bin-infinite-height`, `--item-infinite-copies` and `--unweighted` are available to modify the instance properties.

### Problem type rectangleguillotine (RG)

* Available objectives: `default`, `bin-packing` (`BPP`), `knapsack` (`KP`), `strip-packing-width` (`SPPW`), `strip-packing-height` (`SPPH`), `bin-packing-with-leftovers` (`BPPL`)
* Available branching schemes: `rectangle-guillotine` (`RG`)

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

Compatible algorithms: `A*`, `DFS`, `IMBA*`, `DPA*`

## Benchmarks

The performances of PackingSolver have been compared to all published results from the scientific literature on corresponding Packing Problems.
Detailed results are available in `results_*.ods`.
`output.7z` contains all output files and solutions.

Do not hesitate to contact us if you are aware of any variant or article that we missed.

All experiments can be reproduced using the following scripts:
```shell
python3 packingsolver/scripts/bench.py "roadef2018_A" "roadef2018_B" "roadef2018_X" # ~50h
python3 packingsolver/scripts/bench.py "3NEGH-BPP-O" "3NEGH-BPP-R" "3GH-BPP-O" "3HGV-BPP-O" "long2020" # ~30h
python3 packingsolver/scripts/bench.py "2NEGH-BPP-O" "2NEGH-BPP-R" "2GH-BPP-O" # ~30h
python3 packingsolver/scripts/bench.py "3NEG-KP-O" "3NEG-KP-R" "3NEGV-KP-O" "3HG-KP-O" # ~10h
python3 packingsolver/scripts/bench.py "2NEG-KP-O" "2NEGH-KP-O" "2NEGV-KP-O" "2NEGH-KP-R" "2G-KP-O" "2GH-KP-O" "2GV-KP-O" # 1h
python3 packingsolver/scripts/bench.py "3NEGH-SPP-O" "3NEGH-SPP-R" # ~20h
python3 packingsolver/scripts/bench.py "2NEGH-SPP-O" "2NEGH-SPP-R" # ~4h
```

