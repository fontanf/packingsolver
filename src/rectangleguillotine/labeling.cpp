#include "rectangleguillotine/labeling.hpp"

#include "packingsolver/rectangleguillotine/algorithm_formatter.hpp"
#include "packingsolver/rectangleguillotine/post_process.hpp"
#include "rectangleguillotine/solution_builder.hpp"

#include <algorithm>
#include <limits>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

namespace
{

////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////// Label //////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/**
 * Identifier of a label within the label list of a given plate size.
 */
using LabelId = int64_t;

/**
 * Index into the compact labels array for a given plate size (w, h).
 * Non-useful plates map to -1.
 */
using PlateId = int64_t;

/**
 * The way a label was produced.
 */
enum class LabelOrigin
{
    /** No item produced; the plate is left as waste. */
    Waste,
    /** A single item exactly fills the plate. */
    Item,
    /** The plate is split by a guillotine cut into two sub-plates. */
    Cut,
    /**
     * The plate's items fit within a smaller useful sub-plate along the
     * x-axis; the remainder on the right is waste.  'norm_width' is the
     * width of the smaller useful plate, and 'norm_label_id' is the index
     * of the source label in that plate's label set.
     */
    NormX,
    /**
     * Same as NormX but along the y-axis; remainder on top is waste.
     */
    NormY,
};

/**
 * A label attached to a plate of a given size (width x height).
 *
 * A label represents one non-dominated way of selling a multiset of items
 * out of a guillotine cutting of the plate, following the labeling
 * algorithm of Dolatabadi, Lodi and Monaci (2012), cast in the
 * decision-hypergraph formalism of Léonard and Clautiaux (2025).
 */
struct Label
{
    /**
     * Multiset of item instances selected, encoded as a bitset.
     *
     * Item type i occupies bit_offsets[i] .. bit_offsets[i+1]-1.
     * The canonical form has the lowest 'count' bits set within each item's
     * range, so that bitwise subset (⊆) equals the item-count domination order.
     */
    std::vector<uint64_t> item_bitset;

    /** Profit of the label, i.e. the sum over all item instances in 'item_bitset'. */
    Profit profit = 0.0;

    /** How this label was produced. */
    LabelOrigin origin = LabelOrigin::Waste;

    /** Item type sold; valid only when 'origin' is 'Item'. */
    ItemTypeId item_type_id = -1;

    /** Orientation of the cut; valid only when 'origin' is 'Cut'. */
    CutOrientation cut_orientation = CutOrientation::Vertical;

    /**
     * Position of the cut relative to the plate origin, i.e. the width (for
     * a vertical cut) or the height (for a horizontal cut) of the first
     * sub-plate; valid only when 'origin' is 'Cut'.
     */
    Length cut_position = 0;

    /**
     * Label used in the first sub-plate (of size 'cut_position' x height for
     * a vertical cut, or width x 'cut_position' for a horizontal cut); valid
     * only when 'origin' is 'Cut'.
     */
    LabelId child_1_label_id = -1;

    /** Label used in the second sub-plate; valid only when 'origin' is 'Cut'. */
    LabelId child_2_label_id = -1;

    /**
     * For 'NormX': the width of the useful sub-plate this label was propagated
     * from; the source plate is (norm_width, current_height).
     * For 'NormY': the height of the useful sub-plate; the source plate is
     * (current_width, norm_height).
     * The label index within that source plate's label set is 'norm_label_id'.
     */
    Length norm_width  = 0;
    Length norm_height = 0;
    LabelId norm_label_id = -1;
};

////////////////////////////////////////////////////////////////////////////////
///////////////////////////// Bitset infrastructure ////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/** Count trailing zeros of a nonzero 64-bit word. */
static int ctz64(uint64_t word)
{
#ifdef __GNUC__
    return __builtin_ctzll(word);
#else
    int n = 0;
    if (!(word & 0xFFFFFFFFULL)) { n += 32; word >>= 32; }
    if (!(word & 0x0000FFFFULL)) { n += 16; word >>= 16; }
    if (!(word & 0x000000FFULL)) { n += 8;  word >>= 8;  }
    if (!(word & 0x0000000FULL)) { n += 4;  word >>= 4;  }
    if (!(word & 0x00000003ULL)) { n += 2;  word >>= 2;  }
    if (!(word & 0x00000001ULL))   n += 1;
    return n;
#endif
}

/** Total 64-bit chunks needed for all item-copy bits combined. */
static int compute_nb_chunks(const Instance& instance)
{
    int total_copies = 0;
    for (ItemTypeId id = 0; id < instance.number_of_item_types(); ++id)
        total_copies += instance.item_type(id).copies;
    return (total_copies + 63) / 64;
}

/**
 * Starting bit offset of each item type's range within the bitset.
 * bit_offsets[id] is the first bit for item type 'id';
 * bit_offsets[id+1] - bit_offsets[id] = item_type(id).copies.
 */
static std::vector<int> compute_bit_offsets(const Instance& instance)
{
    int nb_item_types = instance.number_of_item_types();
    std::vector<int> offsets(nb_item_types + 1, 0);
    for (ItemTypeId id = 0; id < nb_item_types; ++id)
        offsets[id + 1] = offsets[id] + instance.item_type(id).copies;
    return offsets;
}

/**
 * Merge two bitset labels into 'bits_result', capping each item type at its
 * copy limit.  Returns the merged profit (= profit_a + profit_b minus any
 * excess profit for items whose combined count was capped).
 */
static Profit bitset_merge(
        const Instance& instance,
        const std::vector<int>& bit_offsets,
        int nb_chunks,
        const uint64_t* bits_a,
        const uint64_t* bits_b,
        Profit profit_a,
        Profit profit_b,
        uint64_t* bits_result)
{
    std::fill(bits_result, bits_result + nb_chunks, (uint64_t)0);
    Profit profit = profit_a + profit_b;
    for (ItemTypeId id = 0; id < instance.number_of_item_types(); ++id) {
        int start = bit_offsets[id];
        int end   = bit_offsets[id + 1];
        int count_a = 0;
        int count_b = 0;
        for (int bit = start; bit < end; ++bit) {
            if ((bits_a[bit / 64] >> (bit % 64)) & 1) ++count_a;
            if ((bits_b[bit / 64] >> (bit % 64)) & 1) ++count_b;
        }
        int merged = std::min(count_a + count_b, end - start);
        int excess = (count_a + count_b) - merged;
        if (excess > 0)
            profit -= instance.item_type(id).profit * excess;
        for (int k = 0; k < merged; ++k) {
            int bit = start + k;
            bits_result[bit / 64] |= (uint64_t)1 << (bit % 64);
        }
    }
    return profit;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// LabelSet ////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/**
 * A Pareto-optimal set of labels for one plate size, backed by a per-bit
 * inverted index for fast dominance checking (Léonard and Clautiaux 2025).
 *
 * Dominance order: label A dominates label B iff A has at least as many
 * copies of every item type as B, which in the bitset encoding is equivalent
 * to B.bits ⊆ A.bits (i.e. every bit set in B is also set in A).
 *
 * The rarest-bit optimization chooses, for the "is candidate dominated?"
 * check, the bit that is set in the fewest accepted labels; that bit's
 * inverted-index list is then the smallest set of candidates to compare
 * against, giving an average-case speedup of ~N/K where N is the number of
 * accepted labels and K is the number of bits.
 */
struct LabelSet {
    int nb_chunks = 0;
    std::vector<Label> labels;
    std::vector<bool> alive;
    /** bit_index[b] = IDs of labels (alive or dead) that have bit b set. */
    std::vector<std::vector<LabelId>> bit_index;
    /** Compact list of alive label IDs; kept sorted by insertion order. */
    std::vector<LabelId> alive_ids;
    int nb_alive = 0;

    void init(int nb_chunks_in)
    {
        nb_chunks = nb_chunks_in;
        if (nb_chunks_in > 0)
            bit_index.assign(64 * nb_chunks_in, {});
        nb_alive = 0;
    }

    /**
     * Attempt to add 'candidate' to this label set.
     *
     * The candidate is rejected if:
     *   (a) rdp_value + candidate.profit < lb, or
     *   (b) some existing alive label dominates it.
     * Otherwise it is accepted, and all alive labels it dominates are removed.
     * Returns true iff the candidate was accepted.
     */
    bool add_label(Label candidate, Profit rdp_value, Profit lb)
    {
        if (rdp_value + candidate.profit < lb)
            return false;

        const uint64_t* cand_bits = candidate.item_bitset.data();

        // Find the rarest set bit in candidate for the fast dominance check.
        int best_bit = -1;
        {
            size_t best_count = static_cast<size_t>(-1);
            for (int chunk = 0; chunk < nb_chunks; ++chunk) {
                uint64_t word = cand_bits[chunk];
                while (word) {
                    int bit = 64 * chunk + ctz64(word);
                    if (bit_index[bit].size() < best_count) {
                        best_count = bit_index[bit].size();
                        best_bit = bit;
                    }
                    word &= word - 1;
                }
            }
        }

        // Check if candidate is dominated by an existing alive label.
        // Any dominator must have 'best_bit' set, so only check those.
        if (best_bit >= 0) {
            for (LabelId existing_id: bit_index[best_bit]) {
                if (!alive[existing_id]) continue;
                const uint64_t* e_bits = labels[existing_id].item_bitset.data();
                bool dominated = true;
                for (int c = 0; c < nb_chunks; ++c) {
                    if ((cand_bits[c] | e_bits[c]) != e_bits[c]) {
                        dominated = false;
                        break;
                    }
                }
                if (dominated) return false;
            }
        }

        // Mark existing alive labels dominated by candidate as dead.
        // A label e is dominated by candidate iff e.bits ⊆ candidate.bits.
        for (LabelId existing_id: alive_ids) {
            const uint64_t* e_bits = labels[existing_id].item_bitset.data();
            bool is_dominated = true;
            for (int c = 0; c < nb_chunks; ++c) {
                if ((e_bits[c] | cand_bits[c]) != cand_bits[c]) {
                    is_dominated = false;
                    break;
                }
            }
            if (is_dominated) {
                alive[existing_id] = false;
                --nb_alive;
            }
        }
        // Compact alive_ids: remove entries just marked dead.
        {
            int write = 0;
            for (int read = 0; read < (int)alive_ids.size(); ++read)
                if (alive[alive_ids[read]])
                    alive_ids[write++] = alive_ids[read];
            alive_ids.resize(write);
        }

        // Commit candidate: update bit_index, labels, alive arrays.
        LabelId new_id = (LabelId)labels.size();
        for (int chunk = 0; chunk < nb_chunks; ++chunk) {
            uint64_t word = cand_bits[chunk];
            while (word) {
                int bit = 64 * chunk + ctz64(word);
                bit_index[bit].push_back(new_id);
                word &= word - 1;
            }
        }
        labels.push_back(std::move(candidate));
        alive.push_back(true);
        alive_ids.push_back(new_id);
        ++nb_alive;
        return true;
    }

    const Label& get(LabelId label_id) const { return labels[label_id]; }
};

/**
 * Compute which plate dimensions (1 .. max_dimension) are reachable as
 * sub-plate sizes along a given axis.
 *
 * A dimension is reachable if a bounded multiset of items (respecting copy
 * limits) has widths summing to it, or if it can be written as
 * first + cut_thickness + second where both first and second are reachable.
 * The full bin dimension is always included.
 *
 * Item copy limits are respected in the initial seeding via a bounded 0/1
 * knapsack.  The cut-combination propagation is unbounded (conservative):
 * it may mark a few additional dimensions as reachable, but it never omits
 * a truly reachable one.
 */
static std::vector<bool> compute_useful_dimensions(
        const Instance& instance,
        Length max_dimension,
        bool is_width_axis)
{
    Length cut_thickness = instance.parameters().cut_thickness;
    std::vector<bool> useful(max_dimension + 1, false);
    useful[0] = true;  // base case for the bounded knapsack

    // Bounded 0/1 knapsack: each copy of each item is processed separately.
    // The reverse sweep ensures we do not count the same physical copy twice.
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        Length dim_same  = is_width_axis ? item_type.rect.w : item_type.rect.h;
        Length dim_other = is_width_axis ? item_type.rect.h : item_type.rect.w;
        for (ItemPos copy = 0; copy < item_type.copies; ++copy) {
            if (dim_same <= max_dimension) {
                for (Length d = max_dimension; d >= dim_same; --d)
                    if (useful[d - dim_same]) useful[d] = true;
            }
            if (!item_type.oriented && dim_other <= max_dimension) {
                for (Length d = max_dimension; d >= dim_other; --d)
                    if (useful[d - dim_other]) useful[d] = true;
            }
        }
    }

    for (Length d = 2; d <= max_dimension; ++d) {
        if (useful[d])
            continue;
        for (Length first = 1; first + cut_thickness + 1 <= d; ++first) {
            Length second = d - first - cut_thickness;
            if (second >= 1 && useful[first] && useful[second]) {
                useful[d] = true;
                break;
            }
        }
    }

    useful[max_dimension] = true;
    return useful;
}

/**
 * For each dimension d (0 .. max_dimension), compute the largest useful
 * dimension ≤ d, or 0 if none exists.
 *
 * This is the "normalization" map: a plate of dimension d can always be
 * treated as a plate of dimension norm[d] with the remainder as waste.
 */
static std::vector<Length> compute_normalization(
        const std::vector<bool>& useful,
        Length max_dimension)
{
    std::vector<Length> norm(max_dimension + 1, 0);
    for (Length d = 1; d <= max_dimension; ++d)
        norm[d] = useful[d] ? d : norm[d - 1];
    return norm;
}

/**
 * Build a flat lookup table that maps every plate dimension pair (w, h)
 * (encoded as w + (eff_width+1)*h) to its compact index in the labels array,
 * or -1 if the plate is non-useful (i.e. !useful_widths[w] || !useful_heights[h]).
 *
 * Indices are assigned in the same order as the compute_labels outer loop
 * (w increasing, h increasing) so that all sub-plates that a given plate
 * can refer to already have lower indices.
 *
 * The total number of useful plates is returned via 'nb_useful_plates'.
 */
static std::vector<PlateId> compute_plate_ids(
        const std::vector<bool>& useful_widths,
        const std::vector<bool>& useful_heights,
        Length eff_width,
        Length eff_height,
        int64_t& nb_useful_plates)
{
    std::vector<PlateId> plate_id((eff_width + 1) * (eff_height + 1), -1);
    nb_useful_plates = 0;
    for (Length width = 1; width <= eff_width; ++width) {
        if (!useful_widths[width]) continue;
        for (Length height = 1; height <= eff_height; ++height) {
            if (!useful_heights[height]) continue;
            plate_id[width + (eff_width + 1) * height] = nb_useful_plates++;
        }
    }
    return plate_id;
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Dynamic programming ////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/*
 * Classical (unbounded copies) dynamic programming, identical to the
 * recurrence used by 'dynamic_programming_infinite_copies_array'. This
 * relaxation (item copy limits are ignored) is used to derive the upper
 * bounds ('rdp_values' below) guiding the labeling algorithm.
 */
std::vector<Profit> compute_dp(const Instance& instance)
{
    const BinType& bin_type = instance.bin_type(0);
    Length eff_width  = bin_type.rect.w - bin_type.left_trim - bin_type.right_trim;
    Length eff_height = bin_type.rect.h - bin_type.bottom_trim - bin_type.top_trim;
    Length cut_thickness = instance.parameters().cut_thickness;

    std::vector<Profit> dp_values((eff_width + 1) * (eff_height + 1), 0.0);

    auto dp = [&](Length width, Length height) -> Profit& {
        return dp_values[width + (eff_width + 1) * height];
    };

    for (Length width = 1; width <= eff_width; ++width) {
        for (Length height = 1; height <= eff_height; ++height) {
            Profit& dp_current = dp(width, height);

            // Try placing each item type (exact fit, with or without rotation).
            for (ItemTypeId item_type_id = 0;
                    item_type_id < instance.number_of_item_types();
                    ++item_type_id) {
                const ItemType& item_type = instance.item_type(item_type_id);
                if (item_type.rect.w == width && item_type.rect.h == height)
                    dp_current = std::max(dp_current, item_type.profit);
                if (!item_type.oriented
                        && item_type.rect.h == width
                        && item_type.rect.w == height)
                    dp_current = std::max(dp_current, item_type.profit);
            }

            // Try all splits along x.
            for (Length cut = 1; 2 * cut + cut_thickness <= width; ++cut)
                dp_current = std::max(dp_current,
                        dp(cut, height) + dp(width - cut - cut_thickness, height));

            // Try all splits along y.
            for (Length cut = 1; 2 * cut + cut_thickness <= height; ++cut)
                dp_current = std::max(dp_current,
                        dp(width, cut) + dp(width, height - cut - cut_thickness));
        }
    }

    return dp_values;
}

/*
 * "Reverse" dynamic programming (Claut et al. 2018): rdp_values[v] is an
 * upper bound on the profit obtainable from the rest of the bin whenever a
 * plate v is used somewhere in the cutting process.
 *
 * Recurrence:
 *   rdp[eff_width][eff_height] = 0  // the whole bin (root).
 *   rdp[width][height] = max(
 *       max_{W > width, 2nd piece >= 1} rdp[W][height] + dp[W - width - t][height],
 *       max_{H > height, 2nd piece >= 1} rdp[width][H] + dp[width][H - height - t]
 *   )
 *
 * Processed by decreasing width and decreasing height, so that both terms
 * of the maximum are already available.
 */
std::vector<Profit> compute_rdp(
        const std::vector<Profit>& dp_values,
        Length eff_width,
        Length eff_height,
        Length cut_thickness)
{
    std::vector<Profit> rdp_values(
            (eff_width + 1) * (eff_height + 1),
            -std::numeric_limits<Profit>::infinity());

    auto dp = [&](Length width, Length height) {
        return dp_values[width + (eff_width + 1) * height];
    };
    auto rdp = [&](Length width, Length height) -> Profit& {
        return rdp_values[width + (eff_width + 1) * height];
    };

    rdp(eff_width, eff_height) = 0.0;

    for (Length width = eff_width; width >= 1; --width) {
        for (Length height = eff_height; height >= 1; --height) {
            if (width == eff_width && height == eff_height)
                continue;

            Profit& rdp_current = rdp(width, height);

            // 'v' embedded via a vertical cut into a wider plate of the
            // same height.
            for (Length total_width = width + cut_thickness + 1;
                    total_width <= eff_width;
                    ++total_width) {
                Length other_width = total_width - width - cut_thickness;
                rdp_current = std::max(
                        rdp_current,
                        rdp(total_width, height) + dp(other_width, height));
            }

            // 'v' embedded via a horizontal cut into a taller plate of the
            // same width.
            for (Length total_height = height + cut_thickness + 1;
                    total_height <= eff_height;
                    ++total_height) {
                Length other_height = total_height - height - cut_thickness;
                rdp_current = std::max(
                        rdp_current,
                        rdp(width, total_height) + dp(width, other_height));
            }
        }
    }

    return rdp_values;
}

////////////////////////////////////////////////////////////////////////////////
//////////////////////////////// Labeling algorithm ////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/*
 * Build, for every plate size, the set of non-dominated labels (Algorithm 1
 * of Léonard and Clautiaux (2025), originally from Dolatabadi, Lodi and
 * Monaci (2012)), using 'rdp_values' for the profit criterion.
 *
 * Processed in the same order as 'compute_dp', so that, for every plate
 * (width, height), the labels of every smaller sub-plate it could be split
 * into are already available.
 */
std::vector<LabelSet> compute_labels(
        const Instance& instance,
        const std::vector<Profit>& dp_values,
        const std::vector<Profit>& rdp_values,
        Length eff_width,
        Length eff_height,
        Length cut_thickness,
        const std::vector<bool>& useful_widths,
        const std::vector<bool>& useful_heights,
        const std::vector<Length>& norm_x,
        const std::vector<Length>& norm_y,
        const std::vector<PlateId>& plate_id,
        int64_t nb_useful_plates,
        int nb_chunks,
        const std::vector<int>& bit_offsets,
        Profit lb)
{
    std::vector<LabelSet> label_sets(nb_useful_plates);
    for (LabelSet& label_set_entry: label_sets)
        label_set_entry.init(nb_chunks);

    auto label_set = [&](Length width, Length height) -> LabelSet& {
        return label_sets[plate_id[width + (eff_width + 1) * height]];
    };
    auto dp = [&](Length width, Length height) {
        return dp_values[width + (eff_width + 1) * height];
    };
    auto rdp = [&](Length width, Length height) {
        return rdp_values[width + (eff_width + 1) * height];
    };

    for (Length width = 1; width <= eff_width; ++width) {
        if (!useful_widths[width])
            continue;
        for (Length height = 1; height <= eff_height; ++height) {
            if (!useful_heights[height])
                continue;
            Profit rdp_value = rdp(width, height);
            // Plate-level filter: even the relaxation upper bound for this
            // plate combined with its complement cannot reach the threshold.
            if (dp(width, height) + rdp_value < lb)
                continue;
            LabelSet& current_label_set = label_set(width, height);

            // Trivial label: no items produced (waste). Kept unless a better
            // label dominates it; NormX/NormY covers one-sided-waste cuts.
            {
                Label waste_label;
                waste_label.item_bitset.assign(nb_chunks, 0);
                current_label_set.add_label(std::move(waste_label), rdp_value, lb);
            }

            // Single-item labels: an item exactly fills the plate.
            for (ItemTypeId item_type_id = 0;
                    item_type_id < instance.number_of_item_types();
                    ++item_type_id) {
                const ItemType& item_type = instance.item_type(item_type_id);
                if (item_type.copies < 1)
                    continue;
                bool fits_normal = (item_type.rect.w == width && item_type.rect.h == height);
                bool fits_rotated = (!item_type.oriented
                        && item_type.rect.h == width
                        && item_type.rect.w == height);
                if (!fits_normal && !fits_rotated)
                    continue;

                Label candidate;
                candidate.origin = LabelOrigin::Item;
                candidate.item_type_id = item_type_id;
                candidate.item_bitset.assign(nb_chunks, 0);
                {
                    int bit = bit_offsets[item_type_id];
                    candidate.item_bitset[bit / 64] |= (uint64_t)1 << (bit % 64);
                }
                candidate.profit = item_type.profit;
                current_label_set.add_label(std::move(candidate), rdp_value, lb);
            }

            // Normalization labels: propagate from the nearest smaller useful
            // plate along x and y.  This handles the case where one sub-plate
            // of a cut is pure waste (non-useful width/height): instead of
            // trying every cut position, we represent the "waste strip on the
            // right/top" implicitly via a NormX/NormY label.

            // NormX: items fit within (norm_x[width-1], height), waste on right.
            Length prev_useful_x = norm_x[width - 1];
            if (prev_useful_x > 0) {
                const LabelSet& src_x = label_set(prev_useful_x, height);
                for (LabelId src_id: src_x.alive_ids) {
                    const Label& src = src_x.get(src_id);
                    if (src.origin == LabelOrigin::Waste) continue;
                    Label candidate;
                    candidate.origin = LabelOrigin::NormX;
                    candidate.norm_width  = prev_useful_x;
                    candidate.norm_height = height;
                    candidate.norm_label_id = src_id;
                    candidate.item_bitset = src.item_bitset;
                    candidate.profit = src.profit;
                    current_label_set.add_label(std::move(candidate), rdp_value, lb);
                }
            }

            // NormY: items fit within (width, norm_y[height-1]), waste on top.
            Length prev_useful_y = norm_y[height - 1];
            if (prev_useful_y > 0) {
                const LabelSet& src_y = label_set(width, prev_useful_y);
                for (LabelId src_id: src_y.alive_ids) {
                    const Label& src = src_y.get(src_id);
                    if (src.origin == LabelOrigin::Waste) continue;
                    Label candidate;
                    candidate.origin = LabelOrigin::NormY;
                    candidate.norm_width  = width;
                    candidate.norm_height = prev_useful_y;
                    candidate.norm_label_id = src_id;
                    candidate.item_bitset = src.item_bitset;
                    candidate.profit = src.profit;
                    current_label_set.add_label(std::move(candidate), rdp_value, lb);
                }
            }

            // Cut labels: split the plate into two sub-plates, and combine
            // every pair of labels of the two sub-plates.
            // Only try cut positions where BOTH sub-plates are useful: cuts
            // that produce a non-useful sub-plate on one side are already
            // covered by NormX/NormY propagation above.

            // Vertical cuts.
            for (Length cut = 1; 2 * cut + cut_thickness <= width; ++cut) {
                if (!useful_widths[cut]) continue;
                Length other_width = width - cut - cut_thickness;
                if (!useful_widths[other_width]) continue;
                // Cut-level filter: best possible combination of labels from
                // both sub-plates cannot reach the threshold.
                if (rdp_value + dp(cut, height) + dp(other_width, height) < lb) continue;
                const LabelSet& left_label_set  = label_set(cut, height);
                const LabelSet& right_label_set = label_set(other_width, height);
                for (LabelId left_id: left_label_set.alive_ids) {
                    for (LabelId right_id: right_label_set.alive_ids) {
                        Label candidate;
                        candidate.origin = LabelOrigin::Cut;
                        candidate.cut_orientation = CutOrientation::Vertical;
                        candidate.cut_position = cut;
                        candidate.child_1_label_id = left_id;
                        candidate.child_2_label_id = right_id;
                        candidate.item_bitset.resize(nb_chunks);
                        candidate.profit = bitset_merge(
                                instance, bit_offsets, nb_chunks,
                                left_label_set.get(left_id).item_bitset.data(),
                                right_label_set.get(right_id).item_bitset.data(),
                                left_label_set.get(left_id).profit,
                                right_label_set.get(right_id).profit,
                                candidate.item_bitset.data());
                        current_label_set.add_label(std::move(candidate), rdp_value, lb);
                    }
                }
            }

            // Horizontal cuts.
            for (Length cut = 1; 2 * cut + cut_thickness <= height; ++cut) {
                if (!useful_heights[cut]) continue;
                Length other_height = height - cut - cut_thickness;
                if (!useful_heights[other_height]) continue;
                // Cut-level filter.
                if (rdp_value + dp(width, cut) + dp(width, other_height) < lb) continue;
                const LabelSet& bottom_label_set = label_set(width, cut);
                const LabelSet& top_label_set    = label_set(width, other_height);
                for (LabelId bottom_id: bottom_label_set.alive_ids) {
                    for (LabelId top_id: top_label_set.alive_ids) {
                        Label candidate;
                        candidate.origin = LabelOrigin::Cut;
                        candidate.cut_orientation = CutOrientation::Horizontal;
                        candidate.cut_position = cut;
                        candidate.child_1_label_id = bottom_id;
                        candidate.child_2_label_id = top_id;
                        candidate.item_bitset.resize(nb_chunks);
                        candidate.profit = bitset_merge(
                                instance, bit_offsets, nb_chunks,
                                bottom_label_set.get(bottom_id).item_bitset.data(),
                                top_label_set.get(top_id).item_bitset.data(),
                                bottom_label_set.get(bottom_id).profit,
                                top_label_set.get(top_id).profit,
                                candidate.item_bitset.data());
                        current_label_set.add_label(std::move(candidate), rdp_value, lb);
                    }
                }
            }
        }
    }

    return label_sets;
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Solution reconstruction ////////////////////////
////////////////////////////////////////////////////////////////////////////////

/*
 * Trace back through a label to build the solution tree, following the same
 * alternating-depth / pass-through convention as
 * 'dynamic_programming_infinite_copies_array::reconstruct_rec'.
 */
void reconstruct_label(
        SolutionBuilder& builder,
        const Instance& instance,
        const std::vector<LabelSet>& label_sets,
        Length eff_width,
        const std::vector<PlateId>& plate_id,
        Length cut_thickness,
        CutOrientation first_cut_orientation,
        Length width,
        Length height,
        LabelId label_id,
        Depth depth,
        Length x_offset,
        Length y_offset)
{
    const Label& label = label_sets[plate_id[width + (eff_width + 1) * height]].get(label_id);

    if (label.origin == LabelOrigin::Waste)
        return;

    if (label.origin == LabelOrigin::Item) {
        builder.set_last_node_item(label.item_type_id);
        return;
    }

    bool cut_is_vertical = (first_cut_orientation == CutOrientation::Vertical)
        == (depth % 2 == 1);

    if (label.origin == LabelOrigin::NormX) {
        // Items occupy [x_offset, x_offset+norm_width]; [norm_width, width] is
        // waste and will be filled automatically by the SolutionBuilder.
        if (!cut_is_vertical) {
            // Mismatch: horizontal cuts at this depth; add a pass-through and
            // retry at depth+1 where vertical cuts are expected.
            builder.add_node(depth, y_offset + height);
            reconstruct_label(
                    builder, instance, label_sets, eff_width, plate_id, cut_thickness,
                    first_cut_orientation, width, height, label_id,
                    depth + 1, x_offset, y_offset);
            return;
        }
        builder.add_node(depth, x_offset + label.norm_width);
        reconstruct_label(
                builder, instance, label_sets, eff_width, plate_id, cut_thickness,
                first_cut_orientation,
                label.norm_width, height, label.norm_label_id,
                depth + 1, x_offset, y_offset);
        return;
    }

    if (label.origin == LabelOrigin::NormY) {
        // Items occupy [y_offset, y_offset+norm_height]; [norm_height, height]
        // is waste and will be filled automatically by the SolutionBuilder.
        if (cut_is_vertical) {
            // Mismatch: vertical cuts at this depth; add a pass-through and
            // retry at depth+1 where horizontal cuts are expected.
            builder.add_node(depth, x_offset + width);
            reconstruct_label(
                    builder, instance, label_sets, eff_width, plate_id, cut_thickness,
                    first_cut_orientation, width, height, label_id,
                    depth + 1, x_offset, y_offset);
            return;
        }
        builder.add_node(depth, y_offset + label.norm_height);
        reconstruct_label(
                builder, instance, label_sets, eff_width, plate_id, cut_thickness,
                first_cut_orientation,
                width, label.norm_height, label.norm_label_id,
                depth + 1, x_offset, y_offset);
        return;
    }

    // label.origin == LabelOrigin::Cut.
    bool label_is_vertical = (label.cut_orientation == CutOrientation::Vertical);

    if (cut_is_vertical != label_is_vertical) {
        // No cut of the expected orientation at this depth: insert a
        // pass-through node and retry one level deeper, where the expected
        // orientation will match the label's.
        if (cut_is_vertical) {
            builder.add_node(depth, x_offset + width);
        } else {
            builder.add_node(depth, y_offset + height);
        }
        reconstruct_label(
                builder, instance, label_sets, eff_width, plate_id, cut_thickness,
                first_cut_orientation, width, height, label_id,
                depth + 1, x_offset, y_offset);
        return;
    }

    Length cut = label.cut_position;
    if (label_is_vertical) {
        builder.add_node(depth, x_offset + cut);
        reconstruct_label(
                builder, instance, label_sets, eff_width, plate_id, cut_thickness,
                first_cut_orientation, cut, height, label.child_1_label_id,
                depth + 1, x_offset, y_offset);
        builder.add_node(depth, x_offset + width);
        reconstruct_label(
                builder, instance, label_sets, eff_width, plate_id, cut_thickness,
                first_cut_orientation, width - cut - cut_thickness, height,
                label.child_2_label_id,
                depth + 1, x_offset + cut + cut_thickness, y_offset);
    } else {
        builder.add_node(depth, y_offset + cut);
        reconstruct_label(
                builder, instance, label_sets, eff_width, plate_id, cut_thickness,
                first_cut_orientation, width, cut, label.child_1_label_id,
                depth + 1, x_offset, y_offset);
        builder.add_node(depth, y_offset + height);
        reconstruct_label(
                builder, instance, label_sets, eff_width, plate_id, cut_thickness,
                first_cut_orientation, width, height - cut - cut_thickness,
                label.child_2_label_id,
                depth + 1, x_offset, y_offset + cut + cut_thickness);
    }
}

Solution reconstruct_solution(
        const Instance& instance,
        const std::vector<LabelSet>& label_sets,
        Length eff_width,
        Length eff_height,
        const std::vector<PlateId>& plate_id)
{
    const BinType& bin_type = instance.bin_type(0);

    // Hard trims physically shift the usable area; soft trims do not.
    Length x_offset = (bin_type.left_trim_type  == TrimType::Hard)? bin_type.left_trim:  0;
    Length y_offset = (bin_type.bottom_trim_type == TrimType::Hard)? bin_type.bottom_trim: 0;

    CutOrientation first_cut_orientation
        = instance.parameters().first_stage_orientation;
    if (first_cut_orientation == CutOrientation::Any)
        first_cut_orientation = CutOrientation::Vertical;

    Length cut_thickness = instance.parameters().cut_thickness;

    SolutionBuilder builder(instance);
    builder.add_bin(0, 1, first_cut_orientation);

    PlateId root_plate_id = plate_id[eff_width + (eff_width + 1) * eff_height];
    const LabelSet& root_label_set = label_sets[root_plate_id];
    LabelId best_label_id = -1;
    for (LabelId label_id: root_label_set.alive_ids) {
        if (best_label_id == -1
                || root_label_set.get(label_id).profit
                        > root_label_set.get(best_label_id).profit)
            best_label_id = label_id;
    }

    if (best_label_id != -1 && root_label_set.get(best_label_id).profit > 0.0) {
        builder.add_node(1, x_offset + eff_width);
        reconstruct_label(
                builder, instance, label_sets, eff_width, plate_id, cut_thickness,
                first_cut_orientation, eff_width, eff_height, best_label_id,
                2, x_offset, y_offset);
    }

    return builder.build();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Main function ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

const LabelingOutput packingsolver::rectangleguillotine::labeling(
        const Instance& instance,
        const LabelingParameters& parameters)
{
    LabelingOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    const BinType& bin_type = instance.bin_type(0);
    Length eff_width  = bin_type.rect.w - bin_type.left_trim - bin_type.right_trim;
    Length eff_height = bin_type.rect.h - bin_type.bottom_trim - bin_type.top_trim;
    Length cut_thickness = instance.parameters().cut_thickness;

    std::vector<Profit> dp_values = compute_dp(instance);
    std::vector<Profit> rdp_values = compute_rdp(dp_values, eff_width, eff_height, cut_thickness);

    std::vector<bool> useful_widths  = compute_useful_dimensions(instance, eff_width,  true);
    std::vector<bool> useful_heights = compute_useful_dimensions(instance, eff_height, false);
    std::vector<Length> norm_x = compute_normalization(useful_widths,  eff_width);
    std::vector<Length> norm_y = compute_normalization(useful_heights, eff_height);

    int64_t nb_useful_plates = 0;
    std::vector<PlateId> plate_id = compute_plate_ids(
            useful_widths, useful_heights, eff_width, eff_height, nb_useful_plates);

    int nb_chunks = compute_nb_chunks(instance);
    std::vector<int> bit_offsets = compute_bit_offsets(instance);

    // Relaxation upper bound: dp without copy limits.
    Profit dp_root = dp_values[eff_width + (eff_width + 1) * eff_height];

    // Iterative descent: start at the relaxation upper bound and lower the
    // threshold until a feasible solution is found.  Any root label surviving
    // at threshold lb has profit >= lb, and its existence proves lb <= P*, so
    // the first successful pass yields the global optimum.
    Profit lb = dp_root;
    while (true) {
        std::vector<LabelSet> label_sets = compute_labels(
                instance,
                dp_values,
                rdp_values,
                eff_width,
                eff_height,
                cut_thickness,
                useful_widths,
                useful_heights,
                norm_x,
                norm_y,
                plate_id,
                nb_useful_plates,
                nb_chunks,
                bit_offsets,
                lb);

        PlateId root_plate_id = plate_id[eff_width + (eff_width + 1) * eff_height];
        if (!label_sets[root_plate_id].alive_ids.empty()) {
            Solution solution = reconstruct_solution(
                    instance, label_sets, eff_width, eff_height, plate_id);
            solution = minimize_number_of_stages(solution);
            algorithm_formatter.update_solution(solution, "Labeling");
            break;
        }

        // No feasible solution at this threshold: P* < lb.
        // Decrease lb; when lb <= 0 the filter is inactive and the pass
        // will always find a solution (at worst the empty solution).
        lb *= 0.999;
        if (lb < 1e-9) {
            // Run one final pass with no filtering.
            std::vector<LabelSet> label_sets_final = compute_labels(
                    instance,
                    dp_values,
                    rdp_values,
                    eff_width,
                    eff_height,
                    cut_thickness,
                    useful_widths,
                    useful_heights,
                    norm_x,
                    norm_y,
                    plate_id,
                    nb_useful_plates,
                    nb_chunks,
                    bit_offsets,
                    0.0);
            Solution solution = reconstruct_solution(
                    instance, label_sets_final, eff_width, eff_height, plate_id);
            solution = minimize_number_of_stages(solution);
            algorithm_formatter.update_solution(solution, "Labeling");
            break;
        }
    }

    algorithm_formatter.end();
    return output;
}
