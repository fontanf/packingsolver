#include "rectangle/tree_search_packing_classes.hpp"

#include "packingsolver/rectangle/algorithm_formatter.hpp"
#include "rectangle/solution_builder.hpp"

#include <algorithm>
#include <array>
#include <numeric>

using namespace packingsolver;
using namespace packingsolver::rectangle;

namespace
{

/** State of a pair of items along an axis. */
enum PairState: char
{
    Unknown = 0,
    Overlap = 1,
    Disjoint = 2,
};

/** Copy of an item type. */
struct Unit
{
    ItemTypeId item_type_id;

    /** Sizes along each axis, not rotated and rotated. */
    std::array<std::array<Length, 2>, 2> sizes;

    /** Allowed rotations. */
    bool not_rotated_allowed;
    bool rotated_allowed;

    Area area;
};

struct Node
{
    /** Rotation of each unit: -1 if not fixed, 0 or 1. */
    std::vector<int8_t> rotations;

    /** State of each pair of units, along each axis, of size n * n. */
    std::array<std::vector<char>, 2> states;
};

struct TreeSearchPackingClassesData
{
    /** Copies of the item types. */
    std::vector<Unit> units;

    /** Number of units. */
    ItemPos number_of_units = 0;

    /** Size of the bin along each axis. */
    std::array<Length, 2> bin_sizes;

    /*
     * Buffers.
     */

    std::vector<ItemPos> candidates;
    std::vector<ItemPos> clique;
};

/** Smallest size of a unit along an axis, over its remaining rotations. */
Length size(
        const TreeSearchPackingClassesData& data,
        const Node& node,
        ItemPos unit_pos,
        int axis)
{
    const Unit& unit = data.units[unit_pos];
    int8_t rotation = node.rotations[unit_pos];
    if (rotation != -1)
        return unit.sizes[axis][rotation];
    return (std::min)(unit.sizes[axis][0], unit.sizes[axis][1]);
}

char state(
        const TreeSearchPackingClassesData& data,
        const Node& node,
        int axis,
        ItemPos unit_pos_1,
        ItemPos unit_pos_2)
{
    return node.states[axis][unit_pos_1 * data.number_of_units + unit_pos_2];
}

/**
 * Set the state of a pair along an axis; return 'false' if it contradicts its
 * current state.
 */
bool set_state(
        const TreeSearchPackingClassesData& data,
        Node& node,
        int axis,
        ItemPos unit_pos_1,
        ItemPos unit_pos_2,
        char value,
        bool& changed)
{
    char& current = node.states[axis][unit_pos_1 * data.number_of_units + unit_pos_2];
    if (current == value)
        return true;
    if (current != Unknown)
        return false;
    current = value;
    node.states[axis][unit_pos_2 * data.number_of_units + unit_pos_1] = value;
    changed = true;
    return true;
}

/**
 * Return 'true' iff two units are indistinguishable: they have the same sizes,
 * and the same states with the other units.
 */
bool indistinguishable(
        const TreeSearchPackingClassesData& data,
        const Node& node,
        ItemPos unit_pos_1,
        ItemPos unit_pos_2)
{
    for (int axis = 0; axis < 2; ++axis)
        if (size(data, node, unit_pos_1, axis) != size(data, node, unit_pos_2, axis))
            return false;
    for (int axis = 0; axis < 2; ++axis) {
        for (ItemPos unit_pos = 0; unit_pos < data.number_of_units; ++unit_pos) {
            if (unit_pos == unit_pos_1 || unit_pos == unit_pos_2)
                continue;
            if (state(data, node, axis, unit_pos_1, unit_pos)
                    != state(data, node, axis, unit_pos_2, unit_pos)) {
                return false;
            }
        }
    }
    return true;
}

/*
 * Propagation
 */

/**
 * Weight of a clique of units pairwise disjoint along an axis, containing the
 * two units of a pair, built greedily from their common disjoint neighbors,
 * the largest ones first.
 */
Length clique_weight(
        TreeSearchPackingClassesData& data,
        const Node& node,
        int axis,
        ItemPos unit_pos_1,
        ItemPos unit_pos_2)
{
    data.candidates.clear();
    for (ItemPos unit_pos = 0; unit_pos < data.number_of_units; ++unit_pos) {
        if (unit_pos == unit_pos_1 || unit_pos == unit_pos_2)
            continue;
        if (state(data, node, axis, unit_pos_1, unit_pos) == Disjoint
                && state(data, node, axis, unit_pos_2, unit_pos) == Disjoint) {
            data.candidates.push_back(unit_pos);
        }
    }
    std::sort(
            data.candidates.begin(),
            data.candidates.end(),
            [&data, &node, axis](ItemPos unit_pos_a, ItemPos unit_pos_b)
            {
                return size(data, node, unit_pos_a, axis) > size(data, node, unit_pos_b, axis);
            });
    Length weight = size(data, node, unit_pos_1, axis) + size(data, node, unit_pos_2, axis);
    data.clique.clear();
    for (ItemPos unit_pos: data.candidates) {
        bool compatible = true;
        for (ItemPos clique_unit_pos: data.clique) {
            if (state(data, node, axis, unit_pos, clique_unit_pos) != Disjoint) {
                compatible = false;
                break;
            }
        }
        if (!compatible)
            continue;
        data.clique.push_back(unit_pos);
        weight += size(data, node, unit_pos, axis);
    }
    return weight;
}

/** Propagate; return 'false' if the node is infeasible. */
bool propagate(
        TreeSearchPackingClassesData& data,
        Node& node)
{
    for (;;) {
        bool changed = false;
        for (ItemPos unit_pos_1 = 0; unit_pos_1 < data.number_of_units; ++unit_pos_1) {
            for (ItemPos unit_pos_2 = unit_pos_1 + 1; unit_pos_2 < data.number_of_units; ++unit_pos_2) {
                // Two units which don't fit side by side along an axis
                // overlap along it.
                for (int axis = 0; axis < 2; ++axis) {
                    if (size(data, node, unit_pos_1, axis) + size(data, node, unit_pos_2, axis) > data.bin_sizes[axis]
                            && !set_state(data, node, axis, unit_pos_1, unit_pos_2, Overlap, changed)) {
                        return false;
                    }
                }
                // Two units are disjoint along at least one axis.
                char state_x = state(data, node, 0, unit_pos_1, unit_pos_2);
                char state_y = state(data, node, 1, unit_pos_1, unit_pos_2);
                if (state_x == Overlap && state_y == Overlap)
                    return false;
                if (state_x == Overlap
                        && !set_state(data, node, 1, unit_pos_1, unit_pos_2, Disjoint, changed)) {
                    return false;
                }
                if (state_y == Overlap
                        && !set_state(data, node, 0, unit_pos_1, unit_pos_2, Disjoint, changed)) {
                    return false;
                }
            }
        }
        // The units pairwise disjoint along an axis fit in the length of the
        // bin; a pair which would complete a clique which doesn't fit
        // overlaps.
        for (int axis = 0; axis < 2; ++axis) {
            for (ItemPos unit_pos_1 = 0; unit_pos_1 < data.number_of_units; ++unit_pos_1) {
                for (ItemPos unit_pos_2 = unit_pos_1 + 1; unit_pos_2 < data.number_of_units; ++unit_pos_2) {
                    char pair_state = state(data, node, axis, unit_pos_1, unit_pos_2);
                    if (pair_state == Overlap)
                        continue;
                    if (clique_weight(data, node, axis, unit_pos_1, unit_pos_2) <= data.bin_sizes[axis])
                        continue;
                    if (pair_state == Disjoint)
                        return false;
                    set_state(data, node, axis, unit_pos_1, unit_pos_2, Overlap, changed);
                }
            }
        }
        // The graph of the overlapping pairs of a packing class is an
        // interval graph, so it contains no induced cycle of length 4 (a, b,
        // c, d): if three sides overlap and both chords are disjoint, the
        // fourth side is disjoint; if the four sides overlap and a chord is
        // disjoint, the other chord overlaps.
        for (int axis = 0; axis < 2; ++axis) {
            const std::vector<char>& states = node.states[axis];
            for (ItemPos a = 0; a < data.number_of_units; ++a) {
                for (ItemPos c = a + 1; c < data.number_of_units; ++c) {
                    // Chord a-c.
                    char state_ac = states[a * data.number_of_units + c];
                    if (state_ac == Overlap)
                        continue;
                    for (ItemPos b = 0; b < data.number_of_units; ++b) {
                        if (b == a || b == c)
                            continue;
                        // Sides a-b and b-c.
                        if (states[a * data.number_of_units + b] != Overlap || states[b * data.number_of_units + c] != Overlap)
                            continue;
                        for (ItemPos d = b + 1; d < data.number_of_units; ++d) {
                            if (d == a || d == c)
                                continue;
                            char state_bd = states[b * data.number_of_units + d];
                            char state_cd = states[c * data.number_of_units + d];
                            char state_da = states[d * data.number_of_units + a];
                            if (state_bd == Overlap)
                                continue;
                            if (state_ac == Disjoint && state_bd == Disjoint) {
                                if (state_cd == Overlap && state_da == Overlap)
                                    return false;
                                if (state_cd == Overlap && state_da == Unknown)
                                    set_state(data, node, axis, d, a, Disjoint, changed);
                                if (state_da == Overlap && state_cd == Unknown)
                                    set_state(data, node, axis, c, d, Disjoint, changed);
                            }
                            if (state_cd == Overlap && state_da == Overlap) {
                                if (state_ac == Disjoint && state_bd == Unknown)
                                    set_state(data, node, axis, b, d, Overlap, changed);
                                if (state_bd == Disjoint && state_ac == Unknown) {
                                    set_state(data, node, axis, a, c, Overlap, changed);
                                    state_ac = Overlap;
                                    break;
                                }
                            }
                        }
                        if (state_ac == Overlap)
                            break;
                    }
                }
            }
        }
        if (!changed)
            return true;
    }
}

/*
 * Obstructions
 */

/**
 * Pairs of units which are not known to overlap along an axis are considered
 * disjoint along it. Look for an obstruction preventing these pairs from being
 * the disjoint pairs of a packing class along the axis:
 * - the graph of these pairs is not a comparability graph: an implication
 *   class of the graph contains an edge in both directions (Golumbic, 1980).
 *   The pairs used to derive it are written in 'obstruction';
 * - otherwise, the longest chain of a transitive orientation of the graph
 *   doesn't fit in the bin. Its pairs are written in 'obstruction'.
 *
 * Some pairs of an obstruction must overlap. Otherwise, the positions of the
 * units along the axis are written in 'positions'.
 *
 * Return 'true' iff an obstruction is found.
 */
bool find_obstruction(
        const TreeSearchPackingClassesData& data,
        const Node& node,
        int axis,
        std::vector<std::pair<ItemPos, ItemPos>>& obstruction,
        std::vector<Length>& positions)
{
    const std::vector<char>& states = node.states[axis];
    obstruction.clear();
    auto adjacent = [&states, &data](ItemPos a, ItemPos b)
    {
        return a != b && states[a * data.number_of_units + b] != Overlap;
    };

    // Implication classes. The edges p -> q and p -> w are in the same class
    // if q and w are not adjacent; the edges p -> q and w -> q too if p and w
    // are not adjacent. The graph is a comparability graph iff no class
    // contains an edge and its reverse.
    std::vector<Counter> classes(data.number_of_units * data.number_of_units, -1);
    // Edge from which each edge of a class was reached.
    std::vector<ItemPos> parents(data.number_of_units * data.number_of_units, -1);
    std::vector<ItemPos> queue;
    Counter class_id = 0;
    // Add the pairs of the derivation of an edge to the obstruction.
    auto add_derivation = [&](ItemPos edge)
    {
        while (edge != -1) {
            ItemPos p = edge / data.number_of_units;
            ItemPos q = edge % data.number_of_units;
            obstruction.push_back({(std::min)(p, q), (std::max)(p, q)});
            edge = parents[edge];
        }
    };
    for (ItemPos a = 0; a < data.number_of_units; ++a) {
        for (ItemPos b = a + 1; b < data.number_of_units; ++b) {
            if (!adjacent(a, b)
                    || classes[a * data.number_of_units + b] != -1
                    || classes[b * data.number_of_units + a] != -1) {
                continue;
            }
            queue.clear();
            classes[a * data.number_of_units + b] = class_id;
            queue.push_back(a * data.number_of_units + b);
            for (std::size_t queue_pos = 0; queue_pos < queue.size(); ++queue_pos) {
                ItemPos edge = queue[queue_pos];
                ItemPos p = edge / data.number_of_units;
                ItemPos q = edge % data.number_of_units;
                for (ItemPos w = 0; w < data.number_of_units; ++w) {
                    if (w == p || w == q)
                        continue;
                    // Edges p -> w and w -> q.
                    for (int side = 0; side < 2; ++side) {
                        ItemPos p_new = (side == 0)? p: w;
                        ItemPos q_new = (side == 0)? w: q;
                        bool related = (side == 0)?
                            adjacent(p, w) && !adjacent(q, w):
                            adjacent(w, q) && !adjacent(p, w);
                        if (!related)
                            continue;
                        if (classes[q_new * data.number_of_units + p_new] == class_id) {
                            add_derivation(edge);
                            add_derivation(q_new * data.number_of_units + p_new);
                            std::sort(obstruction.begin(), obstruction.end());
                            obstruction.erase(
                                    std::unique(obstruction.begin(), obstruction.end()),
                                    obstruction.end());
                            return true;
                        }
                        if (classes[p_new * data.number_of_units + q_new] != class_id) {
                            classes[p_new * data.number_of_units + q_new] = class_id;
                            parents[p_new * data.number_of_units + q_new] = edge;
                            queue.push_back(p_new * data.number_of_units + q_new);
                        }
                    }
                }
            }
            class_id++;
        }
    }

    // Transitive orientation (Golumbic, 1980, algorithm TRO): the
    // implication class of an edge in the graph of the edges not oriented
    // yet is oriented, and its edges are removed, until no edge is left.
    std::vector<char> remaining(data.number_of_units * data.number_of_units, 0);
    for (ItemPos a = 0; a < data.number_of_units; ++a)
        for (ItemPos b = 0; b < data.number_of_units; ++b)
            remaining[a * data.number_of_units + b] = adjacent(a, b);
    std::vector<char> directed(data.number_of_units * data.number_of_units, 0);
    std::fill(classes.begin(), classes.end(), -1);
    class_id = 0;
    for (ItemPos a = 0; a < data.number_of_units; ++a) {
        for (ItemPos b = a + 1; b < data.number_of_units; ++b) {
            if (!remaining[a * data.number_of_units + b])
                continue;
            queue.clear();
            classes[a * data.number_of_units + b] = class_id;
            queue.push_back(a * data.number_of_units + b);
            for (std::size_t queue_pos = 0; queue_pos < queue.size(); ++queue_pos) {
                ItemPos edge = queue[queue_pos];
                ItemPos p = edge / data.number_of_units;
                ItemPos q = edge % data.number_of_units;
                for (ItemPos w = 0; w < data.number_of_units; ++w) {
                    if (w == p || w == q)
                        continue;
                    for (int side = 0; side < 2; ++side) {
                        ItemPos p_new = (side == 0)? p: w;
                        ItemPos q_new = (side == 0)? w: q;
                        bool related = (side == 0)?
                            remaining[p * data.number_of_units + w] && !remaining[q * data.number_of_units + w]:
                            remaining[w * data.number_of_units + q] && !remaining[p * data.number_of_units + w];
                        if (!related || classes[p_new * data.number_of_units + q_new] == class_id)
                            continue;
                        if (classes[q_new * data.number_of_units + p_new] == class_id) {
                            throw std::logic_error(
                                    FUNC_SIGNATURE + ": "
                                    "the graph is not a comparability graph.");
                        }
                        classes[p_new * data.number_of_units + q_new] = class_id;
                        queue.push_back(p_new * data.number_of_units + q_new);
                    }
                }
            }
            for (ItemPos edge: queue) {
                ItemPos p = edge / data.number_of_units;
                ItemPos q = edge % data.number_of_units;
                directed[p * data.number_of_units + q] = 1;
                remaining[p * data.number_of_units + q] = 0;
                remaining[q * data.number_of_units + p] = 0;
            }
            class_id++;
        }
    }

    // Positions: longest paths in the orientation.
    std::vector<ItemPos> in_degrees(data.number_of_units, 0);
    for (ItemPos a = 0; a < data.number_of_units; ++a)
        for (ItemPos b = 0; b < data.number_of_units; ++b)
            if (directed[a * data.number_of_units + b])
                in_degrees[b]++;
    positions.assign(data.number_of_units, 0);
    std::vector<ItemPos> predecessors(data.number_of_units, -1);
    std::vector<ItemPos> order;
    for (ItemPos a = 0; a < data.number_of_units; ++a)
        if (in_degrees[a] == 0)
            order.push_back(a);
    for (std::size_t order_pos = 0; order_pos < order.size(); ++order_pos) {
        ItemPos a = order[order_pos];
        for (ItemPos b = 0; b < data.number_of_units; ++b) {
            if (!directed[a * data.number_of_units + b])
                continue;
            if (positions[a] + size(data, node, a, axis) > positions[b]) {
                positions[b] = positions[a] + size(data, node, a, axis);
                predecessors[b] = a;
            }
            if (--in_degrees[b] == 0)
                order.push_back(b);
        }
    }
    if ((ItemPos)order.size() != data.number_of_units) {
        throw std::logic_error(
                FUNC_SIGNATURE + ": the orientation is not acyclic.");
    }

    // Longest chain.
    ItemPos last = -1;
    for (ItemPos a = 0; a < data.number_of_units; ++a)
        if (positions[a] + size(data, node, a, axis) > data.bin_sizes[axis])
            if (last == -1 || positions[a] + size(data, node, a, axis) > positions[last] + size(data, node, last, axis))
                last = a;
    if (last == -1)
        return false;
    // The orientation is transitive, so the units of the chain are pairwise
    // adjacent.
    std::vector<ItemPos> chain;
    for (ItemPos a = last; a != -1; a = predecessors[a])
        chain.push_back(a);
    for (std::size_t pos_1 = 0; pos_1 < chain.size(); ++pos_1) {
        for (std::size_t pos_2 = pos_1 + 1; pos_2 < chain.size(); ++pos_2) {
            ItemPos a = chain[pos_1];
            ItemPos b = chain[pos_2];
            obstruction.push_back({(std::min)(a, b), (std::max)(a, b)});
        }
    }
    return true;
}

/**
 * Process a node: propagate, fix the pairs of the obstructions with a single
 * unknown pair, and write its children in 'children'; none if it is
 * infeasible or a solution.
 *
 * Return 'true' iff the node is a solution; the positions of the units are
 * then written in 'positions'.
 */
bool branch(
        TreeSearchPackingClassesData& data,
        Node& node,
        std::vector<Node>& children,
        std::array<std::vector<Length>, 2>& positions)
{
    children.clear();
    std::vector<std::pair<ItemPos, ItemPos>> obstruction;
    for (;;) {
        if (!propagate(data, node))
            return false;

        // Rotations, the largest units first.
        ItemPos rotation_unit_pos = -1;
        for (ItemPos unit_pos = 0; unit_pos < data.number_of_units; ++unit_pos) {
            if (node.rotations[unit_pos] != -1)
                continue;
            if (rotation_unit_pos == -1
                    || data.units[unit_pos].area > data.units[rotation_unit_pos].area) {
                rotation_unit_pos = unit_pos;
            }
        }
        if (rotation_unit_pos != -1) {
            for (int8_t rotation = 0; rotation < 2; ++rotation) {
                Node child = node;
                child.rotations[rotation_unit_pos] = rotation;
                children.push_back(child);
            }
            return false;
        }

        // Obstructions, along the x-axis first.
        int branching_axis = -1;
        ItemPos best_unit_pos_1 = -1;
        ItemPos best_unit_pos_2 = -1;
        Counter best_number_of_unknown_pairs = 0;
        for (int axis = 0; axis < 2; ++axis) {
            if (!find_obstruction(data, node, axis, obstruction, positions[axis]))
                continue;
            // Some unknown pair of the obstruction overlaps. The one taking
            // the largest fraction of the bin along the other axis, where it
            // is then disjoint, is chosen: it is the most constrained one.
            ItemPos unit_pos_1 = -1;
            ItemPos unit_pos_2 = -1;
            Counter number_of_unknown_pairs = 0;
            double best_ratio = 0;
            for (const auto& pair: obstruction) {
                if (state(data, node, axis, pair.first, pair.second) != Unknown)
                    continue;
                number_of_unknown_pairs++;
                double ratio = (double)(size(data, node, pair.first, 1 - axis)
                        + size(data, node, pair.second, 1 - axis)) / data.bin_sizes[1 - axis];
                if (unit_pos_1 == -1 || ratio > best_ratio) {
                    unit_pos_1 = pair.first;
                    unit_pos_2 = pair.second;
                    best_ratio = ratio;
                }
            }
            if (number_of_unknown_pairs == 0)
                return false;
            branching_axis = axis;
            best_unit_pos_1 = unit_pos_1;
            best_unit_pos_2 = unit_pos_2;
            best_number_of_unknown_pairs = number_of_unknown_pairs;
            break;
        }
        if (branching_axis != -1) {
            int axis = branching_axis;
            if (best_number_of_unknown_pairs == 1) {
                bool changed = false;
                set_state(data, node, axis, best_unit_pos_1, best_unit_pos_2, Overlap, changed);
                continue;
            }
            Node child_overlap = node;
            bool changed = false;
            set_state(data, child_overlap, axis, best_unit_pos_1, best_unit_pos_2, Overlap, changed);
            children.push_back(child_overlap);
            // In the child where the pair is disjoint, the pairs which are
            // indistinguishable from it are disjoint too: a packing class
            // where one of them overlaps is isomorphic to one where the pair
            // overlaps, in the other child (Fekete, Schepers and van der
            // Veen, 2007, Lemma 8).
            Node child_disjoint = node;
            set_state(data, child_disjoint, axis, best_unit_pos_1, best_unit_pos_2, Disjoint, changed);
            for (ItemPos unit_pos_1 = 0; unit_pos_1 < data.number_of_units; ++unit_pos_1) {
                if (unit_pos_1 != best_unit_pos_1
                        && !indistinguishable(data, node, best_unit_pos_1, unit_pos_1)) {
                    continue;
                }
                for (ItemPos unit_pos_2 = 0; unit_pos_2 < data.number_of_units; ++unit_pos_2) {
                    if (unit_pos_2 == unit_pos_1
                            || unit_pos_1 == best_unit_pos_2
                            || unit_pos_2 == best_unit_pos_1
                            || (unit_pos_1 == best_unit_pos_1 && unit_pos_2 == best_unit_pos_2)) {
                        continue;
                    }
                    if (unit_pos_2 != best_unit_pos_2
                            && !indistinguishable(data, node, best_unit_pos_2, unit_pos_2)) {
                        continue;
                    }
                    if (state(data, node, axis, unit_pos_1, unit_pos_2) == Unknown)
                        set_state(data, child_disjoint, axis, unit_pos_1, unit_pos_2, Disjoint, changed);
                }
            }
            children.push_back(child_disjoint);
            return false;
        }

        // Solution.
        return true;
    }
}

}

TreeSearchPackingClassesOutput packingsolver::rectangle::tree_search_packing_classes(
        const Instance& instance,
        const TreeSearchPackingClassesParameters& parameters)
{
    TreeSearchPackingClassesOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    if (instance.objective() != Objective::Feasibility) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": unsupported objective.");
    }
    if (instance.number_of_bin_types() != 1
            || instance.bin_type(0).copies != 1) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": the instance must have a single bin.");
    }
    if (!instance.bin_type(0).defects.empty()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": defects are not supported.");
    }
    if (!instance.bin_type(0).fixed_items.empty()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": fixed items are not supported.");
    }
    if (instance.parameters().unloading_constraint != UnloadingConstraint::None) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": unloading constraints are not supported.");
    }
    if (instance.weight_matters()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": weight constraints are not supported.");
    }
    if (instance.resources_matter()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": resources are not supported.");
    }

    TreeSearchPackingClassesData data;
    const BinType& bin_type = instance.bin_type(0);
    data.bin_sizes = {bin_type.rect.x, bin_type.rect.y};

    // Units.
    Area total_area = 0;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        Unit unit;
        unit.item_type_id = item_type_id;
        unit.sizes[0] = {item_type.x(false), item_type.x(true)};
        unit.sizes[1] = {item_type.y(false), item_type.y(true)};
        unit.not_rotated_allowed = (unit.sizes[0][0] <= data.bin_sizes[0]
                && unit.sizes[1][0] <= data.bin_sizes[1]);
        unit.rotated_allowed = (!item_type.oriented
                && item_type.rect.x != item_type.rect.y
                && unit.sizes[0][1] <= data.bin_sizes[0]
                && unit.sizes[1][1] <= data.bin_sizes[1]);
        unit.area = item_type.area();
        for (ItemPos copy = 0; copy < item_type.copies; ++copy) {
            data.units.push_back(unit);
            total_area += unit.area;
        }
    }
    data.number_of_units = data.units.size();
    ItemPos n = data.number_of_units;

    Node root;
    root.rotations.assign(n, -1);
    for (ItemPos unit_pos = 0; unit_pos < n; ++unit_pos) {
        const Unit& unit = data.units[unit_pos];
        // All the items must be packed, so an item which doesn't fit in the
        // bin, or isn't eligible for it, makes the instance infeasible.
        if (!instance.item_type_fits_bin_type(unit.item_type_id, 0)
                || (!unit.not_rotated_allowed && !unit.rotated_allowed)) {
            algorithm_formatter.update_is_proven_infeasible();
            algorithm_formatter.end();
            return output;
        }
        if (!unit.rotated_allowed)
            root.rotations[unit_pos] = 0;
        else if (!unit.not_rotated_allowed)
            root.rotations[unit_pos] = 1;
    }
    if (total_area > data.bin_sizes[0] * data.bin_sizes[1]) {
        algorithm_formatter.update_is_proven_infeasible();
        algorithm_formatter.end();
        return output;
    }
    root.states[0].assign(n * n, Unknown);
    root.states[1].assign(n * n, Unknown);

    // Depth-first search.
    std::vector<Node> stack = {root};
    std::vector<Node> children;
    std::array<std::vector<Length>, 2> positions;
    bool found = false;
    Node solution_node;
    while (!stack.empty()) {
        if (parameters.timer.needs_to_end()) {
            algorithm_formatter.end();
            return output;
        }
        Node node = stack.back();
        stack.pop_back();
        output.number_of_nodes++;
        if (branch(data, node, children, positions)) {
            found = true;
            solution_node = node;
            break;
        }
        for (auto it = children.rbegin(); it != children.rend(); ++it)
            stack.push_back(*it);
    }

    if (!found) {
        algorithm_formatter.update_is_proven_infeasible();
        algorithm_formatter.end();
        return output;
    }
    SolutionBuilder solution_builder(instance);
    solution_builder.add_bin(0, 1);
    for (ItemPos unit_pos = 0; unit_pos < n; ++unit_pos) {
        solution_builder.add_item(
                0,
                data.units[unit_pos].item_type_id,
                {positions[0][unit_pos], positions[1][unit_pos]},
                solution_node.rotations[unit_pos] == 1);
    }
    Solution solution = solution_builder.build();
    if (!solution.feasible()) {
        throw std::logic_error(
                FUNC_SIGNATURE + ": the solution is not feasible.");
    }
    algorithm_formatter.update_solution(solution, "");

    algorithm_formatter.end();
    return output;
}
