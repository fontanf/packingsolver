# LP model: `linear_programming_minimize_shrinkage`

## Overview

`linear_programming_minimize_shrinkage` takes a single-bin solution whose items
may overlap or lie outside the bin and tries to grow each item as large as
possible while keeping them mutually non-overlapping and inside the bin.  It is
used as the inner LP solver of the local-search algorithm.

Items are **scaled**: item $i$ is represented at scale $\lambda_i \in [0,1]$,
meaning its shape is $\lambda_i \cdot S_i$ centered at position $(X_i, Y_i)$
(scaled world coordinates, see §Coordinates).  At $\lambda_i = 0$ the item is a
point; at $\lambda_i = 1$ it has its original full size.

The outer loop re-linearises the non-overlap constraints and iterates until no
further improvement is possible.

---

## Coordinates

All item shapes are stored pre-multiplied by a scale factor $s$ (`scale_value`)
chosen so that coordinates are integers:
$$
S_i^{\text{scaled}} = s \cdot S_i^{\text{orig}}.
$$
The LP works entirely in scaled coordinates.  The LP variable for item $i$'s
position is
$$
X_i = s \cdot \text{bl\_corner}_i^x, \qquad Y_i = s \cdot \text{bl\_corner}_i^y.
$$
After solving, the stored `bl_corner` is recovered by dividing by $s$.

---

## Decision variables

For each item $i \in \{1,\dots,n\}$:

| Variable | Domain | Meaning |
|---|---|---|
| $X_i$ | $[\underline{X}_i,\, \overline{X}_i]$ | $x$-coordinate of bl\_corner (scaled) |
| $Y_i$ | $[\underline{Y}_i,\, \overline{Y}_i]$ | $y$-coordinate of bl\_corner (scaled) |
| $\lambda_i$ | $[0, 1]$ | scale factor of item $i$ |

**Variable bounds for $X_i$, $Y_i$.**  Let $[\underline{x}_i, \overline{x}_i]$
(resp. $y$) be the extent of the rotated full-size item in the $x$ (resp. $y$)
direction.  The bl\_corner bounds that keep the full-size item inside the bin
AABB $[B^x_{\min}, B^x_{\max}] \times [B^y_{\min}, B^y_{\max}]$ are:
$$
\underline{X}_i = s B^x_{\min} - \underline{x}_i, \qquad
\overline{X}_i  = s B^x_{\max} - \overline{x}_i,
$$
and analogously for $Y_i$.  If the item is larger than the bin in a given
dimension ($\underline{X}_i > \overline{X}_i$), the raw bin-AABB bounds are
used instead (the item will be forced to $\lambda_i < 1$).

A movement box of half-size $\delta = \|B\|_2 \cdot s$ (bin diagonal in scaled
units) is applied around the current position, so the effective bounds are:
$$
\underline{X}_i^{\text{eff}} = \max\!\bigl(\underline{X}_i,\; X_i^{\text{cur}} - \delta\bigr),
\qquad
\overline{X}_i^{\text{eff}}  = \min\!\bigl(\overline{X}_i,\;  X_i^{\text{cur}} + \delta\bigr),
$$
and analogously for $Y_i$.  Because $\delta$ equals the bin diagonal, these
reduce to $[\underline{X}_i, \overline{X}_i]$ in practice.

---

## Objective

$$
\max \sum_{i=1}^{n} w_i \lambda_i
$$

where $w_i > 0$ is a per-item penalty weight (`item_penalties[i]`, defaulting to
$1$).  Weights are increased by the outer local-search loop whenever item $i$
remains shrunken ($\lambda_i < 1$) after an LP call, so frequently-problematic
items are prioritised.

---

## Constraints

Non-overlap is enforced via **linearised supporting-hyperplane constraints**.
At each outer iteration the separating direction $(a, b)$ for each conflicting
pair is fixed from the current positions (using the shrunken shapes
$\lambda_i^{\text{cur}} \cdot S_i$), and the resulting linear constraints are
added to the LP.  $H_i$ and $L_j$ below always use the **full-size** shapes
(since $\lambda$ is an LP variable).

### Support function

For a convex polygon $P$ with vertices $\{v_k\}$:
$$
h(P,\, \mathbf{d}) = \max_k\; \mathbf{d}^\top v_k.
$$

### Separating direction

Given the current positions $X_i^{\text{cur}}$, $Y_i^{\text{cur}}$ and the
current shrunken shapes $\lambda_i^{\text{cur}} \cdot S_i$, the direction
$(a, b)$ for a pair of shapes is the outward normal of the edge (from either
shape) that maximises the current signed separation:
$$
(a, b) = \operatorname*{arg\,max}_{\text{edges}} \Bigl[
  a\,(X_j^{\text{cur}} - X_i^{\text{cur}}) + b\,(Y_j^{\text{cur}} - Y_i^{\text{cur}})
  - h(\lambda_i^{\text{cur}} S_i,\; (a,b))
  - h(\lambda_j^{\text{cur}} S_j,\; (-a,-b))
\Bigr].
$$
For a border/defect obstacle $O$ (fixed at world origin) and item $i$, the
shrunken item shape is used as shape_to and the direction $(a, b)$ maximises
the separation of the item center from the obstacle.

### Item–border and item–defect constraints

Let $O$ be a convex part of a bin border or defect (fixed).  Define:
$$
H_O = h(O,\; (a,b)), \qquad L_i = h(S_i,\; (-a,-b)).
$$
Constraint:
$$
a X_i + b Y_i - L_i\, \lambda_i \;\ge\; H_O.
$$
At $\lambda_i = 1$ this reduces to $a X_i + b Y_i \ge H_O + L_i$, which is
exactly the Minkowski-sum separation condition for the full-size item and the
obstacle.  At $\lambda_i = 0$ the item is a point and the constraint requires
its center to lie outside the obstacle.

### Item–item constraints

For a pair of items $i, j$ with convex parts $S_i$, $S_j$, define:
$$
H_i = h(S_i,\; (a,b)), \qquad L_j = h(S_j,\; (-a,-b)).
$$
Constraint:
$$
a\,(X_j - X_i) + b\,(Y_j - Y_i) - H_i\,\lambda_i - L_j\,\lambda_j \;\ge\; 0.
$$
At $\lambda_i = \lambda_j = 1$ this is the standard edge-based separation
condition for the two full-size items.  At $\lambda_i = \lambda_j = 0$ both
items are points and the constraint becomes $a(X_j - X_i) + b(Y_j - Y_i) \ge
0$; because both scaled shapes are degenerate, the separating direction
$(a, b)$ reduces to $(0, 0)$ and the constraint is dropped entirely, so
zero-size items are unconstrained relative to each other.

---

## Feasibility at $\lambda = 0$

When all $\lambda_i = 0$ items are zero-sized.  The only active constraints are
the border/defect constraints, which require each item's center to lie in the
bin interior (outside every border/defect obstacle).  Since items can be placed
at any position in the bin interior simultaneously, the LP is always feasible at
$\lambda = 0$.

---

## Outer iteration loop

```
current_value ← 0
current_lambda ← [0, …, 0]

repeat:
    for each item i:
        compute rotated convex decomposition of λᵢ·Sᵢ  (shrunken)
    for each pair (item, obstacle) and (item, item):
        find best separating direction (a, b) using shrunken shapes
        compute constraint coefficients H, L from full-size shapes
    build and solve LP
    if LP objective ≤ current_value: stop
    current_value   ← LP objective
    current_lambda  ← LP solution λ values
    update item positions from LP solution
```

**Convergence.**  The objective is non-decreasing.  It is bounded above by
$\sum_i w_i$.  The loop terminates when no improving move is found.

**Output.**  The function returns:
- `solution`: item positions at the last improving LP solution.
- `final_lambda`: $\lambda_i$ values at that solution.
- `items_shrunken`: whether $\lambda_i < 1 - 10^{-6}$ for each item.
- `feasible`: `true` iff all $\lambda_i = 1$ (all items at full size, no overlaps).
