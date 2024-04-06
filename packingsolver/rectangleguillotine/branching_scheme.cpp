#include "packingsolver/rectangleguillotine/branching_scheme.hpp"

#include <sstream>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

BranchingScheme::BranchingScheme(
        const Instance& instance,
        const Parameters& parameters):
    instance_(instance),
    parameters_(parameters)
{
    // Compute first_stage_orientation_.
    CutOrientation first_stage_orientation = (instance.first_stage_orientation() != CutOrientation::Any)?
        instance.first_stage_orientation():
        parameters.first_stage_orientation;
    if (instance.number_of_stages() == 3) {
        first_stage_orientation_ = first_stage_orientation;
    } else if (instance.number_of_stages() == 2) {
        if (first_stage_orientation == CutOrientation::Horinzontal) {
            first_stage_orientation_ = CutOrientation::Vertical;
        } else if (first_stage_orientation == CutOrientation::Vertical) {
            first_stage_orientation_ = CutOrientation::Horinzontal;
        }
    }

    // Compute no_oriented_items_;
    no_oriented_items_ = true;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        if (instance.item_type(item_type_id).oriented) {
            no_oriented_items_ = false;
            break;
        }
    }

    // Update stack_pred_
    stack_pred_ = std::vector<StackId>(instance.number_of_stacks(), -1);
    for (StackId s = 0; s < instance.number_of_stacks(); ++s) {
        for (StackId s0 = s - 1; s0 >= 0; --s0) {
            if (equals(s, s0)) {
                stack_pred_[s] = s0;
                break;
            }
        }
    }
}

bool BranchingScheme::equals(StackId s1, StackId s2)
{
    if (instance().stack_size(s1) != instance().stack_size(s2))
        return false;
    for (ItemPos pos = 0; pos < instance().stack_size(s1); ++pos) {
        ItemTypeId item_type_id_1 = instance().item(s1, pos);
        ItemTypeId item_type_id_2 = instance().item(s2, pos);
        const ItemType& item_type_1 = instance().item_type(item_type_id_1);
        const ItemType& item_type_2 = instance().item_type(item_type_id_2);
        if (item_type_1.oriented && item_type_2.oriented
                && item_type_1.rect.w == item_type_2.rect.w
                && item_type_1.rect.h == item_type_2.rect.h
                && item_type_1.profit == item_type_2.profit
                && item_type_1.copies == item_type_2.copies)
            continue;
        if (!item_type_1.oriented && !item_type_2.oriented
                && item_type_1.profit == item_type_2.profit
                && ((item_type_1.rect.w == item_type_2.rect.w && item_type_1.rect.h == item_type_2.rect.h)
                    || (item_type_1.rect.w == item_type_2.rect.h && item_type_1.rect.h == item_type_2.rect.w)))
            continue;
        return false;
    }
    return true;
}

const std::shared_ptr<BranchingScheme::Node> BranchingScheme::root() const
{
    BranchingScheme::Node node;
    node.id = node_id_;
    node_id_++;
    node.pos_stack = std::vector<ItemPos>(instance().number_of_stacks(), 0);
    return std::shared_ptr<Node>(new BranchingScheme::Node(node));
}

bool BranchingScheme::bound(
        const Node& node,
        const Solution& solution_best) const
{
    switch (solution_best.instance().objective()) {
    case Objective::Default: {
        if (!solution_best.full()) {
            return (ubkp(node) <= solution_best.profit());
        } else {
            if (ubkp(node) != solution_best.profit())
                return (ubkp(node) <= solution_best.profit());
            return node.waste >= solution_best.waste();
        }
    } case Objective::BinPacking: case Objective::VariableSizedBinPacking: {
        if (!solution_best.full())
            return false;
        BinPos bin_pos = -1;
        Area a = instance().item_area() + node.waste;
        while (a > 0) {
            bin_pos++;
            if (bin_pos >= instance().number_of_bins())
                return true;
            BinTypeId bin_type_id = instance().bin_type_id(bin_pos);
            a -= instance().bin_type(bin_type_id).area();
        }
        return (bin_pos + 1 >= solution_best.number_of_bins());
    } case Objective::BinPackingWithLeftovers: {
        if (!solution_best.full())
            return false;
        return node.waste >= solution_best.waste();
    } case Objective::Knapsack: {
        return ubkp(node) <= solution_best.profit();
    } case Objective::OpenDimensionX: {
        if (!solution_best.full())
            return false;
        return std::max(width(node), (node.waste + instance().item_area() - 1) / instance().height(instance().bin_type(0), CutOrientation::Vertical) + 1) >= solution_best.width();
    } case Objective::OpenDimensionY: {
        if (!solution_best.full())
            return false;
        return std::max(height(node), (node.waste + instance().item_area() - 1) / instance().height(instance().bin_type(0), CutOrientation::Horinzontal) + 1) >= solution_best.height();
    } default: {
        std::stringstream ss;
        ss << "Branching scheme 'rectangleguillotine::BranchingScheme'"
            << "does not support objective '" << instance().objective() << "'.";
        throw std::logic_error(ss.str());
        return false;
    }
    }
}

bool BranchingScheme::better(
        const Node& node,
        const Solution& solution_best) const
{
    switch (instance().objective()) {
    case Objective::Default: {
        if (solution_best.profit() > node.profit)
            return false;
        if (solution_best.profit() < node.profit)
            return true;
        return solution_best.waste() > node.waste;
    } case Objective::BinPacking: case Objective::VariableSizedBinPacking: {
        if (!leaf(node))
            return false;
        if (!solution_best.full())
            return true;
        return solution_best.number_of_bins() > node.number_of_bins;
    } case Objective::BinPackingWithLeftovers: {
        if (!leaf(node))
            return false;
        if (!solution_best.full())
            return true;
        return solution_best.waste() > node.waste;
    } case Objective::OpenDimensionX: {
        if (!leaf(node))
            return false;
        if (!solution_best.full())
            return true;
        return solution_best.width() > width(node);
    } case Objective::OpenDimensionY: {
        if (!leaf(node))
            return false;
        if (!solution_best.full())
            return true;
        return solution_best.height() > height(node);
    } case Objective::Knapsack: {
        return solution_best.profit() < node.profit;
    } default: {
        std::stringstream ss;
        ss << "Branching scheme 'rectangleguillotine::BranchingScheme'"
            << "does not support objective \"" << instance().objective() << "\".";
        throw std::logic_error(ss.str());
        return false;
    }
    }
}

bool BranchingScheme::dominates(
        const Front& f1,
        const Front& f2) const
{
    if (f1.i < f2.i)
        return true;
    if (f1.i == f2.i && f1.o == f2.o) {
        if (f1.x1_curr <= f2.x1_prev)
            return true;
        if (f1.x1_prev <= f2.x1_prev
                && f1.x1_curr <= f2.x1_curr
                && f1.y2_curr <= f2.y2_prev)
            return true;
        BinTypeId bin_type_id = instance().bin_type_id(f1.i);
        const BinType& bin_type = instance().bin_type(bin_type_id);
        Length h = instance().height(bin_type, f1.o);
        if (f1.y2_curr != h
                && f1.x1_prev <= f2.x1_prev
                && f1.x3_curr <= f2.x3_curr
                && f1.x1_curr <= f2.x1_curr
                && f1.y2_prev <= f2.y2_prev
                && f1.y2_curr <= f2.y2_curr)
            return true;
        if (f2.y2_curr == h
                && f1.x1_prev >= f2.x1_prev
                && f1.x3_curr <= f2.x3_curr
                && f1.x1_curr <= f2.x1_curr
                && f1.y2_prev <= f2.y2_prev
                && f1.y2_curr <= f2.y2_curr)
            return true;
        if (f1.y2_curr != h
                && f2.y2_curr == h
                && f1.x3_curr <= f2.x3_curr
                && f1.x1_curr <= f2.x1_curr
                && f1.y2_prev <= f2.y2_prev
                && f1.y2_curr <= f2.y2_curr)
            return true;
    }
    return false;
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// Children ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

Length BranchingScheme::x1_prev(const Node& node, Depth df) const
{
    switch (df) {
    case -2: {
        BinTypeId bin_type_id = instance().bin_type_id(node.number_of_bins);
        return instance().left_trim(
                instance().bin_type(bin_type_id),
                CutOrientation::Horinzontal);
    } case -1: {
        BinTypeId bin_type_id = instance().bin_type_id(node.number_of_bins);
        return instance().left_trim(
                instance().bin_type(bin_type_id),
                CutOrientation::Vertical);
    } case 0: {
        return node.x1_curr + instance().cut_thickness();
    } case 1: {
        return node.x1_prev;
    } case 2: {
        return node.x1_prev;
    } default: {
        assert(false);
        return -1;
    }
    }
}

Length BranchingScheme::x3_prev(const Node& node, Depth df) const
{
    switch (df) {
    case -2: {
        BinTypeId bin_type_id = instance().bin_type_id(node.number_of_bins);
        return instance().left_trim(
                instance().bin_type(bin_type_id),
                CutOrientation::Horinzontal);
    } case -1: {
        BinTypeId bin_type_id = instance().bin_type_id(node.number_of_bins);
        return instance().left_trim(
                instance().bin_type(bin_type_id),
                CutOrientation::Vertical);
    } case 0: {
        return node.x1_curr + instance().cut_thickness();
    } case 1: {
        return node.x1_prev;
    } case 2: {
        return node.x3_curr + instance().cut_thickness();
    } default: {
        assert(false);
        return -1;
    }
    }
}

Length BranchingScheme::y2_prev(const Node& node, Depth df) const
{
    switch (df) {
    case -2: {
        BinTypeId bin_type_id = instance().bin_type_id(node.number_of_bins);
        return instance().bottom_trim(
                instance().bin_type(bin_type_id),
                CutOrientation::Horinzontal);
    } case -1: {
        BinTypeId bin_type_id = instance().bin_type_id(node.number_of_bins);
        return instance().bottom_trim(
                instance().bin_type(bin_type_id),
                CutOrientation::Vertical);
    } case 0: {
        BinTypeId bin_type_id = instance().bin_type_id(node.number_of_bins - 1);
        return instance().bottom_trim(
                instance().bin_type(bin_type_id),
                node.first_stage_orientation);
    } case 1: {
        return node.y2_curr + instance().cut_thickness();
    } case 2: {
        return node.y2_prev;
    } default: {
        assert(false);
        return -1;
    }
    }
}

BinPos BranchingScheme::last_bin(const Node& node, Depth df) const
{
    if (df <= -1) {
        return node.number_of_bins;
    } else {
        return node.number_of_bins - 1;
    }
}

CutOrientation BranchingScheme::last_bin_orientation(const Node& node, Depth df) const
{
    switch (df) {
    case -1: {
        return CutOrientation::Vertical;
    } case -2: {
        return CutOrientation::Horinzontal;
    } default: {
        return node.first_stage_orientation;
    }
    }
}

BranchingScheme::Front BranchingScheme::front(
        const Node& node,
        const Insertion& insertion) const
{
    switch (insertion.df) {
    case -1: case -2: {
        return {last_bin(node, insertion.df), last_bin_orientation(node, insertion.df),
            0, insertion.x3, insertion.x1,
            0, insertion.y2};
    } case 0: {
        return {last_bin(node, insertion.df), last_bin_orientation(node, insertion.df),
            node.x1_curr, insertion.x3, insertion.x1,
            0, insertion.y2};
    } case 1: {
        return {last_bin(node, insertion.df), last_bin_orientation(node, insertion.df),
            node.x1_prev, insertion.x3, insertion.x1,
            node.y2_curr, insertion.y2};
    } case 2: {
        return {last_bin(node, insertion.df), last_bin_orientation(node, insertion.df),
            node.x1_prev, insertion.x3, insertion.x1,
            node.y2_prev, insertion.y2};
    } default: {
        assert(false);
        return {-1, CutOrientation::Vertical,
            -1, -1, -1,
            -1, -1};
    }
    }
}

std::shared_ptr<BranchingScheme::Node> BranchingScheme::child(
        const std::shared_ptr<Node>& pfather,
        const Insertion& insertion) const
{
    const Node& father = *pfather;
    assert(insertion.df <= -1 || insertion.x1 >= father.x1_curr);
    auto pnode = std::shared_ptr<Node>(new BranchingScheme::Node());
    Node& node = *pnode;

    node.id = node_id_;
    node_id_++;
    node.father = pfather;
    node.item_type_id_1 = insertion.item_type_id_1;
    node.item_type_id_2 = insertion.item_type_id_2;
    node.df = insertion.df;
    node.x1_curr = insertion.x1;
    node.y2_curr = insertion.y2;
    node.x3_curr = insertion.x3;
    node.x1_max = insertion.x1_max;
    node.y2_max = insertion.y2_max;
    node.z1 = insertion.z1;
    node.z2 = insertion.z2;

    // Compute number_of_bins and first_stage_orientation.
    if (insertion.df < 0) {
        node.number_of_bins = father.number_of_bins + 1;
        node.first_stage_orientation = (insertion.df == -1)?
            CutOrientation::Vertical: CutOrientation::Horinzontal;
    } else {
        node.number_of_bins = father.number_of_bins;
        node.first_stage_orientation = father.first_stage_orientation;
    }

    BinPos i = node.number_of_bins - 1;
    CutOrientation o = node.first_stage_orientation;
    BinTypeId bin_type_id = instance().bin_type_id(i);
    const BinType& bin_type = instance().bin_type(bin_type_id);
    Length h = instance().height(bin_type, o)
        - instance().top_trim(bin_type, o)
        - instance().bottom_trim(bin_type, o);
    Length w = instance().width(bin_type, o)
        - instance().right_trim(bin_type, o)
        - instance().left_trim(bin_type, o);

    // Compute x1_prev and y2_prev.
    switch (insertion.df) {
    case -1: case -2: {
        node.x1_prev = instance().left_trim(bin_type, o);
        node.y2_prev = instance().bottom_trim(bin_type, o);
        break;
    } case 0: {
        node.x1_prev = father.x1_curr + instance().cut_thickness();
        node.y2_prev = instance().bottom_trim(bin_type, o);
        break;
    } case 1: {
        node.x1_prev = father.x1_prev;
        node.y2_prev = father.y2_curr + instance().cut_thickness();
        break;
    } case 2: {
        node.x1_prev = father.x1_prev;
        node.y2_prev = father.y2_prev;
        break;
    } default: {
        assert(false);
        break;
    }
    }

    Length w_j = node.x3_curr - x3_prev(father, node.df);
    //bool rotate_j1 = (insertion.item_type_id_1 == -1)? false: (instance().width(instance().item(insertion.item_type_id_1), true, o) == w_j);
    bool rotate_j2 = (node.item_type_id_2 == -1)?
        false:
        (instance().width(instance().item_type(node.item_type_id_2), true, o) == w_j);
    //Length h_j1 = (insertion.item_type_id_1 == -1)? -1: instance().height(instance().item_type(insertion.item_type_id_1), rotate_j1, o);
    //Length h_j2 = (insertion.item_type_id_2 == -1)? -1: instance().height(instance().item_type(insertion.item_type_id_2), rotate_j2, o);

    // Compute subplate2curr_items_above_defect.
    if (node.df == 2)
        node.subplate2curr_items_above_defect = father.subplate2curr_items_above_defect;
    if (node.item_type_id_1 == -1 && node.item_type_id_2 != -1) {
        JRX jrx;
        jrx.item_type_id = insertion.item_type_id_2;
        jrx.rotate = rotate_j2;
        jrx.x = x3_prev(father, node.df);
        node.subplate2curr_items_above_defect.push_back(jrx);
    }

    // Update pos_stack, items_area, squared_item_area and profit.
    node.pos_stack = father.pos_stack;
    node.number_of_items = father.number_of_items;
    node.item_area = father.item_area;
    node.squared_item_area = father.squared_item_area;
    node.profit = father.profit;
    if (insertion.item_type_id_1 != -1) {
        const ItemType& item = instance().item_type(insertion.item_type_id_1);
        node.pos_stack[item.stack_id]++;
        node.number_of_items += 1;
        node.item_area += item.area();
        node.squared_item_area += item.area() * item.area();
        node.profit += item.profit;
    }
    if (insertion.item_type_id_2 != -1) {
        const ItemType& item = instance().item_type(insertion.item_type_id_2);
        node.pos_stack[item.stack_id]++;
        node.number_of_items += 1;
        node.item_area += item.area();
        node.squared_item_area += item.area() * item.area();
        node.profit += item.profit;
    }
    assert(node.item_area <= instance().bin_area());

    // Update current_area_ and waste_
    node.current_area = instance().previous_bin_area(i);
    if (leaf(node)) {
        node.current_area += (instance().number_of_stages() == 3)?
            (node.x1_curr - instance().left_trim(bin_type, o)) * h:
            (node.y2_curr - instance().bottom_trim(bin_type, o)) * w;
    } else {
        node.current_area += (node.x1_prev - instance().left_trim(bin_type, o)) * h
            + (node.x1_curr - node.x1_prev) * (node.y2_prev - instance().bottom_trim(bin_type, o))
            + (node.x3_curr - node.x1_prev) * (node.y2_curr - node.y2_prev);
    }
    node.waste = node.current_area - node.item_area;
    assert(node.waste >= 0);
    return pnode;
}

std::vector<BranchingScheme::Insertion> BranchingScheme::insertions(
        const std::shared_ptr<Node>& pfather) const
{
    const Node& father = *pfather;

    if (leaf(father))
        return {};

    std::vector<Insertion> insertions;

    // Compute df_min
    Depth df_min = -2;
    if (father.number_of_bins == instance().number_of_bins()) {
        df_min = 0;
    } else if (first_stage_orientation_ == CutOrientation::Vertical) {
        df_min = -1;
    } else if (first_stage_orientation_ == CutOrientation::Any
            // Next bin has no defects,
            && instance().bin_type(instance().bin_type_id(father.number_of_bins)).defects.size() == 0
            // is a square,
            && instance().bin_type(instance().bin_type_id(father.number_of_bins)).rect.w
            == instance().bin_type(instance().bin_type_id(father.number_of_bins)).rect.h
            // and items can be rotated
            && no_oriented_items_) {
        df_min = -1;
    }

    // Compute df_max
    Depth df_max = 2;
    if (father.father == nullptr)
        df_max = -1;

    for (Depth df = df_max; df >= df_min; --df) {
        if (df == -1 && first_stage_orientation_ == CutOrientation::Horinzontal)
            continue;

        if (df == -2) {
            i = father.number_of_bins;
            o = CutOrientation::Horinzontal;
        } else if (df == -1) {
            i = father.number_of_bins;
            o = CutOrientation::Vertical;
        } else {
            i = father.number_of_bins - 1;
            o = father.first_stage_orientation;
        }

        // Simple dominance rule
        bool stop = false;
        for (const Insertion& insertion: insertions) {
            if (insertion.item_type_id_1 == -1 && insertion.item_type_id_2 == -1)
                continue;
            if (df == 1
                    && insertion.x1 == father.x1_curr
                    && insertion.y2 == father.y2_curr) {
                stop = true;
                break;
            } else if (df == 0
                    && insertion.x1 == father.x1_curr) {
                stop = true;
                break;
            } else if (df < 0
                    && insertion.df >= 0) {
                stop = true;
                break;
            }
        }
        if (stop)
            break;

        Length x = x3_prev(father, df);
        Length y = y2_prev(father, df);

        // Try adding an item
        for (StackId s = 0; s < instance().number_of_stacks(); ++s) {
            if (father.pos_stack[s] == instance().stack_size(s))
                continue;
            StackId sp = stack_pred_[s];
            if (sp != -1 && father.pos_stack[sp] <= father.pos_stack[s])
                continue;

            ItemTypeId item_type_id = instance().item(s, father.pos_stack[s]);
            const ItemType& item_type = instance().item_type(item_type_id);

            if (!item_type.oriented) {
                bool b = item_type.rect.w > item_type.rect.h;
                insertion_1_item(
                        father,
                        insertions,
                        item_type_id,
                        !b,  // rotate
                        df);
                insertion_1_item(
                        father,
                        insertions,
                        item_type_id,
                        b,  // rotate
                        df);
            } else {
                insertion_1_item(
                        father,
                        insertions,
                        item_type_id,
                        false,  // rotate
                        df);
            }

            // Try adding it with a second item
            if (instance().cut_type() == CutType::Roadef2018) {
                Length wj = instance().width(item_type, false, o);
                Length wjr = instance().width(item_type, true, o);
                for (StackId s2 = s; s2 < instance().number_of_stacks(); ++s2) {
                    ItemTypeId item_type_id_2 = -1;
                    if (s2 == s) {
                        // Check is the whold stack has already been packed.
                        if (father.pos_stack[s2] + 1 == instance().stack_size(s2))
                            continue;
                        StackId sp2 = stack_pred_[s2];
                        if (sp2 != -1
                                && father.pos_stack[sp2]
                                <= father.pos_stack[s2])
                            continue;
                        item_type_id_2 = instance().item(s2, father.pos_stack[s2] + 1);
                    } else {
                        // Check is the whold stack has already been packed.
                        if (father.pos_stack[s2] == instance().stack_size(s2))
                            continue;
                        StackId sp2 = stack_pred_[s2];
                        if ((sp2 == s
                                    && father.pos_stack[sp2] + 1
                                    <= father.pos_stack[s2])
                                || (sp2 != -1
                                    && sp2 != s
                                    && father.pos_stack[sp2]
                                    <= father.pos_stack[s2]))
                            continue;
                        item_type_id_2 = instance().item(s2, father.pos_stack[s2]);
                    }

                    const ItemType& item_type_2 = instance().item_type(item_type_id_2);
                    Length wj2 = instance().width(item_type_2, false, o);
                    Length wj2r = instance().width(item_type_2, true, o);
                    if (wj == wj2)
                        insertion_2_items(
                                father,
                                insertions,
                                item_type_id,
                                false,  // rotate_1
                                item_type_id_2,
                                false,  // rotate_2
                                df);
                    if (!item_type_2.oriented)
                        if (wj == wj2r)
                            insertion_2_items(
                                    father,
                                    insertions,
                                    item_type_id,
                                    false,  // rotate_1
                                    item_type_id_2,
                                    true,  // rotate_2
                                    df);
                    if (!item_type.oriented) {
                        if (wjr == wj2)
                            insertion_2_items(
                                    father,
                                    insertions,
                                    item_type_id,
                                    true,  // rotate_1
                                    item_type_id_2,
                                    false,  // rotate_2
                                    df);
                        if (!item_type_2.oriented)
                            if (wjr == wj2r)
                                insertion_2_items(
                                        father,
                                        insertions,
                                        item_type_id,
                                        true,  // rotate_1
                                        item_type_id_2,
                                        true,  // rotate_2
                                        df);
                    }
                }
            }
        }

        if (father.father == nullptr || father.item_type_id_1 != -1 || father.item_type_id_2 != -1) {
            BinTypeId bin_type_id = instance().bin_type_id(i);
            const BinType& bin_type = instance().bin_type(bin_type_id);
            for (DefectId defect_id = 0;
                    defect_id < (DefectId)bin_type.defects.size();
                    ++defect_id) {
                const Defect& defect = bin_type.defects[defect_id];
                if (instance().left(defect, o) >= x && instance().bottom(defect, o) >= y)
                    insertion_defect(father, insertions, defect_id, df);
            }
        }
    }

    return insertions;
}

Area BranchingScheme::waste(
        const Node& node,
        const Insertion& insertion) const
{
    BinTypeId bin_type_id = instance().bin_type_id(i);
    const BinType& bin_type = instance().bin_type(bin_type_id);
    Length h = instance().height(bin_type, o);
    Front f = front(node, insertion);
    ItemPos n = node.number_of_items;
    Area item_area = node.item_area;
    if (insertion.item_type_id_1 != -1) {
        n++;
        item_area += instance().item_type(insertion.item_type_id_1).area();
    }
    if (insertion.item_type_id_2 != -1) {
        n++;
        item_area += instance().item_type(insertion.item_type_id_2).area();
    }
    Area current_area = (n == instance().number_of_items())?
        instance().previous_bin_area(i)
        + (f.x1_curr * h):
        instance().previous_bin_area(i)
        + f.x1_prev * h
        + (f.x1_curr - f.x1_prev) * f.y2_prev
        + (f.x3_curr - f.x1_prev) * (f.y2_curr - f.y2_prev);
    return current_area - item_area;
}

Length BranchingScheme::x1_max(const Node& node, Depth df) const
{
    switch (df) {
    case -1: case -2: {
        BinTypeId bin_type_id = instance().bin_type_id(i);
        const BinType& bin_type = instance().bin_type(bin_type_id);
        Length x = instance().width(bin_type, o) - instance().right_trim(bin_type, o);
        if (instance().max1cut() != -1)
            if (x > x1_prev(node, df) + instance().max1cut())
                x = x1_prev(node, df) + instance().max1cut();
        return x;
    } case 0: {
        BinTypeId bin_type_id = instance().bin_type_id(i);
        const BinType& bin_type = instance().bin_type(bin_type_id);
        Length x = instance().width(bin_type, o) - instance().right_trim(bin_type, o);
        if (instance().max1cut() != -1)
            if (x > x1_prev(node, df) + instance().max1cut())
                x = x1_prev(node, df) + instance().max1cut();
        return x;
    } case 1: {
        BinTypeId bin_type_id = instance().bin_type_id(i);
        const BinType& bin_type = instance().bin_type(bin_type_id);
        Length x = node.x1_max;
        if (!instance().cut_through_defects())
            for (const Defect& defect: bin_type.defects)
                if (instance().bottom(defect, o) < node.y2_curr + instance().cut_thickness()
                        && instance().top(defect, o) > node.y2_curr)
                    if (instance().left(defect, o) > node.x1_prev)
                        if (x > instance().left(defect, o) - instance().cut_thickness())
                            x = instance().left(defect, o) - instance().cut_thickness();;
        return x;
    } case 2: {
        return node.x1_max;
    } default: {
        return -1;
    }
    }
}

Length BranchingScheme::y2_max(
        const Node& node,
        Depth df,
        Length x3) const
{
    BinTypeId bin_type_id = instance().bin_type_id(i);
    const BinType& bin_type = instance().bin_type(bin_type_id);
    Length y = (df == 2)?
        node.y2_max:
        instance().height(bin_type, o) - instance().top_trim(bin_type, o);
    if (!instance().cut_through_defects())
        for (const Defect& defect: bin_type.defects)
            if (instance().left(defect, o) < x3 + instance().cut_thickness()
                    && instance().right(defect, o) > x3)
                if (instance().bottom(defect, o) >= y2_prev(node, df))
                    if (y > instance().bottom(defect, o) - instance().cut_thickness())
                        y = instance().bottom(defect, o) - instance().cut_thickness();
    return y;
}

BranchingScheme::Front BranchingScheme::front(
        const BranchingScheme::Node& node) const
{
    Front f;
    f.i = static_cast<BinPos>(node.number_of_bins - 1);
    f.o = node.first_stage_orientation;
    f.x1_prev = node.x1_prev;
    f.x3_curr = node.x3_curr;
    f.x1_curr = node.x1_curr;
    f.y2_prev = node.y2_prev;
    f.y2_curr = node.y2_curr;
    return f;
}

void BranchingScheme::insertion_1_item(
        const Node& father,
        std::vector<Insertion>& insertions,
        ItemTypeId item_type_id,
        bool rotate,
        Depth df) const
{
    assert(-2 <= df); assert(df <= 3);

    // Check defect intersection
    const rectangleguillotine::ItemType& item_type = instance().item_type(item_type_id);
    Length x = x3_prev(father, df) + instance().width(item_type, rotate, o);
    Length y = y2_prev(father, df) + instance().height(item_type, rotate, o);
    BinTypeId bin_type_id = instance().bin_type_id(i);
    const BinType& bin_type = instance().bin_type(bin_type_id);
    Length w = instance().width(bin_type, o) - instance().right_trim(bin_type, o);
    Length h = instance().height(bin_type, o) - instance().top_trim(bin_type, o);
    if (x > w) {
        return;
    }
    if (y > h) {
        return;
    }

    // Homogenous
    if (df == 2 && instance().cut_type() == CutType::Homogenous
            && father.item_type_id_1 != item_type_id) {
        return;
    }

    Insertion insertion {
        item_type_id, -1, df,
        x, y, x,
        x1_max(father, df), y2_max(father, df, x), 0, 0};

    // Check defect intersection
    DefectId defect_id = instance().item_intersects_defect(
            x3_prev(father, df),
            y2_prev(father, df),
            item_type,
            rotate,
            bin_type,
            o);
    if (defect_id >= 0) {
        if (instance().cut_type() == CutType::Roadef2018
                || instance().cut_type() == CutType::NonExact) {
            // Place the item on top of its third-level sub-plate
            insertion.item_type_id_1 = -1;
            insertion.item_type_id_2 = item_type_id;
            Length min_waste = instance().min_waste();
            if (df <= 0)  // y1_prev is the bottom trim.
                if (instance().bottom_trim_type(bin_type, o) == TrimType::Soft)
                    min_waste = std::max(Length(0), min_waste - instance().bottom_trim(bin_type, o));
            insertion.y2 += min_waste;
            insertion.z2 = 1;
        } else {
            return;
        }
    }

    // Update insertion.z2 with respect to cut_type()
    if (instance().cut_type() == CutType::Exact
            || instance().cut_type() == CutType::Homogenous)
        insertion.z2 = 2;

    update(father, insertions, insertion);
}

void BranchingScheme::insertion_2_items(
        const Node& father,
        std::vector<Insertion>& insertions,
        ItemTypeId item_type_id_1,
        bool rotate1,
        ItemTypeId item_type_id_2,
        bool rotate2,
        Depth df) const
{
    assert(-2 <= df); assert(df <= 3);

    // Check defect intersection
    const ItemType& item_type_1 = instance().item_type(item_type_id_1);
    const ItemType& item_type_2 = instance().item_type(item_type_id_2);
    BinTypeId bin_type_id = instance().bin_type_id(i);
    const BinType& bin_type = instance().bin_type(bin_type_id);
    Length w = instance().width(bin_type, o) - instance().right_trim(bin_type, o);
    Length h = instance().height(bin_type, o) - instance().top_trim(bin_type, o);
    Length h_j1 = instance().height(item_type_1, rotate1, o);
    Length x = x3_prev(father, df) + instance().width(item_type_1, rotate1, o);
    Length y = y2_prev(father, df)
        + h_j1
        + instance().parameters().cut_thickness
        + instance().height(item_type_2, rotate2, o);
    if (x > w || y > h) {
        return;
    }
    if (instance().item_intersects_defect(
                x3_prev(father, df),
                y2_prev(father, df),
                item_type_1,
                rotate1,
                bin_type,
                o) >= 0
            || instance().item_intersects_defect(
                x3_prev(father, df),
                y2_prev(father, df) + h_j1,
                item_type_2,
                rotate2,
                bin_type,
                o) >= 0) {
        return;
    }

    Insertion insertion {
        item_type_id_1, item_type_id_2, df,
        x, y, x,
        x1_max(father, df), y2_max(father, df, x), 0, 2};

    update(father, insertions, insertion);
}

void BranchingScheme::insertion_defect(
        const Node& father,
        std::vector<Insertion>& insertions,
        DefectId defect_id,
        Depth df) const
{
    assert(-2 <= df);
    assert(df <= 3);

    // Check defect intersection
    BinTypeId bin_type_id = instance().bin_type_id(i);
    const BinType& bin_type = instance().bin_type(bin_type_id);
    const Defect& defect = bin_type.defects[defect_id];
    Length w = instance().width(bin_type, o) - instance().right_trim(bin_type, o);
    Length h = instance().height(bin_type, o) - instance().top_trim(bin_type, o);
    Length min_waste = instance().min_waste();
    Length x_min = x3_prev(father, df) + min_waste;
    Length y_min = y2_prev(father, df) + min_waste;
    if (df <= -1)  // x1_prev is the left trim.
        if (instance().left_trim_type(bin_type, o) == TrimType::Soft)
            x_min = min_waste;
    if (df <= 0)  // y2_prev is the bottom trim.
        if (instance().bottom_trim_type(bin_type, o) == TrimType::Soft)
            y_min = min_waste;
    Length x = std::max(instance().right(defect, o), x_min);
    Length y = std::max(instance().top(defect, o), y_min);
    if (x > w || y > h) {
        return;
    }

    Insertion insertion {
        -1, -1, df,
        x, y, x,
        x1_max(father, df), y2_max(father, df, x), 1, 1};

    update(father, insertions, insertion);
}

void BranchingScheme::update(
        const Node& father, 
        std::vector<Insertion>& insertions,
        Insertion& insertion) const
{
    Length min_waste = instance().min_waste();
    Length cut_thickness = instance().cut_thickness();
    BinTypeId bin_type_id = instance().bin_type_id(i);
    const BinType& bin_type = instance().bin_type(bin_type_id);
    Length w_orig = instance().width(bin_type, o);
    Length h_orig = instance().height(bin_type, o);
    Length w = w_orig - instance().right_trim(bin_type, o);
    Length h = h_orig - instance().top_trim(bin_type, o);
    Length w_physical = (instance().right_trim_type(bin_type, o) == TrimType::Soft)? w_orig: w;
    Length h_physical = (instance().top_trim_type(bin_type, o) == TrimType::Soft)?  h_orig: h;

    // If there is no min_waste constraint, we set insertion.z1 and
    // insertion.z2 to 1 instead of 0 directly. This simplifies the handling of
    // the constraint when the cut thickness is non-null.
    if (min_waste == 1) {
        if (insertion.z1 == 0)
            insertion.z1 = 1;
        if (insertion.z2 == 0)
            insertion.z2 = 1;
    }

    // Update insertion.x1 and insertion.z1 with respect to min1cut()
    if ((insertion.item_type_id_1 != -1 || insertion.item_type_id_2 != -1)
            && insertion.x1 - x1_prev(father, insertion.df) < instance().min1cut()) {
        if (insertion.z1 == 0) {
            insertion.x1 = std::max(
                    insertion.x1 + cut_thickness + min_waste,
                    x1_prev(father, insertion.df) + instance().min1cut());
            insertion.z1 = 1;
        } else { // insertion.z1 = 1
            insertion.x1 = x1_prev(father, insertion.df) + instance().min1cut();
        }
    }

    // Update insertion.y2 and insertion.z2 with respect to min2cut()
    if ((insertion.item_type_id_1 != -1 || insertion.item_type_id_2 != -1)
            && insertion.y2 - y2_prev(father, insertion.df) < instance().min2cut()) {
        if (insertion.z2 == 0) {
            insertion.y2 = std::max(
                    insertion.y2 + cut_thickness + min_waste,
                    y2_prev(father, insertion.df) + instance().min2cut());
            insertion.z2 = 1;
        } else if (insertion.z2 == 1) {
            insertion.y2 = y2_prev(father, insertion.df) + instance().min2cut();
        } else { // insertion.z2 == 2
            return;
        }
    }

    // Update insertion.y2 and insertion.z2 with respect to one2cut()
    if (instance().one2cut() && insertion.df == 1
            && y2_prev(father, insertion.df) != 0 && insertion.y2 != h) {
        if (insertion.z2 == 0) {
            if (insertion.y2 + cut_thickness + min_waste > h_physical)
                return;
            insertion.y2 = h;
        } else if (insertion.z2 == 1) {
            insertion.y2 = h;
        } else { // insertion.z2 == 2
            return;
        }
    }

    // Update insertion.x1 if 2-staged
    if (instance().number_of_stages() == 2 && insertion.x1 != w) {
        if (insertion.z1 == 0) {
            if (insertion.x1 + cut_thickness + min_waste > w_physical)
                return;
            insertion.x1 = w;
        } else { // insertion.z1 == 1
            insertion.x1 = w;
        }
    }

    // Update insertion.x1 and insertion.z1 with respect to x1_curr() and z1().
    if (insertion.df >= 1) {
        if (insertion.z1 == 0) {
            if (insertion.x1 + min_waste <= father.x1_curr) {
                insertion.x1 = father.x1_curr;
                insertion.z1 = father.z1;
            } else if (insertion.x1 < father.x1_curr) { // x - min_waste < insertion.x1 < x
                if (father.z1 == 0) {
                    insertion.x1 = father.x1_curr + cut_thickness + min_waste;
                    insertion.z1 = 1;
                } else {
                    insertion.x1 = insertion.x1 + cut_thickness + min_waste;
                    insertion.z1 = 1;
                }
            } else if (insertion.x1 == father.x1_curr) {
            } else { // x1_curr() < insertion.x1
                if (father.z1 == 0 && insertion.x1 < father.x1_curr + min_waste) {
                    insertion.x1 = insertion.x1 + cut_thickness + min_waste;
                    insertion.z1 = 1;
                }
            }
        } else { // insertion.z1 == 1
            if (insertion.x1 <= father.x1_curr) {
                insertion.x1 = father.x1_curr;
                insertion.z1 = father.z1;
            } else { // x1_curr() < insertion.x1
                if (father.z1 == 0 && father.x1_curr + min_waste > insertion.x1)
                    insertion.x1 = father.x1_curr + cut_thickness + min_waste;
            }
        }
    }

    // Update insertion.y2 and insertion.z2 with respect to y2_curr() and z1().
    if (insertion.df == 2) {
        if (insertion.z2 == 0) {
            if (insertion.y2 + min_waste <= father.y2_curr) {
                insertion.y2 = father.y2_curr;
                insertion.z2 = father.z2;
            } else if (insertion.y2 < father.y2_curr) { // y_curr() - min_waste < insertion.y4 < y_curr()
                if (father.z2 == 2) {
                    return;
                } else if (father.z2 == 0) {
                    insertion.y2 = father.y2_curr + cut_thickness + min_waste;
                    insertion.z2 = 1;
                } else { // z2() == 1
                    insertion.y2 = insertion.y2 + cut_thickness + min_waste;
                    insertion.z2 = 1;
                }
            } else if (insertion.y2 == father.y2_curr) {
                if (father.z2 == 2)
                    insertion.z2 = 2;
            } else if (father.y2_curr < insertion.y2
                    && insertion.y2 < father.y2_curr + min_waste) {
                if (father.z2 == 2) {
                    return;
                } else if (father.z2 == 0) {
                    insertion.y2 = insertion.y2 + cut_thickness + min_waste;
                    insertion.z2 = 1;
                } else { // z2() == 1
                }
            } else { // y2_curr() + min_waste <= insertion.y2
                if (father.z2 == 2) {
                    return;
                }
            }
        } else if (insertion.z2 == 1) {
            if (insertion.y2 <= father.y2_curr) {
                insertion.y2 = father.y2_curr;
                insertion.z2 = father.z2;
            } else if (father.y2_curr < insertion.y2
                    && insertion.y2 < father.y2_curr + min_waste) {
                if (father.z2 == 2) {
                    return;
                } else if (father.z2 == 0) {
                    insertion.y2 = father.y2_curr + cut_thickness + min_waste;
                } else { // z2() == 1
                }
            } else {
                if (father.z2 == 2) {
                    return;
                }
            }
        } else { // insertion.z2 == 2
            if (insertion.y2 < father.y2_curr) {
                return;
            } else if (insertion.y2 == father.y2_curr) {
            } else if (father.y2_curr < insertion.y2
                    && insertion.y2 < father.y2_curr + cut_thickness + min_waste) {
                if (father.z2 == 2) {
                    return;
                } else if (father.z2 == 0) {
                    return;
                } else { // z2() == 1
                }
            } else { // y2_curr() + min_waste <= insertion.y2
                if (father.z2 == 2) {
                    return;
                }
            }
        }
    }

    // Update insertion.x1 and insertion.z1 with respect to defect intersections.
    if (!instance().cut_through_defects()) {
        for (;;) {
            DefectId defect_id = instance().rect_intersects_defect(
                    insertion.x1,
                    insertion.x1 + instance().cut_thickness(),
                    0,
                    h_orig,
                    bin_type,
                    o);
            if (defect_id == -1)
                break;
            const Defect& defect = bin_type.defects[defect_id];
            insertion.x1 = (insertion.z1 == 0)?
                std::max(instance().right(defect, o), insertion.x1 + cut_thickness + min_waste):
                instance().right(defect, o);
            insertion.z1 = 1;
        }
    }

    // Check max width
    if (insertion.x1 < w
            && min_waste > 1
            && insertion.x1 + cut_thickness > w_physical) {
        return;
    }

    // Increase width if too close from border
    if (insertion.x1 + cut_thickness < w
            && insertion.x1 + cut_thickness + min_waste > w_physical) {
        if (insertion.z1 == 1) {
            insertion.x1 = w;
            insertion.z1 = 0;
        } else { // insertion.z1 == 0
            return;
        }
    }

    // Check max width
    if (insertion.x1 > insertion.x1_max) {
        return;
    }

    // Update insertion.y2 and insertion.z2 with respect to defect intersections.
    bool y2_fixed = (insertion.z2 == 2 || (insertion.df == 2 && father.z2 == 2));

    for (;;) {
        bool found = false;

        // Increase y2 if it intersects a defect.
        if (!instance().cut_through_defects()) {
            DefectId defect_id = instance().rect_intersects_defect(
                    x1_prev(father, insertion.df),
                    insertion.x1,
                    insertion.y2,
                    insertion.y2 + cut_thickness,
                    bin_type,
                    o);
            if (defect_id != -1) {
                const Defect& defect = bin_type.defects[defect_id];
                if (y2_fixed) {
                    return;
                }
                insertion.y2 = (insertion.z2 == 0)?
                    std::max(
                            instance().top(defect, o),
                            insertion.y2 + cut_thickness + min_waste):
                    instance().top(defect, o);
                insertion.z2 = 1;
                found = true;
            }
        }

        // Increase y2 if an item on top of its third-level sub-plate
        // intersects a defect.
        if (insertion.df == 2) {
            for (auto jrx: father.subplate2curr_items_above_defect) {
                const ItemType& item_type = instance().item_type(jrx.item_type_id);
                Length h_j2 = instance().height(item_type, jrx.rotate, o);
                Length l = jrx.x;
                DefectId defect_id = instance().item_intersects_defect(
                        l,
                        insertion.y2 - h_j2,
                        item_type,
                        jrx.rotate,
                        bin_type,
                        o);
                if (defect_id >= 0) {
                    const Defect& defect = bin_type.defects[defect_id];
                    if (y2_fixed) {
                        return;
                    }
                    insertion.y2 = (insertion.z2 == 0)?
                        std::max(
                                instance().top(defect, o) + h_j2,
                                insertion.y2 + cut_thickness + min_waste):
                        instance().top(defect, o) + h_j2;
                    insertion.z2 = 1;
                    found = true;
                }
            }
        }
        if (insertion.item_type_id_1 == -1 && insertion.item_type_id_2 != -1) {
            const ItemType& item_type = instance().item_type(insertion.item_type_id_2);
            Length w_j = insertion.x3 - x3_prev(father, insertion.df);
            bool rotate_j2 = (instance().width(item_type, true, o) == w_j);
            Length h_j2 = instance().height(item_type, rotate_j2, o);
            Length l = x3_prev(father, insertion.df);
            DefectId defect_id = instance().item_intersects_defect(
                    l,
                    insertion.y2 - h_j2,
                    item_type,
                    rotate_j2,
                    bin_type,
                    o);
            if (defect_id >= 0) {
                const Defect& defect = bin_type.defects[defect_id];
                if (y2_fixed) {
                    return;
                }
                insertion.y2 = (insertion.z2 == 0)?
                    std::max(
                            instance().top(defect, o) + h_j2,
                            insertion.y2 + cut_thickness + min_waste):
                    instance().top(defect, o) + h_j2;
                insertion.z2 = 1;
                found = true;
            }
        }
        if (!found)
            break;
    }

    // Now check bin's height
    if (insertion.y2 + cut_thickness < h
            && insertion.y2 + cut_thickness + min_waste > h_physical) {
        if (insertion.z2 == 1) {
            insertion.y2 = h;
            insertion.z2 = 0;

            if (insertion.df == 2) {
                for (auto jrx: father.subplate2curr_items_above_defect) {
                    const ItemType& item_type = instance().item_type(jrx.item_type_id);
                    Length l = jrx.x;
                    Length h_j2 = instance().height(item_type, jrx.rotate, o);
                    DefectId defect_id = instance().item_intersects_defect(
                            l,
                            insertion.y2 - h_j2,
                            item_type,
                            jrx.rotate,
                            bin_type,
                            o);
                    if (defect_id >= 0) {
                        return;
                    }
                }
            }

            if (insertion.item_type_id_1 == -1 && insertion.item_type_id_2 != -1) {
                const ItemType& item_type = instance().item_type(insertion.item_type_id_2);
                Length w_j = insertion.x3 - x3_prev(father, insertion.df);
                bool rotate_j2 = (instance().width(item_type, true, o) == w_j);
                Length h_j2 = instance().height(item_type, rotate_j2, o);
                Length l = x3_prev(father, insertion.df);
                DefectId defect_id = instance().item_intersects_defect(
                        l,
                        insertion.y2 - h_j2,
                        item_type,
                        rotate_j2,
                        bin_type,
                        o);
                if (defect_id >= 0) {
                    return;
                }
            }

        } else { // insertion.z2 == 0 or insertion.z2 == 2
            return;
        }
    }

    // Check max height
    if (insertion.y2 < h
            && min_waste > 1
            && insertion.y2 + cut_thickness > h_physical) {
        return;
    }

    // Check max height
    if (insertion.y2 > insertion.y2_max) {
        return;
    }

    // Check if 3-cut is cutting through a defect when cutting through a defect
    // is not allowed and cut thickness is non-null.
    if (!instance().cut_through_defects() && cut_thickness > 0) {
        DefectId defect_id = instance().rect_intersects_defect(
                insertion.x3,
                insertion.x3 + cut_thickness,
                y2_prev(father, insertion.df),
                insertion.y2,
                bin_type,
                o);
        if (defect_id != -1)
            return;
    }

    // Check dominance
    for (auto it = insertions.begin(); it != insertions.end();) {
        bool b = true;
        if (insertion.item_type_id_1 == -1 && insertion.item_type_id_2 == -1
                && it->item_type_id_1 == -1 && it->item_type_id_2 == -1) {
            if (insertion.df != -1 && insertion.x1 == it->x1 && insertion.y2 == it->y2 && insertion.x3 == it->x3) {
                return;
            }
        }
        if ((it->item_type_id_1 != -1 || it->item_type_id_2 != -1)
                && (insertion.item_type_id_1 == -1 || insertion.item_type_id_1 == it->item_type_id_1 || insertion.item_type_id_1 == it->item_type_id_2)
                && (insertion.item_type_id_2 == -1 || insertion.item_type_id_2 == it->item_type_id_2 || insertion.item_type_id_2 == it->item_type_id_2)) {
            if (dominates(front(father, *it), front(father, insertion))) {
                return;
            }
        }
        if ((insertion.item_type_id_1 != -1 || insertion.item_type_id_2 != -1)
                && (it->item_type_id_1 == insertion.item_type_id_1 || it->item_type_id_1 == insertion.item_type_id_2)
                && (it->item_type_id_2 == insertion.item_type_id_2 || it->item_type_id_2 == insertion.item_type_id_1)) {
            if (dominates(front(father, insertion), front(father, *it))) {
                if (std::next(it) != insertions.end()) {
                    *it = insertions.back();
                    insertions.pop_back();
                    b = false;
                } else {
                    insertions.pop_back();
                    break;
                }
            }
        }
        if (b)
            ++it;
    }

    insertions.push_back(insertion);
}

////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////// Export ////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/*
bool BranchingScheme::BranchingScheme::check(
        const std::vector<SolutionNode>& nodes) const
{
    std::vector<ItemPos> items(instance().number_of_items(), 0);

    for (const SolutionNode& node: nodes) {
        const BinType& bin_type = instance().bin(node.i);
        Length w = bin_type.rect.w;
        Length h = bin_type.rect.h;

        // TODO Check tree consistency

        // Check node logic.
        if (node.r <= node.l
                || node.t <= node.b) {
            std::cerr << "\033[31m" << "ERROR, "
                "Node " << node << " inconsistent"
                << "\033[0m" << std::endl;
            return false;
        }

        // Check defect intersection
        if (!instance().cut_through_defects()) {
            for (Defect defect: instance().bin(node.i).defects) {
                Length l = defect.pos.x;
                Length r = defect.pos.x + defect.rect.w;
                Length b = defect.pos.y;
                Length t = defect.pos.y + defect.rect.h;
                if (
                           (node.l > l && node.l < r && node.b < t && node.t > b)
                        || (node.r > l && node.r < r && node.b < t && node.t > b)
                        || (node.b > b && node.b < t && node.l < r && node.r > l)
                        || (node.t > b && node.t < t && node.l < r && node.r > l)) {
                    std::cerr << "\033[31m" << "ERROR, "
                        "Node " << node << " cut intersects defect " << defect
                        << "\033[0m" << std::endl;
                    assert(false);
                    return false;
                }
            }
        }

        if (node.j >= 0) {
            // TODO Check item order

            // Check item type copy number
            items[node.j]++;
            if (items[node.j] > instance().item_type(node.j).copies) {
                std::cerr << "\033[31m" << "ERROR, "
                    "item " << node.j << " produced more that one time"
                    << "\033[0m" << std::endl;
                return false;
            }

            // Check if item j contains a defect
            for (Defect defect: instance().bin(node.i).defects) {
                Coord c1;
                c1.x = node.l;
                c1.y = node.b;
                Rectangle r1;
                r1.w = node.r - node.l;
                r1.h = node.t - node.b;
                if (rect_intersection(c1, r1, defect.pos, defect.rect)) {
                    std::cerr << "\033[31m" << "ERROR, "
                        "Node " << node << " intersects defect " << defect
                        << "\033[0m" << std::endl;
                    assert(false);
                    return false;
                }
            }

        } else if (node.j == -1) {
            // Check minimum waste constraint
            if ((node.r <= w - bin_type.right_trim)
                    && (node.t <= h - bin_type.top_trim)
                    && (node.l >= bin_type.left_trim)
                    && (node.b >= bin_type.bottom_trim)
                    && (node.r != w - bin_type.right_trim
                        || bin_type.right_trim_type == TrimType::Hard)
                    && (node.t != h - bin_type.top_trim
                        || bin_type.top_trim_type == TrimType::Hard)
                    && (node.l != bin_type.left_trim
                        || bin_type.left_trim_type == TrimType::Hard)
                    && (node.b != bin_type.bottom_trim
                        || bin_type.bottom_trim_type == TrimType::Hard)
                    && (node.r - node.l < instance().min_waste()
                        || node.t - node.b < instance().min_waste())) {
                std::cerr << "\033[31m" << "ERROR, "
                    "Node " << node << " violates min_waste constraint"
                    << "\033[0m" << std::endl;
                return false;
            }
        }

    }

    return true;
}
*/

Solution BranchingScheme::to_solution(
        const Node& node) const
{
    std::vector<const BranchingScheme::Node*> descendents;
    for (const Node* current_node = &node;
            current_node->father != nullptr;
            current_node = current_node->father.get()) {
        descendents.push_back(current_node);
    }
    std::reverse(descendents.begin(), descendents.end());

    Solution solution(instance());
    std::vector<SolutionNode> nodes;

    SolutionNodeId subplate0_curr = -1;
    SolutionNodeId subplate1_curr = -1;
    SolutionNodeId subplate2_curr = -1;
    SolutionNodeId subplate3_curr = -1;
    Length subplate1_curr_x1 = -1;
    Length subplate2_curr_y2 = -1;
    for (SolutionNodeId node_pos = 0;
            node_pos < (SolutionNodeId)descendents.size();
            ++node_pos) {

        const Node* current_node = descendents[node_pos];
        BinPos i = current_node->number_of_bins - 1;
        BinTypeId bin_type_id = instance().bin_type_id(i);
        const BinType& bin_type = instance().bin_type(bin_type_id);
        CutOrientation o = current_node->first_stage_orientation;
        bool has_item = (current_node->item_type_id_1 != -1 || current_node->item_type_id_2 != -1);
        Depth df_next = (node_pos < (SolutionNodeId)descendents.size() - 1)?
            descendents[node_pos + 1]->df: -1;
        //std::cout << ">>> node"
        //    << " o " << o
        //    << " df " << current_node->df
        //    << " x1p " << current_node->x1_prev
        //    << " x1c " << current_node->x1_curr
        //    << " y2p " << current_node->y2_prev
        //    << " y2c " << current_node->y2_curr
        //    << " x3c " << current_node->x3_curr
        //    << " z1 " << current_node->z1
        //    << " z2 " << current_node->z2
        //    << " j1 " << current_node->item_type_id_1
        //    << " j2 " << current_node->item_type_id_2
        //    << std::endl;

        // We don't want sub-plates with children containing only waste.
        bool b1 = (current_node->df <= -1) && (df_next <= -1) && (!has_item);
        bool b2 = (current_node->df <= 0) && (df_next <= 0) && (!has_item);
        bool b3 = (current_node->df <= 1) && (df_next <= 1) && (!has_item);

        // Create a new bin.
        if (current_node->df <= -1) {
            //std::cout << "Create a new bin..." << std::endl;
            nodes.push_back(SolutionNode());
            SolutionNode& n = nodes.back();
            SolutionNodeId id = nodes.size() - 1;
            n.id = id;
            n.d = 0;
            n.f = -1;
            n.l = 0;
            n.r = bin_type.rect.w;
            n.b = 0;
            n.t = bin_type.rect.h;
            n.item_type_id = (b1)? -1: -2;
            subplate0_curr = id;
            NodeId father_id = id;

            // Trims.
            if (bin_type.left_trim > 0
                    || bin_type.right_trim > 0
                    || bin_type.bottom_trim > 0
                    || bin_type.top_trim > 0) {

                if (bin_type.left_trim != 0 && bin_type.bottom_trim != 0) {
                    nodes.push_back(SolutionNode());
                    SolutionNode& n = nodes.back();
                    SolutionNodeId id = nodes.size() - 1;
                    n.id = id;
                    n.d = -1;
                    n.f = father_id;
                    n.l = 0;
                    n.r = bin_type.left_trim;
                    n.b = 0;
                    n.t = bin_type.bottom_trim;
                    n.item_type_id = -1;
                }

                if (bin_type.left_trim != 0 && bin_type.top_trim != 0) {
                    nodes.push_back(SolutionNode());
                    SolutionNode& n = nodes.back();
                    SolutionNodeId id = nodes.size() - 1;
                    n.id = id;
                    n.d = -1;
                    n.f = father_id;
                    n.l = 0;
                    n.r = bin_type.left_trim;
                    n.b = bin_type.rect.h - bin_type.top_trim;
                    n.t = bin_type.rect.h;
                    n.item_type_id = -1;
                }

                if (bin_type.right_trim != 0 && bin_type.bottom_trim != 0) {
                    nodes.push_back(SolutionNode());
                    SolutionNode& n = nodes.back();
                    SolutionNodeId id = nodes.size() - 1;
                    n.id = id;
                    n.d = -1;
                    n.f = father_id;
                    n.l = bin_type.rect.w - bin_type.right_trim;
                    n.r = bin_type.rect.w;
                    n.b = 0;
                    n.t = bin_type.bottom_trim;
                    n.item_type_id = -1;
                }

                if (bin_type.right_trim != 0 && bin_type.top_trim != 0) {
                    nodes.push_back(SolutionNode());
                    SolutionNode& n = nodes.back();
                    SolutionNodeId id = nodes.size() - 1;
                    n.id = id;
                    n.d = -1;
                    n.f = father_id;
                    n.l = bin_type.rect.w - bin_type.right_trim;
                    n.r = bin_type.rect.w;
                    n.b = bin_type.rect.h - bin_type.top_trim;
                    n.t = bin_type.rect.h;
                    n.item_type_id = -1;
                }

                if (bin_type.left_trim != 0) {
                    nodes.push_back(SolutionNode());
                    SolutionNode& n = nodes.back();
                    SolutionNodeId id = nodes.size() - 1;
                    n.id = id;
                    n.d = -1;
                    n.f = father_id;
                    n.l = 0;
                    n.r = bin_type.left_trim;
                    n.b = bin_type.bottom_trim;
                    n.t = bin_type.rect.h - bin_type.top_trim;
                    n.item_type_id = -1;
                }

                if (bin_type.bottom_trim != 0) {
                    nodes.push_back(SolutionNode());
                    SolutionNode& n = nodes.back();
                    SolutionNodeId id = nodes.size() - 1;
                    n.id = id;
                    n.d = -1;
                    n.f = father_id;
                    n.l = bin_type.left_trim;
                    n.r = bin_type.rect.w - bin_type.right_trim;
                    n.b = 0;
                    n.t = bin_type.bottom_trim;
                    n.item_type_id = -1;
                }

                if (bin_type.right_trim != 0) {
                    nodes.push_back(SolutionNode());
                    SolutionNode& n = nodes.back();
                    SolutionNodeId id = nodes.size() - 1;
                    n.id = id;
                    n.d = -1;
                    n.f = father_id;
                    n.l = bin_type.rect.w - bin_type.right_trim;
                    n.r = bin_type.rect.w;
                    n.b = bin_type.bottom_trim;
                    n.t = bin_type.rect.h - bin_type.top_trim;
                    n.item_type_id = -1;
                }

                if (bin_type.top_trim != 0) {
                    nodes.push_back(SolutionNode());
                    SolutionNode& n = nodes.back();
                    SolutionNodeId id = nodes.size() - 1;
                    n.id = id;
                    n.d = -1;
                    n.f = father_id;
                    n.l = bin_type.left_trim;
                    n.r = bin_type.rect.w - bin_type.right_trim;
                    n.b = bin_type.rect.h - bin_type.top_trim;
                    n.t = bin_type.rect.h;
                    n.item_type_id = -1;
                }

                nodes.push_back(SolutionNode());
                SolutionNode& n = nodes.back();
                SolutionNodeId id = nodes.size() - 1;
                n.id = id;
                n.d = -1;
                n.f = father_id;
                n.l = bin_type.left_trim;
                n.r = bin_type.rect.w - bin_type.right_trim;
                n.b = bin_type.bottom_trim;
                n.t = bin_type.rect.h - bin_type.top_trim;
                n.item_type_id = (b1)? -1: -2;
                subplate0_curr = id;
            }

            //std::cout << n << std::endl;
        }

        // Create a new first-level sub-plate.
        if (instance().number_of_stages() == 2
                || current_node->df >= 1) {
        } else if (b1) {
            subplate1_curr = -1;
        } else {
            //std::cout << "Create a new first-level sub-plate..." << std::endl;
            // Find subplate1_curr_x1.
            subplate1_curr_x1 = current_node->x1_curr;
            for (SolutionNodeId node_pos_2 = node_pos + 1;
                    node_pos_2 < (SolutionNodeId)descendents.size()
                    && descendents[node_pos_2]->df >= 1;
                    node_pos_2++) {
                subplate1_curr_x1 = descendents[node_pos_2]->x1_curr;
            }
            //std::cout << "subplate1_curr_x1 " << subplate1_curr_x1 << std::endl;

            nodes.push_back(SolutionNode());
            SolutionNode& n = nodes.back();
            SolutionNodeId id = nodes.size() - 1;
            n.id = id;
            n.d = 1;
            n.f = subplate0_curr;
            if (o == CutOrientation::Vertical) {
                n.l = current_node->x1_prev;
                n.r = subplate1_curr_x1;
                n.b = nodes[subplate0_curr].b;
                n.t = nodes[subplate0_curr].t;
            } else {
                n.l = nodes[subplate0_curr].l;
                n.r = nodes[subplate0_curr].r;
                n.b = current_node->x1_prev;
                n.t = subplate1_curr_x1;
            }
            n.item_type_id = (b2)? -1: -2;
            nodes[subplate0_curr].children.push_back(id);
            subplate1_curr = id;
            //std::cout << n << std::endl;
        }

        // Create a new second-level sub-plate.
        if (current_node->df >= 2) {
        } else if (b2) {
            subplate2_curr = -1;
        } else {
            //std::cout << "Create a new second-level sub-plate..." << std::endl;
            // Find subplate1_curr_y2.
            subplate2_curr_y2 = current_node->y2_curr;
            for (SolutionNodeId node_pos_2 = node_pos + 1;
                    node_pos_2 < (SolutionNodeId)descendents.size()
                    && descendents[node_pos_2]->df >= 2;
                    node_pos_2++) {
                subplate2_curr_y2 = descendents[node_pos_2]->y2_curr;
            }
            //std::cout << "subplate2_curr_y2 " << subplate2_curr_y2 << std::endl;

            nodes.push_back(SolutionNode());
            SolutionNode& n = nodes.back();
            SolutionNodeId id = nodes.size() - 1;
            n.id = id;
            SolutionNodeId s = (instance().number_of_stages() == 3)? subplate1_curr: subplate0_curr;
            n.d = (instance().number_of_stages() != 2)? 2: 1;
            n.f = s;
            nodes[s].children.push_back(id);
            if (o == CutOrientation::Vertical) {
                n.l = nodes[s].l;
                n.r = nodes[s].r;
                n.b = current_node->y2_prev;
                n.t = subplate2_curr_y2;
            } else {
                n.l = current_node->y2_prev;
                n.r = subplate2_curr_y2;
                n.b = nodes[s].b;
                n.t = nodes[s].t;
            }
            n.item_type_id = (b3)? -1: -2;
            subplate2_curr = id;
            //std::cout << n << std::endl;
        }

        // Create a new third-level sub-plate.
        if (b3) {
            subplate3_curr = -1;
        } else {
            //std::cout << "Create a new third-level sub-plate..." << std::endl;
            nodes.push_back(SolutionNode());
            SolutionNode& n = nodes.back();
            SolutionNodeId id = nodes.size() - 1;
            n.id = id;
            n.d = (instance().number_of_stages() != 2)? 3: 2;
            n.f = subplate2_curr;
            nodes[subplate2_curr].children.push_back(id);
            if (o == CutOrientation::Vertical) {
                n.l = x3_prev(*current_node->father, current_node->df);
                n.r = current_node->x3_curr;
                n.b = nodes[subplate2_curr].b;
                n.t = nodes[subplate2_curr].t;
            } else {
                n.l = nodes[subplate2_curr].l;
                n.r = nodes[subplate2_curr].r;
                n.b = x3_prev(*current_node->father, current_node->df);
                n.t = current_node->x3_curr;
            }
            n.item_type_id = (has_item)? -2: -1;
            subplate3_curr = id;
            //std::cout << n << std::endl;
        }

        // Create a new fourth-level sub-plate.
        if (has_item) {
            //std::cout << "Create a new fourth-level sub-plate..." << std::endl;
            nodes.push_back(SolutionNode());
            SolutionNode& n = nodes.back();
            SolutionNodeId id = nodes.size() - 1;
            n.id = id;
            n.d = (instance().number_of_stages() != 2)? 4: 3;
            n.f = subplate3_curr;
            nodes[subplate3_curr].children.push_back(id);
            if (o == CutOrientation::Vertical) {
                Length w_tmp = nodes[subplate3_curr].r - nodes[subplate3_curr].l;

                n.l = nodes[subplate3_curr].l;
                n.r = nodes[subplate3_curr].r;
                n.b = nodes[subplate3_curr].b;
                if (current_node->item_type_id_1 != -1) {
                    //std::cout << instance().item_type(current_node->item_type_id_1) << std::endl;
                    Length hj = (w_tmp == instance().item_type(current_node->item_type_id_1).rect.w)?
                        instance().item_type(current_node->item_type_id_1).rect.h:
                        instance().item_type(current_node->item_type_id_1).rect.w;
                    n.t = nodes[subplate3_curr].b + hj;
                } else if (current_node->item_type_id_2 != -1) {
                    Length hj = (w_tmp == instance().item_type(current_node->item_type_id_2).rect.w)?
                        instance().item_type(current_node->item_type_id_2).rect.h:
                        instance().item_type(current_node->item_type_id_2).rect.w;
                    n.t = nodes[subplate3_curr].t - hj;
                } else {
                    n.t = nodes[subplate3_curr].t;
                }
            } else {
                Length h_tmp = nodes[subplate3_curr].t - nodes[subplate3_curr].b;

                n.l = nodes[subplate3_curr].l;
                if (current_node->item_type_id_1 != -1) {
                    //std::cout << instance().item_type(current_node->item_type_id_1) << std::endl;
                    Length wj = (h_tmp == instance().item_type(current_node->item_type_id_1).rect.h)?
                        instance().item_type(current_node->item_type_id_1).rect.w:
                        instance().item_type(current_node->item_type_id_1).rect.h;
                    n.r = nodes[subplate3_curr].l + wj;
                } else if (current_node->item_type_id_2 != -1) {
                    Length wj = (h_tmp == instance().item_type(current_node->item_type_id_2).rect.h)?
                        instance().item_type(current_node->item_type_id_2).rect.w:
                        instance().item_type(current_node->item_type_id_2).rect.h;
                    n.r = nodes[subplate3_curr].r - wj;
                } else {
                    n.r = nodes[subplate3_curr].r;
                }
                n.b = nodes[subplate3_curr].b;
                n.t = nodes[subplate3_curr].t;
            }
            n.item_type_id = (current_node->item_type_id_1 != -1)? current_node->item_type_id_1: -1;
            //std::cout << n << std::endl;

            // Add an additional fourth-level sub-plate if necessary.
            Length t = nodes.back().t;
            Length r = nodes.back().r;
            if (t != nodes[subplate3_curr].t) {
                //std::cout << "Create an additional fourth-level sub-plate..." << std::endl;
                nodes.push_back(SolutionNode());
                SolutionNode& n = nodes.back();
                SolutionNodeId id = nodes.size() - 1;
                n.id = id;
                n.d = (instance().number_of_stages() != 2)? 4: 3;
                n.f = subplate3_curr;
                nodes[subplate3_curr].children.push_back(id);
                n.l = nodes[subplate3_curr].l;
                n.r = nodes[subplate3_curr].r;
                n.t = nodes[subplate3_curr].t;
                if (current_node->item_type_id_2 == -1) {
                    n.b = t;
                    n.item_type_id = -1;
                } else {
                    Length w_tmp = nodes[subplate3_curr].r - nodes[subplate3_curr].l;
                    Length hj = (w_tmp == instance().item_type(current_node->item_type_id_2).rect.w)?
                        instance().item_type(current_node->item_type_id_2).rect.h:
                        instance().item_type(current_node->item_type_id_2).rect.w;
                    n.b = n.t - hj;
                    n.item_type_id = current_node->item_type_id_2;
                }
                //std::cout << n << std::endl;
            }
            if (r != nodes[subplate3_curr].r) {
                //std::cout << "Create an additional fourth-level sub-plate..." << std::endl;
                nodes.push_back(SolutionNode());
                SolutionNode& n = nodes.back();
                SolutionNodeId id = nodes.size() - 1;
                n.id = id;
                n.d = (instance().number_of_stages() != 2)? 4: 3;
                n.f = subplate3_curr;
                nodes[subplate3_curr].children.push_back(id);
                n.r = nodes[subplate3_curr].r;
                n.b = nodes[subplate3_curr].b;
                n.t = nodes[subplate3_curr].t;
                if (current_node->item_type_id_2 == -1) {
                    n.l = r;
                    n.item_type_id = -1;
                } else {
                    Length h_tmp = nodes[subplate3_curr].t - nodes[subplate3_curr].b;
                    Length wj = (h_tmp == instance().item_type(current_node->item_type_id_1).rect.h)?
                        instance().item_type(current_node->item_type_id_1).rect.w:
                        instance().item_type(current_node->item_type_id_1).rect.h;
                    n.l = n.r - wj;
                    n.item_type_id = current_node->item_type_id_2;
                }
                //std::cout << n << std::endl;
            }
        }

        // Add waste to the right of the 2-level sub-plate.
        if (df_next <= 1
                && subplate3_curr != -1
                && nodes[subplate3_curr].r != nodes[subplate2_curr].r) {
            //std::cout << "Add waste to the right of the second-level sub-plate..." << std::endl;
            nodes.push_back(SolutionNode());
            SolutionNode& n = nodes.back();
            SolutionNodeId id = nodes.size() - 1;
            n.id = id;
            n.d = (instance().number_of_stages() != 2)? 3: 2;
            n.f = subplate2_curr;
            nodes[subplate2_curr].children.push_back(id);
            n.l = nodes[subplate3_curr].r;
            n.r = nodes[subplate2_curr].r;
            n.b = nodes[subplate2_curr].b;
            n.t = nodes[subplate2_curr].t;
            n.item_type_id = -1;
            //std::cout << n << std::endl;
        }
        if (df_next <= 1
                && subplate3_curr != -1
                && nodes[subplate3_curr].t != nodes[subplate2_curr].t) {
            //std::cout << "Add waste to the right of the second-level sub-plate..." << std::endl;
            nodes.push_back(SolutionNode());
            SolutionNode& n = nodes.back();
            SolutionNodeId id = nodes.size() - 1;
            n.id = id;
            n.d = (instance().number_of_stages() != 2)? 3: 2;
            n.f = subplate2_curr;
            nodes[subplate2_curr].children.push_back(id);
            n.l = nodes[subplate2_curr].l;
            n.r = nodes[subplate2_curr].r;
            n.b = nodes[subplate3_curr].t;
            n.t = nodes[subplate2_curr].t;
            n.item_type_id = -1;
            //std::cout << n << std::endl;
        }

        // Add waste at the top of the 1-level sub-plate.
        SolutionNodeId s = (instance().number_of_stages() == 3)? subplate1_curr: subplate0_curr;
        if (df_next <= 0
                && subplate2_curr != -1
                && nodes[subplate2_curr].t != nodes[s].t) {
            //std::cout << "Add waste at the top of the first-level sub-plate..." << std::endl;
            nodes.push_back(SolutionNode());
            SolutionNode& n = nodes.back();
            SolutionNodeId id = nodes.size() - 1;
            n.id = id;
            n.d = (instance().number_of_stages() != 2)? 2: 1;
            n.f = s;
            nodes[s].children.push_back(id);
            n.l = nodes[s].l;
            n.r = nodes[s].r;
            n.b = nodes[subplate2_curr].t;
            n.t = nodes[s].t;
            // Might be the residual if two-staged.
            n.item_type_id = (instance().number_of_stages() == 2 && node_pos == (SolutionNodeId)descendents.size() - 1)? -3: -1;
            //std::cout << n << std::endl;
        }
        if (df_next <= 0
                && subplate2_curr != -1
                && nodes[subplate2_curr].r != nodes[s].r) {
            //std::cout << "Add waste at the top of the first-level sub-plate..." << std::endl;
            nodes.push_back(SolutionNode());
            SolutionNode& n = nodes.back();
            SolutionNodeId id = nodes.size() - 1;
            n.id = id;
            n.d = (instance().number_of_stages() != 2)? 2: 1;
            n.f = s;
            nodes[s].children.push_back(id);
            n.l = nodes[subplate2_curr].r;
            n.r = nodes[s].r;
            n.b = nodes[s].b;
            n.t = nodes[s].t;
            // Might be the residual if two-staged.
            n.item_type_id = (instance().number_of_stages() == 2 && node_pos == (SolutionNodeId)descendents.size() - 1)? -3: -1;
            //std::cout << n << std::endl;
        }

        // Add waste to the right of the plate.
        if (instance().number_of_stages() != 2
                && df_next <= -1
                && subplate1_curr != -1
                && nodes[subplate1_curr].r != nodes[subplate0_curr].r) {
            //std::cout << "Add waste to the right of the plate..." << std::endl;
            nodes.push_back(SolutionNode());
            SolutionNode& n = nodes.back();
            SolutionNodeId id = nodes.size() - 1;
            n.id = id;
            n.d = 1;
            n.f = subplate0_curr;
            nodes[subplate0_curr].children.push_back(id);
            n.l = nodes[subplate1_curr].r;
            n.r = nodes[subplate0_curr].r;
            n.b = nodes[subplate0_curr].b;
            n.t = nodes[subplate0_curr].t;
            // Might be the residual.
            n.item_type_id = (node_pos < (SolutionNodeId)descendents.size() - 1)? -1: -3;
            //std::cout << n << std::endl;
        }
        if (instance().number_of_stages() != 2
                && df_next <= -1
                && subplate1_curr != -1
                && nodes[subplate1_curr].t != nodes[subplate0_curr].t) {
            //std::cout << "Add waste to the right of the plate..." << std::endl;
            nodes.push_back(SolutionNode());
            SolutionNode& n = nodes.back();
            SolutionNodeId id = nodes.size() - 1;
            n.id = id;
            n.d = 1;
            n.f = subplate0_curr;
            nodes[subplate0_curr].children.push_back(id);
            n.l = nodes[subplate0_curr].l;
            n.r = nodes[subplate0_curr].r;
            n.b = nodes[subplate1_curr].t;
            n.t = nodes[subplate0_curr].t;
            // Might be the residual.
            n.item_type_id = (node_pos < (SolutionNodeId)descendents.size() - 1)? -1: -3;
            //std::cout << n << std::endl;
        }

        // If the next node is a new bin or this is the last node, add the bin
        // to the solution.
        if (df_next < 0) {
            solution.add_bin(bin_type.id, nodes);
            nodes.clear();
        }

    }

    return solution;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

bool BranchingScheme::Insertion::operator==(const Insertion& insertion) const
{
    return ((item_type_id_1 == insertion.item_type_id_1)
            && (item_type_id_2 == insertion.item_type_id_2)
            && (df == insertion.df)
            && (x1 == insertion.x1)
            && (y2 == insertion.y2)
            && (x3 == insertion.x3)
            && (x1_max == insertion.x1_max)
            && (y2_max == insertion.y2_max)
            && (z1 == insertion.z1)
            && (z2 == insertion.z2)
            );
}

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream& os,
        const BranchingScheme::Insertion& insertion)
{
    os << "item_type_id_1 " << insertion.item_type_id_1
        << " item_type_id_2 " << insertion.item_type_id_2
        << " df " << insertion.df
        << " x1 " << insertion.x1
        << " y2 " << insertion.y2
        << " x3 " << insertion.x3
        << " x1_max " << insertion.x1_max
        << " y2_max " << insertion.y2_max
        << " z1 " << insertion.z1
        << " z2 " << insertion.z2
        ;
    return os;
}

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream& os,
        const std::vector<BranchingScheme::Insertion>& insertions)
{
    std::copy(insertions.begin(), insertions.end(), std::ostream_iterator<BranchingScheme::Insertion>(os, "\n"));
    return os;
}

bool BranchingScheme::Front::operator==(const Front& front) const
{
    return ((i == front.i)
            && (o == front.o)
            && (x1_prev == front.x1_prev)
            && (x3_curr == front.x3_curr)
            && (x1_curr == front.x1_curr)
            && (y2_prev == front.y2_prev)
            && (y2_curr == front.y2_curr)
            );
}

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream& os,
        const BranchingScheme::Front& front)
{
    os << "i " << front.i
        << " o " << front.o
        << " x1_prev " << front.x1_prev
        << " x3_curr " << front.x3_curr
        << " x1_curr " << front.x1_curr
        << " y2_prev " << front.y2_prev
        << " y2_curr " << front.y2_curr
        ;
    return os;
}

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream& os,
        const BranchingScheme::Node& node)
{
    os << "number_of_items " << node.number_of_items
        << " number_of_bins " << node.number_of_bins
        << std::endl;
    os << "item_area " << node.item_area
        << " current_area " << node.current_area
        << std::endl;
    os << "waste " << node.waste
        << " profit " << node.profit
        << std::endl;
    os << "x1_curr " << node.x1_curr
        << " x1_prev " << node.x1_prev
        << " y2_curr " << node.y2_curr
        << " y2_prev " << node.y2_prev
        << " x3_curr " << node.x3_curr
        << std::endl;

    os << "pos_stack" << std::flush;
    for (ItemPos j_pos: node.pos_stack)
        os << " " << j_pos;
    os << std::endl;

    return os;
}
