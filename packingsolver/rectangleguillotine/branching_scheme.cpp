#include "packingsolver/rectangleguillotine/branching_scheme.hpp"

#include <string>
#include <fstream>
#include <iomanip>
#include <locale>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

/****************************** BranchingScheme *******************************/

BranchingScheme::BranchingScheme(const Instance& instance, const Parameters& parameters):
    instance_(instance), parameters_(parameters)
{
    if (parameters_.cut_type_1 == CutType1::TwoStagedGuillotine) {
        if (parameters_.first_stage_orientation == CutOrientation::Horinzontal) {
            parameters_.first_stage_orientation = CutOrientation::Vertical;
        } else if (parameters_.first_stage_orientation == CutOrientation::Vertical) {
            parameters_.first_stage_orientation = CutOrientation::Horinzontal;
        }
    }

    // Compute no_oriented_items_;
    if (parameters_.no_item_rotation) {
        no_oriented_items_ = false;
    } else {
        no_oriented_items_ = true;
        for (ItemTypeId j = 0; j < instance.item_type_number(); ++j) {
            if (instance.item_type(j).oriented) {
                no_oriented_items_ = false;
                break;
            }
        }
    }

    // Update stack_pred_
    stack_pred_ = std::vector<StackId>(instance.stack_number(), -1);
    for (StackId s = 0; s < instance.stack_number(); ++s) {
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
    if (instance_.stack_size(s1) != instance_.stack_size(s2))
        return false;
    for (ItemPos j = 0; j < instance_.stack_size(s1); ++j) {
        const ItemType& j1 = instance_.item(s1, j);
        const ItemType& j2 = instance_.item(s2, j);
        if (j1.oriented && j2.oriented
                && j1.rect.w == j2.rect.w
                && j1.rect.h == j2.rect.h
                && j1.profit == j2.profit
                && j1.copies == j2.copies)
            continue;
        if (!j1.oriented && !j2.oriented
                && j1.profit == j2.profit
                && ((j1.rect.w == j2.rect.w && j1.rect.h == j2.rect.h)
                    || (j1.rect.w == j2.rect.h && j1.rect.h == j2.rect.w)))
            continue;
        return false;
    }
    return true;
}

void BranchingScheme::Parameters::set_predefined(std::string str)
{
    if (str.length() != 4) {
        std::cerr << "\033[31m" << "ERROR, predefined branching scheme parameter \"" << str << "\" should contain four characters." << "\033[0m" << std::endl;
        if (str.length() < 4)
            return;
    }
    switch (str[0]) {
    case '3': {
        cut_type_1 = rectangleguillotine::CutType1::ThreeStagedGuillotine;
        break;
    } case '2': {
        cut_type_1 = rectangleguillotine::CutType1::TwoStagedGuillotine;
        break;
    } default: {
        std::cerr << "\033[31m" << "ERROR, predefined branching scheme parameter 1st character \"" << str[0] << "\" invalid." << "\033[0m" << std::endl;
    }
    }
    switch (str[1]) {
    case 'R': {
        cut_type_2 = rectangleguillotine::CutType2::Roadef2018;
        break;
    } case 'N': {
        cut_type_2 = rectangleguillotine::CutType2::NonExact;
        break;
    } case 'E': {
        cut_type_2 = rectangleguillotine::CutType2::Exact;
        break;
    } case 'H': {
        cut_type_2 = rectangleguillotine::CutType2::Homogenous;
        break;
    } default: {
        std::cerr << "\033[31m" << "ERROR, predefined branching scheme parameter 2nd character \"" << str[1] << "\" invalid." << "\033[0m" << std::endl;
    }
    }
    switch (str[2]) {
    case 'V': {
        first_stage_orientation = rectangleguillotine::CutOrientation::Vertical;
        break;
    } case 'H': {
        first_stage_orientation = rectangleguillotine::CutOrientation::Horinzontal;
        break;
    } case 'A': {
        first_stage_orientation = rectangleguillotine::CutOrientation::Any;
        break;
    } default: {
        std::cerr << "\033[31m" << "ERROR, predefined branching scheme parameter 3rd character \"" << str[2] << "\" invalid." << "\033[0m" << std::endl;
    }
    }
    switch (str[3]) {
    case 'R': {
        no_item_rotation = false;
        break;
    } case 'O': {
        no_item_rotation = true;
        break;
    } default: {
        std::cerr << "\033[31m" << "ERROR, predefined branching scheme parameter 4th character \"" << str[3] << "\" invalid." << "\033[0m" << std::endl;
    }
    }
}

/********************************** children **********************************/

Length BranchingScheme::x1_prev(const Node& node, Depth df) const
{
    switch (df) {
    case -1: case -2: {
        return 0;
    } case 0: {
        return node.x1_curr;
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
    case -1: case -2: {
        return 0;
    } case 0: {
        return node.x1_curr;
    } case 1: {
        return node.x1_prev;
    } case 2: {
        return node.x3_curr;
    } default: {
        assert(false);
        return -1;
    }
    }
}

Length BranchingScheme::y2_prev(const Node& node, Depth df) const
{
    switch (df) {
    case -1: case -2: {
        return 0;
    } case 0: {
        return 0;
    } case 1: {
        return node.y2_curr;
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
        return (node.bin_number == 0)? 0: node.bin_number;
    } else {
        return node.bin_number - 1;
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
    node.j1 = insertion.j1;
    node.j2 = insertion.j2;
    node.df = insertion.df;
    node.x1_curr = insertion.x1;
    node.y2_curr = insertion.y2;
    node.x3_curr = insertion.x3;
    node.x1_max = insertion.x1_max;
    node.y2_max = insertion.y2_max;
    node.z1 = insertion.z1;
    node.z2 = insertion.z2;

    // Compute bin_number and first_stage_orientation.
    if (insertion.df < 0) {
        node.bin_number = father.bin_number + 1;
        node.first_stage_orientation = last_bin_orientation(node, insertion.df);
    } else {
        node.bin_number = father.bin_number;
        node.first_stage_orientation = father.first_stage_orientation;
    }

    BinPos i = node.bin_number - 1;
    CutOrientation o = node.first_stage_orientation;
    Length h = instance_.bin(i).height(o);
    Length w = instance_.bin(i).width(o);

    // Compute x1_prev and y2_prev.
    switch (insertion.df) {
    case -1: case -2: {
        node.x1_prev = 0;
        node.y2_prev = 0;
        break;
    } case 0: {
        node.x1_prev = father.x1_curr;
        node.y2_prev = 0;
        break;
    } case 1: {
        node.x1_prev = father.x1_prev;
        node.y2_prev = father.y2_curr;
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
    //bool rotate_j1 = (insertion.j1 == -1)? false: (instance_.width(instance().item(insertion.j1), true, o) == w_j);
    bool rotate_j2 = (node.j2 == -1)? false: (instance_.width(instance_.item_type(node.j2), true, o) == w_j);
    //Length h_j1 = (insertion.j1 == -1)? -1: instance_.height(instance().item_type(insertion.j1), rotate_j1, o);
    //Length h_j2 = (insertion.j2 == -1)? -1: instance_.height(instance().item_type(insertion.j2), rotate_j2, o);

    // Compute subplate2curr_items_above_defect.
    if (node.df == 2)
        node.subplate2curr_items_above_defect = father.subplate2curr_items_above_defect;
    if (node.j1 == -1 && node.j2 != -1) {
        JRX jrx;
        jrx.j = insertion.j2;
        jrx.rotate = rotate_j2;
        jrx.x = x3_prev(father, node.df);
        node.subplate2curr_items_above_defect.push_back(jrx);
    }

    // Update pos_stack, items_area, squared_item_area and profit.
    node.pos_stack         = father.pos_stack;
    node.item_number       = father.item_number;
    node.item_area         = father.item_area;
    node.squared_item_area = father.squared_item_area;
    node.profit            = father.profit;
    if (insertion.j1 != -1) {
        const ItemType& item = instance_.item_type(insertion.j1);
        node.pos_stack[item.stack]++;
        node.item_number       += 1;
        node.item_area         += item.rect.area();
        node.squared_item_area += item.rect.area() * item.rect.area();
        node.profit            += item.profit;
    }
    if (insertion.j2 != -1) {
        const ItemType& item = instance_.item_type(insertion.j2);
        node.pos_stack[item.stack]++;
        node.item_number       += 1;
        node.item_area         += item.rect.area();
        node.squared_item_area += item.rect.area() * item.rect.area();
        node.profit            += item.profit;
    }
    assert(node.item_area <= instance_.packable_area());

    // Update current_area_ and waste_
    node.current_area = instance_.previous_bin_area(i);
    if (full(node)) {
        node.current_area += (parameters_.cut_type_1 == CutType1::ThreeStagedGuillotine)?
            node.x1_curr * h: node.y2_curr * w;
    } else {
        node.current_area += node.x1_prev * h
            + (node.x1_curr - node.x1_prev) * node.y2_prev
            + (node.x3_curr - node.x1_prev) * (node.y2_curr - node.y2_prev);
    }
    node.waste = node.current_area - node.item_area;
    assert(node.waste >= 0);
    return pnode;
}

std::vector<BranchingScheme::Insertion> BranchingScheme::insertions(
        const std::shared_ptr<Node>& pfather,
        Info& info) const
{
    const Node& father = *pfather;
    LOG_FOLD_START(info, "insertions" << std::endl);

    if (full(father))
        return {};

    std::vector<Insertion> insertions;

    // Compute df_min
    Depth df_min = -2;
    if (father.bin_number == instance_.bin_number()) {
        df_min = 0;
    } else if (parameters_.first_stage_orientation == CutOrientation::Vertical) {
        df_min = -1;
    } else if (parameters_.first_stage_orientation == CutOrientation::Any
            && instance_.bin(father.bin_number).defects.size() == 0                           // Next bin has no defects,
            && instance_.bin(father.bin_number).rect.w == instance_.bin(father.bin_number).rect.h // is a square,
            && no_oriented_items_) {                                  // and items can be rotated
        df_min = -1;
    }

    // Compute df_max
    Depth df_max = 2;
    if (father.father == nullptr)
        df_max = -1;

    LOG(info, "df_max " << df_max << " df_min " << df_min << std::endl);
    for (Depth df = df_max; df >= df_min; --df) {
        LOG(info, "df " << df << std::endl);
        if (df == -1 && parameters_.first_stage_orientation == CutOrientation::Horinzontal)
            continue;

        // Simple dominance rule
        bool stop = false;
        for (const Insertion& insertion: insertions) {
            if (insertion.j1 == -1 && insertion.j2 == -1)
                continue;
            if        (df == 1 && insertion.x1 == father.x1_curr && insertion.y2 == father.y2_curr) {
                stop = true;
                break;
            } else if (df == 0 && insertion.x1 == father.x1_curr) {
                stop = true;
                break;
            } else if (df < 0 && insertion.df >= 0) {
                stop = true;
                break;
            }
        }
        if (stop)
            break;

        CutOrientation o = last_bin_orientation(father, df);
        Length x = x3_prev(father, df);
        Length y = y2_prev(father, df);

        // Try adding an item
        for (StackId s = 0; s < instance_.stack_number(); ++s) {
            if (father.pos_stack[s] == instance_.stack_size(s))
                continue;
            StackId sp = stack_pred_[s];
            if (sp != -1 && father.pos_stack[sp] <= father.pos_stack[s])
                continue;

            ItemTypeId j = instance_.item(s, father.pos_stack[s]).id;

            if (!oriented(j)) {
                bool b = instance_.item_type(j).rect.w > instance_.item_type(j).rect.h;
                insertion_1_item(father, insertions, j, !b, df, info);
                insertion_1_item(father, insertions, j,  b, df, info);
                //insertion_1_item(insertions, j, false, df, df_min, info);
                //insertion_1_item(insertions, j, true, df, df_min, info);
            } else {
                insertion_1_item(father, insertions, j, false, df, info);
            }

            // Try adding it with a second item
            if (parameters_.cut_type_2 == CutType2::Roadef2018) {
                LOG(info, "try adding with a second item" << std::endl);
                for (StackId s2 = s; s2 < instance_.stack_number(); ++s2) {
                    ItemTypeId j2 = -1;
                    if (s2 == s) {
                        if (father.pos_stack[s2] + 1 == instance_.stack_size(s2))
                            continue;
                        StackId sp2 = stack_pred_[s2];
                        if (sp2 != -1 && father.pos_stack[sp2] <= father.pos_stack[s2])
                            continue;
                        j2 = instance_.item(s2, father.pos_stack[s2] + 1).id;
                    } else {
                        if (father.pos_stack[s2] == instance_.stack_size(s2))
                            continue;
                        StackId sp2 = stack_pred_[s2];
                        if (                    (sp2 == s && father.pos_stack[sp2] + 1 <= father.pos_stack[s2])
                                || (sp2 != -1 && sp2 != s && father.pos_stack[sp2]     <= father.pos_stack[s2]))
                            continue;
                        j2 = instance_.item(s2, father.pos_stack[s2]).id;
                    }

                    // To break symmetries, the item with the smallest id is always
                    // placed at the bottom.
                    // This doesn't create precedency issues since all the
                    // predecessors of an item have smaller id.
                    if (j2 < j) {
                        ItemTypeId tmp = j;
                        j = j2;
                        j2 = tmp;
                    }
                    const ItemType& item1 = instance_.item_type(j);
                    const ItemType& item2 = instance_.item_type(j2);
                    if (instance_.width(item1, false, o) == instance_.width(item2, false, o))
                        insertion_2_items(father, insertions, j, false, j2, false, df, info);

                    if (!oriented(j2))
                        if (instance_.width(item1, false, o) == instance_.width(item2, true, o))
                            insertion_2_items(father, insertions, j, false, j2, true,  df, info);
                    if (!oriented(j))
                        if (instance_.width(item1, true, o) == instance_.width(item2, false, o))
                            insertion_2_items(father, insertions, j, true,  j2, false, df, info);
                    if (!oriented(j2) && !oriented(j))
                        if (instance_.width(item1, true, o) == instance_.width(item2, true, o))
                            insertion_2_items(father, insertions, j, true,  j2, true,  df, info);
                }
            }
        }

        if (father.father == nullptr || father.j1 != -1 || father.j2 != -1) {
            const std::vector<Defect>& defects = instance_.bin(last_bin(father, df)).defects;
            for (const Defect& defect: defects)
                if (instance_.left(defect, o) >= x && instance_.bottom(defect, o) >= y)
                    insertion_defect(father, insertions, defect, df, info);
        }
    }

    DBG(
        LOG_FOLD_START(info, "insertions" << std::endl);
        for (const Insertion& insertion: insertions)
            LOG(info, insertion << std::endl);
        LOG_FOLD_END(info, "");
    );

    LOG_FOLD_END(info, "insertions");
    return insertions;
}

Area BranchingScheme::waste(const Node& node, const Insertion& insertion) const
{
    BinPos i = last_bin(node, insertion.df);
    CutOrientation o = last_bin_orientation(node, insertion.df);
    Length h = instance_.bin(i).height(o);
    Front f = front(node, insertion);
    ItemPos n = node.item_number;
    Area item_area = node.item_area;
    if (insertion.j1 != -1) {
        n++;
        item_area += instance_.item_type(insertion.j1).rect.area();
    }
    if (insertion.j2 != -1) {
        n++;
        item_area += instance_.item_type(insertion.j2).rect.area();
    }
    Area current_area = (n == instance_.item_number())?
        instance_.previous_bin_area(i)
        + (f.x1_curr * h):
        instance_.previous_bin_area(i)
        + f.x1_prev * h
        + (f.x1_curr - f.x1_prev) * f.y2_prev
        + (f.x3_curr - f.x1_prev) * (f.y2_curr - f.y2_prev);
    return current_area - item_area;
}

Length BranchingScheme::x1_max(const Node& node, Depth df) const
{
    switch (df) {
    case -1: case -2: {
        BinPos i = last_bin(node, df);
        CutOrientation o = last_bin_orientation(node, df);
        Length x = instance_.bin(i).width(o);
        if (parameters_.max1cut != -1)
            if (x > x1_prev(node, df) + parameters_.max1cut)
                x = x1_prev(node, df) + parameters_.max1cut;
        return x;
    } case 0: {
        BinPos i = last_bin(node, df);
        CutOrientation o = last_bin_orientation(node, df);
        Length x = instance_.bin(i).width(o);
        if (parameters_.max1cut != -1)
            if (x > x1_prev(node, df) + parameters_.max1cut)
                x = x1_prev(node, df) + parameters_.max1cut;
        return x;
    } case 1: {
        BinPos i = last_bin(node, df);
        CutOrientation po = last_bin_orientation(node, df);
        Length x = node.x1_max;
        if (!parameters_.cut_through_defects)
            for (const Defect& k: instance_.bin(i).defects)
                if (instance_.bottom(k, po) < node.y2_curr && instance_.top(k, po) > node.y2_curr)
                    if (instance_.left(k, po) > node.x1_prev)
                        if (x > instance_.left(k, po))
                            x = instance_.left(k, po);
        return x;
    } case 2: {
        return node.x1_max;
    } default: {
        return -1;
    }
    }
}

Length BranchingScheme::y2_max(const Node& node, Depth df, Length x3) const
{
    BinPos i = last_bin(node, df);
    CutOrientation o = last_bin_orientation(node, df);
    Length y = (df == 2)? node.y2_max: instance_.bin(i).height(o);
    if (!parameters_.cut_through_defects)
        for (const Defect& k: instance_.bin(i).defects)
            if (instance_.left(k, o) < x3 && instance_.right(k, o) > x3)
                if (instance_.bottom(k, o) >= y2_prev(node, df))
                    if (y > instance_.bottom(k, o))
                        y = instance_.bottom(k, o);
    return y;
}

BranchingScheme::Front BranchingScheme::front(
        const BranchingScheme::Node& node) const
{
    Front f;
    f.i = static_cast<BinPos>(node.bin_number - 1);
    f.o = node.first_stage_orientation;
    f.x1_prev = node.x1_prev;
    f.x3_curr = node.x3_curr;
    f.x1_curr = node.x1_curr;
    f.y2_prev = node.y2_prev;
    f.y2_curr = node.y2_curr;
    return f;
}

void BranchingScheme::insertion_1_item(
        const Node& father, std::vector<Insertion>& insertions,
        ItemTypeId j, bool rotate, Depth df, Info& info) const
{
    LOG_FOLD_START(info, "insertion_1_item"
            << " j " << j << " rotate " << rotate << " df " << df << std::endl);
    assert(-2 <= df); assert(df <= 3);

    // Check defect intersection
    BinPos i = last_bin(father, df);
    CutOrientation o = last_bin_orientation(father, df);
    const rectangleguillotine::ItemType& item = instance_.item_type(j);
    Length x = x3_prev(father, df) + instance_.width(item, rotate, o);
    Length y = y2_prev(father, df) + instance_.height(item, rotate, o);
    Length w = instance_.bin(i).width(o);
    Length h = instance_.bin(i).height(o);
    LOG(info, "x3_prev(df) " << x3_prev(father, df) << " y2_prev(df) " << y2_prev(father, df)
            << " i " << i
            << " w " << instance_.width(item, rotate, o)
            << " h " << instance_.height(item, rotate, o)
            << std::endl);
    if (x > w) {
        LOG_FOLD_END(info, "too wide x " << x << " > w " << w);
        return;
    }
    if (y > h) {
        LOG_FOLD_END(info, "too high y " << y << " > h " << h);
        return;
    }

    // Homogenous
    if (df == 2 && parameters_.cut_type_2 == CutType2::Homogenous
            && father.j1 != j) {
        LOG_FOLD_END(info, "homogenous father.j1 " << father.j1 << " j " << j);
        return;
    }

    Insertion insertion {
        j, -1, df,
        x, y, x,
        x1_max(father, df), y2_max(father, df, x), 0, 0};
    LOG(info, insertion << std::endl);

    // Check defect intersection
    DefectId k = instance_.item_intersects_defect(
            x3_prev(father, df), y2_prev(father, df), item, rotate, i, o);
    if (k >= 0) {
        if (parameters_.cut_type_2 == CutType2::Roadef2018
                || parameters_.cut_type_2 == CutType2::NonExact) {
            // Place the item on top of its third-level sub-plate
            insertion.j1 = -1;
            insertion.j2 = j;
        } else {
            LOG_FOLD_END(info, "intersects defect");
            return;
        }
    }

    // Update insertion.z2 with respect to cut_type_2()
    if (parameters_.cut_type_2 == CutType2::Exact
            || parameters_.cut_type_2 == CutType2::Homogenous)
        insertion.z2 = 2;

    update(father, insertions, insertion, info);
}

void BranchingScheme::insertion_2_items(
        const Node& father, std::vector<Insertion>& insertions,
        ItemTypeId j1, bool rotate1, ItemTypeId j2, bool rotate2, Depth df, Info& info) const
{
    LOG_FOLD_START(info, "insertion_2_items"
            << " j1 " << j1 << " rotate1 " << rotate1
            << " j2 " << j2 << " rotate2 " << rotate2
            << " df " << df << std::endl);
    assert(-2 <= df); assert(df <= 3);

    // Check defect intersection
    BinPos i = last_bin(father, df);
    CutOrientation o = last_bin_orientation(father, df);
    const ItemType& item1 = instance_.item_type(j1);
    const ItemType& item2 = instance_.item_type(j2);
    Length w = instance_.bin(i).width(o);
    Length h = instance_.bin(i).height(o);
    Length h_j1 = instance_.height(item1, rotate1, o);
    Length x = x3_prev(father, df) + instance_.width(item1, rotate1, o);
    Length y = y2_prev(father, df) + h_j1
        + instance_.height(item2, rotate2, o);
    if (x > w || y > h) {
        LOG_FOLD_END(info, "too wide/high");
        return;
    }
    if (instance_.item_intersects_defect(
                x3_prev(father, df), y2_prev(father, df), item1, rotate1, i, o) >= 0
            || instance_.item_intersects_defect(
                x3_prev(father, df), y2_prev(father, df) + h_j1, item2, rotate2, i, o) >= 0) {
        LOG_FOLD_END(info, "intersects defect");
        return;
    }

    Insertion insertion {
        j1, j2, df,
        x, y, x,
        x1_max(father, df), y2_max(father, df, x), 0, 2};
    LOG(info, insertion << std::endl);

    update(father, insertions, insertion, info);
}

void BranchingScheme::insertion_defect(
        const Node& father, std::vector<Insertion>& insertions,
        const Defect& k, Depth df, Info& info) const
{
    LOG_FOLD_START(info, "insertion_defect"
            << " k " << k.id << " df " << df << std::endl);
    assert(-2 <= df);
    assert(df <= 3);

    // Check defect intersection
    BinPos i = last_bin(father, df);
    CutOrientation o = last_bin_orientation(father, df);
    Length w = instance_.bin(i).width(o);
    Length h = instance_.bin(i).height(o);
    Length min_waste = parameters_.min_waste;
    Length x = std::max(instance_.right(k, o), x3_prev(father, df) + min_waste);
    Length y = std::max(instance_.top(k, o),   y2_prev(father, df) + min_waste);
    if (x > w || y > h) {
        LOG_FOLD_END(info, "too wide/high");
        return;
    }

    Insertion insertion {
        -1, -1, df,
        x, y, x,
        x1_max(father, df), y2_max(father, df, x), 1, 1};
    LOG(info, insertion << std::endl);

    update(father, insertions, insertion, info);
}

void BranchingScheme::update(
        const Node& father, 
        std::vector<Insertion>& insertions,
        Insertion& insertion,
        Info& info) const
{
    Length min_waste = parameters_.min_waste;
    BinPos i = last_bin(father, insertion.df);
    CutOrientation o = last_bin_orientation(father, insertion.df);
    Length w = instance_.bin(i).width(o);
    Length h = instance_.bin(i).height(o);

    // Update insertion.x1 and insertion.z1 with respect to min1cut()
    if ((insertion.j1 != -1 || insertion.j2 != -1)
            && insertion.x1 - x1_prev(father, insertion.df) < parameters_.min1cut) {
        if (insertion.z1 == 0) {
            insertion.x1 = std::max(
                    insertion.x1 + min_waste,
                    x1_prev(father, insertion.df) + parameters_.min1cut);
            insertion.z1 = 1;
        } else { // insertion.z1 = 1
            insertion.x1 = x1_prev(father, insertion.df) + parameters_.min1cut;
        }
    }

    // Update insertion.y2 and insertion.z2 with respect to min2cut()
    if ((insertion.j1 != -1 || insertion.j2 != -1)
            && insertion.y2 - y2_prev(father, insertion.df) < parameters_.min2cut) {
        if (insertion.z2 == 0) {
            insertion.y2 = std::max(
                    insertion.y2 + min_waste,
                    y2_prev(father, insertion.df) + parameters_.min2cut);
            insertion.z2 = 1;
        } else if (insertion.z2 == 1) {
            insertion.y2 = y2_prev(father, insertion.df) + parameters_.min2cut;
        } else { // insertion.z2 == 2
            return;
        }
    }

    // Update insertion.y2 and insertion.z2 with respect to one2cut()
    if (parameters_.one2cut && insertion.df == 1
            && y2_prev(father, insertion.df) != 0 && insertion.y2 != h) {
        if (insertion.z2 == 0) {
            if (insertion.y2 + min_waste > h)
                return;
            insertion.y2 = h;
        } else if (insertion.z2 == 1) {
            insertion.y2 = h;
        } else { // insertion.z2 == 2
            return;
        }
    }

    // Update insertion.x1 if 2-staged
    if (parameters_.cut_type_1 == CutType1::TwoStagedGuillotine && insertion.x1 != w) {
        if (insertion.z1 == 0) {
            if (insertion.x1 + min_waste > w)
                return;
            insertion.x1 = w;
        } else { // insertion.z1 == 1
            insertion.x1 = w;
        }
    }

    // Update insertion.x1 and insertion.z1 with respect to x1_curr() and z1().
    if (insertion.df >= 1) {
        LOG(info, "i.x3 " << insertion.x3 << " x1_curr() " << father.x1_curr << std::endl);
        if (insertion.z1 == 0) {
            if (insertion.x1 + min_waste <= father.x1_curr) {
                insertion.x1 = father.x1_curr;
                insertion.z1 = father.z1;
            } else if (insertion.x1 < father.x1_curr) { // x - min_waste < insertion.x1 < x
                if (father.z1 == 0) {
                    insertion.x1 = father.x1_curr + min_waste;
                    insertion.z1 = 1;
                } else {
                    insertion.x1 = insertion.x1 + min_waste;
                    insertion.z1 = 1;
                }
            } else if (insertion.x1 == father.x1_curr) {
            } else { // x1_curr() < insertion.x1
                if (father.z1 == 0 && insertion.x1 < father.x1_curr + min_waste) {
                    insertion.x1 = insertion.x1 + min_waste;
                    insertion.z1 = 1;
                }
            }
        } else { // insertion.z1 == 1
            if (insertion.x1 <= father.x1_curr) {
                insertion.x1 = father.x1_curr;
                insertion.z1 = father.z1;
            } else { // x1_curr() < insertion.x1
                if (father.z1 == 0 && father.x1_curr + min_waste > insertion.x1)
                    insertion.x1 = father.x1_curr + min_waste;
            }
        }
    }

    // Update insertion.y2 and insertion.z2 with respect to y2_curr() and z1().
    if (insertion.df == 2) {
        LOG(info, "i.y2 " << insertion.y2 << " y2_curr() " << father.y2_curr << std::endl);
        if (insertion.z2 == 0) {
            if (insertion.y2 + min_waste <= father.y2_curr) {
                insertion.y2 = father.y2_curr;
                insertion.z2 = father.z2;
            } else if (insertion.y2 < father.y2_curr) { // y_curr() - min_waste < insertion.y4 < y_curr()
                if (father.z2 == 2) {
                    LOG_FOLD_END(info, "too high, y2_curr() " << father.y2_curr << " insertion.y2 " << insertion.y2 << " insertion.z2 " << insertion.z2);
                    return;
                } else if (father.z2 == 0) {
                    insertion.y2 = father.y2_curr + min_waste;
                    insertion.z2 = 1;
                } else { // z2() == 1
                    insertion.y2 = insertion.y2 + min_waste;
                    insertion.z2 = 1;
                }
            } else if (insertion.y2 == father.y2_curr) {
                if (father.z2 == 2)
                    insertion.z2 = 2;
            } else if (father.y2_curr < insertion.y2 && insertion.y2 < father.y2_curr + min_waste) {
                if (father.z2 == 2) {
                    LOG_FOLD_END(info, "too high, y2_curr() " << father.y2_curr << " insertion.y2 " << insertion.y2 << " insertion.z2 " << insertion.z2);
                    return;
                } else if (father.z2 == 0) {
                    insertion.y2 = insertion.y2 + min_waste;
                    insertion.z2 = 1;
                } else { // z2() == 1
                }
            } else { // y2_curr() + min_waste <= insertion.y2
                if (father.z2 == 2) {
                    LOG_FOLD_END(info, "too high, y2_curr() " << father.y2_curr << " insertion.y2 " << insertion.y2 << " insertion.z2 " << insertion.z2);
                    return;
                }
            }
        } else if (insertion.z2 == 1) {
            if (insertion.y2 <= father.y2_curr) {
                insertion.y2 = father.y2_curr;
                insertion.z2 = father.z2;
            } else if (father.y2_curr < insertion.y2 && insertion.y2 < father.y2_curr + min_waste) {
                if (father.z2 == 2) {
                    LOG_FOLD_END(info, "too high, y2_curr() " << father.y2_curr << " insertion.y2 " << insertion.y2 << " insertion.z2 " << insertion.z2);
                    return;
                } else if (father.z2 == 0) {
                    insertion.y2 = father.y2_curr + min_waste;
                } else { // z2() == 1
                }
            } else {
                if (father.z2 == 2) {
                    LOG_FOLD_END(info, "too high, y2_curr() " << father.y2_curr << " insertion.y2 " << insertion.y2 << " insertion.z2 " << insertion.z2);
                    return;
                }
            }
        } else { // insertion.z2 == 2
            if (insertion.y2 < father.y2_curr) {
                LOG_FOLD_END(info, "too high, y2_curr() " << father.y2_curr << " insertion.y2 " << insertion.y2 << " insertion.z2 " << insertion.z2);
                return;
            } else if (insertion.y2 == father.y2_curr) {
            } else if (father.y2_curr < insertion.y2 && insertion.y2 < father.y2_curr + min_waste) {
                if (father.z2 == 2) {
                    LOG_FOLD_END(info, "too high, y2_curr() " << father.y2_curr << " insertion.y2 " << insertion.y2 << " insertion.z2 " << insertion.z2);
                    return;
                } else if (father.z2 == 0) {
                    LOG_FOLD_END(info, "too high, y2_curr() " << father.y2_curr << " insertion.y2 " << insertion.y2 << " insertion.z2 " << insertion.z2);
                    return;
                } else { // z2() == 1
                }
            } else { // y2_curr() + min_waste <= insertion.y2
                if (father.z2 == 2) {
                    LOG_FOLD_END(info, "too high, y2_curr() " << father.y2_curr << " insertion.y2 " << insertion.y2 << " insertion.z2 " << insertion.z2);
                    return;
                }
            }
        }
    }

    // Update insertion.x1 and insertion.z1 with respect to defect intersections.
    for (;;) {
        DefectId k = instance_.x_intersects_defect(insertion.x1, i, o);
        if (k == -1)
            break;
        const Defect& defect = instance_.defect(k);
        insertion.x1 = (insertion.z1 == 0)?
            std::max(instance_.right(defect, o), insertion.x1 + min_waste):
            instance_.right(defect, o);
        insertion.z1 = 1;
    }

    // Increase width if too close from border
    if (insertion.x1 < w && insertion.x1 + min_waste > w) {
        if (insertion.z1 == 1) {
            insertion.x1 = w;
            insertion.z1 = 0;
        } else { // insertion.z1 == 0
            LOG(info, insertion << std::endl);
            LOG_FOLD_END(info, "too long w - min_waste < insertion.x1 < w and insertion.z1 == 0");
            return;
        }
    }

    // Check max width
    if (insertion.x1 > insertion.x1_max) {
        LOG(info, insertion << std::endl);
        LOG_FOLD_END(info, "too long insertion.x1 > insertion.x1_max");
        return;
    }
    LOG(info, "width OK" << std::endl);

    // Update insertion.y2 and insertion.z2 with respect to defect intersections.
    bool y2_fixed = (insertion.z2 == 2 || (insertion.df == 2 && father.z2 == 2));

    for (;;) {
        bool found = false;

        // Increase y2 if it intersects a defect.
        DefectId k = instance_.y_intersects_defect(
                x1_prev(father, insertion.df), insertion.x1, insertion.y2, i, o);
        if (k != -1) {
            const Defect& defect = instance_.defect(k);
            if (y2_fixed) {
                LOG_FOLD_END(info, "y2_fixed");
                return;
            }
            insertion.y2 = (insertion.z2 == 0)?
                std::max(instance_.top(defect, o), insertion.y2 + min_waste):
                instance_.top(defect, o);
            insertion.z2 = 1;
            found = true;
        }

        // Increase y2 if an item on top of its third-level sub-plate
        // intersects a defect.
        if (insertion.df == 2) {
            for (auto jrx: father.subplate2curr_items_above_defect) {
                const ItemType& item = instance_.item_type(jrx.j);
                Length h_j2 = instance_.height(item, jrx.rotate, o);
                Length l = jrx.x;
                DefectId k = instance_.item_intersects_defect(
                        l, insertion.y2 - h_j2, item, jrx.rotate, i, o);
                if (k >= 0) {
                    const Defect& defect = instance_.defect(k);
                    if (y2_fixed) {
                        LOG_FOLD_END(info, "y2_fixed");
                        return;
                    }
                    insertion.y2 = (insertion.z2 == 0)?
                        std::max(instance_.top(defect, o) + h_j2,
                                insertion.y2 + min_waste):
                        instance_.top(defect, o) + h_j2;
                    insertion.z2 = 1;
                    found = true;
                }
            }
        }
        if (insertion.j1 == -1 && insertion.j2 != -1) {
            const ItemType& item = instance_.item_type(insertion.j2);
            Length w_j = insertion.x3 - x3_prev(father, insertion.df);
            bool rotate_j2 = (instance_.width(item, true, o) == w_j);
            Length h_j2 = instance_.height(item, rotate_j2, o);
            Length l = x3_prev(father, insertion.df);
            DefectId k = instance_.item_intersects_defect(
                    l, insertion.y2 - h_j2, item, rotate_j2, i, o);
            if (k >= 0) {
                const Defect& defect = instance_.defect(k);
                if (y2_fixed) {
                    LOG_FOLD_END(info, "y2_fixed");
                    return;
                }
                insertion.y2 = (insertion.z2 == 0)?
                    std::max(instance_.top(defect, o) + h_j2, insertion.y2 + min_waste):
                    instance_.top(defect, o) + h_j2;
                insertion.z2 = 1;
                found = true;
            }
        }
        LOG(info, "found " << found << std::endl);
        if (!found)
            break;
    }

    // Now check bin's height
    if (insertion.y2 < h && insertion.y2 + min_waste > h) {
        if (insertion.z2 == 1) {
            insertion.y2 = h;
            insertion.z2 = 0;

            if (insertion.df == 2) {
                for (auto jrx: father.subplate2curr_items_above_defect) {
                    const ItemType& item = instance_.item_type(jrx.j);
                    Length l = jrx.x;
                    Length h_j2 = instance_.height(item, jrx.rotate, o);
                    DefectId k = instance_.item_intersects_defect(
                            l, insertion.y2 - h_j2, item, jrx.rotate, i, o);
                    if (k >= 0) {
                        LOG_FOLD_END(info, "too high");
                        return;
                    }
                }
            }

            if (insertion.j1 == -1 && insertion.j2 != -1) {
                const ItemType& item = instance_.item_type(insertion.j2);
                Length w_j = insertion.x3 - x3_prev(father, insertion.df);
                bool rotate_j2 = (instance_.width(item, true, o) == w_j);
                Length h_j2 = instance_.height(item, rotate_j2, o);
                Length l = x3_prev(father, insertion.df);
                DefectId k = instance_.item_intersects_defect(
                        l, insertion.y2 - h_j2, item, rotate_j2, i, o);
                if (k >= 0) {
                    LOG_FOLD_END(info, "too high");
                    return;
                }
            }

        } else { // insertion.z2 == 0 or insertion.z2 == 2
            LOG(info, insertion << std::endl);
            LOG_FOLD_END(info, "too high");
            return;
        }
    }

    // Check max height
    if (insertion.y2 > insertion.y2_max) {
        LOG(info, insertion << std::endl);
        LOG_FOLD_END(info, "too high");
        return;
    }
    LOG(info, "height OK" << std::endl);

    // Check dominance
    for (auto it = insertions.begin(); it != insertions.end();) {
        bool b = true;
        LOG(info, "f_i  " << front(father, insertion) << std::endl);
        LOG(info, "f_it " << front(father, *it) << std::endl);
        if (insertion.j1 == -1 && insertion.j2 == -1
                && it->j1 == -1 && it->j2 == -1) {
            if (insertion.df != -1 && insertion.x1 == it->x1 && insertion.y2 == it->y2 && insertion.x3 == it->x3) {
                LOG_FOLD_END(info, "dominated by " << *it);
                return;
            }
        }
        if ((it->j1 != -1 || it->j2 != -1)
                && (insertion.j1 == -1 || insertion.j1 == it->j1 || insertion.j1 == it->j2)
                && (insertion.j2 == -1 || insertion.j2 == it->j2 || insertion.j2 == it->j2)) {
            if (dominates(front(father, *it), front(father, insertion))) {
                LOG_FOLD_END(info, "dominated by " << *it);
                return;
            }
        }
        if ((insertion.j1 != -1 || insertion.j2 != -1)
                && (it->j1 == insertion.j1 || it->j1 == insertion.j2)
                && (it->j2 == insertion.j2 || it->j2 == insertion.j1)) {
            if (dominates(front(father, insertion), front(father, *it))) {
                LOG(info, "dominates " << *it << std::endl);
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
    LOG_FOLD_END(info, "ok");
}

/******************************************************************************/

const std::shared_ptr<BranchingScheme::Node> BranchingScheme::root() const
{
    BranchingScheme::Node node;
    node.id = node_id_;
    node_id_++;
    node.pos_stack = std::vector<ItemPos>(instance_.stack_number(), 0);
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
    } case Objective::BinPacking: {
        if (!solution_best.full())
            return false;
        BinPos i_pos = -1;
        Area a = instance_.item_area() + node.waste;
        while (a > 0) {
            i_pos++;
            a -= instance_.bin(i_pos).rect.area();
        }
        return (i_pos + 1 >= solution_best.bin_number());
    } case Objective::BinPackingWithLeftovers: {
        if (!solution_best.full())
            return false;
        return node.waste >= solution_best.waste();
    } case Objective::Knapsack: {
        return ubkp(node) <= solution_best.profit();
    } case Objective::StripPackingWidth: {
        if (!solution_best.full())
            return false;
        return std::max(width(node), (node.waste + instance_.item_area() - 1) / instance_.bin(0).height(CutOrientation::Vertical) + 1) >= solution_best.width();
    } case Objective::StripPackingHeight: {
        if (!solution_best.full())
            return false;
        return std::max(height(node), (node.waste + instance_.item_area() - 1) / instance_.bin(0).height(CutOrientation::Horinzontal) + 1) >= solution_best.height();
    } default: {
        assert(false);
        std::cerr << "\033[31m" << "ERROR, branching scheme rectangle::BranchingScheme does not implement objective \"" << solution_best.instance().objective() << "\"" << "\033[0m" << std::endl;
        return false;
    }
    }
}

bool BranchingScheme::better(
        const Node& node,
        const Solution& solution_best) const
{
    switch (instance_.objective()) {
    case Objective::Default: {
        if (solution_best.profit() > node.profit)
            return false;
        if (solution_best.profit() < node.profit)
            return true;
        return solution_best.waste() > node.waste;
    } case Objective::BinPacking: {
        if (!leaf(node))
            return false;
        if (!solution_best.full())
            return true;
        return solution_best.bin_number() > node.bin_number;
    } case Objective::BinPackingWithLeftovers: {
        if (!leaf(node))
            return false;
        if (!solution_best.full())
            return true;
        return solution_best.waste() > node.waste;
    } case Objective::StripPackingWidth: {
        if (!leaf(node))
            return false;
        if (!solution_best.full())
            return true;
        return solution_best.width() > width(node);
    } case Objective::StripPackingHeight: {
        if (!leaf(node))
            return false;
        if (!solution_best.full())
            return true;
        return solution_best.height() > height(node);
    } case Objective::Knapsack: {
        return solution_best.profit() < node.profit;
    } default: {
        assert(false);
        std::cerr << "\033[31m" << "ERROR, branching scheme rectangle::BranchingScheme does not implement objective \"" << instance_.objective() << "\"" << "\033[0m" << std::endl;
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
        Length h = instance_.bin(f1.i).height(f1.o);
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

/*********************************** export ***********************************/

bool empty(const std::vector<Solution::Node>& v, SolutionNodeId f_v)
{
    if (v[f_v].children.size() == 0)
        return (v[f_v].j < 0);
    for (SolutionNodeId c: v[f_v].children)
        if (!empty(v, c))
            return false;
    return true;
}

SolutionNodeId sort(std::vector<Solution::Node>& res,
        const std::vector<Solution::Node>& v,
        SolutionNodeId gf_res, SolutionNodeId f_v)
{
    SolutionNodeId id_res = res.size();
    res.push_back(v[f_v]);
    res[id_res].id = id_res;
    res[id_res].f  = gf_res;
    res[id_res].children = {};
    if (empty(v, f_v)) {
        res[id_res].j = -1;
    } else {
        for (SolutionNodeId c_v: v[f_v].children) {
            SolutionNodeId c_id_res = sort(res, v, id_res, c_v);
            res[id_res].children.push_back(c_id_res);
        }
    }
    return id_res;
}

bool BranchingScheme::BranchingScheme::check(const std::vector<Solution::Node>& nodes) const
{
    std::vector<ItemPos> items(instance_.item_number(), 0);

    for (const Solution::Node& node: nodes) {
        Length w = instance_.bin(node.i).rect.w;
        Length h = instance_.bin(node.i).rect.h;

        // TODO Check tree consistency

        // Check defect intersection
        if (!parameters_.cut_through_defects) {
            for (Defect defect: instance_.bin(node.i).defects) {
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
            if (items[node.j] > instance_.item_type(node.j).copies) {
                std::cerr << "\033[31m" << "ERROR, "
                    "item " << node.j << " produced more that one time"
                    << "\033[0m" << std::endl;
                return false;
            }

            // Check if item j contains a defect
            for (Defect defect: instance_.bin(node.i).defects) {
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
            if (
                       node.r - node.l < parameters_.min_waste
                    || node.t - node.b < parameters_.min_waste) {
                std::cerr << "\033[31m" << "ERROR, "
                    "Node " << node << " violates min_waste constraint"
                    << "\033[0m" << std::endl;
                return false;
            }
        }

        if (node.d == 0) {
             if (node.l != 0 || node.r != w || node.b != 0 || node.t != h) {
                 std::cerr << "\033[31m" << "ERROR, "
                     "Node " << node << " incorrect dimensions"
                     << "\033[0m" << std::endl;
                 return false;
             }
         } else if (node.d == 1 && node.j != -1 && node.j != -3) {
             if (node.r - node.l < parameters_.min1cut) {
                 std::cerr << "\033[31m" << "ERROR, "
                     "Node " << node << " violates min1cut constraint"
                     << "\033[0m" << std::endl;
                 return false;
             }
             if (parameters_.max1cut >= 0
                     && node.r - node.l > parameters_.max1cut) {
                 std::cerr << "\033[31m" << "ERROR, "
                     "Node " << node << " violates max1cut constraint"
                     << "\033[0m" << std::endl;
                 return false;
             }
         } else if (node.d == 2 && node.j != -1) {
             if (node.t - node.b < parameters_.min2cut) {
                 std::cerr << "\033[31m" << "ERROR, "
                     "Node " << node << " violates min2cut constraint"
                     << "\033[0m" << std::endl;
                 return false;
             }
             if (parameters_.max2cut >= 0
                     && node.t - node.b > parameters_.max2cut) {
                 std::cerr << "\033[31m" << "ERROR, "
                     "Node " << node << " violates max2cut constraint"
                     << "\033[0m" << std::endl;
                 return false;
             }
         }
    }

    return true;
}

Solution BranchingScheme::to_solution(
        const Node& node,
        const Solution&) const
{
    // Get nodes, items and bins
    std::vector<SolutionNode> nodes;
    std::vector<NodeItem> items;
    std::vector<CutOrientation> first_stage_orientations;
    std::vector<const BranchingScheme::Node*> descendents {&node};
    while (descendents.back()->father != nullptr)
        descendents.push_back(static_cast<const BranchingScheme::Node*>(
                    descendents.back()->father.get()));
    descendents.pop_back();

    std::vector<SolutionNodeId> nodes_curr(4, -1);
    for (auto it = descendents.rbegin(); it != descendents.rend(); ++it) {
        const Node* n = *it;

        if (n->df < 0)
            first_stage_orientations.push_back(last_bin_orientation(*n, n->df));

        SolutionNodeId id = (n->df >= 0)? nodes.size() + 2 - n->df: nodes.size() + 2;
        if (n->j1 != -1)
            items.push_back({n->j1, id});
        if (n->j2 != -1)
            items.push_back({n->j2, id});

        // Add new solution nodes
        SolutionNodeId f = (n->df <= 0)? -first_stage_orientations.size(): nodes_curr[n->df];
        Depth          d = (n->df < 0)? 0: n->df;
        SolutionNodeId c = nodes.size() - 1;
        do {
            c++;
            nodes.push_back({f, -1});
            f = c;
            d++;
            nodes_curr[d] = nodes.size() - 1;
        } while (d != 3);

        nodes[nodes_curr[1]].p = n->x1_curr;
        nodes[nodes_curr[2]].p = n->y2_curr;
        nodes[nodes_curr[3]].p = n->x3_curr;
    }

    std::vector<SolutionNodeId> bins;
    std::vector<Solution::Node> res(nodes.size());
    for (BinPos i = 0; i < node.bin_number; ++i) {
        CutOrientation o = first_stage_orientations[i];
        Length w = instance_.bin(i).width(o);
        Length h = instance_.bin(i).height(o);
        SolutionNodeId id = res.size();
        bins.push_back(id);
        res.push_back({id, -1, 0, i, 0, w, 0, h, {}, -1, false, -1});
    }
    for (SolutionNodeId id = 0; id < (SolutionNodeId)nodes.size(); ++id) {
        SolutionNodeId f = (nodes[id].f >= 0)? nodes[id].f: bins[(-nodes[id].f)-1];
        Depth d = res[f].d + 1;
        res[id].id = id;
        res[id].f  = f;
        res[id].d  = d;
        res[id].i  = res[f].i;
        if (d == 1 || d == 3) {
            res[id].r  = nodes[id].p;
            res[id].l  = (res[f].children.size() == 0)?
                res[f].l:
                res[res[f].children.back()].r;
            res[id].b  = res[f].b;
            res[id].t  = res[f].t;
        } else { // d == 2
            res[id].t  = nodes[id].p;
            res[id].b  = (res[f].children.size() == 0)?
                res[f].b:
                res[res[f].children.back()].t;
            res[id].l  = res[f].l;
            res[id].r  = res[f].r;
        }
        res[f].children.push_back(id);
    }

    for (SolutionNodeId f = 0; f < (SolutionNodeId)(nodes.size()+bins.size()); ++f) {
        SolutionNodeId nb = res[f].children.size();
        if (nb == 0)
            continue;
        SolutionNodeId c_last = res[f].children.back();
        if ((res[f].d == 0 || res[f].d == 2) && res[f].r != res[c_last].r) {
            if (res[f].r - res[c_last].r < parameters_.min_waste) {
                res[c_last].r = res[f].r;
            } else {
                SolutionNodeId id = res.size();
                res.push_back({id, f,
                        static_cast<Depth>(res[f].d+1), res[f].i,
                        res[c_last].r, res[f].r,
                        res[f].b,      res[f].t,
                        {}, -1, false, -1});
                res[f].children.push_back(id);
            }
        } else if ((res[f].d == 1 || res[f].d == 3) && res[f].t != res[c_last].t) {
            if (res[f].t - res[c_last].t < parameters_.min_waste) {
                res[c_last].t = res[f].t;
            } else {
                SolutionNodeId id = res.size();
                res.push_back({id, f,
                        static_cast<Depth>(res[f].d+1), res[f].i,
                        res[f].l,      res[f].r,
                        res[c_last].t, res[f].t,
                        {}, -1, false, -1});
                res[f].children.push_back(id);
            }
        }
    }

    for (SolutionNodeId id = 0; id < (SolutionNodeId)res.size(); ++id)
        res[id].j  = -1;

    for (Counter j_pos = 0; j_pos < node.item_number; ++j_pos) {
        ItemTypeId     j  = items[j_pos].j;
        SolutionNodeId id = items[j_pos].node;
        Length         wj = instance_.item_type(j).rect.w;
        Length         hj = instance_.item_type(j).rect.h;
        if (res[id].children.size() > 0) { // Second item of the third-level sub-plate
            res[res[id].children[1]].j = j; // Alone in its third-level sub-plate
        } else if ((res[id].t - res[id].b == hj && res[id].r - res[id].l == wj)
                || (res[id].t - res[id].b == wj && res[id].r - res[id].l == hj)) {
            res[id].j = j;
            continue;
        } else {
            Length t = (res[id].r - res[id].l == wj)? hj: wj;
            BinPos i = res[id].i;
            CutOrientation o = first_stage_orientations[i];
            DefectId k = instance_.rect_intersects_defect(
                    res[id].l, res[id].r, res[id].b, res[id].b + t, i, o);
            if (k == -1) { // First item of the third-level sub-plate
                SolutionNodeId c1 = res.size();
                res.push_back({c1, id,
                        static_cast<Depth>(res[id].d + 1), res[id].i,
                        res[id].l, res[id].r,
                        res[id].b, res[id].b + t,
                        {}, j, false, -1});
                res[id].children.push_back(c1);
                SolutionNodeId c2 = res.size();
                res.push_back({c2, id,
                        static_cast<Depth>(res[id].d+1), res[id].i,
                        res[id].l, res[id].r,
                        res[id].b + t, res[id].t,
                        {}, -1, false, -1});
                res[id].children.push_back(c2);
            } else {
                SolutionNodeId c1 = res.size();
                res.push_back({c1, id,
                        static_cast<Depth>(res[id].d+1), res[id].i,
                        res[id].l, res[id].r,
                        res[id].b, res[id].t - t,
                        {}, -1, false, -1});
                res[id].children.push_back(c1);
                SolutionNodeId c2 = res.size();
                res.push_back({c2, id,
                        static_cast<Depth>(res[id].d+1), res[id].i,
                        res[id].l, res[id].r,
                        res[id].t - t, res[id].t,
                        {}, j, false, -1});
                res[id].children.push_back(c2);
            }
        }
    }

    // Set j to -2 for intermediate nodes and to -3 for residual
    for (Solution::Node& n: res)
        if (n.j == -1 && n.children.size() != 0)
            n.j = -2;

    if (parameters_.cut_type_1 == CutType1::TwoStagedGuillotine) {
        for (Solution::Node& n: res) {
            if (n.d == 0) {
                assert(n.children.size() == 1);
                n.children = res[n.children[0]].children;
            }
            if (n.d == 2)
                n.f = res[n.f].f;
            if (n.d >= 2)
                n.d--;
        }
    }

    // Sort nodes
    std::vector<Solution::Node> res2;
    for (SolutionNodeId c: bins)
        sort(res2, res, -1, c);

    if (res2.rbegin()->j == -1 && res2.rbegin()->d == 1)
        res2.rbegin()->j = -3;

    for (Solution::Node& n: res2) {
        n.bin_type_id = instance_.bin(n.i).id;
        CutOrientation o = first_stage_orientations[n.i];
        if (o == CutOrientation::Horinzontal) {
            Length tmp_1 = n.l;
            Length tmp_2 = n.r;
            n.l = n.b;
            n.r = n.t;
            n.b = tmp_1;
            n.t = tmp_2;
        }
    }

    if (!check(res2))
        return Solution(instance_, {});
    return Solution(instance_, res2);
}

/******************************************************************************/

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream &os, const BranchingScheme::Parameters& parameters)
{
    os
        << "cut_type_1 " << parameters.cut_type_1
        << " cut_type_2 " << parameters.cut_type_2
        << " first_stage_orientation " << parameters.first_stage_orientation
        << " min1cut " << parameters.min1cut
        << " max1cut " << parameters.max1cut
        << " min2cut " << parameters.min2cut
        << " max2cut " << parameters.max2cut
        << " min_waste " << parameters.min_waste
        << " one2cut " << parameters.one2cut
        << " no_item_rotation " << parameters.no_item_rotation
        << " cut_through_defects " << parameters.cut_through_defects
        ;
    return os;
}

bool BranchingScheme::SolutionNode::operator==(
        const BranchingScheme::SolutionNode& node) const
{
    return ((f == node.f) && (p == node.p));
}

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream &os, const BranchingScheme::SolutionNode& node)
{
    os << "f " << node.f << " p " << node.p;
    return os;
}

bool BranchingScheme::NodeItem::operator==(
        const BranchingScheme::NodeItem& node_item) const
{
    return ((j == node_item.j) && (node == node_item.node));
}

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream &os, const BranchingScheme::NodeItem& node_item)
{
    os << "j " << node_item.j << " node " << node_item.node;
    return os;
}

bool BranchingScheme::Insertion::operator==(const Insertion& insertion) const
{
    return ((j1 == insertion.j1)
            && (j2 == insertion.j2)
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
        std::ostream &os, const BranchingScheme::Insertion& insertion)
{
    os << "j1 " << insertion.j1
        << " j2 " << insertion.j2
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
        std::ostream &os,
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
        std::ostream &os, const BranchingScheme::Front& front)
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
        std::ostream &os, const BranchingScheme::Node& node)
{
    os << "item_number " << node.item_number
        << " bin_number " << node.bin_number
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
