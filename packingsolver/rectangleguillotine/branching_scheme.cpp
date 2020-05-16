#include "packingsolver/rectangleguillotine/branching_scheme.hpp"

#include <string>
#include <fstream>
#include <iomanip>
#include <locale>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

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
    if (no_item_rotation()) {
        no_oriented_items_ = false;
    } else {
        no_oriented_items_ = true;
        for (ItemTypeId j = 0; j < instance.item_type_number(); ++j) {
            if (instance.item(j).oriented) {
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

bool BranchingScheme::oriented(ItemTypeId j) const
{
    if (instance().item(j).oriented)
        return true;
    return no_item_rotation();
}

bool BranchingScheme::equals(StackId s1, StackId s2)
{
    if (instance().stack_size(s1) != instance().stack_size(s2))
        return false;
    for (ItemPos j = 0; j < instance().stack_size(s1); ++j) {
        const Item& j1 = instance().item(s1, j);
        const Item& j2 = instance().item(s2, j);
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

std::function<bool(const BranchingScheme::Node&, const BranchingScheme::Node&)> BranchingScheme::compare(GuideId guide_id)
{
    switch(guide_id) {
    case 0: {
        return [](const BranchingScheme::Node& node_1, const BranchingScheme::Node& node_2)
        {
            if (node_1.area() == 0)
                return true;
            if (node_2.area() == 0)
                return false;
            return node_1.waste_percentage() < node_2.waste_percentage();
        };
    } case 1: {
        return [](const BranchingScheme::Node& node_1, const BranchingScheme::Node& node_2)
        {
            if (node_1.area() == 0)
                return true;
            if (node_2.area() == 0)
                return false;
            if (node_1.item_number() == 0)
                return false;
            if (node_2.item_number() == 0)
                return true;
            return node_1.waste_percentage() / node_1.mean_item_area()
                < node_2.waste_percentage() / node_2.mean_item_area();
        };
    } case 2: {
        return [](const BranchingScheme::Node& node_1, const BranchingScheme::Node& node_2)
        {
            if (node_1.area() == 0)
                return true;
            if (node_2.area() == 0)
                return false;
            if (node_1.item_number() == 0)
                return false;
            if (node_2.item_number() == 0)
                return true;
            return (0.1 + node_1.waste_percentage()) / node_1.mean_item_area()
                < (0.1 + node_2.waste_percentage()) / node_2.mean_item_area();
        };
    } case 3: {
        return [](const BranchingScheme::Node& node_1, const BranchingScheme::Node& node_2)
        {
            if (node_1.area() == 0)
                return true;
            if (node_2.area() == 0)
                return false;
            if (node_1.item_number() == 0)
                return false;
            if (node_2.item_number() == 0)
                return true;
            return (0.1 + node_1.waste_percentage()) / node_1.mean_squared_item_area()
                < (0.1 + node_2.waste_percentage()) / node_2.mean_squared_item_area();
        };
    } case 4: {
        return [](const BranchingScheme::Node& node_1, const BranchingScheme::Node& node_2)
        {
            if (node_1.profit() == 0)
                return false;
            if (node_2.profit() == 0)
                return true;
            return (double)node_1.area() / node_1.profit()
                < (double)node_2.area() / node_2.profit();
        };
    } case 5: {
        return [](const BranchingScheme::Node& node_1, const BranchingScheme::Node& node_2)
        {
            return node_1.waste() < node_2.waste();
        };
    } case 6: {
        return [](const BranchingScheme::Node& node_1, const BranchingScheme::Node& node_2)
        {
            return node_1.ub_profit() < node_2.ub_profit();
        };
    } case 7: {
        return [](const BranchingScheme::Node& node_1, const BranchingScheme::Node& node_2)
        {
            if (node_1.ub_profit() != node_2.ub_profit())
                return node_1.ub_profit() < node_2.ub_profit();
            return node_1.waste() < node_2.waste();
        };
    }
    }
    return 0;
}

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
        << " symmetry_depth " << parameters.symmetry_depth
        << " symmetry_2 " << parameters.symmetry_2
        ;
    return os;
}

/******************************** SolutionNode ********************************/

bool BranchingScheme::SolutionNode::operator==(const BranchingScheme::SolutionNode& node) const
{
    return ((f == node.f) && (p == node.p));
}

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream &os, const BranchingScheme::SolutionNode& node)
{
    os << "f " << node.f << " p " << node.p;
    return os;
}

/********************************** NodeItem **********************************/

bool BranchingScheme::NodeItem::operator==(const BranchingScheme::NodeItem& node_item) const
{
    return ((j == node_item.j) && (node == node_item.node));
}

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream &os, const BranchingScheme::NodeItem& node_item)
{
    os << "j " << node_item.j << " node " << node_item.node;
    return os;
}

/********************************* Insertion **********************************/

bool BranchingScheme::Insertion::operator==(const Insertion& insertion) const
{
    return (
               (j1 == insertion.j1) && (j2 == insertion.j2) && (df == insertion.df)
            && (x1 == insertion.x1) && (y2 == insertion.y2) && (x3 == insertion.x3)
            && (x1_max == insertion.x1_max) && (y2_max == insertion.y2_max)
            && (z1 == insertion.z1) && (z2 == insertion.z2));
}

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream &os, const BranchingScheme::Insertion& insertion)
{
    os
        << "j1 " << insertion.j1 << " j2 " << insertion.j2 << " df " << insertion.df
        << " x1 " << insertion.x1 << " y2 " << insertion.y2 << " x3 " << insertion.x3
        << " x1_max " << insertion.x1_max << " y2_max " << insertion.y2_max
        << " z1 " << insertion.z1 << " z2 " << insertion.z2;
    return os;
}

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream &os, const std::vector<BranchingScheme::Insertion>& insertions)
{
    std::copy(insertions.begin(), insertions.end(), std::ostream_iterator<BranchingScheme::Insertion>(os, "\n"));
    return os;
}

/********************************** SubPlate **********************************/

bool BranchingScheme::SubPlate::operator==(const BranchingScheme::SubPlate& c) const
{
    return ((node == c.node) && (n == c.n)
            && (l == c.l) && (r == c.r)
            && (b == c.b) && (t == c.t));
}

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream &os, const BranchingScheme::SubPlate& c)
{
    os << "node " << c.node << " n " << c.n
        << " l " << c.l << " b " << c.b
        << " r " << c.r << " t " << c.t;
    return os;
}

/*********************************** Front ************************************/

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream &os, const BranchingScheme::Front& front)
{
    os << "i " << front.i
        << " x1_prev " << front.x1_prev << " x3_curr " << front.x3_curr
        << " x1_curr " << front.x1_curr
        << " y2_prev " << front.y2_prev << " y2_curr " << front.y2_curr
        ;
    return os;
}

/************************************ Node ************************************/

BranchingScheme::Node::Node(const BranchingScheme& branching_scheme):
    branching_scheme_(branching_scheme),
    pos_stack_(instance().stack_number(), 0)
{
    compute_ub_profit();
}

BranchingScheme::Node::Node(const BranchingScheme::Node& node):
    branching_scheme_(node.branching_scheme_),
    nodes_(node.nodes_),
    pos_stack_(node.pos_stack_),
    items_(node.items_),
    first_stage_orientation_(node.first_stage_orientation_),
    item_area_(node.item_area_),
    squared_item_area_(node.squared_item_area_),
    current_area_(node.current_area_),
    waste_(node.waste_),
    profit_(node.profit_),
    ub_profit_(node.ub_profit_),
    subplates_curr_(node.subplates_curr_),
    subplates_prev_(node.subplates_prev_),
    x1_max_(node.x1_max_),
    y2_max_(node.y2_max_),
    z1_(node.z1_),
    z2_(node.z2_),
    df_min_(node.df_min_),
    subplate2curr_items_above_defect_(node.subplate2curr_items_above_defect_)
{ }

BranchingScheme::Node& BranchingScheme::Node::operator=(const BranchingScheme::Node& node)
{
    if (this != &node) {
        nodes_                            = node.nodes_;
        items_                            = node.items_;
        pos_stack_                        = node.pos_stack_;
        first_stage_orientation_          = node.first_stage_orientation_;
        item_area_                        = node.item_area_;
        squared_item_area_                = node.squared_item_area_;
        current_area_                     = node.current_area_;
        waste_                            = node.waste_;
        profit_                           = node.profit_;
        ub_profit_                        = node.ub_profit_;
        subplates_prev_                   = node.subplates_prev_;
        subplates_curr_                   = node.subplates_curr_;
        x1_max_                           = node.x1_max_;
        y2_max_                           = node.y2_max_;
        z1_                               = node.z1_;
        z2_                               = node.z2_;
        df_min_                           = node.df_min_;
        subplate2curr_items_above_defect_ = node.subplate2curr_items_above_defect_;
    }
    return *this;
}

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream &os, const BranchingScheme::Node& node)
{
    os << "n " << node.item_number()
        << " m " << node.bin_number() << std::endl;
    os << "item_area " << node.item_area()
        << " current_area " << node.area() << std::endl;
    os
        << "waste " << node.waste()
        << " waste_percentage " << node.waste_percentage()
        << " profit " << node.profit()
        << " ub_profit " << node.ub_profit()
        << std::endl;
    os << "x1_max " << node.x1_max() << " y2_max " << node.y2_max()
        << " z1 " << node.z1() << " z2 " << node.z2() << std::endl;
    os << "pos_stack" << std::flush;
    for (StackId s = 0; s < node.instance().stack_number(); ++s)
        os << " " << node.pos_stack(s);
    os << std::endl;
    for (Depth d = 0; d <= 3; ++d) {
        os << "subplate_curr(" << d << ")";
        if (node.subplate_curr(d).n != -1)
            os
                << " node " << node.subplate_curr(d).node
                << " n " << node.subplate_curr(d).n
                << " p " << node.node(node.subplate_curr(d).node).p;
        os << " - " << node.subplate_curr(d) << std::endl;
        os << "subplate_prev(" << d << ")";
        if (node.subplate_prev(d).n != -1)
            os
                << " node " << node.subplate_prev(d).node
                << " n " << node.subplate_prev(d).n
                << " p " << node.node(node.subplate_prev(d).node).p;
        os << " - " << node.subplate_prev(d) << std::endl;
    }
    os << std::endl;

    for (SolutionNodeId id = 0; id < (SolutionNodeId)node.nodes().size(); ++id) {
        const BranchingScheme::SolutionNode& solution_node = node.node(id);
        os << "id " << id
            << "\tf " << solution_node.f
            << "\tp " << solution_node.p << std::endl;
    }
    os << std::endl;

    for (const BranchingScheme::NodeItem& node_item: node.items())
        os << "j " << node_item.j << "\tnode " << node_item.node << std::endl;

    return os;
}

bool BranchingScheme::Node::bound(const Solution& sol_best) const
{
    switch (sol_best.instance().objective()) {
    case Objective::Default: {
        if (!sol_best.full()) {
            return (ub_profit() <= sol_best.profit());
        } else {
            if (ub_profit() != sol_best.profit())
                return (ub_profit() <= sol_best.profit());
            return waste() >= sol_best.waste();
        }
    } case Objective::BinPacking: {
        if (!sol_best.full())
            return false;
        if (bin_number() >= sol_best.bin_number())
            return true;
        return waste() >= sol_best.waste();
    } case Objective::BinPackingWithLeftovers: {
        if (!sol_best.full())
            return false;
        return waste() >= sol_best.waste();
    } case Objective::Knapsack: {
        return ub_profit() <= sol_best.profit();
    } case Objective::StripPackingWidth: {
        if (!sol_best.full())
            return false;
        return waste() >= sol_best.waste();
    } case Objective::StripPackingHeight: {
        if (!sol_best.full())
            return false;
        return waste() >= sol_best.waste();
    } default: {
        assert(false);
        std::cerr << "\033[31m" << "ERROR, branching scheme rectangle::BranchingScheme does not implement objective \"" << sol_best.instance().objective() << "\"" << "\033[0m" << std::endl;
        return false;
    }
    }
}

/****************************** apply_insertion *******************************/

void BranchingScheme::Node::compute_ub_profit()
{
    Area remaining_item_area = instance().item_area() - item_area();
    Area remaining_packabla_area = instance().packable_area() - area();
    if (remaining_packabla_area >= remaining_item_area) {
        ub_profit_ = instance().item_profit();
    } else {
        ItemTypeId j = instance().max_efficiency_item();
        ub_profit_ = profit_ + remaining_packabla_area
            * instance().item(j).profit / instance().item(j).rect.area();
    }
}

void BranchingScheme::Node::apply_insertion(const BranchingScheme::Insertion& insertion, Info& info)
{
    LOG_FOLD_START(info, "apply_insertion " << insertion << std::endl);
    assert(insertion.df <= -1 || insertion.x1 >= x1_curr());

    // Update bin_number_
    if (insertion.df < 0)
        first_stage_orientation_.push_back(last_bin_orientation(insertion.df));

    BinPos i = bin_number() - 1;
    CutOrientation o = first_stage_orientation_.back();
    Length h = instance().bin(i).height(o);
    Length w = instance().bin(i).width(o);

    Length w_j = insertion.x3 - x3_prev(insertion.df);
    bool rotate_j1 = (insertion.j1 == -1)? false: (instance().width(instance().item(insertion.j1), true, o) == w_j);
    bool rotate_j2 = (insertion.j2 == -1)? false: (instance().width(instance().item(insertion.j2), true, o) == w_j);
    Length h_j1 = (insertion.j1 == -1)? -1: instance().height(instance().item(insertion.j1), rotate_j1, o);
    Length h_j2 = (insertion.j2 == -1)? -1: instance().height(instance().item(insertion.j2), rotate_j2, o);

    // Update items_, items_area_ and pos_stack_
    SolutionNodeId id = (insertion.df >= 0)? nodes().size() + 2 - insertion.df: nodes().size() + 2;
    if (insertion.j1 != -1) {
        items_.push_back({.j = insertion.j1, .node = id});
        item_area_ += instance().item(insertion.j1).rect.area();
        squared_item_area_ += instance().item(insertion.j1).rect.area() * instance().item(insertion.j1).rect.area();
        pos_stack_[instance().item(insertion.j1).stack]++;
        profit_ += instance().item(insertion.j1).profit;
    }
    if (insertion.j2 != -1) {
        items_.push_back({.j = insertion.j2, .node = id});
        item_area_ += instance().item(insertion.j2).rect.area();
        squared_item_area_ += instance().item(insertion.j2).rect.area() * instance().item(insertion.j2).rect.area();
        pos_stack_[instance().item(insertion.j2).stack]++;
        profit_ += instance().item(insertion.j2).profit;
    }

    // Update subplate2curr_items_above_defect_
    if (insertion.df != 2)
        subplate2curr_items_above_defect_.clear();
    if (insertion.j1 == -1 && insertion.j2 != -1) {
        JRX jrx;
        jrx.j = insertion.j2;
        jrx.rotate = rotate_j2;
        jrx.x = x3_prev(insertion.df);
        subplate2curr_items_above_defect_.push_back(jrx);
    }

    // Update df_min_
    // TODO testing needed
    df_min_ = -2;
    if (branching_scheme().symmetry_2()) {
        if (insertion.j1 == -1 && insertion.j2 == -1) { // add defect
            if (insertion.df >= 0)
                df_min_ = 0;
            if (insertion.df >= 1 && insertion.x1 == x1_curr())
                df_min_ = 1;
            if (insertion.df == 2 && insertion.y2 == y2_curr())
                df_min_ = 2;
        } else if (insertion.j1 == -1 && insertion.j2 != -1) { // add item above defect
            // If df < 2 and y2 - hj doesn't intersect a defect, then we force to
            // add the next item in the same 2-cut.
            if (insertion.df < 2 && instance().y_intersects_defect(insertion.x3, w, insertion.y2 - h_j2, i, o) == -1)
                df_min_ = 2;
        } else if (insertion.j1 != -1 && insertion.j2 != -1) { // add 2 items
            // If df < 2 and both y2 - hj1 and y2 - hj2 intersect a defect, then we
            // force to add the next item in the same 2-cut.
            if (insertion.df < 2) {
                if (instance().item(insertion.j1).stack == instance().item(insertion.j2).stack) {
                    if (instance().y_intersects_defect(insertion.x3, w, y2_prev(insertion.df) + h_j1, i, o) == -1)
                        df_min_ = 2;
                } else {
                    if (instance().y_intersects_defect(insertion.x3, w, insertion.y2 - h_j1, i, o) == -1
                            || instance().y_intersects_defect(insertion.x3, w, insertion.y2 - h_j2, i, o) == -1)
                        df_min_ = 2;
                }
            }
        }
    }

    // Update subplates_prev_ and subplates_curr_
    ItemPos n = 0;
    if (insertion.j1 != -1)
        n++;
    if (insertion.j2 != -1)
        n++;
    switch (insertion.df) {
    case -1: case -2: {
        subplates_prev_[0] = subplate_curr(0);
        subplates_curr_[0] = {.node = static_cast<SolutionNodeId>(-bin_number()),
            .n = n, .l = 0, .b = 0};
        for (Depth d = 1; d <= 3; ++d) {
            subplates_prev_[d].n = -1;
            subplates_curr_[d] = {
                .node = static_cast<SolutionNodeId>(node_number() + d - 1),
                .n = n, .l = 0, .b = 0};
        }
        break;
    } case 0: {
        subplates_curr_[0].n += n;
        subplates_prev_[1] = subplate_curr(1);
        subplates_curr_[1] = {.node = node_number(), .n = n, .l = x1_prev(), .b = 0};
        subplates_prev_[2].n = -1;
        subplates_curr_[2] = {.node = static_cast<SolutionNodeId>(node_number() + 1),
            .n = n, .l = x1_prev(), .b = 0};
        subplates_prev_[3].n = -1;
        subplates_curr_[3] = {.node = static_cast<SolutionNodeId>(node_number() + 2),
            .n = n, .l = x1_prev(), .b = 0};
        break;
    } case 1: {
        subplates_curr_[0].n += n;
        subplates_curr_[1].n += n;
        subplates_prev_[2] = subplate_curr(2);
        subplates_curr_[2] = {.node = node_number(), .n = n, .l = x1_prev(), .b = y2_prev()};
        subplates_prev_[3].n = -1;
        subplates_curr_[3] = {.node = static_cast<SolutionNodeId>(node_number() + 1),
            .n = n, .l = x1_prev(), .b = y2_prev()};
        break;
    } case 2: {
        subplates_curr_[0].n += n;
        subplates_curr_[1].n += n;
        subplates_curr_[2].n += n;
        subplates_prev_[3] = subplate_curr(3);
        subplates_curr_[3] = {.node = node_number(), .n = n, .l = x3_curr(), .b = y2_prev()};
        break;
    } default: {
        assert(false);
        break;
    }
    }
    // Update subplates coordinates
    subplates_curr_[0].r = insertion.x1;
    if (subplates_curr_[0].t < insertion.y2)
        subplates_curr_[0].t = insertion.y2;
    subplates_curr_[1].r = insertion.x1;
    subplates_curr_[1].t = insertion.y2;
    subplates_curr_[2].r = insertion.x3;
    subplates_curr_[2].t = insertion.y2;
    subplates_curr_[3].r = insertion.x3;
    subplates_curr_[3].t = insertion.y2;

    // Add new solution nodes
    SolutionNodeId f = (insertion.df <= -1)? -bin_number(): subplate_curr(insertion.df).node;
    Depth d = (insertion.df <= -1)? 0: insertion.df;
    SolutionNodeId c = nodes_.size() - 1;
    do {
        c++;
        nodes_.push_back({.f = f});
        f = c;
        d++;
    } while (d != 3);
    nodes_[subplate_curr(1).node].p = insertion.x1;
    nodes_[subplate_curr(2).node].p = insertion.y2;
    nodes_[subplate_curr(3).node].p = insertion.x3;

    // Update x1_max_, y2_max_, z1_ and z2_
    x1_max_ = insertion.x1_max;
    y2_max_ = insertion.y2_max;
    z1_     = insertion.z1;
    z2_     = insertion.z2;

    // Update current_area_ and waste_
    current_area_ = instance().previous_bin_area(i);
    if (full()) {
        current_area_ += (branching_scheme().cut_type_1() == CutType1::ThreeStagedGuillotine)?
            x1_curr() * h: y2_curr() * w;
    } else {
        current_area_ += x1_prev() * h
            + (x1_curr() - x1_prev()) * y2_prev()
            + (x3_curr() - x1_prev()) * (y2_curr() - y2_prev());
    }
    waste_ = current_area_ - item_area_;
    assert(waste_ >= 0);
    compute_ub_profit();

    LOG_FOLD_END(info, "apply_insertion");
}

/********************************** children **********************************/

std::vector<BranchingScheme::Insertion> BranchingScheme::Node::children(Info& info) const
{
    LOG_FOLD_START(info, "children" << std::endl);

    std::vector<Insertion> insertions;

    if (full())
        return insertions;

    // Compute df_min
    Depth df_min = -2;
    if (bin_number() == instance().bin_number()) {
        df_min = 0;
    } else if (branching_scheme().first_stage_orientation() == CutOrientation::Vertical) {
        df_min = -1;
    } else if (branching_scheme().first_stage_orientation() == CutOrientation::Any
            && instance().bin(bin_number()).defects.size() == 0                           // Next bin has no defects,
            && instance().bin(bin_number()).rect.w == instance().bin(bin_number()).rect.h // is a square,
            && branching_scheme().no_oriented_items()) {                                  // and item can be rotated
        df_min = -1;
    }
    if (df_min < df_min_)
        df_min = df_min_;

    // Compute df_max
    Depth df_max = 2;
    if (node_number() == 0)
        df_max = -1;

    LOG(info, "df_max " << df_max << " df_min " << df_min << std::endl);
    for (Depth df = df_max; df >= df_min; --df) {
        LOG(info, "df " << df << std::endl);
        if (df == -1 && branching_scheme().first_stage_orientation() == CutOrientation::Horinzontal)
            continue;

        // Symmetry breaking strategy
        if (df >= branching_scheme().symmetry_depth() - 1 && !check_symmetries(df, info))
            break;

        // Simple dominance rule
        bool stop = false;
        for (const Insertion& insertion: insertions) {
            if (insertion.j1 == -1 && insertion.j2 == -1)
                continue;
            if        (df == 1 && insertion.x1 == x1_curr() && insertion.y2 == y2_curr()) {
                stop = true;
                break;
            } else if (df == 0 && insertion.x1 == x1_curr()) {
                stop = true;
                break;
            } else if (df < 0 && insertion.df >= 0) {
                stop = true;
                break;
            }
        }
        if (stop)
            break;

        CutOrientation o = last_bin_orientation(df);
        Length x = x3_prev(df);
        Length y = y2_prev(df);

        // Try adding an item
        for (StackId s = 0; s < instance().stack_number(); ++s) {
            if (pos_stack_[s] == instance().stack_size(s))
                continue;
            StackId sp = branching_scheme().stack_pred(s);
            if (sp != -1 && pos_stack_[sp] <= pos_stack_[s])
                continue;

            ItemTypeId j = instance().item(s, pos_stack_[s]).id;

            if (df == 2 && branching_scheme().cut_type_2() == CutType2::Homogenous)
                if (j != items_.back().j)
                    continue;

            if (!branching_scheme().oriented(j)) {
                bool b = instance().item(j).rect.w > instance().item(j).rect.h;
                insertion_1_item(insertions, j, !b, df, info);
                insertion_1_item(insertions, j,  b, df, info);
                //insertion_1_item(insertions, j, false, df, df_min, info);
                //insertion_1_item(insertions, j, true, df, df_min, info);
            } else {
                insertion_1_item(insertions, j, false, df, info);
            }

            // Try adding it with a second item
            if (branching_scheme().cut_type_2() == CutType2::Roadef2018) {
                LOG(info, "try adding with a second item" << std::endl);
                for (StackId s2 = s; s2 < instance().stack_number(); ++s2) {
                    ItemTypeId j2 = -1;
                    if (s2 == s) {
                        if (pos_stack_[s2] + 1 == instance().stack_size(s2))
                            continue;
                        StackId sp2 = branching_scheme().stack_pred(s2);
                        if (sp2 != -1 && pos_stack_[sp2] <= pos_stack_[s2])
                            continue;
                        j2 = instance().item(s2, pos_stack_[s2] + 1).id;
                    } else {
                        if (pos_stack_[s2] == instance().stack_size(s2))
                            continue;
                        StackId sp2 = branching_scheme().stack_pred(s2);
                        if (                    (sp2 == s && pos_stack_[sp2] + 1 <= pos_stack_[s2])
                                || (sp2 != -1 && sp2 != s && pos_stack_[sp2]     <= pos_stack_[s2]))
                            continue;
                        j2 = instance().item(s2, pos_stack_[s2]).id;
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
                    const Item& item1 = instance().item(j);
                    const Item& item2 = instance().item(j2);
                    if (instance().width(item1, false, o) == instance().width(item2, false, o))
                        insertion_2_items(insertions, j, false, j2, false, df, info);

                    if (!branching_scheme().oriented(j2))
                        if (instance().width(item1, false, o) == instance().width(item2, true, o))
                            insertion_2_items(insertions, j, false, j2, true,  df, info);
                    if (!branching_scheme().oriented(j))
                        if (instance().width(item1, true, o) == instance().width(item2, false, o))
                            insertion_2_items(insertions, j, true,  j2, false, df, info);
                    if (!branching_scheme().oriented(j2) && !branching_scheme().oriented(j))
                        if (instance().width(item1, true, o) == instance().width(item2, true, o))
                            insertion_2_items(insertions, j, true,  j2, true,  df, info);
                }
            }
        }

        if (items_.empty() || items_.back().node == (SolutionNodeId)nodes_.size() - 1) {
            const std::vector<Defect>& defects = instance().bin(last_bin(df)).defects;
            for (const Defect& defect: defects)
                if (instance().left(defect, o) >= x && instance().bottom(defect, o) >= y)
                    insertion_defect(insertions, defect, df, info);
        }
    }

    DBG(
        LOG_FOLD_START(info, "insertions" << std::endl);
        for (const Insertion& insertion: insertions)
            LOG(info, insertion << std::endl);
        LOG_FOLD_END(info, "");
    );

    LOG_FOLD_END(info, "children");
    return insertions;
}

bool BranchingScheme::Node::dominates(Front f1, Front f2, const BranchingScheme& branching_scheme)
{
    if (f1.i < f2.i) return true;
    if (f1.i > f2.i) return false;
    if (f1.o != f2.o) return false;
    if (f2.y2_curr != branching_scheme.instance().bin(f1.i).height(f1.o) && f1.x1_prev > f2.x1_prev) return false;
    if (f1.x1_curr > f2.x1_curr) return false;
    if        (f2.y2_prev <  f1.y2_prev) { if (f1.x1_curr > f2.x3_curr) return false;
    } else if (f2.y2_prev <  f1.y2_curr) { if (f1.x3_curr > f2.x3_curr) return false;
    } else  /* f2.y2_prev <= h */        { if (f1.x1_prev > f2.x3_curr) return false; }
    if        (f2.y2_curr <  f1.y2_prev) { if (f1.x1_curr > f2.x1_prev) return false;
    } else if (f2.y2_curr <  f1.y2_curr) { if (f1.x3_curr > f2.x1_prev) return false;
    } else  /* f2.y2_curr <= h */        { /* if (f1.x1_prev > f2.x1_prev) return false */; }
    return true;
}

Area BranchingScheme::Node::waste(const Insertion& insertion) const
{
    BinPos i = last_bin(insertion.df);
    CutOrientation o = last_bin_orientation(insertion.df);
    Length h = instance().bin(i).height(o);
    Front f = front(insertion);
    ItemPos n = item_number();
    Area item_area = item_area_;
    if (insertion.j1 != -1) {
        n++;
        item_area += instance().item(insertion.j1).rect.area();
    }
    if (insertion.j2 != -1) {
        n++;
        item_area += instance().item(insertion.j2).rect.area();
    }
    Area current_area = (n == instance().item_number())?
        instance().previous_bin_area(i)
        + (f.x1_curr * h):
        instance().previous_bin_area(i)
        + f.x1_prev * h
        + (f.x1_curr - f.x1_prev) * f.y2_prev
        + (f.x3_curr - f.x1_prev) * (f.y2_curr - f.y2_prev);
    return current_area - item_area;
}

bool BranchingScheme::Node::check_symmetries(Depth df, Info& info) const
{
    LOG_FOLD_START(info, "check_symmetries df " << df << std::endl);
    // If we want to open a new d-cut, if the current d-cut and the preivous
    // d-cut don't contain defects, then we compute the lowest index of the
    // items of each cut.
    // If the lowest index of the current cut is smaller than the lowest index
    // of the previous cut and the 2 cuts can be exchanged without violating
    // the precedences, then we don't consider this solution.

    if (bin_number() == 0) {
        LOG_FOLD_END(info, "no item");
        return true;
    }
    if (subplate_prev(df + 1).n == -1) {
        LOG_FOLD_END(info, "no previous " << df + 1 << "-cut");
        return true;
    }

    LOG(info, "subplate_prev(" << df + 1 << ") " << subplate_prev(df + 1) << std::endl);
    LOG(info, "subplate_curr(" << df + 1 << ") " << subplate_curr(df + 1) << std::endl);

    // Check defects
    BinPos i = bin_number() - 1;
    CutOrientation o = first_stage_orientation(i);
    switch (df) {
    case -1: case -2: {
        assert(false);
        return true;
        break;
    } case 0: {
        // TODO: check defect intersections
        Length x1 = subplate_prev(1).l;
        Length x2 = subplate_curr(1).r;
        Length y1 = 0;
        Length y2 = std::max(subplate_prev(1).t, subplate_curr(1).t);

        DefectId k = instance().rect_intersects_defect(x1, x2, y1, y2, i, o);
        if (k != -1) {
            LOG_FOLD_END(info, "contains defect");
            return true;
        }
        break;
    } case 1: {
        Length x1 = subplate_curr(2).l;
        Length x2 = std::max(subplate_prev(2).r, subplate_curr(2).r);
        Length y1 = subplate_prev(2).b;
        Length y2 = subplate_curr(2).t;

        Length y_new = subplate_prev(2).b + subplate_curr(2).t - subplate_curr(2).b;
        Length w = instance().bin(i).width(o);
        DefectId k0 = instance().y_intersects_defect(subplate_curr(2).l, w, y_new, i, o);
        if (k0 != -1) {
            LOG_FOLD_END(info, "intersects defect");
            return true;
        }

        DefectId k = instance().rect_intersects_defect(x1, x2, y1, y2, i, o);
        if (k != -1) {
            LOG_FOLD_END(info, "contains defect");
            return true;
        }
        break;
    } case 2: {
        Length x1 = subplate_prev(3).l;
        Length x2 = subplate_curr(3).r;
        Length y1 = subplate_curr(3).b;
        Length y2 = std::max(subplate_prev(3).t, subplate_curr(3).t);

        Length x_new = subplate_prev(3).l + subplate_curr(3).r - subplate_curr(3).l;
        DefectId k0 = instance().x_intersects_defect(x_new, i, o);
        if (k0 != -1) {
            LOG_FOLD_END(info, "intersects defect");
            return true;
        }

        DefectId k = instance().rect_intersects_defect(x1, x2, y1, y2, i, o);
        if (k != -1) {
            LOG_FOLD_END(info, "contains defect");
            return true;
        }
        break;
    } default: {
        assert(false);
    }
    }

    // Compute min
    ItemTypeId jmin_curr = instance().item_number();
    LOG(info, "subplate_curr(" << df + 1 << ")");
    ItemPos n1 = subplate_curr(df + 1).n;
    ItemPos n2 = subplate_curr(df + 1).n + subplate_prev(df + 1).n;
    assert(n1 != 0);
    assert(n2 > n1);
    assert(n2 <= item_number());
    for (auto item = items().rbegin(); item < items().rbegin() + n1; ++item) {
        LOG(info, " " << item->j);
        if (jmin_curr > item->j)
            jmin_curr = item->j;
    }
    LOG(info, " jmin_curr " << jmin_curr << std::endl);

    ItemTypeId jmin_prev = instance().item_number();
    LOG(info, "subplate_prev(" << df + 1 << ")");
    for (auto item = items().rbegin() + n1; item < items().rbegin() + n2; ++item) {
        LOG(info, " " << item->j);
        if (jmin_prev > item->j)
            jmin_prev = item->j;
    }
    LOG(info, " jmin_prev " << jmin_prev << std::endl);

    if (jmin_prev <= jmin_curr) {
        LOG_FOLD_END(info, "jmin_prev < jmin_curr");
        return true;
    }

    // Check precedences
    for (auto item_curr = items().rbegin(); item_curr < items().rbegin() + n1;
            ++item_curr) {
        for (auto item_prev = items().rbegin() + n1;
                item_prev < items().rbegin() + n2; ++item_prev) {
            if (item_prev->j < item_curr->j
                    && instance().item(item_prev->j).stack
                    == instance().item(item_curr->j).stack) {
                LOG_FOLD_END(info, item_prev->j << " precedes " << item_curr->j);
                return true;
            }
        }
    }

    LOG_FOLD_END(info, "");
    return false;
}

BinPos BranchingScheme::Node::last_bin(Depth df) const
{
    if (df <= -1) {
        return (node_number() == 0)? 0: bin_number();
    } else {
        return bin_number() - 1;
    }
}

CutOrientation BranchingScheme::Node::last_bin_orientation(Depth df) const
{
    switch (df) {
    case -1: {
        return CutOrientation::Vertical;
    } case -2: {
        return CutOrientation::Horinzontal;
    } default: {
        return first_stage_orientation_.back();
    }
    }
}

BranchingScheme::Front BranchingScheme::Node::front(const Insertion& insertion) const
{
    switch (insertion.df) {
    case -1: case -2: {
        return {.i = last_bin(insertion.df), .o = last_bin_orientation(insertion.df),
            .x1_prev = 0, .x3_curr = insertion.x3, .x1_curr = insertion.x1,
            .y2_prev = 0, .y2_curr = insertion.y2};
    } case 0: {
        return {.i = last_bin(insertion.df), .o = last_bin_orientation(insertion.df),
            .x1_prev = x1_curr(), .x3_curr = insertion.x3, .x1_curr = insertion.x1,
            .y2_prev = 0, .y2_curr = insertion.y2};
    } case 1: {
        return {.i = last_bin(insertion.df), .o = last_bin_orientation(insertion.df),
            .x1_prev = x1_prev(), .x3_curr = insertion.x3, .x1_curr = insertion.x1,
            .y2_prev = y2_curr(), .y2_curr = insertion.y2};
    } case 2: {
        return {.i = last_bin(insertion.df), .o = last_bin_orientation(insertion.df),
            .x1_prev = x1_prev(), .x3_curr = insertion.x3, .x1_curr = insertion.x1,
            .y2_prev = y2_prev(), .y2_curr = insertion.y2};
    } default: {
        assert(false);
        return {.i = -1, .o = CutOrientation::Vertical,
            .x1_prev = -1, .x3_curr = -1, .x1_curr = -1,
            .y2_prev = -1, .y2_curr = -1};
    }
    }
}

Length BranchingScheme::Node::x1_prev(Depth df) const
{
    switch (df) {
    case -1: case -2: {
        return 0;
    } case 0: {
        return x1_curr();
    } case 1: {
        return x1_prev();
    } case 2: {
        return x1_prev();
    } default: {
        assert(false);
        return -1;
    }
    }
}

Length BranchingScheme::Node::x3_prev(Depth df) const
{
    switch (df) {
    case -1: case -2: {
        return 0;
    } case 0: {
        return x1_curr();
    } case 1: {
        return x1_prev();
    } case 2: {
        return x3_curr();
    } default: {
        assert(false);
        return -1;
    }
    }
}

Length BranchingScheme::Node::x1_max(Depth df) const
{
    switch (df) {
    case -1: case -2: {
        BinPos i = last_bin(df);
        CutOrientation o = last_bin_orientation(df);
        Length x = instance().bin(i).width(o);
        if (branching_scheme().max1cut() != -1)
            if (x > x1_prev(df) + branching_scheme().max1cut())
                x = x1_prev(df) + branching_scheme().max1cut();
        return x;
    } case 0: {
        BinPos i = last_bin(df);
        CutOrientation o = last_bin_orientation(df);
        Length x = instance().bin(i).width(o);
        if (branching_scheme().max1cut() != -1)
            if (x > x1_prev(df) + branching_scheme().max1cut())
                x = x1_prev(df) + branching_scheme().max1cut();
        return x;
    } case 1: {
        BinPos i = last_bin(df);
        CutOrientation po = last_bin_orientation(df);
        Length x = x1_max_;
        if (!branching_scheme().cut_through_defects())
            for (const Defect& k: instance().bin(i).defects)
                if (instance().bottom(k, po) < y2_curr() && instance().top(k, po) > y2_curr())
                    if (instance().left(k, po) > x1_prev())
                        if (x > instance().left(k, po))
                            x = instance().left(k, po);
        return x;
    } case 2: {
        return x1_max_;
    } default: {
        return -1;
    }
    }
}

Length BranchingScheme::Node::y2_prev(Depth df) const
{
    switch (df) {
    case -1: case -2: {
        return 0;
    } case 0: {
        return 0;
    } case 1: {
        return y2_curr();
    } case 2: {
        return y2_prev();
    } default: {
        assert(false);
        return -1;
    }
    }
}

Length BranchingScheme::Node::y2_max(Depth df, Length x3) const
{
    BinPos i = last_bin(df);
    CutOrientation o = last_bin_orientation(df);
    Length y = (df == 2)? y2_max_: instance().bin(i).height(o);
    if (!branching_scheme().cut_through_defects())
        for (const Defect& k: instance().bin(i).defects)
            if (instance().left(k, o) < x3 && instance().right(k, o) > x3)
                if (instance().bottom(k, o) >= y2_prev(df))
                    if (y > instance().bottom(k, o))
                        y = instance().bottom(k, o);
    return y;
}

void BranchingScheme::Node::insertion_1_item(std::vector<Insertion>& insertions,
        ItemTypeId j, bool rotate, Depth df, Info& info) const
{
    LOG_FOLD_START(info, "insertion_1_item"
            << " j " << j << " rotate " << rotate << " df " << df << std::endl);
    assert(-2 <= df); assert(df <= 3);

    // Check defect intersection
    BinPos i = last_bin(df);
    CutOrientation o = last_bin_orientation(df);
    const rectangleguillotine::Item& item = instance().item(j);
    Length x = x3_prev(df) + instance().width(item, rotate, o);
    Length y = y2_prev(df) + instance().height(item, rotate, o);
    Length w = instance().bin(i).width(o);
    Length h = instance().bin(i).height(o);
    LOG(info, "x3_prev(df) " << x3_prev(df) << " y2_prev(df) " << y2_prev(df)
            << " i " << i
            << " w " << instance().width(item, rotate, o)
            << " h " << instance().height(item, rotate, o)
            << std::endl);
    if (x > w) {
        LOG_FOLD_END(info, "too wide x " << x << " > w " << w);
        return;
    }
    if (y > h) {
        LOG_FOLD_END(info, "too high y " << y << " > h " << h);
        return;
    }

    Insertion insertion {
        .j1 = j, .j2 = -1, .df = df,
        .x1 = x, .y2 = y, .x3 = x,
        .x1_max = x1_max(df), .y2_max = y2_max(df, x), .z1 = 0, .z2 = 0};
    LOG(info, insertion << std::endl);

    // Check defect intersection
    DefectId k = instance().item_intersects_defect(x3_prev(df), y2_prev(df), item, rotate, i, o);
    if (k >= 0) {
        if (branching_scheme().cut_type_2() == CutType2::Roadef2018
                || branching_scheme().cut_type_2() == CutType2::NonExact) {
            // Place the item on top of its third-level sub-plate
            insertion.j1 = -1;
            insertion.j2 = j;
        } else {
            LOG_FOLD_END(info, "intersects defect");
            return;
        }
    }

    // Update insertion.z2 with respect to cut_type_2()
    if (branching_scheme().cut_type_2() == CutType2::Exact
            || branching_scheme().cut_type_2() == CutType2::Homogenous)
        insertion.z2 = 2;

    update(insertions, insertion, info);
}

void BranchingScheme::Node::insertion_2_items(std::vector<Insertion>& insertions,
        ItemTypeId j1, bool rotate1, ItemTypeId j2, bool rotate2, Depth df, Info& info) const
{
    LOG_FOLD_START(info, "insertion_2_items"
            << " j1 " << j1 << " rotate1 " << rotate1
            << " j2 " << j2 << " rotate2 " << rotate2
            << " df " << df << std::endl);
    assert(-2 <= df); assert(df <= 3);

    // Check defect intersection
    BinPos i = last_bin(df);
    CutOrientation o = last_bin_orientation(df);
    const Item& item1 = instance().item(j1);
    const Item& item2 = instance().item(j2);
    Length w = instance().bin(i).width(o);
    Length h = instance().bin(i).height(o);
    Length h_j1 = instance().height(item1, rotate1, o);
    Length x = x3_prev(df) + instance().width(item1, rotate1, o);
    Length y = y2_prev(df) + h_j1
                           + instance().height(item2, rotate2, o);
    if (x > w || y > h) {
        LOG_FOLD_END(info, "too wide/high");
        return;
    }
    if (instance().item_intersects_defect(x3_prev(df), y2_prev(df), item1, rotate1, i, o) >= 0
            || instance().item_intersects_defect(x3_prev(df), y2_prev(df) + h_j1, item2, rotate2, i, o) >= 0) {
        LOG_FOLD_END(info, "intersects defect");
        return;
    }

    Insertion insertion {
        .j1 = j1, .j2 = j2, .df = df,
        .x1 = x, .y2 = y, .x3 = x,
        .x1_max = x1_max(df), .y2_max = y2_max(df, x), .z1 = 0, .z2 = 2};
    LOG(info, insertion << std::endl);

    update(insertions, insertion, info);
}

void BranchingScheme::Node::insertion_defect(std::vector<Insertion>& insertions,
            const Defect& k, Depth df, Info& info) const
{
    LOG_FOLD_START(info, "insertion_defect"
            << " k " << k.id << " df " << df << std::endl);
    assert(-2 <= df);
    assert(df <= 3);

    // Check defect intersection
    BinPos i = last_bin(df);
    CutOrientation o = last_bin_orientation(df);
    Length w = instance().bin(i).width(o);
    Length h = instance().bin(i).height(o);
    Length min_waste = branching_scheme().min_waste();
    Length x = std::max(instance().right(k, o), x3_prev(df) + min_waste);
    Length y = std::max(instance().top(k, o),   y2_prev(df) + min_waste);
    if (x > w || y > h) {
        LOG_FOLD_END(info, "too wide/high");
        return;
    }

    Insertion insertion {
        .j1 = -1, .j2 = -1, .df = df,
        .x1 = x, .y2 = y, .x3 = x,
        .x1_max = x1_max(df), .y2_max = y2_max(df, x), .z1 = 1, .z2 = 1};
    LOG(info, insertion << std::endl);

    update(insertions, insertion, info);
}

void BranchingScheme::Node::update(
        std::vector<Insertion>& insertions,
        Insertion& insertion, Info& info) const
{
    Length min_waste = branching_scheme().min_waste();
    BinPos i = last_bin(insertion.df);
    CutOrientation o = last_bin_orientation(insertion.df);
    Length w = instance().bin(i).width(o);
    Length h = instance().bin(i).height(o);

    // Update insertion.x1 and insertion.z1 with respect to min1cut()
    if ((insertion.j1 != -1 || insertion.j2 != -1)
            && insertion.x1 - x1_prev(insertion.df) < branching_scheme().min1cut()) {
        if (insertion.z1 == 0) {
            insertion.x1 = std::max(
                    insertion.x1 + branching_scheme().min_waste(),
                    x1_prev(insertion.df) + branching_scheme().min1cut());
            insertion.z1 = 1;
        } else { // insertion.z1 = 1
            insertion.x1 = x1_prev(insertion.df) + branching_scheme().min1cut();
        }
    }

    // Update insertion.y2 and insertion.z2 with respect to min2cut()
    if ((insertion.j1 != -1 || insertion.j2 != -1)
            && insertion.y2 - y2_prev(insertion.df) < branching_scheme().min2cut()) {
        if (insertion.z2 == 0) {
            insertion.y2 = std::max(
                    insertion.y2 + branching_scheme().min_waste(),
                    y2_prev(insertion.df) + branching_scheme().min2cut());
            insertion.z2 = 1;
        } else if (insertion.z2 == 1) {
            insertion.y2 = y2_prev(insertion.df) + branching_scheme().min2cut();
        } else { // insertion.z2 == 2
            return;
        }
    }

    // Update insertion.y2 and insertion.z2 with respect to one2cut()
    if (branching_scheme().one2cut() && insertion.df == 1
            && y2_prev(insertion.df) != 0 && insertion.y2 != h) {
        if (insertion.z2 == 0) {
            if (insertion.y2 + branching_scheme().min_waste() > h)
                return;
            insertion.y2 = h;
        } else if (insertion.z2 == 1) {
            insertion.y2 = h;
        } else { // insertion.z2 == 2
            return;
        }
    }

    // Update insertion.x1 if 2-staged
    if (branching_scheme().cut_type_1() == CutType1::TwoStagedGuillotine && insertion.x1 != w) {
        if (insertion.z1 == 0) {
            if (insertion.x1 + branching_scheme().min_waste() > w)
                return;
            insertion.x1 = w;
        } else { // insertion.z1 == 1
            insertion.x1 = w;
        }
    }

    // Update insertion.x1 and insertion.z1 with respect to x1_curr() and z1().
    if (insertion.df >= 1) {
        LOG(info, "i.x3 " << insertion.x3 << " x1_curr() " << x1_curr() << std::endl);
        if (insertion.z1 == 0) {
            if (insertion.x1 + min_waste <= x1_curr()) {
                insertion.x1 = x1_curr();
                insertion.z1 = z1();
            } else if (insertion.x1 < x1_curr()) { // x - min_waste < insertion.x1 < x
                if (z1() == 0) {
                    insertion.x1 = x1_curr() + min_waste;
                    insertion.z1 = 1;
                } else {
                    insertion.x1 = insertion.x1 + min_waste;
                    insertion.z1 = 1;
                }
            } else if (insertion.x1 == x1_curr()) {
            } else { // x1_curr() < insertion.x1
                if (z1() == 0 && insertion.x1 < x1_curr() + min_waste) {
                    insertion.x1 = insertion.x1 + min_waste;
                    insertion.z1 = 1;
                }
            }
        } else { // insertion.z1 == 1
            if (insertion.x1 <= x1_curr()) {
                insertion.x1 = x1_curr();
                insertion.z1 = z1();
            } else { // x1_curr() < insertion.x1
                if (z1() == 0 && x1_curr() + min_waste > insertion.x1)
                    insertion.x1 = x1_curr() + min_waste;
            }
        }
    }

    // Update insertion.y2 and insertion.z2 with respect to y2_curr() and z1().
    if (insertion.df == 2) {
        LOG(info, "i.y2 " << insertion.y2 << " y2_curr() " << y2_curr() << std::endl);
        if (insertion.z2 == 0) {
            if (insertion.y2 + min_waste <= y2_curr()) {
                insertion.y2 = y2_curr();
                insertion.z2 = z2();
            } else if (insertion.y2 < y2_curr()) { // y_curr() - min_waste < insertion.y4 < y_curr()
                if (z2() == 2) {
                    LOG_FOLD_END(info, "too high, y2_curr() " << y2_curr() << " insertion.y2 " << insertion.y2 << " insertion.z2 " << insertion.z2);
                    return;
                } else if (z2() == 0) {
                    insertion.y2 = y2_curr() + min_waste;
                    insertion.z2 = 1;
                } else { // z2() == 1
                    insertion.y2 = insertion.y2 + min_waste;
                    insertion.z2 = 1;
                }
            } else if (insertion.y2 == y2_curr()) {
                if (z2() == 2)
                    insertion.z2 = 2;
            } else if (y2_curr() < insertion.y2 && insertion.y2 < y2_curr() + min_waste) {
                if (z2() == 2) {
                    LOG_FOLD_END(info, "too high, y2_curr() " << y2_curr() << " insertion.y2 " << insertion.y2 << " insertion.z2 " << insertion.z2);
                    return;
                } else if (z2() == 0) {
                    insertion.y2 = insertion.y2 + min_waste;
                    insertion.z2 = 1;
                } else { // z2() == 1
                }
            } else { // y2_curr() + min_waste <= insertion.y2
                if (z2() == 2) {
                    LOG_FOLD_END(info, "too high, y2_curr() " << y2_curr() << " insertion.y2 " << insertion.y2 << " insertion.z2 " << insertion.z2);
                    return;
                }
            }
        } else if (insertion.z2 == 1) {
            if (insertion.y2 <= y2_curr()) {
                insertion.y2 = y2_curr();
                insertion.z2 = z2();
            } else if (y2_curr() < insertion.y2 && insertion.y2 < y2_curr() + min_waste) {
                if (z2() == 2) {
                    LOG_FOLD_END(info, "too high, y2_curr() " << y2_curr() << " insertion.y2 " << insertion.y2 << " insertion.z2 " << insertion.z2);
                    return;
                } else if (z2() == 0) {
                    insertion.y2 = y2_curr() + min_waste;
                } else { // z2() == 1
                }
            } else {
                if (z2() == 2) {
                    LOG_FOLD_END(info, "too high, y2_curr() " << y2_curr() << " insertion.y2 " << insertion.y2 << " insertion.z2 " << insertion.z2);
                    return;
                }
            }
        } else { // insertion.z2 == 2
            if (insertion.y2 < y2_curr()) {
                LOG_FOLD_END(info, "too high, y2_curr() " << y2_curr() << " insertion.y2 " << insertion.y2 << " insertion.z2 " << insertion.z2);
                return;
            } else if (insertion.y2 == y2_curr()) {
            } else if (y2_curr() < insertion.y2 && insertion.y2 < y2_curr() + min_waste) {
                if (z2() == 2) {
                    LOG_FOLD_END(info, "too high, y2_curr() " << y2_curr() << " insertion.y2 " << insertion.y2 << " insertion.z2 " << insertion.z2);
                    return;
                } else if (z2() == 0) {
                    LOG_FOLD_END(info, "too high, y2_curr() " << y2_curr() << " insertion.y2 " << insertion.y2 << " insertion.z2 " << insertion.z2);
                    return;
                } else { // z2() == 1
                }
            } else { // y2_curr() + min_waste <= insertion.y2
                if (z2() == 2) {
                    LOG_FOLD_END(info, "too high, y2_curr() " << y2_curr() << " insertion.y2 " << insertion.y2 << " insertion.z2 " << insertion.z2);
                    return;
                }
            }
        }
    }

    // Update insertion.x1 and insertion.z1 with respect to defect intersections.
    for (;;) {
        DefectId k = instance().x_intersects_defect(insertion.x1, i, o);
        if (k == -1)
            break;
        const Defect& defect = instance().defect(k);
        insertion.x1 = (insertion.z1 == 0)?
            std::max(instance().right(defect, o), insertion.x1 + min_waste):
            instance().right(defect, o);
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
    bool y2_fixed = (insertion.z2 == 2 || (insertion.df == 2 && z2() == 2));

    for (;;) {
        bool found = false;

        // Increase y2 if it intersects a defect.
        DefectId k = instance().y_intersects_defect(x1_prev(insertion.df), insertion.x1, insertion.y2, i, o);
        if (k != -1) {
            const Defect& defect = instance().defect(k);
            if (y2_fixed) {
                LOG_FOLD_END(info, "y2_fixed");
                return;
            }
            insertion.y2 = (insertion.z2 == 0)?
                std::max(instance().top(defect, o), insertion.y2 + min_waste):
                instance().top(defect, o);
            insertion.z2 = 1;
            found = true;
        }

        // Increase y2 if an item 'on top of its 3-cut' intersects a defect.
        if (insertion.df == 2) {
            for (auto jrx: subplate2curr_items_above_defect_) {
                const Item& item = instance().item(jrx.j);
                Length h = instance().height(item, jrx.rotate, o);
                Length l = jrx.x;
                Length b = insertion.y2 - h;
                //LOG(info, "j " << j << " l " << l << " r " << r << " b " << b << " t " << t << std::endl);
                DefectId k = instance().item_intersects_defect(l, b, item, jrx.rotate, i, o);
                if (k >= 0) {
                    const Defect& defect = instance().defect(k);
                    if (y2_fixed) {
                        LOG_FOLD_END(info, "y2_fixed");
                        return;
                    }
                    insertion.y2 = (insertion.z2 == 0)?
                        std::max(instance().top(defect, o) + h,
                                insertion.y2 + min_waste):
                        instance().top(defect, o) + h;
                    insertion.z2 = 1;
                    found = true;
                }
            }
        }
        if (insertion.j1 == -1 && insertion.j2 != -1) {
            const Item& item_j2 = instance().item(insertion.j2);
            Length w_j = insertion.x3 - x3_prev(insertion.df);
            bool rotate_j2 = (instance().width(item_j2, true, o) == w_j);
            Length h_j2 = instance().height(item_j2, rotate_j2, o);

            Length l = x3_prev(insertion.df);
            Length b = insertion.y2 - h_j2;
            DefectId k = instance().item_intersects_defect(l, b, item_j2, rotate_j2, i, o);
            if (k >= 0) {
                const Defect& defect = instance().defect(k);
                if (y2_fixed) {
                    LOG_FOLD_END(info, "y2_fixed");
                    return;
                }
                insertion.y2 = (insertion.z2 == 0)?
                    std::max(instance().top(defect, o) + h_j2, insertion.y2 + min_waste):
                    instance().top(defect, o) + h_j2;
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
                for (auto jrx: subplate2curr_items_above_defect_) {
                    const Item& item = instance().item(jrx.j);
                    Length l = jrx.x;
                    Length b = insertion.y2 - instance().height(item, jrx.rotate, o);
                    DefectId k = instance().item_intersects_defect(l, b, item, jrx.rotate, i, o);
                    if (k >= 0) {
                        LOG_FOLD_END(info, "too high");
                        return;
                    }
                }
            }

            if (insertion.j1 == -1 && insertion.j2 != -1) {
                const Item& item_j2 = instance().item(insertion.j2);
                Length w_j = insertion.x3 - x3_prev(insertion.df);
                bool rotate_j2 = (instance().width(item_j2, true, o) == w_j);
                Length h_j2 = instance().height(item_j2, rotate_j2, o);

                Length l = x3_prev(insertion.df);
                Length b = insertion.y2 - h_j2;
                DefectId k = instance().item_intersects_defect(l, b, item_j2, rotate_j2, i, o);
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
        if (insertion.j1 == -1 && insertion.j2 == -1) {
            if (insertion == *it) {
                LOG_FOLD_END(info, "dominated by " << *it);
                return;
            } else {
                ++it;
            }
        } else if ((it->j1 == insertion.j1 && it->j2 == insertion.j2)
                || (it->j1 == insertion.j2 && it->j2 == insertion.j1)) {
            LOG(info, "f_i  " << front(insertion) << std::endl);
            LOG(info, "f_it " << front(*it) << std::endl);
            if (dominates(front(insertion), front(*it), branching_scheme())) {
                LOG(info, "dominates " << *it << std::endl);
                if (std::next(it) != insertions.end()) {
                    *it = insertions.back();
                    insertions.pop_back();
                } else {
                    insertions.pop_back();
                    break;
                }
            } else if (dominates(front(*it), front(insertion), branching_scheme())) {
                LOG_FOLD_END(info, "dominated by " << *it);
                return;
            } else {
                ++it;
            }
        } else {
            ++it;
        }
    }
    insertions.push_back(insertion);
    LOG_FOLD_END(info, "ok");
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

Solution BranchingScheme::Node::convert(const Solution&) const
{
    std::vector<SolutionNodeId> bins;
    std::vector<Solution::Node> res(nodes().size());
    for (BinPos i = 0; i < bin_number(); ++i) {
        CutOrientation o = first_stage_orientation(i);
        Length w = instance().bin(i).width(o);
        Length h = instance().bin(i).height(o);
        SolutionNodeId id = res.size();
        bins.push_back(id);
        res.push_back({.id = id, .f = -1, .d = 0, .i = i,
                .l = 0, .r = w, .b = 0, .t = h, .children = {}, .j = -1});
    }
    for (SolutionNodeId id = 0; id < (SolutionNodeId)nodes().size(); ++id) {
        SolutionNodeId f = (node(id).f >= 0)? node(id).f: bins[(-node(id).f)-1];
        Depth d = res[f].d + 1;
        res[id].id = id;
        res[id].f  = f;
        res[id].d  = d;
        res[id].i  = res[f].i;
        if (d == 1 || d == 3) {
            res[id].r  = node(id).p;
            res[id].l  = (res[f].children.size() == 0)?
                res[f].l:
                res[res[f].children.back()].r;
            res[id].b  = res[f].b;
            res[id].t  = res[f].t;
        } else { // d == 2
            res[id].t  = node(id).p;
            res[id].b  = (res[f].children.size() == 0)?
                res[f].b:
                res[res[f].children.back()].t;
            res[id].l  = res[f].l;
            res[id].r  = res[f].r;
        }
        res[f].children.push_back(id);
    }

    for (SolutionNodeId f = 0; f < (SolutionNodeId)(nodes().size()+bins.size()); ++f) {
        SolutionNodeId nb = res[f].children.size();
        if (nb == 0)
            continue;
        SolutionNodeId c_last = res[f].children.back();
        if ((res[f].d == 0 || res[f].d == 2) && res[f].r != res[c_last].r) {
            if (res[f].r - res[c_last].r < branching_scheme().min_waste()) {
                res[c_last].r = res[f].r;
            } else {
                SolutionNodeId id = res.size();
                res.push_back({.id = id, .f = f,
                        .d = static_cast<Depth>(res[f].d+1), .i = res[f].i,
                        .l = res[c_last].r, .r = res[f].r,
                        .b = res[f].b,      .t = res[f].t,
                        .children = {}, .j = -1});
                res[f].children.push_back(id);
            }
        } else if ((res[f].d == 1 || res[f].d == 3) && res[f].t != res[c_last].t) {
            if (res[f].t - res[c_last].t < branching_scheme().min_waste()) {
                res[c_last].t = res[f].t;
            } else {
                SolutionNodeId id = res.size();
                res.push_back({.id = id, .f = f,
                        .d = static_cast<Depth>(res[f].d+1), .i = res[f].i,
                        .l = res[f].l,      .r = res[f].r,
                        .b = res[c_last].t, .t = res[f].t,
                        .children = {}, .j = -1});
                res[f].children.push_back(id);
            }
        }
    }

    for (SolutionNodeId id = 0; id < (SolutionNodeId)res.size(); ++id)
        res[id].j  = -1;

    for (Counter j_pos = 0; j_pos < item_number(); ++j_pos) {
        ItemTypeId     j  = items_[j_pos].j;
        SolutionNodeId id = items_[j_pos].node;
        Length         wj = instance().item(j).rect.w;
        Length         hj = instance().item(j).rect.h;
        if (res[id].children.size() > 0) { // Second item of the 4-cut
            res[res[id].children[1]].j = j; // Alone in the 3-cut
        } else if ((res[id].t - res[id].b == hj && res[id].r - res[id].l == wj)
                || (res[id].t - res[id].b == wj && res[id].r - res[id].l == hj)) {
            res[id].j = j;
            continue;
        } else {
            Length t = (res[id].r - res[id].l == wj)? hj: wj;
            BinPos i = res[id].i;
            CutOrientation o = first_stage_orientation(i);
            DefectId k = instance().rect_intersects_defect(
                    res[id].l, res[id].r, res[id].b, res[id].b + t, i, o);
            if (k == -1) { // First item of a 4-cut.
                SolutionNodeId c1 = res.size();
                res.push_back({.id = c1, .f = id,
                        .d = static_cast<Depth>(res[id].d + 1), .i = res[id].i,
                        .l = res[id].l, .r = res[id].r,
                        .b = res[id].b, .t = res[id].b + t,
                        .children = {}, .j = j});
                res[id].children.push_back(c1);
                SolutionNodeId c2 = res.size();
                res.push_back({.id = c2, .f = id,
                        .d = static_cast<Depth>(res[id].d+1), .i = res[id].i,
                        .l = res[id].l, .r = res[id].r,
                        .b = res[id].b + t, .t = res[id].t,
                        .children = {}, .j = -1});
                res[id].children.push_back(c2);
            } else {
                SolutionNodeId c1 = res.size();
                res.push_back({.id = c1, .f = id,
                        .d = static_cast<Depth>(res[id].d+1), .i = res[id].i,
                        .l = res[id].l, .r = res[id].r,
                        .b = res[id].b, .t = res[id].t - t,
                        .children = {}, .j = -1});
                res[id].children.push_back(c1);
                SolutionNodeId c2 = res.size();
                res.push_back({.id = c2, .f = id,
                        .d = static_cast<Depth>(res[id].d+1), .i = res[id].i,
                        .l = res[id].l, .r = res[id].r,
                        .b = res[id].t - t, .t = res[id].t,
                        .children = {}, .j = j});
                res[id].children.push_back(c2);
            }
        }
    }

    // Set j to -2 for intermediate nodes and to -3 for residual
    for (Solution::Node& n: res)
        if (n.j == -1 && n.children.size() != 0)
            n.j = -2;

    if (branching_scheme().cut_type_1() == CutType1::TwoStagedGuillotine) {
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

    if (res2.rbegin()->j == -1)
        res2.rbegin()->j = -3;

    for (Solution::Node& n: res2) {
        CutOrientation o = first_stage_orientation(n.i);
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
        return Solution(instance(), {});
    return Solution(instance(), res2);
}

bool BranchingScheme::Node::check(const std::vector<Solution::Node>& nodes) const
{
    std::vector<ItemPos> items(instance().item_number(), 0);

    for (const Solution::Node& node: nodes) {
        Length w = instance().bin(node.i).rect.w;
        Length h = instance().bin(node.i).rect.h;

        // TODO Check tree consistency

        // Check defect intersection
        if (!branching_scheme().cut_through_defects()) {
            for (Defect defect: instance().bin(node.i).defects) { // for each defect in the bin
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
            if (items[node.j] > instance().item(node.j).copies) {
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
            if (
                       node.r - node.l < branching_scheme().min_waste()
                    || node.t - node.b < branching_scheme().min_waste()) {
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
             if (node.r - node.l < branching_scheme().min1cut()) {
                 std::cerr << "\033[31m" << "ERROR, "
                     "Node " << node << " violates min1cut constraint"
                     << "\033[0m" << std::endl;
                 return false;
             }
             if (branching_scheme().max1cut() >= 0
                     && node.r - node.l > branching_scheme().max1cut()) {
                 std::cerr << "\033[31m" << "ERROR, "
                     "Node " << node << " violates max1cut constraint"
                     << "\033[0m" << std::endl;
                 return false;
             }
         } else if (node.d == 2 && node.j != -1) {
             if (node.t - node.b < branching_scheme().min2cut()) {
                 std::cerr << "\033[31m" << "ERROR, "
                     "Node " << node << " violates min2cut constraint"
                     << "\033[0m" << std::endl;
                 return false;
             }
             if (branching_scheme().max2cut() >= 0
                     && node.t - node.b > branching_scheme().max2cut()) {
                 std::cerr << "\033[31m" << "ERROR, "
                     "Node " << node << " violates max2cut constraint"
                     << "\033[0m" << std::endl;
                 return false;
             }
         }
    }

    return true;
}

