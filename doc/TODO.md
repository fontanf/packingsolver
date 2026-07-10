# Documentation TODO

- **`rectangleguillotine`: document the `bin-packing-cutting-cost` objective.**
  Fully implemented (`Objective::BinPackingCuttingCost` in `src/algorithms/common.cpp`,
  parsed from `bin-packing-cutting-cost`/`BinPackingCuttingCost`) but has zero mention in
  `doc/rectangleguillotine.rst`. Unlike every other documented feature, the costs that make
  this objective behave differently from plain `bin-packing` are **CLI-only flags**, not
  `parameters.csv` rows: `--fixed-cutting-costs`, `--variable-cutting-costs`,
  `--waste-cost` (all multitoken, one value per stage; see `src/rectangleguillotine/main.cpp`
  lines ~97-99, 252-268). Without setting these, the objective degenerates to ordinary
  bin-packing, so a meaningful example needs a real per-stage cost tradeoff design (e.g. a
  case where minimizing bins alone picks one cut pattern, but accounting for cutting cost
  favors a different, cheaper-to-cut pattern even at equal bin count). Needs its own
  `Solve:` code block variant showing the extra CLI flags, since every current example
  only passes `--parameters parameters.csv`.

- **`irregular`: add a dedicated "Bin packing with leftovers" section.**
  The objective is used today only incidentally (inside "Basic usage", `doc/irregular.rst`
  line ~114) rather than as its own illustrated section. `doc/objectives.rst` already
  covers the domain-agnostic concept; this section should show what "leftover value"
  concretely means for irregular shapes (e.g. area of remaining shape in the last bin) with
  a without/with-leftovers-objective before/after pair, following the same pattern as
  `rectangleguillotine.rst`'s leftovers example under "Cut types".

- **`rectangle`: add a dedicated "Bin packing with leftovers" section.**
  Same gap as above — the objective is only mentioned in the Features/parameters lists and
  reused as the objective for other examples (rotation, defects), never illustrated on its
  own. Needs a standalone before/after example (e.g. `bin-packing` vs
  `bin-packing-with-leftovers` on the same items/bin, showing the leftover region
  maximized).

## Already resolved this pass (kept here for reference, remove once confirmed stale)

- Removed the "Item type / bin type eligibility" Features bullet and empty section from
  `doc/onedimensional.rst` — the underlying `add_bin_type_eligibility`/
  `set_item_type_eligibility` methods exist only on the C++ `InstanceBuilder` API and are
  never parsed from `items.csv`/`bins.csv`, so the CLI/CSV workflow documented in this file
  can't exercise the feature at all.
