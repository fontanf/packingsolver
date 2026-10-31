"""Minimal reimplementation of the tree search with maximal spaces of
rectangle (src/rectangle/block.cpp, src/rectangle/tree_search_maximal_spaces.cpp),
used to compute the figures of its documentation.

Items can't be rotated, there are no defects, no maximum weight and no
resources.
"""

import math


class Block:

    def __init__(self, w, h, items, simple=None):
        self.w, self.h = w, h
        # Items: (item type, x, y) relative to the bottom-left corner.
        self.items = items
        # (item type, rotation) of a simple block, None otherwise.
        self.simple = simple

    def copies(self):
        result = {}
        for t, _, _ in self.items:
            result[t] = result.get(t, 0) + 1
        return result

    def key(self):
        return (self.w, self.h, tuple(sorted(self.copies().items())))


class Instance:

    def __init__(self, W, H, item_types):
        """item_types: {name: (w, h, copies, profit)}."""
        self.W, self.H = W, H
        self.item_types = item_types

    def size(self, t):
        return self.item_types[t][0], self.item_types[t][1]

    def item_area(self, block):
        return sum(self.size(t)[0] * self.size(t)[1] for t, _, _ in block.items)

    def profit(self, block):
        return sum(self.item_types[t][3] for t, _, _ in block.items)

    def fill_rate(self, block):
        return self.item_area(block) / (block.w * block.h)

    def efficiency(self, block):
        return self.profit(block) / (block.w * block.h)


def simple_blocks(instance):
    blocks = []
    for t, (w, h, copies, _) in instance.item_types.items():
        for cx in range(1, min(copies, instance.W // w) + 1):
            for cy in range(1, min(copies // cx, instance.H // h) + 1):
                items = [(t, i * w, j * h) for i in range(cx) for j in range(cy)]
                blocks.append(Block(cx * w, cy * h, items, simple=t))
    return blocks


def compute_blocks(instance, log=None):
    """Blocks generated as in 'compute_blocks_for_bin'. If 'log' is a list,
    the blocks dropped because of their waste are appended to it."""
    to_process = simple_blocks(instance)
    seen = {b.key() for b in to_process}
    returned = []

    def order(b):
        # Single items first, then by decreasing efficiency, then by
        # increasing number of items.
        return (1 if len(b.items) == 1 else 0, instance.efficiency(b), -len(b.items))

    while to_process:
        # Among equal blocks, the multiset of the C++ code returns the last
        # one inserted.
        block = max(reversed(to_process), key=order)
        to_process.remove(block)
        if len(instance.item_types) <= 3 and instance.fill_rate(block) != 1:
            if log is not None:
                log.append(block)
            continue
        for existing in returned:
            if block.simple is not None and block.simple == existing.simple:
                continue
            for direction in ("x", "y"):
                if direction == "x":
                    w, h = block.w + existing.w, max(block.h, existing.h)
                    shifted = [(t, x + block.w, y) for t, x, y in existing.items]
                else:
                    w, h = max(block.w, existing.w), block.h + existing.h
                    shifted = [(t, x, y + block.h) for t, x, y in existing.items]
                if w > instance.W or h > instance.H:
                    continue
                combined = Block(w, h, block.items + shifted)
                if any(c > instance.item_types[t][2] for t, c in combined.copies().items()):
                    continue
                if combined.key() in seen:
                    continue
                seen.add(combined.key())
                to_process.append(combined)
        returned.append(block)
    return returned


################################################################################
# Branching scheme.
################################################################################

class Space:

    def __init__(self, x, y, w, h):
        self.x, self.y, self.w, self.h = x, y, w, h

    def contains(self, o):
        return (self.x <= o.x and self.y <= o.y
                and self.x + self.w >= o.x + o.w and self.y + self.h >= o.y + o.h)

    def __repr__(self):
        return "Space({}, {}, {}, {})".format(self.x, self.y, self.w, self.h)


def overlaps(space, x, y, w, h):
    return x < space.x + space.w and x + w > space.x and y < space.y + space.h and y + h > space.y


def add_space(spaces, new):
    if new.w == 0 or new.h == 0:
        return
    if any(s.contains(new) for s in spaces):
        return
    spaces.append(new)


def cut_spaces(spaces, x, y, w, h):
    """4-cut of the spaces overlapping the rectangle (x, y, w, h)."""
    i = 0
    while i < len(spaces):
        s = spaces[i]
        if not overlaps(s, x, y, w, h):
            i += 1
            continue
        spaces[i] = spaces[-1]
        spaces.pop()
        if x > s.x:
            add_space(spaces, Space(s.x, s.y, x - s.x, s.h))
        if x + w < s.x + s.w:
            add_space(spaces, Space(x + w, s.y, s.x + s.w - (x + w), s.h))
        if y > s.y:
            add_space(spaces, Space(s.x, s.y, s.w, y - s.y))
        if y + h < s.y + s.h:
            add_space(spaces, Space(s.x, y + h, s.w, s.y + s.h - (y + h)))


def anchor(space, W, H):
    """(distance, dir_x, dir_y): distance of the space to the closest corner
    of the bin, and whether this corner is on the right / at the top."""
    dxs, dxe = space.x, W - (space.x + space.w)
    dys, dye = space.y, H - (space.y + space.h)
    return min(dxs, dxe) + min(dys, dye), dxe < dxs, dye < dys


def reachable(instance, remaining, length, axis):
    """Largest length <= 'length' reachable by stacking remaining items
    along 'axis' (0: x, 1: y)."""
    sums = {0}
    for t, n in remaining.items():
        size = instance.size(t)[axis]
        for _ in range(n):
            sums |= {v + size for v in sums if v + size <= length}
    return max(v for v in sums if v <= length)


PARAMETERS = [
    # alpha, beta, gamma, delta
    (3.7015, 2.6631, 0.3968, 0.0139),
    (2.0934, 1.4609, 0.4732, 0.0366),
]
SWITCH = 0.6740


class Node:

    def __init__(self, instance):
        self.instance = instance
        self.placed = []  # (block, x, y)
        self.spaces = [Space(0, 0, instance.W, instance.H)]
        self.copies = {t: 0 for t in instance.item_types}
        self.profit = 0
        self.item_area = 0

    def copy(self):
        node = Node(self.instance)
        node.placed = list(self.placed)
        node.spaces = [Space(s.x, s.y, s.w, s.h) for s in self.spaces]
        node.copies = dict(self.copies)
        node.profit = self.profit
        node.item_area = self.item_area
        return node

    def remaining(self):
        return {t: self.instance.item_types[t][2] - self.copies[t] for t in self.copies}

    def valid(self, block):
        return all(self.copies[t] + c <= self.instance.item_types[t][2]
                   for t, c in block.copies().items())

    def best_space(self, blocks):
        """Space selected by 'find_best_space', after the removal of the
        spaces in which no valid block fits."""
        best, best_key = None, None
        for s in self.spaces:
            d, dir_x, dir_y = anchor(s, self.instance.W, self.instance.H)
            corner = (2 if dir_x else 0) | (1 if dir_y else 0)
            key = (d, -s.w * s.h, -corner)
            if best_key is None or key < best_key:
                best, best_key = s, key
        return best

    def remove_unusable_spaces(self, blocks):
        self.spaces = [
                s for s in self.spaces
                if any(self.valid(b) and b.w <= s.w and b.h <= s.h for b in blocks)]

    def position(self, space, block):
        _, dir_x, dir_y = anchor(space, self.instance.W, self.instance.H)
        x = space.x + space.w - block.w if dir_x else space.x
        y = space.y + space.h - block.h if dir_y else space.y
        return x, y

    def contact_segments(self, space, block, x, y, delta):
        """Parts of the sides of the block placed at (x, y) in 'space' which
        are in contact with the bin or with the blocks already placed, as
        segments ((x1, y1), (x2, y2))."""
        W, H = self.instance.W, self.instance.H
        tol_x, tol_y = int(delta * space.w), int(delta * space.h)
        btol_x, btol_y = int(delta * block.w), int(delta * block.h)
        xl, xh, yl, yh = space.x, space.x + space.w, space.y, space.y + space.h
        bxl, bxh, byl, byh = x, x + block.w, y, y + block.h
        segments = []

        def vertical(px, a1, a2):
            lo, hi = max(a1, byl), min(a2, byh)
            if lo < hi:
                segments.append(((px, lo), (px, hi)))

        def horizontal(py, a1, a2):
            lo, hi = max(a1, bxl), min(a2, bxh)
            if lo < hi:
                segments.append(((lo, py), (hi, py)))

        if xl <= tol_x and abs(bxl - xl) <= btol_x:
            vertical(bxl, byl, byh)
        if W - xh <= tol_x and abs(bxh - xh) <= btol_x:
            vertical(bxh, byl, byh)
        if yl <= tol_y and abs(byl - yl) <= btol_y:
            horizontal(byl, bxl, bxh)
        if H - yh <= tol_y and abs(byh - yh) <= btol_y:
            horizontal(byh, bxl, bxh)
        for pb, px, py in self.placed:
            pxl, pxh, pyl, pyh = px, px + pb.w, py, py + pb.h
            if abs(pxh - xl) <= tol_x and abs(bxl - pxh) <= btol_x:
                vertical(bxl, max(pyl, yl), min(pyh, yh))
            if abs(pxl - xh) <= tol_x and abs(bxh - pxl) <= btol_x:
                vertical(bxh, max(pyl, yl), min(pyh, yh))
            if abs(pyh - yl) <= tol_y and abs(byl - pyh) <= btol_y:
                horizontal(byl, max(pxl, xl), min(pxh, xh))
            if abs(pyl - yh) <= tol_y and abs(byh - pyl) <= btol_y:
                horizontal(byh, max(pxl, xl), min(pxh, xh))
        return segments

    def contact(self, space, block, x, y, delta):
        """Relative contact C of the block placed at (x, y) in 'space'."""
        length = sum(abs(x2 - x1) + abs(y2 - y1)
                     for (x1, y1), (x2, y2) in self.contact_segments(space, block, x, y, delta))
        return length / (2 * (block.w + block.h))

    def area_loss(self, space, block):
        """Area-loss factor L."""
        remaining = self.remaining()
        rx = reachable(self.instance, remaining, space.w - block.w, 0)
        ry = reachable(self.instance, remaining, space.h - block.h, 1)
        return (block.w + rx) * (block.h + ry) / (space.w * space.h)

    def fitness(self, space, block):
        """(fitness, x, y, V, C, L, N)."""
        fill_rate = self.item_area / (self.instance.W * self.instance.H)
        alpha, beta, gamma, delta = PARAMETERS[0 if fill_rate < SWITCH else 1]
        x, y = self.position(space, block)
        v = self.instance.profit(block)
        c = self.contact(space, block, x, y, delta)
        l = self.area_loss(space, block)
        n = len(block.items)
        return v * c ** alpha * l ** beta * n ** (-gamma), x, y, v, c, l, n

    def insertions(self, blocks):
        """Insertions sorted by decreasing fitness: (fitness, block, x, y, ...)."""
        if not self.spaces:
            return [], None
        space = self.best_space(blocks)
        result = []
        for b in blocks:
            if not self.valid(b) or b.w > space.w or b.h > space.h:
                continue
            f = self.fitness(space, b)
            result.append((f[0], b) + f[1:])
        result.sort(key=lambda r: -r[0])
        return result, space

    def insert(self, blocks, block, x, y):
        node = self.copy()
        node.placed.append((block, x, y))
        for t, c in block.copies().items():
            node.copies[t] += c
        node.profit += self.instance.profit(block)
        node.item_area += self.instance.item_area(block)
        cut_spaces(node.spaces, x, y, block.w, block.h)
        node.remove_unusable_spaces(blocks)
        return node

    def greedy(self, blocks):
        """Greedy completion: the best insertion at each step."""
        node = self
        while True:
            insertions, _ = node.insertions(blocks)
            if not insertions:
                return node
            _, b, x, y = insertions[0][:4]
            node = node.insert(blocks, b, x, y)


if __name__ == "__main__":
    instance = Instance(10, 6, {"A": (4, 3, 2, 12), "B": (2, 2, 4, 4), "C": (3, 1, 3, 3)})
    dropped = []
    blocks = compute_blocks(instance, dropped)
    print(len(blocks), "blocks;", len(dropped), "dropped")
    for b in blocks:
        print(b.w, b.h, b.copies(), b.simple)
    root = Node(instance)
    insertions, space = root.insertions(blocks)
    print("root space", space)
    for r in insertions[:8]:
        print("f={:.2f} block {}x{} {} at ({}, {}) V={} C={:.2f} L={:.2f} N={}".format(
            r[0], r[1].w, r[1].h, r[1].copies(), *r[2:]))
    g = root.greedy(blocks)
    print("greedy", g.profit, [(b.copies(), x, y) for b, x, y in g.placed])
