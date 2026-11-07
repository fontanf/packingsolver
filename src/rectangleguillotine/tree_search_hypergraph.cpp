#include "rectangleguillotine/tree_search_hypergraph.hpp"

#include "packingsolver/rectangleguillotine/algorithm_formatter.hpp"
#include "rectangleguillotine/solution_builder.hpp"
#include "rectangleguillotine/normal_positions.hpp"

#include "optimizationtools/utils/utils.hpp"
#include "columngenerationsolver/algorithms/column_generation.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <tuple>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

bool packingsolver::rectangleguillotine::tree_search_hypergraph_supported(
        const Instance& instance)
{
    if (instance.objective() != Objective::Knapsack
            && instance.objective() != Objective::Feasibility) {
        return false;
    }
    if (instance.number_of_bin_types() != 1
            || instance.number_of_bins() != 1) {
        return false;
    }
    const BinType& bin_type = instance.bin_type(0);
    if (!bin_type.defects.empty()
            || !bin_type.fixed_items.empty()
            || bin_type.number_of_resources() > 0
            || bin_type.has_soft_left_trim()
            || bin_type.has_soft_bottom_trim()) {
        return false;
    }
    if (instance.number_of_stacks() != instance.number_of_item_types())
        return false;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        if (instance.objective() == Objective::Knapsack
                && item_type.copies_min > 0) {
            return false;
        }
    }
    const auto& parameters = instance.parameters();
    if (parameters.minimum_waste_length != 0
            || parameters.minimum_distance_1_cuts != 0
            || parameters.maximum_distance_1_cuts != -1
            || parameters.minimum_distance_2_cuts != 0
            || parameters.maximum_distance_2_cuts != -1
            || parameters.maximum_number_1_cuts != -1
            || parameters.maximum_number_2_cuts != -1) {
        return false;
    }
    if (!instance.number_of_stages_unlimited()
            && parameters.cut_type == CutType::Homogenous) {
        return false;
    }
    return true;
}

namespace
{

using StateId = int64_t;

/**
 * The numbers of copies of the item types, at most the number of copies which
 * fit in the bin; 0 for the item types of non-positive profit with the
 * 'Knapsack' objective.
 */
std::vector<ItemPos> compute_copies(const Instance& instance)
{
    const BinType& bin_type = instance.bin_type(0);
    Length usable_width = bin_type.rect.w - bin_type.left_trim - bin_type.right_trim;
    Length usable_height = bin_type.rect.h - bin_type.bottom_trim - bin_type.top_trim;
    Area usable_area = (Area)usable_width * usable_height;
    std::vector<ItemPos> copies(instance.number_of_item_types(), 0);
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        bool fits = (item_type.rect.w <= usable_width && item_type.rect.h <= usable_height)
            || (!item_type.oriented
                    && item_type.rect.h <= usable_width
                    && item_type.rect.w <= usable_height);
        ItemPos maximum_copies = (fits)? usable_area / item_type.area(): 0;
        copies[item_type_id] = std::min(item_type.copies, maximum_copies);
        if (instance.objective() == Objective::Knapsack && item_type.profit <= 0)
            copies[item_type_id] = 0;
    }
    return copies;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// Hypergraph //////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/*
 * The positions along a dimension of the bin.
 */
struct Axis
{
    /** The lengths of the positions, increasing; 'real[0]' is 0. */
    std::vector<Length> real;

    /**
     * The lengths in which the items can be packed: the real length, except
     * for the dimension of the bin with a soft trim at its end.
     */
    std::vector<Length> usable;

    /**
     * For each length 'l' between 0 and the usable length of the bin, the
     * index of the largest position not larger than 'l' (not the dimension of
     * the bin, if it is not a position).
     */
    std::vector<Counter> floor;

    /** Index of the dimension of the bin. */
    Counter bin;

    /**
     * For each position, the item types (and their rotation) whose dimension
     * along this axis is this position.
     */
    std::vector<std::vector<std::pair<ItemTypeId, bool>>> item_types;
};

/*
 * A pattern of a state without cut of the hypergraph: waste, or items.
 *
 * With a limited number of stages, a leaf is either a single item filling the
 * state exactly, or, at the last stage, the items separated by the extra cut
 * of the 'NonExact' (one item and a waste) and 'Roadef2018' (one or two
 * items, and a waste) cut types. With an unlimited number of stages, a leaf
 * is a single item in the bottom left corner of the state, the rest being
 * waste.
 */
struct Leaf
{
    Counter number_of_items = 0;

    ItemTypeId item_type_ids[2];

    bool rotate[2];
};

/*
 * The decision hypergraph of the instance.
 *
 * With a limited number 'K' of stages, there are two kinds of states:
 * - N(w, h, o, r): a sub-plate of dimensions 'w x h', whose sub-plates (if
 *   it is cut) are obtained by cuts of orientation 'o'; 'r' is the number
 *   of stages left for these cuts. The bin is N(W, H, o1, K), where 'o1' is
 *   the orientation of the first stage. It is a leaf, or it is cut, which is
 *   the hyperarc N(w, h, o, r) <- S(w, h, o, r) (only if 'r >= 1').
 * - S(w, h, o, r): the part of a sub-plate N(., h, o, r) (for 'o' vertical)
 *   which remains to be cut, of width 'w'. Its first strip is N(c, h, o', r - 1)
 *   ('o'' the other orientation), and the rest is S(<w - c - t>, h, o, r),
 *   where 't' is the cut thickness and '<x>' the largest normal position not
 *   larger than 'x'; or it is a single last strip N(c, h, o', r - 1), the
 *   rest being waste; or it is waste.
 * The lengths of the states are normal positions, except the dimensions of
 * the bin: the item of an exact leaf must have the exact dimensions of its
 * state, so the dimensions of a strip are not rounded down.
 *
 * With an unlimited number of stages, a state P(w, h) is a sub-rectangle
 * whose dimensions are raster points, and the hyperarcs are its cuts:
 * P(w, h) <- P(c, h) + P(<w - c - t>, h) and
 * P(w, h) <- P(w, c) + P(w, <h - c - t>), with the first part not larger
 * than the rest.
 *
 * The identifiers of the states follow a topological order: the sources of a
 * hyperarc have a smaller identifier than its target.
 */
class Hypergraph
{

public:

    struct StateInfo
    {
        /** 'true' iff the state is the goal gathering the two orientations of the bin. */
        bool meta = false;

        /** 'true' iff the state is a sub-plate (N, or P with an unlimited number of stages). */
        bool node = true;

        /** Orientation of the cuts of the state (0: vertical, 1: horizontal). */
        Counter orientation = 0;

        /** Number of stages left. */
        Counter stages = 0;

        Counter ix = 0;

        Counter iy = 0;
    };

    Hypergraph(
            const Instance& instance,
            const std::vector<ItemPos>& copies):
        instance_(instance),
        copies_(copies)
    {
        const BinType& bin_type = instance.bin_type(0);
        const auto& parameters = instance.parameters();
        cut_thickness_ = parameters.cut_thickness;
        staged_ = !instance.number_of_stages_unlimited();
        number_of_stages_ = parameters.number_of_stages;
        cut_type_ = parameters.cut_type;

        // The hard trims are cut, the soft trims aren't.
        x_offset_ = (bin_type.left_trim_type == TrimType::Hard)? bin_type.left_trim: 0;
        y_offset_ = (bin_type.bottom_trim_type == TrimType::Hard)? bin_type.bottom_trim: 0;
        Length real_width = bin_type.rect.w - x_offset_
            - ((bin_type.right_trim_type == TrimType::Hard)? bin_type.right_trim: 0);
        Length real_height = bin_type.rect.h - y_offset_
            - ((bin_type.top_trim_type == TrimType::Hard)? bin_type.top_trim: 0);
        Length usable_width = bin_type.rect.w - bin_type.left_trim - bin_type.right_trim;
        Length usable_height = bin_type.rect.h - bin_type.bottom_trim - bin_type.top_trim;

        std::vector<Length> widths;
        std::vector<Length> heights;
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            if (copies_[item_type_id] == 0)
                continue;
            const ItemType& item_type = instance.item_type(item_type_id);
            widths.push_back(item_type.rect.w);
            heights.push_back(item_type.rect.h);
            if (!item_type.oriented) {
                widths.push_back(item_type.rect.h);
                heights.push_back(item_type.rect.w);
            }
        }
        x_ = compute_axis(widths, real_width, usable_width);
        y_ = compute_axis(heights, real_height, usable_height);

        // The item types of each position.
        x_.item_types.resize(x_.real.size());
        y_.item_types.resize(y_.real.size());
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            if (copies_[item_type_id] == 0)
                continue;
            const ItemType& item_type = instance.item_type(item_type_id);
            for (int rotation = 0; rotation < 2; ++rotation) {
                bool rotate = (rotation == 1);
                if (rotate && (item_type.oriented || item_type.rect.w == item_type.rect.h))
                    continue;
                Length width = item_type.width(rotate);
                Length height = item_type.height(rotate);
                if (width > usable_width || height > usable_height)
                    continue;
                Counter ix = x_.floor[width];
                if (x_.real[ix] == width)
                    x_.item_types[ix].push_back({item_type_id, rotate});
                Counter iy = y_.floor[height];
                if (y_.real[iy] == height)
                    y_.item_types[iy].push_back({item_type_id, rotate});
            }
        }

        number_of_cells_ = (StateId)x_.real.size() * (StateId)y_.real.size();
        if (staged_) {
            first_orientation_ = parameters.first_stage_orientation;
            number_of_states_ = 4 * (number_of_stages_ + 1) * number_of_cells_;
            if (first_orientation_ == CutOrientation::Any) {
                goal_ = number_of_states_;
                number_of_states_++;
            } else {
                Counter orientation = (first_orientation_ == CutOrientation::Vertical)? 0: 1;
                goal_ = id(true, orientation, number_of_stages_, x_.bin, y_.bin);
            }
        } else {
            number_of_states_ = number_of_cells_;
            goal_ = x_.bin + (StateId)x_.real.size() * y_.bin;
        }
    }

    StateId number_of_states() const { return number_of_states_; }

    StateId goal() const { return goal_; }

    bool staged() const { return staged_; }

    const Axis& x() const { return x_; }

    const Axis& y() const { return y_; }

    Length cut_thickness() const { return cut_thickness_; }

    Length x_offset() const { return x_offset_; }

    Length y_offset() const { return y_offset_; }

    CutOrientation first_orientation() const { return first_orientation_; }

    StateInfo info(StateId state_id) const
    {
        StateInfo info;
        if (staged_ && state_id == number_of_states_ - 1
                && first_orientation_ == CutOrientation::Any) {
            info.meta = true;
            return info;
        }
        StateId cell = state_id % number_of_cells_;
        info.ix = cell % x_.real.size();
        info.iy = cell / x_.real.size();
        if (staged_) {
            StateId rest = state_id / number_of_cells_;
            info.orientation = rest % 2;
            info.node = ((rest / 2) % 2 == 1);
            info.stages = rest / 4;
        }
        return info;
    }

    /**
     * Call 'function(k, s1, s2)' for each hyperarc of target 'state_id',
     * where 'k' is the index of the hyperarc among the hyperarcs of target
     * 'state_id', and 's1' and 's2' are its sources ('s2 == -1' for a
     * hyperarc with a single source).
     */
    template <typename Function>
    void for_each_hyperarc(
            StateId state_id,
            Function function) const
    {
        StateInfo info = this->info(state_id);
        Counter k = 0;
        if (info.meta) {
            function(0, id(true, 0, number_of_stages_, x_.bin, y_.bin), (StateId)-1);
            function(1, id(true, 1, number_of_stages_, x_.bin, y_.bin), (StateId)-1);
            return;
        }
        if (!staged_) {
            // A cut whose first part is larger than the rest is dominated by
            // the cut at the rest, whose rest is at least the first part: a
            // larger rectangle has more patterns.
            // Vertical cuts.
            Length width = x_.usable[info.ix];
            for (Counter ic = 1; ic < (Counter)x_.real.size(); ++ic) {
                Length rest = width - x_.real[ic] - cut_thickness_;
                if (rest <= 0)
                    break;
                Counter irest = x_.floor[rest];
                if (x_.real[ic] > x_.real[irest])
                    break;
                function(k, cell_id(ic, info.iy), cell_id(irest, info.iy));
                ++k;
            }
            // Horizontal cuts.
            Length height = y_.usable[info.iy];
            for (Counter ic = 1; ic < (Counter)y_.real.size(); ++ic) {
                Length rest = height - y_.real[ic] - cut_thickness_;
                if (rest <= 0)
                    break;
                Counter irest = y_.floor[rest];
                if (y_.real[ic] > y_.real[irest])
                    break;
                function(k, cell_id(info.ix, ic), cell_id(info.ix, irest));
                ++k;
            }
            return;
        }
        if (info.stages == 0)
            return;
        if (info.node) {
            function(0, id(false, info.orientation, info.stages, info.ix, info.iy), (StateId)-1);
            return;
        }
        // The strips of a part of a sub-plate.
        const Axis& axis = (info.orientation == 0)? x_: y_;
        Counter index = (info.orientation == 0)? info.ix: info.iy;
        Length length = axis.usable[index];
        Counter ic_max = axis.floor[length];
        for (Counter ic = 1; ic <= ic_max; ++ic) {
            // With any orientation of the first stage, a vertical first stage
            // with a single strip identical to the bin is dominated by the
            // horizontal first stage, which saves a stage (the horizontal
            // first stage keeps it, for an item identical to the bin).
            if (first_orientation_ == CutOrientation::Any
                    && info.orientation == 0
                    && info.stages == number_of_stages_
                    && index == axis.bin
                    && axis.real[ic] == axis.real[index]) {
                continue;
            }
            StateId strip_id = (info.orientation == 0)?
                id(true, 1, info.stages - 1, ic, info.iy):
                id(true, 0, info.stages - 1, info.ix, ic);
            Length rest = length - axis.real[ic] - cut_thickness_;
            Counter irest = (rest > 0)? axis.floor[rest]: 0;
            if (irest == 0) {
                function(k, strip_id, (StateId)-1);
            } else {
                StateId rest_id = (info.orientation == 0)?
                    id(false, 0, info.stages, irest, info.iy):
                    id(false, 1, info.stages, info.ix, irest);
                function(k, strip_id, rest_id);
            }
            ++k;
        }
    }

    /**
     * Call 'function(k, leaf)' for each leaf of state 'state_id', where 'k'
     * is the index of the leaf among the leaves of the state. The first leaf
     * is the waste.
     */
    template <typename Function>
    void for_each_leaf(
            StateId state_id,
            Function function) const
    {
        StateInfo info = this->info(state_id);
        if (info.meta)
            return;
        Counter k = 0;
        Leaf leaf;
        function(k, leaf);
        ++k;
        if (!staged_) {
            Length width = x_.usable[info.ix];
            Length height = y_.usable[info.iy];
            for (ItemTypeId item_type_id = 0;
                    item_type_id < instance_.number_of_item_types();
                    ++item_type_id) {
                if (copies_[item_type_id] == 0)
                    continue;
                const ItemType& item_type = instance_.item_type(item_type_id);
                for (int rotation = 0; rotation < 2; ++rotation) {
                    bool rotate = (rotation == 1);
                    if (rotate && item_type.oriented)
                        continue;
                    if (item_type.width(rotate) <= width
                            && item_type.height(rotate) <= height) {
                        leaf.number_of_items = 1;
                        leaf.item_type_ids[0] = item_type_id;
                        leaf.rotate[0] = rotate;
                        function(k, leaf);
                        ++k;
                        break;
                    }
                }
            }
            return;
        }
        if (!info.node)
            return;
        Length width = x_.real[info.ix];
        Length height = y_.real[info.iy];
        // An item filling the sub-plate exactly. The bin itself isn't an
        // item.
        if (info.stages < number_of_stages_) {
            for (auto item: x_.item_types[info.ix]) {
                if (instance_.item_type(item.first).height(item.second) != height)
                    continue;
                leaf.number_of_items = 1;
                leaf.item_type_ids[0] = item.first;
                leaf.rotate[0] = item.second;
                function(k, leaf);
                ++k;
            }
        }
        // The items separated by the extra cut.
        if (info.stages == 0
                && (cut_type_ == CutType::NonExact
                    || cut_type_ == CutType::Roadef2018)) {
            // The items whose dimension along the other orientation is the
            // dimension of the sub-plate.
            bool vertical = (info.orientation == 0);
            const auto& items = (vertical)?
                y_.item_types[info.iy]:
                x_.item_types[info.ix];
            Length length = (vertical)? width: height;
            Length usable = (vertical)? x_.usable[info.ix]: y_.usable[info.iy];
            auto item_length = [this, vertical](const std::pair<ItemTypeId, bool>& item)
            {
                const ItemType& item_type = instance_.item_type(item.first);
                return (vertical)?
                    item_type.width(item.second):
                    item_type.height(item.second);
            };
            for (Counter pos_1 = 0; pos_1 < (Counter)items.size(); ++pos_1) {
                Length length_1 = item_length(items[pos_1]);
                if (length_1 >= length || length_1 > usable)
                    continue;
                leaf.number_of_items = 1;
                leaf.item_type_ids[0] = items[pos_1].first;
                leaf.rotate[0] = items[pos_1].second;
                function(k, leaf);
                ++k;
                if (cut_type_ != CutType::Roadef2018)
                    continue;
                for (Counter pos_2 = pos_1; pos_2 < (Counter)items.size(); ++pos_2) {
                    Length length_2 = length_1 + cut_thickness_ + item_length(items[pos_2]);
                    if (length_2 > usable)
                        continue;
                    leaf.number_of_items = 2;
                    leaf.item_type_ids[1] = items[pos_2].first;
                    leaf.rotate[1] = items[pos_2].second;
                    function(k, leaf);
                    ++k;
                }
            }
        }
    }

    /** Get the hyperarc 'k' of target 'state_id'. */
    std::pair<StateId, StateId> hyperarc(
            StateId state_id,
            Counter k) const
    {
        std::pair<StateId, StateId> sources = {-1, -1};
        for_each_hyperarc(
                state_id,
                [&sources, k](Counter k_cur, StateId s1, StateId s2)
                {
                    if (k_cur == k)
                        sources = {s1, s2};
                });
        return sources;
    }

    /** Get the leaf 'k' of state 'state_id'. */
    Leaf leaf(
            StateId state_id,
            Counter k) const
    {
        Leaf leaf;
        for_each_leaf(
                state_id,
                [&leaf, k](Counter k_cur, const Leaf& leaf_cur)
                {
                    if (k_cur == k)
                        leaf = leaf_cur;
                });
        return leaf;
    }

private:

    Axis compute_axis(
            const std::vector<Length>& dimensions,
            Length real_length,
            Length usable_length) const
    {
        NormalPositions normal_positions = compute_normal_positions(
                dimensions,
                usable_length,
                cut_thickness_);
        if (!staged_) {
            normal_positions = compute_raster_points(
                    normal_positions,
                    usable_length,
                    cut_thickness_);
        }
        Axis axis;
        axis.real = normal_positions.positions;
        axis.usable = normal_positions.positions;
        axis.floor = normal_positions.floor;
        axis.bin = normal_positions.floor[usable_length];
        // With a limited number of stages, the dimension of the bin isn't
        // rounded down.
        if (staged_ && axis.real[axis.bin] != real_length) {
            axis.real.push_back(real_length);
            axis.usable.push_back(usable_length);
            axis.bin = axis.real.size() - 1;
        }
        return axis;
    }

    StateId cell_id(Counter ix, Counter iy) const
    {
        return ix + (StateId)x_.real.size() * iy;
    }

    StateId id(
            bool node,
            Counter orientation,
            Counter stages,
            Counter ix,
            Counter iy) const
    {
        return ((StateId)stages * 4 + (node? 2: 0) + orientation) * number_of_cells_
            + cell_id(ix, iy);
    }

    const Instance& instance_;

    const std::vector<ItemPos>& copies_;

    Length cut_thickness_;

    bool staged_;

    Counter number_of_stages_;

    CutType cut_type_;

    CutOrientation first_orientation_ = CutOrientation::Vertical;

    Length x_offset_;

    Length y_offset_;

    Axis x_;

    Axis y_;

    StateId number_of_cells_;

    StateId number_of_states_;

    StateId goal_;

};

////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////// Labels ////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/*
 * The labels are vectors of numbers of copies of the item types, packed into
 * 64-bit words, with one more bit than needed for the number of copies of
 * each item type. The sum of two labels is computed with an addition of the
 * words, and it is feasible iff adding the complement of the numbers of
 * copies doesn't set any of these extra bits.
 */
class Encoder
{

public:

    Encoder(const std::vector<ItemPos>& copies)
    {
        Counter offset = 64;
        for (ItemPos item_copies: copies) {
            Counter bits = 1;
            while (((ItemPos)1 << bits) <= item_copies)
                ++bits;
            if (offset + bits + 1 > 64) {
                remainders_.push_back(0);
                masks_.push_back(0);
                offset = 0;
            }
            Counter word = remainders_.size() - 1;
            words_.push_back(word);
            shifts_.push_back(offset);
            bits_.push_back(bits);
            remainders_[word] |= (((uint64_t)1 << bits) - 1 - item_copies) << offset;
            masks_[word] |= (uint64_t)1 << (offset + bits);
            offset += bits + 1;
        }
        if (remainders_.empty()) {
            remainders_.push_back(0);
            masks_.push_back(0);
        }
    }

    Counter number_of_words() const { return remainders_.size(); }

    /** Add item type 'item_type_id' to a label; return 'false' if infeasible. */
    bool add_item(
            uint64_t* label,
            ItemTypeId item_type_id) const
    {
        Counter word = words_[item_type_id];
        uint64_t value = label[word] + ((uint64_t)1 << shifts_[item_type_id]);
        if ((value + remainders_[word]) & masks_[word])
            return false;
        label[word] = value;
        return true;
    }

    /** 'out = label_1 + label_2'; return 'false' if infeasible. */
    bool add(
            const uint64_t* label_1,
            const uint64_t* label_2,
            uint64_t* out) const
    {
        for (Counter word = 0; word < number_of_words(); ++word) {
            uint64_t value = label_1[word] + label_2[word];
            if ((value + remainders_[word]) & masks_[word])
                return false;
            out[word] = value;
        }
        return true;
    }

    ItemPos get(
            const uint64_t* label,
            ItemTypeId item_type_id) const
    {
        return (label[words_[item_type_id]] >> shifts_[item_type_id])
            & (((uint64_t)1 << bits_[item_type_id]) - 1);
    }

private:

    std::vector<Counter> words_;

    std::vector<Counter> shifts_;

    std::vector<Counter> bits_;

    std::vector<uint64_t> remainders_;

    std::vector<uint64_t> masks_;

};

/*
 * The origin of a label.
 *
 * For a forward label: 'a' is the index of the hyperarc ('a >= 0') or of the
 * leaf ('-1 - a') which produced it, 'b' and 'c' the indices of the labels of
 * its sources.
 *
 * For a backward label: 'a' is the target of the hyperarc which produced it
 * ('-1' for the empty backward label of the goal), 'b' the index of the
 * hyperarc among the hyperarcs of 'a', 'c' the position of the state among
 * the sources of the hyperarc, 'd' the index of the backward label of the
 * target and 'e' the index of the forward label of the other source ('-1' for
 * a hyperarc with a single source).
 */
struct Pred
{
    int32_t a = -1;
    int32_t b = -1;
    int32_t c = -1;
    int32_t d = -1;
    int32_t e = -1;
};

/** Labels sorted by decreasing profit. */
struct LabelSet
{
    Counter number_of_words = 0;

    std::vector<uint64_t> words;

    std::vector<Profit> profits;

    std::vector<Pred> preds;

    Counter size() const { return profits.size(); }

    const uint64_t* label(Counter pos) const { return &words[pos * number_of_words]; }
};

/** A set of labels without duplicates. */
class LabelTable
{

public:

    LabelTable(Counter number_of_words):
        number_of_words_(number_of_words),
        buckets_(16, -1) { }

    Counter size() const { return profits_.size(); }

    /** Insert a label; return 'false' if it was already in the table. */
    bool insert(
            const uint64_t* label,
            Profit profit,
            const Pred& pred)
    {
        if (2 * (size() + 1) > (Counter)buckets_.size())
            rehash(2 * buckets_.size());
        uint64_t mask = buckets_.size() - 1;
        uint64_t bucket = hash(label) & mask;
        for (;;) {
            int32_t pos = buckets_[bucket];
            if (pos == -1)
                break;
            if (std::equal(label, label + number_of_words_, &words_[pos * number_of_words_]))
                return false;
            bucket = (bucket + 1) & mask;
        }
        buckets_[bucket] = size();
        words_.insert(words_.end(), label, label + number_of_words_);
        profits_.push_back(profit);
        preds_.push_back(pred);
        return true;
    }

    /** Return the labels sorted by decreasing profit, and clear the table. */
    LabelSet extract()
    {
        LabelSet label_set;
        label_set.number_of_words = number_of_words_;
        std::vector<Counter> order(size());
        std::iota(order.begin(), order.end(), 0);
        std::sort(
                order.begin(),
                order.end(),
                [this](Counter pos_1, Counter pos_2)
                {
                    return profits_[pos_1] > profits_[pos_2];
                });
        label_set.words.reserve(words_.size());
        label_set.profits.reserve(size());
        label_set.preds.reserve(size());
        for (Counter pos: order) {
            label_set.words.insert(
                    label_set.words.end(),
                    &words_[pos * number_of_words_],
                    &words_[(pos + 1) * number_of_words_]);
            label_set.profits.push_back(profits_[pos]);
            label_set.preds.push_back(preds_[pos]);
        }
        clear();
        return label_set;
    }

    void clear()
    {
        words_.clear();
        profits_.clear();
        preds_.clear();
        if (buckets_.size() > 16)
            buckets_ = std::vector<int32_t>(16, -1);
        else
            std::fill(buckets_.begin(), buckets_.end(), -1);
    }

private:

    uint64_t hash(const uint64_t* label) const
    {
        uint64_t h = 0x9E3779B97F4A7C15ULL;
        for (Counter word = 0; word < number_of_words_; ++word) {
            uint64_t z = label[word] + h;
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
            h = z ^ (z >> 31);
        }
        return h;
    }

    void rehash(Counter number_of_buckets)
    {
        buckets_.assign(number_of_buckets, -1);
        uint64_t mask = number_of_buckets - 1;
        for (Counter pos = 0; pos < size(); ++pos) {
            uint64_t bucket = hash(&words_[pos * number_of_words_]) & mask;
            while (buckets_[bucket] != -1)
                bucket = (bucket + 1) & mask;
            buckets_[bucket] = pos;
        }
    }

    Counter number_of_words_;

    std::vector<uint64_t> words_;

    std::vector<Profit> profits_;

    std::vector<Pred> preds_;

    std::vector<int32_t> buckets_;

};

/*
 * Call 'function(pos_1, pos_2, profit)' for each pair of labels of
 * 'label_set_1' and 'label_set_2' whose sum is feasible and whose profit is
 * at least 'threshold'; the sum is written in 'out'.
 *
 * Return 'false' if interrupted by the timer.
 */
template <typename Function>
bool merge(
        const Encoder& encoder,
        const LabelSet& label_set_1,
        const LabelSet& label_set_2,
        Profit threshold,
        uint64_t* out,
        const optimizationtools::Timer& timer,
        Function function)
{
    if (label_set_1.size() == 0 || label_set_2.size() == 0)
        return true;
    Profit profit_2_max = label_set_2.profits[0];
    Counter number_of_pairs = 0;
    for (Counter pos_1 = 0; pos_1 < label_set_1.size(); ++pos_1) {
        Profit profit_1 = label_set_1.profits[pos_1];
        if (profit_1 + profit_2_max < threshold)
            break;
        const uint64_t* label_1 = label_set_1.label(pos_1);
        for (Counter pos_2 = 0; pos_2 < label_set_2.size(); ++pos_2) {
            Profit profit = profit_1 + label_set_2.profits[pos_2];
            if (profit < threshold)
                break;
            if (encoder.add(label_1, label_set_2.label(pos_2), out))
                function(pos_1, pos_2, profit);
        }
        number_of_pairs += label_set_2.size();
        if (number_of_pairs > (1 << 22)) {
            if (timer.needs_to_end())
                return false;
            number_of_pairs = 0;
        }
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////// Bidirectional search ////////////////////////////
////////////////////////////////////////////////////////////////////////////////

struct TreeSearchHypergraphData
{
    TreeSearchHypergraphData(
            const Instance& instance,
            const Hypergraph& hypergraph,
            const std::vector<ItemPos>& copies,
            const std::vector<Profit>& profits):
        instance(instance),
        hypergraph(hypergraph),
        copies(copies),
        encoder(copies),
        profits(profits),
        reduced_profits(profits)
    {
        zero.number_of_words = encoder.number_of_words();
        zero.words.assign(encoder.number_of_words(), 0);
        zero.profits.push_back(0);
        zero.preds.push_back(Pred());
    }

    const Instance& instance;

    const Hypergraph& hypergraph;

    /** Numbers of copies of the item types. */
    const std::vector<ItemPos>& copies;

    Encoder encoder;

    /** Profits of the item types. */
    const std::vector<Profit>& profits;

    /**
     * Reduced profits of the item types: their profits minus the multipliers
     * of the Lagrangian relaxation of their numbers of copies.
     */
    std::vector<Profit> reduced_profits;

    /**
     * Constant of the Lagrangian relaxation: the sum over the item types of
     * their multiplier times their number of copies. The profit of a pattern
     * is at most its reduced profit plus this constant.
     */
    Profit lagrangian_constant = 0;

    /**
     * For each state, the best reduced profit of a pattern of the state, the
     * numbers of copies of the item types being ignored.
     */
    std::vector<Profit> dp;

    /**
     * For each state, the best reduced profit of a completion of a pattern
     * of the state into a pattern of the bin, the numbers of copies of the
     * item types being ignored.
     */
    std::vector<Profit> rdp;

    /** For each state, the index of its set of forward labels (-1 if empty). */
    std::vector<int32_t> forward;

    /** For each state, the index of its set of backward labels (-1 if empty). */
    std::vector<int32_t> backward;

    std::vector<LabelSet> forward_sets;

    std::vector<LabelSet> backward_sets;

    /** The set containing only the empty label. */
    LabelSet zero;

    /** The empty set. */
    LabelSet empty;
};

const LabelSet& forward_set(
        const TreeSearchHypergraphData& data,
        StateId state_id)
{
    return (data.forward[state_id] == -1)?
        data.empty:
        data.forward_sets[data.forward[state_id]];
}

Profit leaf_profit(
        const TreeSearchHypergraphData& data,
        const Leaf& leaf)
{
    Profit profit = 0;
    for (Counter pos = 0; pos < leaf.number_of_items; ++pos)
        profit += data.reduced_profits[leaf.item_type_ids[pos]];
    return profit;
}

bool encode_leaf(
        const TreeSearchHypergraphData& data,
        const Leaf& leaf,
        uint64_t* label)
{
    std::fill(label, label + data.encoder.number_of_words(), 0);
    for (Counter pos = 0; pos < leaf.number_of_items; ++pos)
        if (!data.encoder.add_item(label, leaf.item_type_ids[pos]))
            return false;
    return true;
}

/**
 * Compute 'dp'.
 *
 * Return 'false' if interrupted.
 */
bool compute_dp(
        TreeSearchHypergraphData& data,
        const optimizationtools::Timer& timer)
{
    const Hypergraph& hypergraph = data.hypergraph;
    StateId number_of_states = hypergraph.number_of_states();
    data.dp.assign(number_of_states, 0);
    for (StateId state_id = 0; state_id < number_of_states; ++state_id) {
        if (state_id % 1024 == 0 && timer.needs_to_end())
            return false;
        Profit value = 0;
        hypergraph.for_each_leaf(
                state_id,
                [&data, &value](Counter, const Leaf& leaf)
                {
                    value = std::max(value, leaf_profit(data, leaf));
                });
        hypergraph.for_each_hyperarc(
                state_id,
                [&data, &value](Counter, StateId s1, StateId s2)
                {
                    value = std::max(value, data.dp[s1] + ((s2 >= 0)? data.dp[s2]: 0));
                });
        data.dp[state_id] = value;
    }
    return true;
}

/**
 * Compute 'rdp' (once 'dp' is computed).
 *
 * Return 'false' if interrupted.
 */
bool compute_rdp(
        TreeSearchHypergraphData& data,
        const optimizationtools::Timer& timer)
{
    const Hypergraph& hypergraph = data.hypergraph;
    StateId number_of_states = hypergraph.number_of_states();
    data.rdp.assign(number_of_states, -std::numeric_limits<Profit>::infinity());
    data.rdp[hypergraph.goal()] = 0;
    for (StateId state_id = number_of_states - 1; state_id >= 0; --state_id) {
        if (state_id % 1024 == 0 && timer.needs_to_end())
            return false;
        Profit value = data.rdp[state_id];
        if (value == -std::numeric_limits<Profit>::infinity())
            continue;
        hypergraph.for_each_hyperarc(
                state_id,
                [&data, value](Counter, StateId s1, StateId s2)
                {
                    data.rdp[s1] = std::max(data.rdp[s1], value + ((s2 >= 0)? data.dp[s2]: 0));
                    if (s2 >= 0)
                        data.rdp[s2] = std::max(data.rdp[s2], value + data.dp[s1]);
                });
    }
    return true;
}

/**
 * The numbers of copies of the item types of a pattern of the bin whose
 * reduced profit is 'dp' of the goal (the numbers of copies of the item
 * types being ignored).
 */
std::vector<ItemPos> compute_dp_pattern(const TreeSearchHypergraphData& data)
{
    const Hypergraph& hypergraph = data.hypergraph;
    std::vector<ItemPos> pattern(data.instance.number_of_item_types(), 0);
    std::vector<StateId> stack = {hypergraph.goal()};
    while (!stack.empty()) {
        StateId state_id = stack.back();
        stack.pop_back();
        Profit value = data.dp[state_id];
        bool found = false;
        hypergraph.for_each_leaf(
                state_id,
                [&data, &pattern, &found, value](Counter, const Leaf& leaf)
                {
                    if (found || leaf_profit(data, leaf) != value)
                        return;
                    for (Counter pos = 0; pos < leaf.number_of_items; ++pos)
                        pattern[leaf.item_type_ids[pos]]++;
                    found = true;
                });
        hypergraph.for_each_hyperarc(
                state_id,
                [&data, &stack, &found, value](Counter, StateId s1, StateId s2)
                {
                    if (found || data.dp[s1] + ((s2 >= 0)? data.dp[s2]: 0) != value)
                        return;
                    stack.push_back(s1);
                    if (s2 >= 0)
                        stack.push_back(s2);
                    found = true;
                });
    }
    return pattern;
}

/*
 * Pricing solver of the linear relaxation of the flow formulation of the
 * hypergraph (Léonard and Clautiaux, 2026):
 *   max sum_p profit(p) x_p
 *   sum_p n_i(p) x_p <= copies_i    for each item type i (dual: lambda_i)
 *   sum_p x_p <= 1                                       (dual: mu)
 * where 'p' ranges over the patterns of the bin with any number of copies of
 * the item types. The pricing problem is the dynamic program 'dp' with the
 * reduced profits 'profit_i - lambda_i'.
 *
 * For any multipliers 'lambda >= 0', 'dp(goal) + sum_i lambda_i copies_i' is
 * a bound on the profit of the patterns of the bin: the multipliers giving
 * the best bound are kept.
 */
class TreeSearchHypergraphPricingSolver: public columngenerationsolver::PricingSolver
{

public:

    TreeSearchHypergraphPricingSolver(
            TreeSearchHypergraphData& data,
            const optimizationtools::Timer& timer):
        data_(data),
        timer_(timer) { }

    virtual PricingOutput solve_pricing(
            bool solve_feasibility,
            const std::vector<std::pair<std::shared_ptr<const columngenerationsolver::Column>, columngenerationsolver::Value>>&,
            const std::vector<std::shared_ptr<const columngenerationsolver::BranchingDecision>>&,
            const std::unordered_set<std::shared_ptr<const columngenerationsolver::Column>>&,
            const std::vector<columngenerationsolver::Value>& duals,
            const std::vector<std::pair<std::shared_ptr<const columngenerationsolver::Cut>, columngenerationsolver::Value>>&,
            PricingType) override
    {
        PricingOutput output;
        ItemTypeId number_of_item_types = data_.instance.number_of_item_types();
        std::vector<Profit> multipliers(number_of_item_types);
        Profit lagrangian_constant = 0;
        for (ItemTypeId item_type_id = 0;
                item_type_id < number_of_item_types;
                ++item_type_id) {
            multipliers[item_type_id] = std::max(0.0, duals[item_type_id]);
            data_.reduced_profits[item_type_id]
                = ((solve_feasibility)? 0: data_.profits[item_type_id])
                - multipliers[item_type_id];
            lagrangian_constant += multipliers[item_type_id] * data_.copies[item_type_id];
        }
        if (!compute_dp(data_, timer_))
            return output;
        Profit value = data_.dp[data_.hypergraph.goal()];
        output.overcost = value - duals[number_of_item_types];
        if (!solve_feasibility && value + lagrangian_constant < bound_) {
            bound_ = value + lagrangian_constant;
            multipliers_ = multipliers;
        }
        if (output.overcost <= 1e-9 * std::max(1.0, std::abs(value)))
            return output;

        std::vector<ItemPos> pattern = compute_dp_pattern(data_);
        auto column = std::make_shared<columngenerationsolver::Column>();
        column->type = columngenerationsolver::VariableType::Continuous;
        for (ItemTypeId item_type_id = 0;
                item_type_id < number_of_item_types;
                ++item_type_id) {
            if (pattern[item_type_id] == 0)
                continue;
            column->objective_coefficient += pattern[item_type_id] * data_.profits[item_type_id];
            columngenerationsolver::LinearTerm element;
            element.row = item_type_id;
            element.coefficient = pattern[item_type_id];
            column->elements.push_back(element);
        }
        columngenerationsolver::LinearTerm element;
        element.row = number_of_item_types;
        element.coefficient = 1;
        column->elements.push_back(element);
        output.columns.push_back(column);
        return output;
    }

    /** Best bound found. */
    Profit bound() const { return bound_; }

    /** Multipliers of the best bound found (empty if none). */
    const std::vector<Profit>& multipliers() const { return multipliers_; }

private:

    TreeSearchHypergraphData& data_;

    const optimizationtools::Timer& timer_;

    Profit bound_ = std::numeric_limits<Profit>::infinity();

    std::vector<Profit> multipliers_;

};

/**
 * Compute the multipliers of the Lagrangian relaxation of the numbers of
 * copies from the linear relaxation of the flow formulation, and set the
 * reduced profits and the Lagrangian constant accordingly. Without
 * multipliers (if interrupted), the reduced profits are the profits.
 */
void compute_multipliers(
        TreeSearchHypergraphData& data,
        const TreeSearchHypergraphParameters& parameters)
{
    ItemTypeId number_of_item_types = data.instance.number_of_item_types();
    const BinType& bin_type = data.instance.bin_type(0);
    Area area = (Area)(bin_type.rect.w - bin_type.left_trim - bin_type.right_trim)
        * (bin_type.rect.h - bin_type.bottom_trim - bin_type.top_trim);

    columngenerationsolver::Model model;
    model.objective_sense = optimizationtools::ObjectiveDirection::Maximize;
    for (ItemTypeId item_type_id = 0;
            item_type_id < number_of_item_types;
            ++item_type_id) {
        columngenerationsolver::Row row;
        row.lower_bound = 0;
        row.upper_bound = data.copies[item_type_id];
        row.coefficient_lower_bound = 0;
        row.coefficient_upper_bound = area / data.instance.item_type(item_type_id).area();
        model.rows.push_back(row);
    }
    columngenerationsolver::Row row;
    row.lower_bound = 0;
    row.upper_bound = 1;
    row.coefficient_lower_bound = 0;
    row.coefficient_upper_bound = 1;
    model.rows.push_back(row);
    auto pricing_solver = new TreeSearchHypergraphPricingSolver(data, parameters.timer);
    model.pricing_solver = std::unique_ptr<columngenerationsolver::PricingSolver>(pricing_solver);

    columngenerationsolver::ColumnGenerationParameters column_generation_parameters;
    column_generation_parameters.verbosity_level = 0;
    column_generation_parameters.timer = parameters.timer;
    column_generation_parameters.solver_name = parameters.linear_programming_solver_name;
    columngenerationsolver::column_generation(model, column_generation_parameters);

    data.lagrangian_constant = 0;
    if (pricing_solver->multipliers().empty()) {
        data.reduced_profits = data.profits;
        return;
    }
    for (ItemTypeId item_type_id = 0;
            item_type_id < number_of_item_types;
            ++item_type_id) {
        Profit multiplier = pricing_solver->multipliers()[item_type_id];
        data.reduced_profits[item_type_id] = data.profits[item_type_id] - multiplier;
        data.lagrangian_constant += multiplier * data.copies[item_type_id];
    }
}

/*
 * A node of the tree of a pattern: a state, and the hyperarc ('choice >= 0')
 * or the leaf ('-1 - choice') which decomposes it.
 */
struct TreeNode
{
    StateId state_id;

    Counter choice = -1;

    Counter children[2] = {-1, -1};
};

/** The best pattern found by the merge phase. */
struct Best
{
    /** Target of the hyperarc, or state of the leaf. */
    StateId state_id = -1;

    /** Hyperarc ('choice >= 0') or leaf ('-1 - choice'). */
    Counter choice = -1;

    /** Indices of the forward labels of the sources of the hyperarc. */
    Counter forward_pos_1 = -1;

    Counter forward_pos_2 = -1;

    /** Index of the backward label of the state. */
    Counter backward_pos = -1;

    /** Profit (not reduced). */
    Profit profit = -std::numeric_limits<Profit>::infinity();
};

void update_best(
        const TreeSearchHypergraphData& data,
        Best& best,
        const uint64_t* label,
        StateId state_id,
        Counter choice,
        Counter forward_pos_1,
        Counter forward_pos_2,
        Counter backward_pos)
{
    Profit profit = 0;
    for (ItemTypeId item_type_id = 0;
            item_type_id < data.instance.number_of_item_types();
            ++item_type_id) {
        profit += data.encoder.get(label, item_type_id) * data.profits[item_type_id];
    }
    if (profit <= best.profit)
        return;
    best.state_id = state_id;
    best.choice = choice;
    best.forward_pos_1 = forward_pos_1;
    best.forward_pos_2 = forward_pos_2;
    best.backward_pos = backward_pos;
    best.profit = profit;
}

/** Add the tree of a forward label; return its root. */
Counter expand_forward(
        const TreeSearchHypergraphData& data,
        std::vector<TreeNode>& tree,
        StateId state_id,
        Counter pos)
{
    const Pred& pred = data.forward_sets[data.forward[state_id]].preds[pos];
    Counter node_id = tree.size();
    tree.push_back(TreeNode());
    tree[node_id].state_id = state_id;
    tree[node_id].choice = pred.a;
    if (pred.a >= 0) {
        auto sources = data.hypergraph.hyperarc(state_id, pred.a);
        Counter child_1 = expand_forward(data, tree, sources.first, pred.b);
        tree[node_id].children[0] = child_1;
        if (sources.second >= 0) {
            Counter child_2 = expand_forward(data, tree, sources.second, pred.c);
            tree[node_id].children[1] = child_2;
        }
    }
    return node_id;
}

/**
 * Add the tree of a backward label; return its root and the node of the
 * state of the label (the hole of the tree).
 */
std::pair<Counter, Counter> expand_backward(
        const TreeSearchHypergraphData& data,
        std::vector<TreeNode>& tree,
        StateId state_id,
        Counter pos)
{
    const Pred& pred = data.backward_sets[data.backward[state_id]].preds[pos];
    if (pred.a == -1) {
        Counter node_id = tree.size();
        tree.push_back(TreeNode());
        tree[node_id].state_id = state_id;
        return {node_id, node_id};
    }
    StateId target_id = pred.a;
    auto root_and_target = expand_backward(data, tree, target_id, pred.d);
    Counter root = root_and_target.first;
    Counter target_node_id = root_and_target.second;
    tree[target_node_id].choice = pred.b;
    auto sources = data.hypergraph.hyperarc(target_id, pred.b);
    Counter node_id = tree.size();
    tree.push_back(TreeNode());
    tree[node_id].state_id = state_id;
    tree[target_node_id].children[pred.c] = node_id;
    if (pred.e >= 0) {
        StateId other = (pred.c == 0)? sources.second: sources.first;
        Counter other_node_id = expand_forward(data, tree, other, pred.e);
        tree[target_node_id].children[1 - pred.c] = other_node_id;
    }
    return {root, node_id};
}

struct SearchOutput
{
    /** 'true' iff the search was interrupted by the time limit. */
    bool interrupted = false;

    /** 'true' iff a pattern was found. */
    bool found = false;

    /** Profit (not reduced) of the best pattern found. */
    Profit profit = 0;

    /** Tree of the best pattern found; its root is the first node. */
    std::vector<TreeNode> tree;

    Counter number_of_forward_labels = 0;

    Counter number_of_backward_labels = 0;
};

/*
 * Forward pass: the forward labels of the states, in topological order, with
 * a reduced profit at least '(threshold + dp - rdp) / 2'.
 *
 * Return 'false' if interrupted.
 */
bool forward_pass(
        TreeSearchHypergraphData& data,
        Profit threshold,
        const optimizationtools::Timer& timer,
        SearchOutput& output)
{
    const Hypergraph& hypergraph = data.hypergraph;
    StateId number_of_states = hypergraph.number_of_states();
    Counter number_of_words = data.encoder.number_of_words();
    std::vector<uint64_t> buffer(number_of_words);
    const Profit infinity = std::numeric_limits<Profit>::infinity();
    LabelTable table(number_of_words);
    for (StateId state_id = 0; state_id < number_of_states; ++state_id) {
        if (state_id % 64 == 0 && timer.needs_to_end())
            return false;
        if (data.rdp[state_id] == -infinity
                || data.dp[state_id] + data.rdp[state_id] < threshold) {
            continue;
        }
        Profit state_threshold = (threshold + data.dp[state_id] - data.rdp[state_id]) / 2;
        hypergraph.for_each_leaf(
                state_id,
                [&data, &table, &buffer, state_threshold](
                    Counter k,
                    const Leaf& leaf)
                {
                    if (!encode_leaf(data, leaf, buffer.data()))
                        return;
                    Profit profit = leaf_profit(data, leaf);
                    if (profit < state_threshold)
                        return;
                    Pred pred;
                    pred.a = -1 - k;
                    table.insert(buffer.data(), profit, pred);
                });
        bool interrupted = false;
        hypergraph.for_each_hyperarc(
                state_id,
                [&data, &table, &buffer, &timer, &interrupted, state_threshold](
                    Counter k,
                    StateId s1,
                    StateId s2)
                {
                    if (interrupted
                            || data.dp[s1] + ((s2 >= 0)? data.dp[s2]: 0) < state_threshold) {
                        return;
                    }
                    interrupted = !merge(
                            data.encoder,
                            forward_set(data, s1),
                            (s2 >= 0)? forward_set(data, s2): data.zero,
                            state_threshold,
                            buffer.data(),
                            timer,
                            [&table, &buffer, k](
                                Counter pos_1,
                                Counter pos_2,
                                Profit profit)
                            {
                                Pred pred;
                                pred.a = k;
                                pred.b = pos_1;
                                pred.c = pos_2;
                                table.insert(buffer.data(), profit, pred);
                            });
                });
        if (interrupted)
            return false;
        if (table.size() > 0) {
            output.number_of_forward_labels += table.size();
            data.forward[state_id] = data.forward_sets.size();
            data.forward_sets.push_back(table.extract());
        }
    }
    return true;
}

/*
 * Backward pass: the backward labels of the states, in reverse topological
 * order, with a reduced profit at least '(threshold + rdp - dp) / 2'. The
 * backward labels of a state are pushed when the targets of its hyperarcs
 * are processed, so they are complete when it is processed.
 *
 * Return 'false' if interrupted.
 */
bool backward_pass(
        TreeSearchHypergraphData& data,
        Profit threshold,
        const optimizationtools::Timer& timer,
        SearchOutput& output)
{
    const Hypergraph& hypergraph = data.hypergraph;
    StateId number_of_states = hypergraph.number_of_states();
    Counter number_of_words = data.encoder.number_of_words();
    std::vector<uint64_t> buffer(number_of_words);
    std::vector<int32_t> tables(number_of_states, -1);
    std::vector<LabelTable> table_pool;
    std::vector<int32_t> free_tables;
    StateId goal = hypergraph.goal();
    if ((threshold + data.rdp[goal] - data.dp[goal]) / 2 <= 0) {
        data.backward[goal] = data.backward_sets.size();
        data.backward_sets.push_back(data.zero);
    }
    for (StateId state_id = number_of_states - 1; state_id >= 0; --state_id) {
        if (state_id % 64 == 0 && timer.needs_to_end())
            return false;
        if (tables[state_id] != -1) {
            LabelTable& state_table = table_pool[tables[state_id]];
            if (state_table.size() > 0) {
                output.number_of_backward_labels += state_table.size();
                data.backward[state_id] = data.backward_sets.size();
                data.backward_sets.push_back(state_table.extract());
            }
            free_tables.push_back(tables[state_id]);
            tables[state_id] = -1;
        }
        if (data.backward[state_id] == -1)
            continue;
        const LabelSet& backward_set = data.backward_sets[data.backward[state_id]];
        Profit rdp_target = data.rdp[state_id];
        bool interrupted = false;
        hypergraph.for_each_hyperarc(
                state_id,
                [&](Counter k, StateId s1, StateId s2)
                {
                    for (Counter pos = 0; pos < 2 && !interrupted; ++pos) {
                        StateId source = (pos == 0)? s1: s2;
                        StateId other = (pos == 0)? s2: s1;
                        if (source < 0 || (pos == 1 && s1 == s2))
                            continue;
                        Profit source_threshold = (threshold + data.rdp[source] - data.dp[source]) / 2;
                        if (rdp_target + ((other >= 0)? data.dp[other]: 0) < source_threshold)
                            continue;
                        const LabelSet& other_set = (other >= 0)? forward_set(data, other): data.zero;
                        if (other_set.size() == 0)
                            continue;
                        if (tables[source] == -1) {
                            if (free_tables.empty()) {
                                tables[source] = table_pool.size();
                                table_pool.emplace_back(number_of_words);
                            } else {
                                tables[source] = free_tables.back();
                                free_tables.pop_back();
                            }
                        }
                        LabelTable& source_table = table_pool[tables[source]];
                        interrupted = !merge(
                                data.encoder,
                                backward_set,
                                other_set,
                                source_threshold,
                                buffer.data(),
                                timer,
                                [&](Counter pos_1, Counter pos_2, Profit profit)
                                {
                                    Pred pred;
                                    pred.a = state_id;
                                    pred.b = k;
                                    pred.c = pos;
                                    pred.d = pos_1;
                                    pred.e = (other >= 0)? pos_2: -1;
                                    source_table.insert(buffer.data(), profit, pred);
                                });
                    }
                });
        if (interrupted)
            return false;
    }
    return true;
}

/*
 * The set of labels merged last at a hyperarc: the largest one among the
 * forward labels of its sources and the backward labels of its target (0 and
 * 1: the sources, 2: the target).
 */
Counter last_set(
        const LabelSet& forward_set_1,
        const LabelSet& forward_set_2,
        const LabelSet& backward_set)
{
    if (backward_set.size() >= forward_set_1.size()
            && backward_set.size() >= forward_set_2.size()) {
        return 2;
    }
    return (forward_set_1.size() >= forward_set_2.size())? 0: 1;
}

/*
 * Merge phase: for each hyperarc, the forward labels of its sources and the
 * backward labels of its target; for each leaf, the backward labels of its
 * state.
 *
 * The two smallest sets of a hyperarc are merged first. As in the
 * implementation of Léonard and Clautiaux, the merges of the two smallest
 * sets of all the hyperarcs whose largest set is the same are gathered in a
 * table, without duplicates, which is then merged with this set:
 * - when it is the set of backward labels of the target, the sums of the
 *   forward labels of the sources of its hyperarcs;
 * - when it is the set of forward labels of a source, the sums of the
 *   backward labels of the targets and of the forward labels of the other
 *   sources of its hyperarcs (backward labels of the source, pushed in the
 *   reverse order as in the backward pass).
 *
 * Return 'false' if interrupted.
 */
bool merge_phase(
        const TreeSearchHypergraphData& data,
        Profit threshold,
        const optimizationtools::Timer& timer,
        Best& best)
{
    const Hypergraph& hypergraph = data.hypergraph;
    StateId number_of_states = hypergraph.number_of_states();
    Counter number_of_words = data.encoder.number_of_words();
    std::vector<uint64_t> buffer(number_of_words);
    std::vector<uint64_t> leaf_label(number_of_words);
    LabelTable target_table(number_of_words);
    std::vector<int32_t> tables(number_of_states, -1);
    std::vector<LabelTable> table_pool;
    std::vector<int32_t> free_tables;
    for (StateId state_id = number_of_states - 1; state_id >= 0; --state_id) {
        if (state_id % 64 == 0 && timer.needs_to_end())
            return false;

        // The hyperarcs whose largest set is the set of forward labels of
        // this state.
        if (tables[state_id] != -1) {
            LabelTable& state_table = table_pool[tables[state_id]];
            LabelSet source_set = state_table.extract();
            free_tables.push_back(tables[state_id]);
            tables[state_id] = -1;
            bool ok = merge(
                    data.encoder,
                    source_set,
                    forward_set(data, state_id),
                    threshold,
                    buffer.data(),
                    timer,
                    [&](Counter pos_1, Counter pos_2, Profit)
                    {
                        const Pred& pred = source_set.preds[pos_1];
                        Counter pos_other = pred.e;
                        update_best(
                                data,
                                best,
                                buffer.data(),
                                pred.a,
                                pred.b,
                                (pred.c == 0)? pos_2: pos_other,
                                (pred.c == 0)? pos_other: pos_2,
                                pred.d);
                    });
            if (!ok)
                return false;
        }

        if (data.backward[state_id] == -1)
            continue;
        const LabelSet& backward_set = data.backward_sets[data.backward[state_id]];
        Profit backward_profit_max = backward_set.profits[0];

        // The leaves.
        hypergraph.for_each_leaf(
                state_id,
                [&](Counter k, const Leaf& leaf)
                {
                    if (!encode_leaf(data, leaf, leaf_label.data()))
                        return;
                    Profit profit = leaf_profit(data, leaf);
                    for (Counter pos = 0; pos < backward_set.size(); ++pos) {
                        if (profit + backward_set.profits[pos] < threshold)
                            break;
                        if (!data.encoder.add(leaf_label.data(), backward_set.label(pos), buffer.data()))
                            continue;
                        update_best(data, best, buffer.data(), state_id, -1 - k, -1, -1, pos);
                    }
                });

        bool interrupted = false;
        hypergraph.for_each_hyperarc(
                state_id,
                [&](Counter k, StateId s1, StateId s2)
                {
                    if (interrupted)
                        return;
                    const LabelSet& forward_set_1 = forward_set(data, s1);
                    const LabelSet& forward_set_2 = (s2 >= 0)? forward_set(data, s2): data.zero;
                    if (forward_set_1.size() == 0 || forward_set_2.size() == 0)
                        return;
                    if (forward_set_1.profits[0] + forward_set_2.profits[0] + backward_profit_max < threshold)
                        return;
                    Counter last = last_set(forward_set_1, forward_set_2, backward_set);
                    if (last == 2) {
                        // Gathered for this target.
                        interrupted = !merge(
                                data.encoder,
                                forward_set_1,
                                forward_set_2,
                                threshold - backward_profit_max,
                                buffer.data(),
                                timer,
                                [&](Counter pos_1, Counter pos_2, Profit profit)
                                {
                                    Pred pred;
                                    pred.a = k;
                                    pred.b = pos_1;
                                    pred.c = (s2 >= 0)? pos_2: -1;
                                    target_table.insert(buffer.data(), profit, pred);
                                });
                        return;
                    }
                    // Gathered for the source whose forward labels are
                    // merged last.
                    StateId source = (last == 0)? s1: s2;
                    StateId other = (last == 0)? s2: s1;
                    const LabelSet& source_set = (last == 0)? forward_set_1: forward_set_2;
                    const LabelSet& other_set = (last == 0)? forward_set_2: forward_set_1;
                    if (tables[source] == -1) {
                        if (free_tables.empty()) {
                            tables[source] = table_pool.size();
                            table_pool.emplace_back(number_of_words);
                        } else {
                            tables[source] = free_tables.back();
                            free_tables.pop_back();
                        }
                    }
                    LabelTable& source_table = table_pool[tables[source]];
                    interrupted = !merge(
                            data.encoder,
                            backward_set,
                            other_set,
                            threshold - source_set.profits[0],
                            buffer.data(),
                            timer,
                            [&](Counter pos_1, Counter pos_2, Profit profit)
                            {
                                Pred pred;
                                pred.a = state_id;
                                pred.b = k;
                                pred.c = last;
                                pred.d = pos_1;
                                pred.e = (other >= 0)? pos_2: -1;
                                source_table.insert(buffer.data(), profit, pred);
                            });
                });
        if (interrupted)
            return false;

        LabelSet target_set = target_table.extract();
        bool ok = merge(
                data.encoder,
                target_set,
                backward_set,
                threshold,
                buffer.data(),
                timer,
                [&](Counter pos_1, Counter pos_2, Profit)
                {
                    const Pred& pred = target_set.preds[pos_1];
                    update_best(data, best, buffer.data(), state_id, pred.a, pred.b, pred.c, pos_2);
                });
        if (!ok)
            return false;
    }
    return true;
}

/**
 * Search all the patterns of reduced profit at least 'threshold', and return
 * the best one.
 */
SearchOutput search(
        TreeSearchHypergraphData& data,
        Profit threshold,
        const optimizationtools::Timer& timer)
{
    SearchOutput output;
    StateId number_of_states = data.hypergraph.number_of_states();
    data.forward.assign(number_of_states, -1);
    data.backward.assign(number_of_states, -1);
    data.forward_sets.clear();
    data.backward_sets.clear();

    Best best;
    if (!forward_pass(data, threshold, timer, output)
            || !backward_pass(data, threshold, timer, output)
            || !merge_phase(data, threshold, timer, best)) {
        output.interrupted = true;
        return output;
    }
    if (best.state_id == -1)
        return output;
    output.found = true;
    output.profit = best.profit;

    // Build the tree of the best pattern.
    auto root_and_hole = expand_backward(data, output.tree, best.state_id, best.backward_pos);
    Counter root = root_and_hole.first;
    Counter hole = root_and_hole.second;
    output.tree[hole].choice = best.choice;
    if (best.choice >= 0) {
        auto sources = data.hypergraph.hyperarc(best.state_id, best.choice);
        Counter child_1 = expand_forward(data, output.tree, sources.first, best.forward_pos_1);
        output.tree[hole].children[0] = child_1;
        if (sources.second >= 0) {
            Counter child_2 = expand_forward(data, output.tree, sources.second, best.forward_pos_2);
            output.tree[hole].children[1] = child_2;
        }
    }
    if (root != 0)
        throw std::logic_error(FUNC_SIGNATURE + ": wrong root.");
    return output;
}

////////////////////////////////////////////////////////////////////////////////
//////////////////////////////// Solution building /////////////////////////////
////////////////////////////////////////////////////////////////////////////////

struct SolutionBuildingData
{
    SolutionBuildingData(
            const Instance& instance,
            const Hypergraph& hypergraph,
            const std::vector<TreeNode>& tree):
        instance(instance),
        hypergraph(hypergraph),
        tree(tree),
        builder(instance) { }

    const Instance& instance;

    const Hypergraph& hypergraph;

    const std::vector<TreeNode>& tree;

    SolutionBuilder builder;

    CutOrientation first_orientation = CutOrientation::Vertical;
};

/** Orientation of the cuts of depth 'depth' ('true' iff vertical). */
bool vertical(
        const SolutionBuildingData& data,
        Depth depth)
{
    return (data.first_orientation == CutOrientation::Vertical) == (depth % 2 == 1);
}

Length length(
        const SolutionBuildingData& data,
        const Hypergraph::StateInfo& info,
        bool vertical)
{
    return (vertical)?
        data.hypergraph.x().real[info.ix]:
        data.hypergraph.y().real[info.iy];
}

void place_part(
        SolutionBuildingData& data,
        Counter node_id,
        Length x,
        Length y,
        Depth depth);

/** Place a sub-plate N, of depth 'depth', at '(x, y)'. */
void place_node(
        SolutionBuildingData& data,
        Counter node_id,
        Length x,
        Length y,
        Depth depth)
{
    const TreeNode& node = data.tree[node_id];
    Hypergraph::StateInfo info = data.hypergraph.info(node.state_id);
    if (node.choice >= 0) {
        place_part(data, node.children[0], x, y, depth);
        return;
    }
    Leaf leaf = data.hypergraph.leaf(node.state_id, -1 - node.choice);
    if (leaf.number_of_items == 0)
        return;
    const ItemType& item_type = data.instance.item_type(leaf.item_type_ids[0]);
    if (leaf.number_of_items == 1
            && item_type.width(leaf.rotate[0]) == length(data, info, true)
            && item_type.height(leaf.rotate[0]) == length(data, info, false)) {
        data.builder.set_last_node_item(leaf.item_type_ids[0]);
        return;
    }
    // The items separated by the extra cut.
    bool vertical = (info.orientation == 0);
    Length position = (vertical)? x: y;
    for (Counter pos = 0; pos < leaf.number_of_items; ++pos) {
        const ItemType& item_type = data.instance.item_type(leaf.item_type_ids[pos]);
        Length item_length = (vertical)?
            item_type.width(leaf.rotate[pos]):
            item_type.height(leaf.rotate[pos]);
        data.builder.add_node(depth + 1, position + item_length);
        data.builder.set_last_node_item(leaf.item_type_ids[pos]);
        position += item_length + data.hypergraph.cut_thickness();
    }
}

/**
 * Place a part S of a sub-plate of depth 'depth', starting at '(x, y)': its
 * strips are of depth 'depth + 1'.
 */
void place_part(
        SolutionBuildingData& data,
        Counter node_id,
        Length x,
        Length y,
        Depth depth)
{
    for (;;) {
        const TreeNode& node = data.tree[node_id];
        if (node.choice < 0)
            return;
        Hypergraph::StateInfo info = data.hypergraph.info(node.state_id);
        bool vertical = (info.orientation == 0);
        Counter strip_id = node.children[0];
        Length strip_length = length(data, data.hypergraph.info(data.tree[strip_id].state_id), vertical);
        data.builder.add_node(depth + 1, ((vertical)? x: y) + strip_length);
        place_node(data, strip_id, x, y, depth + 1);
        if (node.children[1] == -1)
            return;
        if (vertical) {
            x += strip_length + data.hypergraph.cut_thickness();
        } else {
            y += strip_length + data.hypergraph.cut_thickness();
        }
        node_id = node.children[1];
    }
}

bool cut_is_vertical(
        const SolutionBuildingData& data,
        const TreeNode& node)
{
    Hypergraph::StateInfo info = data.hypergraph.info(node.state_id);
    Hypergraph::StateInfo info_1 = data.hypergraph.info(data.tree[node.children[0]].state_id);
    return info_1.ix != info.ix;
}

/**
 * Collect the sub-rectangles obtained by the consecutive cuts of orientation
 * 'cut_vertical' of a sub-rectangle P.
 */
void collect_pieces(
        const SolutionBuildingData& data,
        Counter node_id,
        bool cut_vertical,
        Length start,
        Length piece_length,
        std::vector<std::tuple<Counter, Length, Length>>& pieces)
{
    const TreeNode& node = data.tree[node_id];
    if (node.choice < 0 || cut_is_vertical(data, node) != cut_vertical) {
        pieces.push_back({node_id, start, piece_length});
        return;
    }
    Hypergraph::StateInfo info_1 = data.hypergraph.info(data.tree[node.children[0]].state_id);
    Length cut = length(data, info_1, cut_vertical);
    collect_pieces(data, node.children[0], cut_vertical, start, cut, pieces);
    collect_pieces(
            data,
            node.children[1],
            cut_vertical,
            start + cut + data.hypergraph.cut_thickness(),
            piece_length - cut - data.hypergraph.cut_thickness(),
            pieces);
}

/**
 * Place the pattern of a sub-rectangle P, with an unlimited number of stages,
 * in the sub-plate of depth 'depth' at '(x, y)' of dimensions
 * 'width x height'.
 */
void place_rectangle(
        SolutionBuildingData& data,
        Counter node_id,
        Length x,
        Length y,
        Length width,
        Length height,
        Depth depth)
{
    const TreeNode& node = data.tree[node_id];
    if (node.choice < 0) {
        Leaf leaf = data.hypergraph.leaf(node.state_id, -1 - node.choice);
        if (leaf.number_of_items == 0)
            return;
        ItemTypeId item_type_id = leaf.item_type_ids[0];
        const ItemType& item_type = data.instance.item_type(item_type_id);
        Length item_width = item_type.width(leaf.rotate[0]);
        Length item_height = item_type.height(leaf.rotate[0]);
        if (depth >= 1 && item_width == width && item_height == height) {
            data.builder.set_last_node_item(item_type_id);
            return;
        }
        // A sub-plate with the dimension of the item along the cuts of the
        // next depth, then the item.
        if (vertical(data, depth + 1)) {
            data.builder.add_node(depth + 1, x + item_width);
            if (item_height < height)
                data.builder.add_node(depth + 2, y + item_height);
        } else {
            data.builder.add_node(depth + 1, y + item_height);
            if (item_width < width)
                data.builder.add_node(depth + 2, x + item_width);
        }
        data.builder.set_last_node_item(item_type_id);
        return;
    }

    bool cut_vertical = cut_is_vertical(data, node);
    if (cut_vertical != vertical(data, depth + 1)) {
        // A sub-plate identical to the current one, cut along the other
        // orientation.
        data.builder.add_node(depth + 1, (vertical(data, depth + 1))? x + width: y + height);
        place_rectangle(data, node_id, x, y, width, height, depth + 1);
        return;
    }
    // The consecutive cuts of the same orientation produce sub-plates of the
    // same depth.
    std::vector<std::tuple<Counter, Length, Length>> pieces;
    collect_pieces(
            data,
            node_id,
            cut_vertical,
            (cut_vertical)? x: y,
            (cut_vertical)? width: height,
            pieces);
    for (const auto& piece: pieces) {
        Counter piece_id = std::get<0>(piece);
        Length start = std::get<1>(piece);
        Length piece_length = std::get<2>(piece);
        data.builder.add_node(depth + 1, start + piece_length);
        if (cut_vertical) {
            place_rectangle(data, piece_id, start, y, piece_length, height, depth + 1);
        } else {
            place_rectangle(data, piece_id, x, start, width, piece_length, depth + 1);
        }
    }
}

Solution build_solution(
        const Instance& instance,
        const Hypergraph& hypergraph,
        const std::vector<TreeNode>& tree)
{
    SolutionBuildingData data(instance, hypergraph, tree);
    const BinType& bin_type = instance.bin_type(0);
    Length x = hypergraph.x_offset();
    Length y = hypergraph.y_offset();
    Length width = bin_type.rect.w - x
        - ((bin_type.right_trim_type == TrimType::Hard)? bin_type.right_trim: 0);
    Length height = bin_type.rect.h - y
        - ((bin_type.top_trim_type == TrimType::Hard)? bin_type.top_trim: 0);
    if (hypergraph.staged()) {
        Counter root = 0;
        Hypergraph::StateInfo info = hypergraph.info(tree[root].state_id);
        if (info.meta) {
            root = tree[root].children[0];
            info = hypergraph.info(tree[root].state_id);
        }
        data.first_orientation = (info.orientation == 0)?
            CutOrientation::Vertical:
            CutOrientation::Horizontal;
        data.builder.add_bin(0, 1, data.first_orientation);
        place_node(data, root, x, y, 0);
    } else {
        data.first_orientation = instance.parameters().first_stage_orientation;
        if (data.first_orientation == CutOrientation::Any)
            data.first_orientation = CutOrientation::Vertical;
        data.builder.add_bin(0, 1, data.first_orientation);
        place_rectangle(data, 0, x, y, width, height, 0);
    }
    return data.builder.build();
}

}

Counter packingsolver::rectangleguillotine::tree_search_hypergraph_number_of_states(
        const Instance& instance)
{
    std::vector<ItemPos> copies = compute_copies(instance);
    Hypergraph hypergraph(instance, copies);
    return hypergraph.number_of_states();
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Main function ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

const TreeSearchHypergraphOutput packingsolver::rectangleguillotine::tree_search_hypergraph(
        const Instance& instance,
        const TreeSearchHypergraphParameters& parameters)
{
    TreeSearchHypergraphOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    // The features of the instance which the hypergraph doesn't take into
    // account (see 'tree_search_hypergraph_supported') are ignored: the
    // solutions which don't satisfy them are not reported. They only
    // restrict the patterns, so the bounds remain valid, except the negative
    // penalties of the penalized resources, which may increase the profit,
    // and the other bins: the bounds and the infeasibility are only proven
    // with a single bin.
    bool proofs = (instance.number_of_bins() == 1);
    Profit penalties = negative_penalty_sum(instance);

    // With an objective other than 'Knapsack', a pattern with all the items
    // is searched.
    bool feasibility = (instance.objective() != Objective::Knapsack);

    // The numbers of copies of the item types, at most the number of copies
    // which fit in the bin.
    std::vector<ItemPos> copies = compute_copies(instance);
    // The profits: with an objective other than 'Knapsack', the area of the
    // items, so that the patterns with all the items are the patterns of
    // maximum profit.
    std::vector<Profit> profits(instance.number_of_item_types(), 0);
    Profit total_profit = 0;
    bool all_items_fit = true;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        profits[item_type_id] = (feasibility)? item_type.area(): item_type.profit;
        total_profit += profits[item_type_id] * item_type.copies;
        if (copies[item_type_id] < item_type.copies)
            all_items_fit = false;
    }
    if (feasibility && !all_items_fit) {
        if (proofs)
            algorithm_formatter.update_is_proven_infeasible();
        algorithm_formatter.end();
        return output;
    }

    Hypergraph hypergraph(instance, copies);
    if (parameters.maximum_number_of_states >= 0
            && hypergraph.number_of_states() > parameters.maximum_number_of_states) {
        algorithm_formatter.end();
        return output;
    }

    TreeSearchHypergraphData data(
            instance,
            hypergraph,
            copies,
            profits);
    if (parameters.use_lagrangian_relaxation)
        compute_multipliers(data, parameters);
    Profit lagrangian_constant = data.lagrangian_constant;
    if (!compute_dp(data, parameters.timer)
            || !compute_rdp(data, parameters.timer)) {
        algorithm_formatter.end();
        return output;
    }
    Profit bound = data.dp[hypergraph.goal()] + lagrangian_constant;
    Profit tolerance = 1e-9 * std::max((Profit)1, std::abs(bound));
    if (feasibility) {
        if (bound < total_profit - tolerance) {
            if (proofs)
                algorithm_formatter.update_is_proven_infeasible();
            algorithm_formatter.end();
            return output;
        }
    } else if (proofs) {
        algorithm_formatter.update_knapsack_bound(bound + penalties);
        if (algorithm_formatter.end_boolean()) {
            algorithm_formatter.end();
            return output;
        }
    }

    // Decreasing thresholds, starting from the bound.
    Profit threshold = (feasibility)?
        total_profit - lagrangian_constant:
        data.dp[hypergraph.goal()];
    Profit step = 1e-3 * std::max((Profit)1, std::abs(bound));
    for (;;) {
        // The best solution known, possibly found by other algorithms.
        Profit best_profit = std::max(
                parameters.best_known_profit(),
                (output.solution_pool.best().number_of_items() > 0)?
                output.solution_pool.best().profit():
                0);
        if (!feasibility)
            threshold = std::max(threshold, best_profit - lagrangian_constant);

        SearchOutput result = search(data, threshold - tolerance, parameters.timer);
        if (result.interrupted)
            break;
        output.number_of_searches++;
        output.number_of_forward_labels = result.number_of_forward_labels;
        output.number_of_backward_labels = result.number_of_backward_labels;

        // A pattern which doesn't satisfy the features of the instance which
        // the hypergraph doesn't take into account is not reported.
        bool found_feasible = false;
        if (result.found) {
            Solution solution = build_solution(instance, hypergraph, result.tree);
            found_feasible = solution.feasible()
                && (!feasibility || solution.number_of_items() == instance.number_of_items());
            if (found_feasible && solution.number_of_items() > 0)
                algorithm_formatter.update_solution(solution, "S" + std::to_string(output.number_of_searches));
            if (found_feasible)
                best_profit = std::max(best_profit, result.profit);
        }

        if (feasibility) {
            // All the patterns with all the items have been searched: if the
            // best pattern found doesn't have all the items, there is none.
            if (proofs
                    && (!result.found || result.profit < total_profit - tolerance)) {
                algorithm_formatter.update_is_proven_infeasible();
            }
            break;
        }

        // All the patterns of profit at least 'threshold + lagrangian_constant'
        // have been searched: the best one is the best pattern found, even if
        // it doesn't satisfy the features of the instance which the
        // hypergraph doesn't take into account.
        Profit search_bound = threshold + lagrangian_constant;
        if (result.found)
            search_bound = std::max(search_bound, result.profit);
        if (proofs)
            algorithm_formatter.update_knapsack_bound(search_bound + penalties);
        if (best_profit >= search_bound - tolerance)
            break;
        // The thresholds can't decrease below the best solution known.
        if (threshold + lagrangian_constant <= best_profit + tolerance)
            break;
        if (algorithm_formatter.end_boolean())
            break;
        threshold -= step;
    }

    algorithm_formatter.end();
    return output;
}
