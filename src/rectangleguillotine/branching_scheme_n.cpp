#include "packingsolver/rectangleguillotine/branching_scheme_n.hpp"

#include "packingsolver/rectangleguillotine/solution_builder.hpp"

#include "optimizationtools//containers//indexed_set.hpp"

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

BranchingSchemeN::BranchingSchemeN(
        const Instance& instance,
        const Parameters& parameters):
    instance_(instance),
    parameters_(parameters)
{
}

const std::shared_ptr<BranchingSchemeN::Node> BranchingSchemeN::root() const
{
    //std::cout << std::endl;
    //std::cout << "root" << std::endl;
    //std::cout << std::endl;
    subplate_pool_.clear();
    Node root;
    root.item_number_of_copies = std::vector<ItemPos>(instance().number_of_item_types(), 0);
    root.id = node_id_++;
    return std::make_shared<Node>(root);
}

std::vector<std::shared_ptr<BranchingSchemeN::Node>> BranchingSchemeN::children(
        const std::shared_ptr<Node>& parent) const
{
    //std::cout << "children node " << parent->id
    //    << " width " << ((parent->subplate == nullptr)? 0: parent->subplate->width)
    //    << " height " << ((parent->subplate == nullptr)? 0: parent->subplate->height)
    //    << " # items " << parent->number_of_items
    //    << " profit " << parent->item_profit
    //    << " item_area " << parent->item_area
    //    << " area " << area(*parent)
    //    << " waste_percentage " << waste_percentage(*parent)
    //    << std::endl;
    //for (ItemTypeId item_type_id = 0;
    //        item_type_id < instance().number_of_item_types();
    //        ++item_type_id) {
    //    if (parent->item_number_of_copies[item_type_id] > 0)
    //        std::cout << "  " << item_type_id << " " << parent->item_number_of_copies[item_type_id];
    //}
    //std::cout << std::endl;

    std::vector<std::shared_ptr<BranchingSchemeN::Node>> children;
    const BinType& bin_type = instance().bin_type(0);

    // Root node.
    if (parent->parent == nullptr) {
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance().number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = instance().item_type(item_type_id);

            Subplate subplate;
            subplate.item_type_id = item_type_id;
            subplate.width = item_type.rect.w;
            subplate.height = item_type.rect.h;
            Node child;
            child.parent = parent;
            child.subplate = std::make_shared<Subplate>(subplate);
            child.number_of_items = 1;
            child.item_area = item_type.area();
            child.item_profit = item_type.profit;
            child.item_number_of_copies = parent->item_number_of_copies;
            child.item_number_of_copies[item_type_id]++;
            bool ok = add_subplate_to_pool(
                    child.subplate,
                    child.number_of_items,
                    child.item_number_of_copies);
            if (item_type.rect.w > bin_type.rect.w)
                ok = false;
            if (item_type.rect.h > bin_type.rect.h)
                ok = false;
            if (ok) {
                child.id = node_id_++;
                children.push_back(std::make_shared<Node>(child));
            }

            if (!item_type.oriented) {
                Subplate subplate;
                subplate.item_type_id = item_type_id;
                subplate.width = item_type.rect.h;
                subplate.height = item_type.rect.w;
                Node child;
                child.parent = parent;
                child.subplate = std::make_shared<Subplate>(subplate);
                child.number_of_items = 1;
                child.item_area = item_type.area();
                child.item_profit = item_type.profit;
                child.item_number_of_copies = parent->item_number_of_copies;
                child.item_number_of_copies[item_type_id]++;
                bool ok = add_subplate_to_pool(
                        child.subplate,
                        child.number_of_items,
                        child.item_number_of_copies);
                if (item_type.rect.w > bin_type.rect.w)
                    ok = false;
                if (item_type.rect.h > bin_type.rect.h)
                    ok = false;
                if (ok) {
                    child.id = node_id_++;
                    children.push_back(std::make_shared<Node>(child));
                }
            }
        }
    }

    optimizationtools::IndexedSet modified(instance().number_of_item_types());
    std::vector<const Subplate*> stack;
    std::vector<ItemPos> item_number_of_copies = parent->item_number_of_copies;
    for (ItemPos number_of_items = 1;
            number_of_items <= parent->number_of_items;
            ++number_of_items) {
        if (parent->number_of_items + number_of_items > instance().number_of_items())
            continue;
        if (subplate_pool_.size() <= number_of_items)
            continue;
        //std::cout << "number_of_items " << number_of_items << std::endl;

        for (auto it: subplate_pool_[number_of_items]) {

            //std::cout << "it.first" << std::endl;
            //for (ItemTypeId item_type_id = 0;
            //        item_type_id < instance().number_of_item_types();
            //        ++item_type_id) {
            //    if (it.first[item_type_id] > 0) {
            //        std::cout << "item_type " << item_type_id << " copies " << it.first[item_type_id] << std::endl;
            //    }
            //}

            // Check if subplates are compatible.
            bool ok = true;
            modified.clear();
            stack = {it.second[0].get()};
            Area item_area = 0;
            Profit item_profit = 0;
            while (!stack.empty()) {
                const Subplate* subplate = stack.back();
                stack.pop_back();
                if (subplate->item_type_id != -1) {
                    const ItemType& item_type = instance().item_type(subplate->item_type_id);
                    modified.add(subplate->item_type_id);
                    item_number_of_copies[subplate->item_type_id]++;
                    item_area += item_type.area();
                    item_profit += item_type.profit;
                    //std::cout << "item " << subplate->item_type_id << std::endl;
                    if (item_number_of_copies[subplate->item_type_id]
                            > item_type.copies) {
                        ok = false;
                        break;
                    }
                } else {
                    stack.push_back(subplate->subplate_1.get());
                    stack.push_back(subplate->subplate_2.get());
                }
            }

            if (ok) {
                for (const std::shared_ptr<Subplate>& subplate: it.second) {
                    for (CutOrientation cut_orientation: {CutOrientation::Vertical, CutOrientation::Horinzontal}) {
                        Subplate subplate_new;
                        subplate_new.subplate_1 = parent->subplate;
                        subplate_new.subplate_2 = subplate;
                        subplate_new.cut_orientation = cut_orientation;
                        if (cut_orientation == CutOrientation::Vertical) {
                            subplate_new.height = (std::max)(parent->subplate->height, subplate->height);
                            subplate_new.width = parent->subplate->width + subplate->width;
                            if (subplate_new.width > bin_type.rect.w)
                                continue;
                        } else {
                            subplate_new.width = (std::max)(parent->subplate->width, subplate->width);
                            subplate_new.height = parent->subplate->height + subplate->height;
                            if (subplate_new.height > bin_type.rect.h)
                                continue;
                        }
                        Counter v1_ver = parent->subplate->number_of_stages;
                        if (parent->subplate->item_type_id == -1
                                && parent->subplate->cut_orientation != cut_orientation)
                            v1_ver++;
                        Counter v2_ver = subplate->number_of_stages;
                        if (subplate->item_type_id == -1
                                && subplate->cut_orientation != cut_orientation)
                            v2_ver++;
                        subplate_new.number_of_stages = std::max(v1_ver, v2_ver);
                        if (subplate_new.number_of_stages > instance().number_of_stages())
                            continue;
                        // Add child.
                        Node child;
                        child.parent = parent;
                        child.subplate = std::make_shared<Subplate>(subplate_new);
                        child.number_of_items = parent->number_of_items + number_of_items;
                        child.item_area = parent->item_area + item_area;
                        child.item_profit = parent->item_profit + item_profit;
                        child.item_number_of_copies = item_number_of_copies;
                        bool ok = add_subplate_to_pool(
                                child.subplate,
                                child.number_of_items,
                                child.item_number_of_copies);
                        if (ok) {
                            child.id = node_id_++;
                            children.push_back(std::make_shared<Node>(child));
                        }
                    }
                }
            }
            for (ItemTypeId item_type_id: modified)
                item_number_of_copies[item_type_id] = parent->item_number_of_copies[item_type_id];
        }
    }

    //std::cout << "children end " << children.size() << std::endl;
    return children;
}

bool BranchingSchemeN::bound(
        const std::shared_ptr<Node>& node_1,
        const std::shared_ptr<Node>& node_2) const
{
    switch (instance().objective()) {
    case Objective::OpenDimensionX: {
        if (!leaf(node_2))
            return false;
        return node_1->subplate->height >= node_2->subplate->height;
    } case Objective::OpenDimensionY: {
        if (!leaf(node_2))
            return false;
        return node_1->subplate->width >= node_2->subplate->width;
    } case Objective::Knapsack: {
        //return ubkp(*node_1) <= node_2->item_profit;
        // TODO
        return false;
    } default: {
        std::stringstream ss;
        ss << "Branching scheme 'rectangleguillotine::BranchingScheme'"
            << "does not support objective '" << instance().objective() << "'.";
        throw std::logic_error(ss.str());
        return false;
    }
    }
}

bool BranchingSchemeN::better(
        const std::shared_ptr<Node>& node_1,
        const std::shared_ptr<Node>& node_2) const
{
    switch (instance().objective()) {
    case Objective::OpenDimensionX: {
        if (!leaf(node_1))
            return false;
        if (!leaf(node_2))
            return true;
        return node_2->subplate->width > node_1->subplate->width;
    } case Objective::OpenDimensionY: {
        if (!leaf(node_1))
            return false;
        if (!leaf(node_2))
            return true;
        return node_2->subplate->height > node_1->subplate->height;
    } case Objective::Knapsack: {
        return node_2->item_profit < node_1->item_profit;
    } default: {
        std::stringstream ss;
        ss << "Branching scheme 'rectangleguillotine::BranchingSchemeN'"
            << "does not support objective \"" << instance().objective() << "\".";
        throw std::logic_error(ss.str());
        return false;
    }
    }
    return false;
}

namespace
{

struct BranchingSchemeSolutionNode
{
    /** Subplate. */
    const BranchingSchemeN::Subplate* subplate = nullptr;

    /** Parent node. */
    std::shared_ptr<BranchingSchemeSolutionNode> parent = nullptr;

    /** Children. */
    std::vector<std::shared_ptr<BranchingSchemeSolutionNode>> children;

    Depth depth = 0;

};

}

Solution BranchingSchemeN::to_solution(
        const std::shared_ptr<Node>& node) const
{
    //std::cout << "to_solution" << std::endl;
    //std::cout << "node " << node->id << std::endl;
    //std::cout << "# items " << node->number_of_items << std::endl;
    //std::cout << "profit " << node->item_profit << std::endl;
    //format(*node->subplate, std::cout);

    BranchingSchemeSolutionNode root;
    root.subplate = node->subplate.get();
    auto proot = std::make_shared<BranchingSchemeSolutionNode>(root);
    {
        std::vector<std::shared_ptr<BranchingSchemeSolutionNode>> stack = {proot};
        while (!stack.empty()) {
            std::shared_ptr<BranchingSchemeSolutionNode> n = stack.back();
            stack.pop_back();
            if (n->subplate->item_type_id != -1)
                continue;
            //std::cout << stack.size() << std::endl;
            //format(*n->subplate, std::cout);

            // Find all children.
            std::vector<const Subplate*> stack_2 = {n->subplate};
            while (!stack_2.empty()) {
                const Subplate* subplate = stack_2.back();
                stack_2.pop_back();
                if (subplate->item_type_id != -1
                        || subplate->cut_orientation != n->subplate->cut_orientation) {
                    BranchingSchemeSolutionNode n2;
                    n2.parent = n;
                    n2.subplate = subplate;
                    n2.depth = n->depth + 1;
                    auto pn2 = std::make_shared<BranchingSchemeSolutionNode>(n2);
                    n->children.push_back(pn2);
                } else {
                    stack_2.push_back(subplate->subplate_2.get());
                    stack_2.push_back(subplate->subplate_1.get());
                }
            }

            for (auto it = n->children.rbegin(); it != n->children.rend(); ++it)
                stack.push_back(*it);
        }
    }

    SolutionBuilder solution_builder(instance());
    BinTypeId bin_type_id = 0;
    solution_builder.add_bin(bin_type_id, 1, node->subplate->cut_orientation);
    const BinType& bin_type = instance().bin_type(0);

    if (node->subplate->item_type_id != -1) {
        solution_builder.set_last_node_item(node->subplate->item_type_id);
        Solution solution = solution_builder.build();
        //solution.format(std::cout, 2);
        //std::cout << "to_solution end 0" << std::endl;
        return solution;
    }

    Subplate subplate = *node->subplate;
    if (subplate.cut_orientation == CutOrientation::Vertical) {
        subplate.height = bin_type.rect.h;
    } else {
        subplate.width = bin_type.rect.w;
    }

    std::vector<std::shared_ptr<BranchingSchemeSolutionNode>> stack;
    for (auto it = proot->children.rbegin(); it != proot->children.rend(); ++it)
        stack.push_back(*it);
    while (!stack.empty()) {
        std::shared_ptr<BranchingSchemeSolutionNode> n = stack.back();
        stack.pop_back();
        //std::cout << "children " << n->children.size() << std::endl;
        //format(*n->subplate, std::cout);

        Length size = (n->parent->subplate->cut_orientation == CutOrientation::Vertical)?
            n->subplate->width:
            n->subplate->height;
        solution_builder.add_node(
                n->depth,
                size,
                true);

        ItemTypeId item_type_id = n->subplate->item_type_id;
        if (item_type_id != -1) {
            const ItemType& item_type = instance().item_type(item_type_id);
            // Add an extra subplate if necessary.
            Length size = -1;
            if (n->parent->subplate->cut_orientation == CutOrientation::Vertical) {
                Length size = (item_type.rect.w == n->subplate->width)?
                    item_type.rect.h:
                    item_type.rect.w;
                if (size != n->parent->subplate->height) {
                    solution_builder.add_node(
                            n->depth + 1,
                            size,
                            true);
                }
            } else {
                Length size = (item_type.rect.h == n->subplate->height)?
                    item_type.rect.w:
                    item_type.rect.h;
                if (size != n->parent->subplate->width) {
                    solution_builder.add_node(
                            n->depth + 1,
                            size,
                            true);
                }
            }
            solution_builder.set_last_node_item(item_type_id);
        }

        for (auto it = n->children.rbegin(); it != n->children.rend(); ++it)
            stack.push_back(*it);
    }

    Solution solution = solution_builder.build();
    //solution.format(std::cout, 2);
    //std::cout << "to_solution end" << std::endl;
    if (solution.profit() != node->item_profit) {
        throw std::logic_error(
                "rectangleguillotine::BranchingSchemeN::to_solution");
    }
    return solution;
}


bool BranchingSchemeN::add_subplate_to_pool(
        const std::shared_ptr<Subplate>& subplate,
        ItemPos number_of_items,
        const std::vector<ItemPos>& item_number_of_copies) const
{
    while (subplate_pool_.size() <= number_of_items) {
        SubpalteMap subplate_map(0, subplate_pool_hasher_, subplate_pool_hasher_);
        subplate_pool_.push_back(subplate_map);
    }
    auto& list = subplate_pool_[number_of_items][item_number_of_copies];

    // Check if node is dominated.
    for (const std::shared_ptr<Subplate>& sp: list)
        if (dominates(sp, subplate))
            return false;

    // Remove dominated nodes from history.
    for (auto it = list.begin(); it != list.end();) {
        if (dominates(subplate, *it)) {
            *it = list.back();
            list.pop_back();
        } else {
            ++it;
        }
    }

    // Add node to history.
    //std::cout << "add subplate" << std::endl;
    list.push_back(subplate);
    return true;
}

std::ostream& BranchingSchemeN::format(
        const Subplate& subplate,
        std::ostream& os) const
{
    std::vector<const Subplate*> stack = {nullptr, &subplate};
    Depth depth = 0;
    while (!stack.empty()) {
        const Subplate* subplate = stack.back();
        stack.pop_back();
        if (subplate == nullptr) {
            depth--;
            continue;
        } else {
            depth++;
        }
        os << std::string(depth, ' ')
            << subplate->width << " "
            << subplate->height << " "
            << subplate->item_type_id << std::endl;
        if (subplate->item_type_id == -1) {
            stack.push_back(nullptr);
            stack.push_back(subplate->subplate_2.get());
            stack.push_back(nullptr);
            stack.push_back(subplate->subplate_1.get());
        }
    }
    return os;
}


