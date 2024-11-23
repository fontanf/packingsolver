#include "rectangleguillotine/branching_scheme.hpp"

#include "rectangleguillotine/solution_builder.hpp"

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
    CutOrientation first_stage_orientation = (instance.parameters().first_stage_orientation != CutOrientation::Any)?
        instance.parameters().first_stage_orientation:
        parameters.first_stage_orientation;
    if (instance.parameters().number_of_stages == 3) {
        first_stage_orientation_ = first_stage_orientation;
    } else if (instance.parameters().number_of_stages == 2) {
        if (first_stage_orientation == CutOrientation::Horizontal) {
            first_stage_orientation_ = CutOrientation::Vertical;
        } else if (first_stage_orientation == CutOrientation::Vertical) {
            first_stage_orientation_ = CutOrientation::Horizontal;
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
    return std::make_shared<Node>(node);
}

bool BranchingScheme::bound(
        const std::shared_ptr<Node>& node_1,
        const std::shared_ptr<Node>& node_2) const
{
    switch (instance().objective()) {
    case Objective::Default: {
        if (!leaf(node_2)) {
            return (ubkp(*node_1) <= node_2->profit);
        } else {
            if (ubkp(*node_1) != node_2->profit)
                return (ubkp(*node_1) <= node_2->profit);
            return node_1->waste >= node_2->waste;
        }
    } case Objective::BinPacking: case Objective::VariableSizedBinPacking: {
        if (!leaf(node_2))
            return false;
        BinPos bin_pos = -1;
        Area a = instance().item_area() + node_1->waste;
        while (a > 0) {
            bin_pos++;
            if (bin_pos >= instance().number_of_bins())
                return true;
            BinTypeId bin_type_id = instance().bin_type_id(bin_pos);
            a -= instance().bin_type(bin_type_id).area();
        }
        return (bin_pos + 1 >= node_2->number_of_bins);
    } case Objective::BinPackingWithLeftovers: {
        if (!leaf(node_2))
            return false;
        return node_1->waste >= node_2->waste;
    } case Objective::Knapsack: {
        return ubkp(*node_1) <= node_2->profit;
    } case Objective::OpenDimensionX: {
        if (!leaf(node_2))
            return false;
        return std::max(width(*node_1), (node_1->waste + instance().item_area() - 1) / instance().height(instance().bin_type(0), CutOrientation::Vertical) + 1) >= width(*node_2);
    } case Objective::OpenDimensionY: {
        if (!leaf(node_2))
            return false;
        return std::max(height(*node_1), (node_1->waste + instance().item_area() - 1) / instance().height(instance().bin_type(0), CutOrientation::Horizontal) + 1) >= height(*node_2);
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
        const std::shared_ptr<Node>& node_1,
        const std::shared_ptr<Node>& node_2) const
{
    switch (instance().objective()) {
    case Objective::Default: {
        if (node_2->profit > node_1->profit)
            return false;
        if (node_2->profit < node_1->profit)
            return true;
        return node_2->waste > node_1->waste;
    } case Objective::BinPacking: case Objective::VariableSizedBinPacking: {
        if (!leaf(node_1))
            return false;
        if (!leaf(node_2))
            return true;
        return node_2->number_of_bins > node_1->number_of_bins;
    } case Objective::BinPackingWithLeftovers: {
        if (!leaf(node_1))
            return false;
        if (!leaf(node_2))
            return true;
        return node_2->waste > node_1->waste;
    } case Objective::OpenDimensionX: {
        if (!leaf(node_1))
            return false;
        if (!leaf(node_2))
            return true;
        return width(*node_2) > width(*node_1);
    } case Objective::OpenDimensionY: {
        if (!leaf(node_1))
            return false;
        if (!leaf(node_2))
            return true;
        return height(*node_2) > height(*node_1);
    } case Objective::Knapsack: {
        return node_2->profit < node_1->profit;
    } default: {
        std::stringstream ss;
        ss << "Branching scheme 'rectangleguillotine::BranchingScheme'"
            << "does not support objective \"" << instance().objective() << "\".";
        throw std::logic_error(ss.str());
        return false;
    }
    }
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
                CutOrientation::Horizontal);
    } case -1: {
        BinTypeId bin_type_id = instance().bin_type_id(node.number_of_bins);
        return instance().left_trim(
                instance().bin_type(bin_type_id),
                CutOrientation::Vertical);
    } case 0: {
        return node.x1_curr + instance().parameters().cut_thickness;
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
                CutOrientation::Horizontal);
    } case -1: {
        BinTypeId bin_type_id = instance().bin_type_id(node.number_of_bins);
        return instance().left_trim(
                instance().bin_type(bin_type_id),
                CutOrientation::Vertical);
    } case 0: {
        return node.x1_curr + instance().parameters().cut_thickness;
    } case 1: {
        return node.x1_prev;
    } case 2: {
        return node.x3_curr + instance().parameters().cut_thickness;
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
                CutOrientation::Horizontal);
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
        return node.y2_curr + instance().parameters().cut_thickness;
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
        return CutOrientation::Horizontal;
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

BranchingScheme::Node BranchingScheme::child_tmp(
        const std::shared_ptr<Node>& pparent,
        const Insertion& insertion) const
{
    const Node& parent = *pparent;
    assert(insertion.df <= -1 || insertion.x1 >= parent.x1_curr);
    Node node;

    node.id = node_id_;
    node_id_++;
    node.parent = pparent;
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
        node.number_of_bins = parent.number_of_bins + 1;
        node.first_stage_orientation = (insertion.df == -1)?
            CutOrientation::Vertical: CutOrientation::Horizontal;
    } else {
        node.number_of_bins = parent.number_of_bins;
        node.first_stage_orientation = parent.first_stage_orientation;
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
        node.x1_prev = parent.x1_curr + instance().parameters().cut_thickness;
        node.y2_prev = instance().bottom_trim(bin_type, o);
        break;
    } case 1: {
        node.x1_prev = parent.x1_prev;
        node.y2_prev = parent.y2_curr + instance().parameters().cut_thickness;
        break;
    } case 2: {
        node.x1_prev = parent.x1_prev;
        node.y2_prev = parent.y2_prev;
        break;
    } default: {
        assert(false);
        break;
    }
    }

    Length w_j = node.x3_curr - x3_prev(parent, node.df);
    //bool rotate_j1 = (insertion.item_type_id_1 == -1)? false: (instance().width(instance().item(insertion.item_type_id_1), true, o) == w_j);
    bool rotate_j2 = (node.item_type_id_2 == -1)?
        false:
        (instance().width(instance().item_type(node.item_type_id_2), true, o) == w_j);
    //Length h_j1 = (insertion.item_type_id_1 == -1)? -1: instance().height(instance().item_type(insertion.item_type_id_1), rotate_j1, o);
    //Length h_j2 = (insertion.item_type_id_2 == -1)? -1: instance().height(instance().item_type(insertion.item_type_id_2), rotate_j2, o);

    // Compute subplate2curr_items_above_defect.
    if (node.df == 2)
        node.subplate2curr_items_above_defect = parent.subplate2curr_items_above_defect;
    if (node.item_type_id_1 == -1 && node.item_type_id_2 != -1) {
        JRX jrx;
        jrx.item_type_id = insertion.item_type_id_2;
        jrx.rotate = rotate_j2;
        jrx.x = x3_prev(parent, node.df);
        node.subplate2curr_items_above_defect.push_back(jrx);
    }

    // Update pos_stack, items_area.
    node.pos_stack = parent.pos_stack;
    node.number_of_items = parent.number_of_items;
    node.item_area = parent.item_area;
    node.profit = parent.profit;
    if (insertion.item_type_id_1 != -1) {
        const ItemType& item_type = instance().item_type(insertion.item_type_id_1);
        node.pos_stack[item_type.stack_id]++;
        node.number_of_items++;
        node.item_area += item_type.area();
        node.profit += item_type.profit;
    }
    if (insertion.item_type_id_2 != -1) {
        const ItemType& item_type = instance().item_type(insertion.item_type_id_2);
        node.pos_stack[item_type.stack_id]++;
        node.number_of_items++;
        node.item_area += item_type.area();
        node.profit += item_type.profit;
    }
    assert(node.item_area <= instance().bin_area());

    // Compute subplate1curr_number_of_2_cuts.
    if (node.df < 1) {
        if (node.y2_curr == h) {
            node.subplate1curr_number_of_2_cuts = 0;
        } else {
            node.subplate1curr_number_of_2_cuts = 1;
        }
    } else if (node.df == 1) {
        if (node.y2_curr == h) {
            node.subplate1curr_number_of_2_cuts = parent.subplate1curr_number_of_2_cuts;
        } else {
            node.subplate1curr_number_of_2_cuts = parent.subplate1curr_number_of_2_cuts + 1;
        }
    } else if (node.df > 1) {
        if (node.y2_curr == h) {
            if (parent.y2_curr == h) {
                node.subplate1curr_number_of_2_cuts = parent.subplate1curr_number_of_2_cuts;
            } else {
                node.subplate1curr_number_of_2_cuts = parent.subplate1curr_number_of_2_cuts - 1;
            }
        } else {
            node.subplate1curr_number_of_2_cuts = parent.subplate1curr_number_of_2_cuts;
        }
    }

    // Update current_area_ and waste_
    node.current_area = instance().previous_bin_area(i);
    if (full(node)) {
        node.current_area += (instance().parameters().number_of_stages == 3)?
            (node.x1_curr - instance().left_trim(bin_type, o)) * h:
            (node.y2_curr - instance().bottom_trim(bin_type, o)) * w;
    } else {
        node.current_area += (node.x1_prev - instance().left_trim(bin_type, o)) * h
            + (node.x1_curr - node.x1_prev) * (node.y2_prev - instance().bottom_trim(bin_type, o))
            + (node.x3_curr - node.x1_prev) * (node.y2_curr - node.y2_prev);
    }
    node.waste = node.current_area - node.item_area;
    assert(node.waste >= 0);
    return node;
}

std::vector<std::shared_ptr<BranchingScheme::Node>> BranchingScheme::children(
        const std::shared_ptr<Node>& parent) const
{
    insertions(parent);
    std::vector<std::shared_ptr<Node>> cs(insertions_.size());
    for (Counter i = 0; i < (Counter)insertions_.size(); ++i)
        cs[i] = std::make_shared<Node>(child_tmp(parent, insertions_[i]));
    return cs;
}

const std::vector<BranchingScheme::Insertion>& BranchingScheme::insertions(
        const std::shared_ptr<Node>& pparent) const
{
    const Node& parent = *pparent;
    insertions_.clear();

    // Compute df_min
    Depth df_min = -2;
    if (parent.number_of_bins == instance().number_of_bins()) {
        df_min = 0;
    } else if (first_stage_orientation_ == CutOrientation::Vertical) {
        df_min = -1;
    } else if (first_stage_orientation_ == CutOrientation::Any
            // Next bin has no defects,
            && instance().bin_type(instance().bin_type_id(parent.number_of_bins)).defects.size() == 0
            // is a square,
            && instance().bin_type(instance().bin_type_id(parent.number_of_bins)).rect.w
            == instance().bin_type(instance().bin_type_id(parent.number_of_bins)).rect.h
            // and items can be rotated
            && no_oriented_items_) {
        df_min = -1;
    }

    // Compute df_max
    Depth df_max = 2;
    if (parent.parent == nullptr)
        df_max = -1;

    for (Depth df = df_max; df >= df_min; --df) {
        if (df == -1 && first_stage_orientation_ == CutOrientation::Horizontal)
            continue;

        if (df == -2) {
            i = parent.number_of_bins;
            o = CutOrientation::Horizontal;
        } else if (df == -1) {
            i = parent.number_of_bins;
            o = CutOrientation::Vertical;
        } else {
            i = parent.number_of_bins - 1;
            o = parent.first_stage_orientation;
        }

        // Simple dominance rule
        bool stop = false;
        for (const Insertion& insertion: insertions_) {
            if (insertion.item_type_id_1 == -1 && insertion.item_type_id_2 == -1)
                continue;
            if (df == 1
                    && insertion.x1 == parent.x1_curr
                    && insertion.y2 == parent.y2_curr) {
                stop = true;
                break;
            } else if (df == 0
                    && insertion.x1 == parent.x1_curr) {
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

        Length x = x3_prev(parent, df);
        Length y = y2_prev(parent, df);

        // Try inserting an item
        for (StackId s = 0; s < instance().number_of_stacks(); ++s) {
            if (parent.pos_stack[s] == instance().stack_size(s))
                continue;
            StackId sp = stack_pred_[s];
            if (sp != -1 && parent.pos_stack[sp] <= parent.pos_stack[s])
                continue;

            ItemTypeId item_type_id = instance().item(s, parent.pos_stack[s]);
            const ItemType& item_type = instance().item_type(item_type_id);

            if (!item_type.oriented) {
                bool b = item_type.rect.w > item_type.rect.h;
                insertion_1_item(
                        parent,
                        item_type_id,
                        !b,  // rotate
                        df);
                insertion_1_item(
                        parent,
                        item_type_id,
                        b,  // rotate
                        df);
            } else {
                insertion_1_item(
                        parent,
                        item_type_id,
                        false,  // rotate
                        df);
            }

            // Try adding it with a second item
            if (instance().parameters().cut_type == CutType::Roadef2018) {
                Length wj = instance().width(item_type, false, o);
                Length wjr = instance().width(item_type, true, o);
                for (StackId s2 = s; s2 < instance().number_of_stacks(); ++s2) {
                    ItemTypeId item_type_id_2 = -1;
                    if (s2 == s) {
                        // Check is the whold stack has already been packed.
                        if (parent.pos_stack[s2] + 1 == instance().stack_size(s2))
                            continue;
                        StackId sp2 = stack_pred_[s2];
                        if (sp2 != -1
                                && parent.pos_stack[sp2]
                                <= parent.pos_stack[s2])
                            continue;
                        item_type_id_2 = instance().item(s2, parent.pos_stack[s2] + 1);
                    } else {
                        // Check is the whold stack has already been packed.
                        if (parent.pos_stack[s2] == instance().stack_size(s2))
                            continue;
                        StackId sp2 = stack_pred_[s2];
                        if ((sp2 == s
                                    && parent.pos_stack[sp2] + 1
                                    <= parent.pos_stack[s2])
                                || (sp2 != -1
                                    && sp2 != s
                                    && parent.pos_stack[sp2]
                                    <= parent.pos_stack[s2]))
                            continue;
                        item_type_id_2 = instance().item(s2, parent.pos_stack[s2]);
                    }

                    const ItemType& item_type_2 = instance().item_type(item_type_id_2);
                    Length wj2 = instance().width(item_type_2, false, o);
                    Length wj2r = instance().width(item_type_2, true, o);
                    if (wj == wj2)
                        insertion_2_items(
                                parent,
                                item_type_id,
                                false,  // rotate_1
                                item_type_id_2,
                                false,  // rotate_2
                                df);
                    if (!item_type_2.oriented)
                        if (wj == wj2r)
                            insertion_2_items(
                                    parent,
                                    item_type_id,
                                    false,  // rotate_1
                                    item_type_id_2,
                                    true,  // rotate_2
                                    df);
                    if (!item_type.oriented) {
                        if (wjr == wj2)
                            insertion_2_items(
                                    parent,
                                    item_type_id,
                                    true,  // rotate_1
                                    item_type_id_2,
                                    false,  // rotate_2
                                    df);
                        if (!item_type_2.oriented)
                            if (wjr == wj2r)
                                insertion_2_items(
                                        parent,
                                        item_type_id,
                                        true,  // rotate_1
                                        item_type_id_2,
                                        true,  // rotate_2
                                        df);
                    }
                }
            }
        }

        // Try inserting a defect.
        if (parent.parent == nullptr
                || parent.item_type_id_1 != -1
                || parent.item_type_id_2 != -1) {
            BinTypeId bin_type_id = instance().bin_type_id(i);
            const BinType& bin_type = instance().bin_type(bin_type_id);
            for (DefectId defect_id = 0;
                    defect_id < (DefectId)bin_type.defects.size();
                    ++defect_id) {
                const Defect& defect = bin_type.defects[defect_id];
                if (instance().right(defect, o) >= x
                        && instance().top(defect, o) >= y) {
                    insertion_defect(parent, defect_id, df);
                }
            }
        }
    }
    return insertions_;
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
        if (instance().parameters().maximum_distance_1_cuts != -1)
            if (x > x1_prev(node, df) + instance().parameters().maximum_distance_1_cuts)
                x = x1_prev(node, df) + instance().parameters().maximum_distance_1_cuts;
        return x;
    } case 0: {
        BinTypeId bin_type_id = instance().bin_type_id(i);
        const BinType& bin_type = instance().bin_type(bin_type_id);
        Length x = instance().width(bin_type, o) - instance().right_trim(bin_type, o);
        if (instance().parameters().maximum_distance_1_cuts != -1)
            if (x > x1_prev(node, df) + instance().parameters().maximum_distance_1_cuts)
                x = x1_prev(node, df) + instance().parameters().maximum_distance_1_cuts;
        return x;
    } case 1: {
        BinTypeId bin_type_id = instance().bin_type_id(i);
        const BinType& bin_type = instance().bin_type(bin_type_id);
        Length x = node.x1_max;
        if (!instance().parameters().cut_through_defects)
            for (const Defect& defect: bin_type.defects)
                if (instance().bottom(defect, o) < node.y2_curr + instance().parameters().cut_thickness
                        && instance().top(defect, o) > node.y2_curr)
                    if (instance().left(defect, o) > node.x1_prev)
                        if (x > instance().left(defect, o) - instance().parameters().cut_thickness)
                            x = instance().left(defect, o) - instance().parameters().cut_thickness;;
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
    if (!instance().parameters().cut_through_defects)
        for (const Defect& defect: bin_type.defects)
            if (instance().left(defect, o) < x3 + instance().parameters().cut_thickness
                    && instance().right(defect, o) > x3)
                if (instance().bottom(defect, o) >= y2_prev(node, df))
                    if (y > instance().bottom(defect, o) - instance().parameters().cut_thickness)
                        y = instance().bottom(defect, o) - instance().parameters().cut_thickness;
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
        const Node& parent,
        ItemTypeId item_type_id,
        bool rotate,
        Depth df) const
{
    assert(-2 <= df); assert(df <= 3);

    // Check defect intersection
    const rectangleguillotine::ItemType& item_type = instance().item_type(item_type_id);
    Length x = x3_prev(parent, df) + instance().width(item_type, rotate, o);
    Length y = y2_prev(parent, df) + instance().height(item_type, rotate, o);
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
    if (df == 2 && instance().parameters().cut_type == CutType::Homogenous
            && parent.item_type_id_1 != item_type_id) {
        return;
    }

    Insertion insertion {
        item_type_id, -1, df,
        x, y, x,
        x1_max(parent, df), y2_max(parent, df, x), 0, 0};

    // Check defect intersection
    DefectId defect_id = instance().item_intersects_defect(
            x3_prev(parent, df),
            y2_prev(parent, df),
            item_type,
            rotate,
            bin_type,
            o);
    if (defect_id >= 0) {
        if (instance().parameters().cut_type == CutType::Roadef2018
                || instance().parameters().cut_type == CutType::NonExact) {
            // Place the item on top of its third-level sub-plate
            insertion.item_type_id_1 = -1;
            insertion.item_type_id_2 = item_type_id;
            Length min_waste = instance().parameters().minimum_waste_length;
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
    if (instance().parameters().cut_type == CutType::Exact
            || instance().parameters().cut_type == CutType::Homogenous)
        insertion.z2 = 2;

    update(parent, insertion);
}

void BranchingScheme::insertion_2_items(
        const Node& parent,
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
    Length x = x3_prev(parent, df) + instance().width(item_type_1, rotate1, o);
    Length y = y2_prev(parent, df)
        + h_j1
        + instance().parameters().cut_thickness
        + instance().height(item_type_2, rotate2, o);
    if (x > w || y > h) {
        return;
    }
    if (instance().item_intersects_defect(
                x3_prev(parent, df),
                y2_prev(parent, df),
                item_type_1,
                rotate1,
                bin_type,
                o) >= 0
            || instance().item_intersects_defect(
                x3_prev(parent, df),
                y2_prev(parent, df) + h_j1,
                item_type_2,
                rotate2,
                bin_type,
                o) >= 0) {
        return;
    }

    Insertion insertion {
        item_type_id_1, item_type_id_2, df,
        x, y, x,
        x1_max(parent, df), y2_max(parent, df, x), 0, 2};

    update(parent, insertion);
}

void BranchingScheme::insertion_defect(
        const Node& parent,
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
    Length min_waste = instance().parameters().minimum_waste_length;
    Length x_min = x3_prev(parent, df) + min_waste;
    Length y_min = y2_prev(parent, df) + min_waste;
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
        x1_max(parent, df), y2_max(parent, df, x), 1, 1};

    update(parent, insertion);
}

void BranchingScheme::update(
        const Node& parent, 
        Insertion& insertion) const
{
    Length min_waste = instance().parameters().minimum_waste_length;
    Length cut_thickness = instance().parameters().cut_thickness;
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
            && insertion.x1 - x1_prev(parent, insertion.df) < instance().parameters().minimum_distance_1_cuts) {
        if (insertion.z1 == 0) {
            insertion.x1 = std::max(
                    insertion.x1 + cut_thickness + min_waste,
                    x1_prev(parent, insertion.df) + instance().parameters().minimum_distance_1_cuts);
            insertion.z1 = 1;
        } else { // insertion.z1 = 1
            insertion.x1 = x1_prev(parent, insertion.df) + instance().parameters().minimum_distance_1_cuts;
        }
    }

    // Update insertion.y2 and insertion.z2 with respect to min2cut()
    if ((insertion.item_type_id_1 != -1 || insertion.item_type_id_2 != -1)
            && insertion.y2 - y2_prev(parent, insertion.df) < instance().parameters().minimum_distance_2_cuts) {
        if (insertion.z2 == 0) {
            insertion.y2 = std::max(
                    insertion.y2 + cut_thickness + min_waste,
                    y2_prev(parent, insertion.df) + instance().parameters().minimum_distance_2_cuts);
            insertion.z2 = 1;
        } else if (insertion.z2 == 1) {
            insertion.y2 = y2_prev(parent, insertion.df) + instance().parameters().minimum_distance_2_cuts;
        } else { // insertion.z2 == 2
            return;
        }
    }

    // Update insertion.y2 and insertion.z2 with respect to one2cut()
    if (instance().parameters().maximum_number_2_cuts != -1
            && insertion.df == 1
            && parent.subplate1curr_number_of_2_cuts == instance().parameters().maximum_number_2_cuts
            && insertion.y2 != h) {
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
    if (instance().parameters().number_of_stages == 2 && insertion.x1 != w) {
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
            if (insertion.x1 + min_waste <= parent.x1_curr) {
                insertion.x1 = parent.x1_curr;
                insertion.z1 = parent.z1;
            } else if (insertion.x1 < parent.x1_curr) { // x - min_waste < insertion.x1 < x
                if (parent.z1 == 0) {
                    insertion.x1 = parent.x1_curr + cut_thickness + min_waste;
                    insertion.z1 = 1;
                } else {
                    insertion.x1 = insertion.x1 + cut_thickness + min_waste;
                    insertion.z1 = 1;
                }
            } else if (insertion.x1 == parent.x1_curr) {
            } else { // x1_curr() < insertion.x1
                if (parent.z1 == 0 && insertion.x1 < parent.x1_curr + min_waste) {
                    insertion.x1 = insertion.x1 + cut_thickness + min_waste;
                    insertion.z1 = 1;
                }
            }
        } else { // insertion.z1 == 1
            if (insertion.x1 <= parent.x1_curr) {
                insertion.x1 = parent.x1_curr;
                insertion.z1 = parent.z1;
            } else { // x1_curr() < insertion.x1
                if (parent.z1 == 0 && parent.x1_curr + min_waste > insertion.x1)
                    insertion.x1 = parent.x1_curr + cut_thickness + min_waste;
            }
        }
    }

    // Update insertion.y2 and insertion.z2 with respect to y2_curr() and z1().
    if (insertion.df == 2) {
        if (insertion.z2 == 0) {
            if (insertion.y2 + min_waste <= parent.y2_curr) {
                insertion.y2 = parent.y2_curr;
                insertion.z2 = parent.z2;
            } else if (insertion.y2 < parent.y2_curr) { // y_curr() - min_waste < insertion.y4 < y_curr()
                if (parent.z2 == 2) {
                    return;
                } else if (parent.z2 == 0) {
                    insertion.y2 = parent.y2_curr + cut_thickness + min_waste;
                    insertion.z2 = 1;
                } else { // z2() == 1
                    insertion.y2 = insertion.y2 + cut_thickness + min_waste;
                    insertion.z2 = 1;
                }
            } else if (insertion.y2 == parent.y2_curr) {
                if (parent.z2 == 2)
                    insertion.z2 = 2;
            } else if (parent.y2_curr < insertion.y2
                    && insertion.y2 < parent.y2_curr + min_waste) {
                if (parent.z2 == 2) {
                    return;
                } else if (parent.z2 == 0) {
                    insertion.y2 = insertion.y2 + cut_thickness + min_waste;
                    insertion.z2 = 1;
                } else { // z2() == 1
                }
            } else { // y2_curr() + min_waste <= insertion.y2
                if (parent.z2 == 2) {
                    return;
                }
            }
        } else if (insertion.z2 == 1) {
            if (insertion.y2 <= parent.y2_curr) {
                insertion.y2 = parent.y2_curr;
                insertion.z2 = parent.z2;
            } else if (parent.y2_curr < insertion.y2
                    && insertion.y2 < parent.y2_curr + min_waste) {
                if (parent.z2 == 2) {
                    return;
                } else if (parent.z2 == 0) {
                    insertion.y2 = parent.y2_curr + cut_thickness + min_waste;
                } else { // z2() == 1
                }
            } else {
                if (parent.z2 == 2) {
                    return;
                }
            }
        } else { // insertion.z2 == 2
            if (insertion.y2 < parent.y2_curr) {
                return;
            } else if (insertion.y2 == parent.y2_curr) {
            } else if (parent.y2_curr < insertion.y2
                    && insertion.y2 < parent.y2_curr + cut_thickness + min_waste) {
                if (parent.z2 == 2) {
                    return;
                } else if (parent.z2 == 0) {
                    return;
                } else { // z2() == 1
                }
            } else { // y2_curr() + min_waste <= insertion.y2
                if (parent.z2 == 2) {
                    return;
                }
            }
        }
    }

    // Update insertion.x1 and insertion.z1 with respect to defect intersections.
    if (!instance().parameters().cut_through_defects) {
        for (;;) {
            DefectId defect_id = instance().rect_intersects_defect(
                    insertion.x1,
                    insertion.x1 + instance().parameters().cut_thickness,
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
    bool y2_fixed = (insertion.z2 == 2 || (insertion.df == 2 && parent.z2 == 2));

    for (;;) {
        bool found = false;

        // Increase y2 if it intersects a defect.
        if (!instance().parameters().cut_through_defects) {
            DefectId defect_id = instance().rect_intersects_defect(
                    x1_prev(parent, insertion.df),
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
            for (auto jrx: parent.subplate2curr_items_above_defect) {
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
            Length w_j = insertion.x3 - x3_prev(parent, insertion.df);
            bool rotate_j2 = (instance().width(item_type, true, o) == w_j);
            Length h_j2 = instance().height(item_type, rotate_j2, o);
            Length l = x3_prev(parent, insertion.df);
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
                for (auto jrx: parent.subplate2curr_items_above_defect) {
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
                Length w_j = insertion.x3 - x3_prev(parent, insertion.df);
                bool rotate_j2 = (instance().width(item_type, true, o) == w_j);
                Length h_j2 = instance().height(item_type, rotate_j2, o);
                Length l = x3_prev(parent, insertion.df);
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
    if (!instance().parameters().cut_through_defects && cut_thickness > 0) {
        DefectId defect_id = instance().rect_intersects_defect(
                insertion.x3,
                insertion.x3 + cut_thickness,
                y2_prev(parent, insertion.df),
                insertion.y2,
                bin_type,
                o);
        if (defect_id != -1)
            return;
    }

    // Check dominance
    for (auto it = insertions_.begin(); it != insertions_.end();) {
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
            if (dominates(front(parent, *it), front(parent, insertion))) {
                return;
            }
        }
        if ((insertion.item_type_id_1 != -1 || insertion.item_type_id_2 != -1)
                && (it->item_type_id_1 == insertion.item_type_id_1 || it->item_type_id_1 == insertion.item_type_id_2)
                && (it->item_type_id_2 == insertion.item_type_id_2 || it->item_type_id_2 == insertion.item_type_id_1)) {
            if (dominates(front(parent, insertion), front(parent, *it))) {
                if (std::next(it) != insertions_.end()) {
                    *it = insertions_.back();
                    insertions_.pop_back();
                    b = false;
                } else {
                    insertions_.pop_back();
                    break;
                }
            }
        }
        if (b)
            ++it;
    }

    insertions_.push_back(insertion);
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
        if (!instance().parameters().cut_through_defects) {
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
                    && (node.r - node.l < instance().parameters().minimum_waste_length
                        || node.t - node.b < instance().parameters().minimum_waste_length)) {
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
        const std::shared_ptr<Node>& node) const
{
    std::vector<const BranchingScheme::Node*> descendents;
    for (const Node* current_node = node.get();
            current_node->parent != nullptr;
            current_node = current_node->parent.get()) {
        descendents.push_back(current_node);
    }
    std::reverse(descendents.begin(), descendents.end());

    SolutionBuilder solution_builder(instance());
    Length subplate1_curr_x1 = -1;
    Length subplate2_curr_y2 = -1;
    for (SolutionNodeId node_pos = 0;
            node_pos < (SolutionNodeId)descendents.size();
            ++node_pos) {

        const Node* current_node = descendents[node_pos];
        //std::cout << *current_node << std::endl;
        BinPos i = current_node->number_of_bins - 1;
        BinTypeId bin_type_id = instance().bin_type_id(i);
        const BinType& bin_type = instance().bin_type(bin_type_id);
        bool has_item = (current_node->item_type_id_1 != -1 || current_node->item_type_id_2 != -1);
        Depth df_next = (node_pos < (SolutionNodeId)descendents.size() - 1)?
            descendents[node_pos + 1]->df: -1;

        // Create a new bin
        if (current_node->df <= -1) {
            CutOrientation cut_orientation = (
                    (instance().parameters().number_of_stages == 3 && current_node->first_stage_orientation == CutOrientation::Vertical)
                    || (instance().parameters().number_of_stages == 2 && current_node->first_stage_orientation == CutOrientation::Horizontal))?
                    CutOrientation::Vertical: CutOrientation::Horizontal;
            solution_builder.add_bin(
                    bin_type_id,
                    1,
                    cut_orientation);
        }

        // Create a new first-level sub-plate.
        if ((current_node->df <= -1) && (df_next <= -1) && (!has_item))
            continue;
        if (current_node->df < 1) {
            if (instance().parameters().number_of_stages == 3) {
                subplate1_curr_x1 = current_node->x1_curr;
                for (SolutionNodeId node_pos_2 = node_pos + 1;
                        node_pos_2 < (SolutionNodeId)descendents.size()
                        && descendents[node_pos_2]->df >= 1;
                        node_pos_2++) {
                    subplate1_curr_x1 = descendents[node_pos_2]->x1_curr;
                }
                solution_builder.add_node(1, subplate1_curr_x1);
            }
        }

        // Create a new second-level sub-plate.
        if ((current_node->df <= 0) && (df_next <= 0) && (!has_item))
            continue;
        if (current_node->df < 2) {
            Depth d = (instance().parameters().number_of_stages != 2)? 2: 1;
            subplate2_curr_y2 = current_node->y2_curr;
            for (SolutionNodeId node_pos_2 = node_pos + 1;
                    node_pos_2 < (SolutionNodeId)descendents.size()
                    && descendents[node_pos_2]->df >= 2;
                    node_pos_2++) {
                subplate2_curr_y2 = descendents[node_pos_2]->y2_curr;
            }
            solution_builder.add_node(d, subplate2_curr_y2);
        }

        // Create a new third-level sub-plate.
        if ((current_node->df <= 1) && (df_next <= 1) && (!has_item))
            continue;
        Depth d = (instance().parameters().number_of_stages != 2)? 3: 2;
        solution_builder.add_node(d, current_node->x3_curr);

        // Create a new fourth-level sub-plate.
        if (!has_item)
            continue;
        //std::cout << *current_node << std::endl;
        d = (instance().parameters().number_of_stages != 2)? 4: 3;
        Length cut_position = -1;
        Length w_tmp = current_node->x3_curr - x3_prev(*current_node->parent, current_node->df);
        if (current_node->item_type_id_1 != -1) {
            //std::cout << instance().item_type(current_node->item_type_id_1) << std::endl;
            cut_position = current_node->y2_prev + (
                    (w_tmp == instance().item_type(current_node->item_type_id_1).rect.w)?
                    instance().item_type(current_node->item_type_id_1).rect.h:
                    instance().item_type(current_node->item_type_id_1).rect.w);
        } else if (current_node->item_type_id_2 != -1) {
            cut_position = subplate2_curr_y2 - (
                    (w_tmp == instance().item_type(current_node->item_type_id_2).rect.w)?
                    instance().item_type(current_node->item_type_id_2).rect.h:
                    instance().item_type(current_node->item_type_id_2).rect.w);
        }
        solution_builder.add_node(d, cut_position);
        if (current_node->item_type_id_1 != -1)
            solution_builder.set_last_node_item(current_node->item_type_id_1);

        // Add an additional fourth-level sub-plate if necessary.
        if (current_node->item_type_id_2 != -1) {
            //std::cout << "subplate2_curr_y2 " << subplate2_curr_y2 << std::endl;
            solution_builder.add_node(d, subplate2_curr_y2);
            solution_builder.set_last_node_item(current_node->item_type_id_2);
        }
    }

    Solution solution = solution_builder.build();
    if (solution.profit() != node->profit) {
        throw std::logic_error(
                "rectangleguillotine::BranchingScheme::to_solution");
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
