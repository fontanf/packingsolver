"""Reimplementation, on one-dimensional instances, of the rules of the generic
algorithms (src/algorithms/), used to compute the examples of the figures of
the documentation.

The sub-problems are solved exactly, by enumeration: the instances of the
figures are small.
"""

import itertools
from fractions import Fraction


def knapsack(capacity, item_types):
    """Best single knapsack solution.

    'item_types' is a list of (length, profit, copies); returns the number of
    copies of each item type packed. Ties are broken in favor of the largest
    number of copies of the first item types.
    """
    best, best_profit = None, None
    ranges = [range(copies + 1) for _, _, copies in item_types]
    for x in itertools.product(*ranges):
        length = sum(xj * lj for xj, (lj, _, _) in zip(x, item_types))
        if length > capacity:
            continue
        profit = sum(xj * pj for xj, (_, pj, _) in zip(x, item_types))
        if best is None or profit >= best_profit - 1e-9:
            best, best_profit = x, profit
    return list(best)


################################################################################
# Sequential value correction
################################################################################

def sequential_value_correction(
        bin_length,
        number_of_bins,
        item_types,
        number_of_passes,
        initial_profit_exponent=1.1):
    """Bin packing objective, a single bin type.

    'item_types' is a list of (length, copies). Returns, for each pass, the
    profits used, the bins (pattern, number of copies) and the profits after
    the correction.
    """
    profits = [float(length) ** initial_profit_exponent for length, _ in item_types]
    passes = []
    for _ in range(number_of_passes):
        packed = [0] * len(item_types)
        adjusted_space = [0.0] * len(item_types)
        bins = []
        used_bins = 0
        while sum(packed) < sum(c for _, c in item_types) and used_bins < number_of_bins:
            sub_item_types = [
                    (length, profits[j], copies - packed[j])
                    for j, (length, copies) in enumerate(item_types)]
            pattern = knapsack(bin_length, sub_item_types)
            # Number of copies of the pattern.
            copies_pattern = number_of_bins - used_bins
            for j, xj in enumerate(pattern):
                if xj > 0:
                    copies_pattern = min(copies_pattern, (item_types[j][1] - packed[j]) // xj)
            item_space = sum(xj * item_types[j][0] for j, xj in enumerate(pattern))
            waste = bin_length - item_space
            for j, xj in enumerate(pattern):
                ratio = xj * item_types[j][0] / item_space
                adjusted_space[j] += copies_pattern * (xj * item_types[j][0] + ratio * waste)
            for j, xj in enumerate(pattern):
                packed[j] += copies_pattern * xj
            bins.append((pattern, copies_pattern))
            used_bins += copies_pattern
        new_profits = []
        for j, (length, copies) in enumerate(item_types):
            adjusted = adjusted_space[j] + 100 * bin_length * (copies - packed[j])
            new_profits.append(adjusted / copies)
        corrected = [0.5 * p + 0.5 * q for p, q in zip(profits, new_profits)]
        passes.append({
            "profits": profits,
            "bins": bins,
            "adjusted_space": adjusted_space,
            "new_profits": new_profits,
            "corrected_profits": corrected,
        })
        profits = corrected
    return passes


################################################################################
# Sequential single knapsack
################################################################################

def sequential_single_knapsack(bin_length, number_of_bins, item_types):
    """Bin packing objective, a single bin type, profits equal to the lengths.

    Returns the bins (pattern, number of copies).
    """
    passes = sequential_value_correction(
            bin_length, number_of_bins, item_types, 1, initial_profit_exponent=1.0)
    return passes[0]["bins"]


################################################################################
# Column generation
################################################################################

def simplex(costs, columns, rhs):
    """Minimize costs x subject to sum_k columns[k] x_k = rhs, x >= 0.

    Exact (fractions), with a phase 1 on artificial variables, as the dummy
    columns of the column generation, and Bland's rule. Returns the value, the
    solution and the duals of the rows.
    """
    m = len(rhs)
    n = len(columns)
    # Tableau: columns, then artificials.
    a = [[Fraction(columns[k][i]) for k in range(n)] + [Fraction(int(i == r)) for r in range(m)]
         for i in range(m)]
    b = [Fraction(v) for v in rhs]
    basis = [n + i for i in range(m)]

    def solve(c):
        while True:
            # Duals y = c_B B^-1 from the current tableau: reduced costs.
            cb = [c[basis[i]] for i in range(m)]
            entering = None
            for k in range(len(c)):
                if k in basis:
                    continue
                rc = c[k] - sum(cb[i] * a[i][k] for i in range(m))
                if rc < 0:
                    entering = k
                    break
            if entering is None:
                return
            leaving, best = None, None
            for i in range(m):
                if a[i][entering] > 0:
                    ratio = b[i] / a[i][entering]
                    if best is None or ratio < best or (ratio == best and basis[i] < basis[leaving]):
                        leaving, best = i, ratio
            pivot = a[leaving][entering]
            a[leaving] = [v / pivot for v in a[leaving]]
            b[leaving] /= pivot
            for i in range(m):
                if i != leaving and a[i][entering] != 0:
                    f = a[i][entering]
                    a[i] = [v - f * w for v, w in zip(a[i], a[leaving])]
                    b[i] -= f * b[leaving]
            basis[leaving] = entering

    # Phase 1.
    solve([Fraction(0)] * n + [Fraction(1)] * m)
    phase_1_value = sum(b[i] for i in range(m) if basis[i] >= n)
    if phase_1_value > 0:
        # Infeasible: the duals of phase 1 are returned.
        c = [Fraction(0)] * n + [Fraction(1)] * m
        return None, None, duals(a, basis, c, m, n), phase_1_value
    # Phase 2 (artificials forbidden).
    c = [Fraction(v) for v in costs] + [Fraction(10 ** 9)] * m
    solve(c)
    x = [Fraction(0)] * n
    for i in range(m):
        if basis[i] < n:
            x[basis[i]] = b[i]
    value = sum(Fraction(costs[k]) * x[k] for k in range(n))
    return value, x, duals(a, basis, c, m, n), Fraction(0)


def duals(a, basis, c, m, n):
    """Duals y = c_B B^-1: the reduced costs of the artificial columns give
    -y."""
    cb = [c[basis[i]] for i in range(m)]
    return [c[n + r] - (c[n + r] - sum(cb[i] * a[i][n + r] for i in range(m)))
            for r in range(m)]


def column_generation(bin_length, item_types, fixed=None, columns=None, log=None):
    """Column generation of the bin packing objective, a single bin type, each
    bin costing 1.

    'item_types' is a list of (length, copies); 'fixed' a list of (pattern,
    value) already in the solution, whose items are removed from the
    demands. 'columns' is the list of the patterns already generated, updated
    in place. Returns the value and the solution (pattern, value) of the
    linear relaxation.
    """
    fixed = fixed or []
    columns = columns if columns is not None else []
    demands = [copies - sum(v * p[j] for p, v in fixed) for j, (_, copies) in enumerate(item_types)]
    if all(d == 0 for d in demands):
        return Fraction(0), []
    rows = [j for j, d in enumerate(demands) if d > 0]
    while True:
        usable = [p for p in columns
                  if all(p[j] <= demands[j] for j in range(len(item_types)))]
        value, x, y, infeasibility = simplex(
                [1] * len(usable),
                [[p[j] for j in rows] for p in usable],
                [demands[j] for j in rows])
        phase_1 = (value is None)
        duals = [Fraction(0)] * len(item_types)
        for r, j in enumerate(rows):
            duals[j] = y[r]
        # Pricing: a bounded knapsack with profits the duals.
        pattern = knapsack(bin_length, [
            (length, duals[j], demands[j]) for j, (length, _) in enumerate(item_types)])
        profit = sum(duals[j] * pattern[j] for j in range(len(item_types)))
        reduced_cost = (0 if phase_1 else 1) - profit
        if log is not None:
            log.append({
                "phase": 1 if phase_1 else 2,
                "value": infeasibility if phase_1 else value,
                "duals": duals,
                "pattern": pattern,
                "reduced_cost": reduced_cost,
                "number_of_columns": len(usable)})
        if reduced_cost < 0 and pattern not in columns:
            columns.append(pattern)
            continue
        if phase_1:
            return None, None
        return value, [(p, v) for p, v in zip(usable, x) if v > 0]


def branching_column(solution, fixed):
    """Column branched on by the limited discrepancy search and its value."""
    best = None
    for pattern, value in solution:
        fixed_value = sum(v for p, v in fixed if p == pattern)
        main = value.numerator // value.denominator
        if main <= fixed_value:
            main = fixed_value + 1
        diff = main - value
        if best is None or diff < best[2]:
            best = (pattern, main, diff)
    return best[0], best[1]


def dive(bin_length, item_types):
    """Nodes of discrepancy 0 of the limited discrepancy search, down to an
    integer solution."""
    columns = []
    fixed = []
    nodes = []
    while True:
        value, solution = column_generation(bin_length, item_types, fixed, columns)
        nodes.append({"fixed": list(fixed), "value": value, "solution": solution})
        if all(v.denominator == 1 for _, v in solution):
            return nodes
        pattern, main = branching_column(solution, fixed)
        fixed = [(p, v) for p, v in fixed if p != pattern] + [(pattern, main)]


################################################################################
# Dichotomic search
################################################################################

def fits(bins, items):
    """Whether the items (lengths) fit in the bins (lengths); returns the
    contents of the bins, or None."""
    items = sorted(items, reverse=True)
    contents = [[] for _ in bins]
    loads = [0] * len(bins)

    def rec(i):
        if i == len(items):
            return True
        tried = set()
        for b in range(len(bins)):
            key = (bins[b], loads[b])
            if key in tried or loads[b] + items[i] > bins[b]:
                continue
            tried.add(key)
            loads[b] += items[i]
            contents[b].append(items[i])
            if rec(i + 1):
                return True
            loads[b] -= items[i]
            contents[b].pop()
        return False

    return contents if rec(0) else None


def dichotomic_search(bin_types, items, initial_waste_percentage=0.1):
    """Variable-sized bin packing objective.

    'bin_types' is a list of (length, cost, copies), 'items' a list of
    lengths. Returns the iterations: the waste estimate, the bins chosen (number
    of copies of each bin type), whether the items fit in them, and the bounds
    of the search after it.
    """
    item_space = sum(items)
    bin_space = sum(length * copies for length, _, copies in bin_types)
    lower, upper = 0.0, float("inf")
    memory = {}
    iterations = []
    while upper - lower > 0.00001:
        if upper == float("inf"):
            waste = initial_waste_percentage if lower == 0 else 2 * lower
        else:
            waste = (lower + upper) / 2
        # Knapsack: the bins not used. The weights of the knapsack solver are
        # integers.
        capacity = bin_space - int(item_space * (1 + waste))
        best, best_cost = None, None
        for unused in itertools.product(*[range(c + 1) for _, _, c in bin_types]):
            space = sum(u * l for u, (l, _, _) in zip(unused, bin_types))
            if space > capacity:
                continue
            cost = sum(u * c for u, (_, c, _) in zip(unused, bin_types))
            if best is None or cost > best_cost:
                best, best_cost = unused, cost
        chosen = tuple(c - u for u, (_, _, c) in zip(best, bin_types))
        if chosen not in memory:
            bins = [l for (l, _, _), k in zip(bin_types, chosen) for _ in range(k)]
            memory[chosen] = fits(bins, items)
        contents = memory[chosen]
        if contents is not None:
            upper = waste
        else:
            lower = waste
        iterations.append({
            "waste": waste,
            "required": int(item_space * (1 + waste)),
            "chosen": chosen,
            "contents": contents,
            "lower": lower,
            "upper": upper})
    return iterations


################################################################################
# Sequential feasibility
################################################################################

def two_rows(items, length):
    """Feasibility solver of the figure: the items (of height 1) in two rows
    of the given length, by a depth-first search placing each item, by
    decreasing length, in the least loaded row first. Returns the rows, or
    None."""
    items = sorted(items, reverse=True)
    rows = [[], []]
    loads = [0, 0]

    def rec(i):
        if i == len(items):
            return True
        for r in sorted(range(2), key=lambda r: loads[r]):
            if loads[r] + items[i] <= length:
                loads[r] += items[i]
                rows[r].append(items[i])
                if rec(i + 1):
                    return True
                loads[r] -= items[i]
                rows[r].pop()
        return False

    return [list(row) for row in rows] if rec(0) else None


def sequential_feasibility_open_dimension(items, strip_height):
    """Open dimension x objective, items of height 1 and a strip of height
    2."""
    area = sum(items)
    # Twice the area of the items, divided by the height of the strip.
    length = -(-2 * area // strip_height)
    iterations = []
    while length > 0:
        rows = two_rows(items, length)
        iteration = {"length": length, "rows": rows}
        iterations.append(iteration)
        if rows is None:
            break
        x_max = max(sum(row) for row in rows)
        iteration["x_max"] = x_max
        length = min(int(0.99 * x_max), x_max - 1)
        iteration["next_length"] = length
    return iterations
