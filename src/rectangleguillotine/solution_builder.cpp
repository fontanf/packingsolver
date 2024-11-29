#include "rectangleguillotine/solution_builder.hpp"

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

void SolutionBuilder::add_bin(
        BinTypeId bin_type_id,
        BinPos copies,
        CutOrientation first_cut_orientation)
{
    //std::cout << "add_bin bin_type_id " << bin_type_id << " copies " << copies << " first_cut_orientation " << first_cut_orientation << std::endl;
    const BinType& bin_type = solution_.instance().bin_type(bin_type_id);
    SolutionBin bin;
    bin.bin_type_id = bin_type_id;
    bin.copies = copies;
    bin.first_cut_orientation = first_cut_orientation;

    SolutionNodeId root_id = 0;
    SolutionNode root;
    root.f = -1;
    root.d = 0;
    root.l = 0;
    root.r = bin_type.rect.w;
    root.b = 0;
    root.t = bin_type.rect.h;
    root.item_type_id = -1;
    bin.nodes.push_back(root);

    // Trims.
    if (bin_type.left_trim > 0
            || bin_type.right_trim > 0
            || bin_type.bottom_trim > 0
            || bin_type.top_trim > 0) {

        if (bin_type.left_trim != 0 && bin_type.bottom_trim != 0) {
            SolutionNode node;
            node.d = -1;
            node.f = 0;
            node.l = 0;
            node.r = bin_type.left_trim;
            node.b = 0;
            node.t = bin_type.bottom_trim;
            node.item_type_id = -1;
            root.children.push_back(bin.nodes.size());
            bin.nodes.push_back(node);
        }

        if (bin_type.left_trim != 0 && bin_type.top_trim != 0) {
            SolutionNode node;
            node.d = -1;
            node.f = 0;
            node.l = 0;
            node.r = bin_type.left_trim;
            node.b = bin_type.rect.h - bin_type.top_trim;
            node.t = bin_type.rect.h;
            node.item_type_id = -1;
            root.children.push_back(bin.nodes.size());
            bin.nodes.push_back(node);
        }

        if (bin_type.right_trim != 0 && bin_type.bottom_trim != 0) {
            SolutionNode node;
            node.d = -1;
            node.f = 0;
            node.l = bin_type.rect.w - bin_type.right_trim;
            node.r = bin_type.rect.w;
            node.b = 0;
            node.t = bin_type.bottom_trim;
            node.item_type_id = -1;
            root.children.push_back(bin.nodes.size());
            bin.nodes.push_back(node);
        }

        if (bin_type.right_trim != 0 && bin_type.top_trim != 0) {
            SolutionNode node;
            node.d = -1;
            node.f = 0;
            node.l = bin_type.rect.w - bin_type.right_trim;
            node.r = bin_type.rect.w;
            node.b = bin_type.rect.h - bin_type.top_trim;
            node.t = bin_type.rect.h;
            node.item_type_id = -1;
            root.children.push_back(bin.nodes.size());
            bin.nodes.push_back(node);
        }

        if (bin_type.left_trim != 0) {
            SolutionNode node;
            node.d = -1;
            node.f = 0;
            node.l = 0;
            node.r = bin_type.left_trim;
            node.b = bin_type.bottom_trim;
            node.t = bin_type.rect.h - bin_type.top_trim;
            node.item_type_id = -1;
            root.children.push_back(bin.nodes.size());
            bin.nodes.push_back(node);
        }

        if (bin_type.bottom_trim != 0) {
            SolutionNode node;
            node.d = -1;
            node.f = 0;
            node.l = bin_type.left_trim;
            node.r = bin_type.rect.w - bin_type.right_trim;
            node.b = 0;
            node.t = bin_type.bottom_trim;
            node.item_type_id = -1;
            root.children.push_back(bin.nodes.size());
            bin.nodes.push_back(node);
        }

        if (bin_type.right_trim != 0) {
            SolutionNode node;
            node.d = -1;
            node.f = 0;
            node.l = bin_type.rect.w - bin_type.right_trim;
            node.r = bin_type.rect.w;
            node.b = bin_type.bottom_trim;
            node.t = bin_type.rect.h - bin_type.top_trim;
            node.item_type_id = -1;
            root.children.push_back(bin.nodes.size());
            bin.nodes.push_back(node);
        }

        if (bin_type.top_trim != 0) {
            SolutionNode node;
            node.d = -1;
            node.f = 0;
            node.l = bin_type.left_trim;
            node.r = bin_type.rect.w - bin_type.right_trim;
            node.b = bin_type.rect.h - bin_type.top_trim;
            node.t = bin_type.rect.h;
            node.item_type_id = -1;
            root.children.push_back(bin.nodes.size());
            bin.nodes.push_back(node);
        }

        SolutionNode node;
        node.d = 0;
        node.f = 0;
        node.l = bin_type.left_trim;
        node.r = bin_type.rect.w - bin_type.right_trim;
        node.b = bin_type.bottom_trim;
        node.t = bin_type.rect.h - bin_type.top_trim;
        node.item_type_id = -1;
        root.children.push_back(bin.nodes.size());
        bin.nodes.push_back(node);
    }

    solution_.bins_.push_back(bin);
}

void SolutionBuilder::set_last_node_item(
        ItemTypeId item_type_id)
{
    if (solution_.bins_.empty()) {
        throw std::logic_error(
                "rectangleguillotine::SolutionBuilder::set_last_node_item: "
                "at least one bin must have been added to the solution.");
    }

    const ItemType& item_type = solution_.instance().item_type(item_type_id);
    SolutionBin& bin = solution_.bins_.back();
    SolutionNode& node = bin.nodes.back();

    // Check node dimensions.
    bool ok = (
            (node.r - node.l == item_type.rect.w && node.t - node.b == item_type.rect.h)
            || (node.r - node.l == item_type.rect.h && node.t - node.b == item_type.rect.w));
    if (!ok) {
        throw std::logic_error(
                "rectangleguillotine::SolutionBuilder::set_last_node_item: "
                "wrong item dimensions.");
    }

    node.item_type_id = item_type_id;
}

void SolutionBuilder::add_node(
        Depth depth,
        Length cut_position)
{
    Length cut_thickness = solution_.instance().parameters().cut_thickness;
    //std::cout << "add_node depth " << depth << " cut_position " << cut_position << std::endl;
    if (solution_.bins_.empty()) {
        throw std::logic_error(
                "rectangleguillotine::SolutionBuilder::add_node: "
                "at least one bin must have been added to the solution.");
    }
    if (depth < 1) {
        throw std::logic_error(
                "rectangleguillotine::SolutionBuilder::add_node: "
                "wrong depth: " + std::to_string(depth) + ".");
    }
    SolutionBin& bin = solution_.bins_.back();
    //std::cout << "bin.nodes.size() " << bin.nodes.size() << std::endl;

    SolutionNodeId parent_id = bin.nodes.size() - 1;
    //std::cout << "parent_id " << parent_id << " depth " << bin.nodes[parent_id].d << std::endl;
    if (depth > bin.nodes[parent_id].d + 1) {
        throw std::logic_error(
                "rectangleguillotine::SolutionBuilder::add_node: "
                "wrong depth: " + std::to_string(depth)
                + "; parent.depth: " + std::to_string(bin.nodes[parent_id].d)
                + ".");
    }

    while (bin.nodes[parent_id].d != depth - 1) {
        SolutionNode& parent = bin.nodes[parent_id];
        // Add a final child to the last node if necessary.
        if (!parent.children.empty()) {
            const SolutionNode& brother = bin.nodes[parent.children.back()];
            if (parent.r != brother.r) {
                SolutionNode child;
                child.f = parent_id;
                child.d = parent.d + 1;
                child.l = brother.r;
                child.r = parent.r;
                child.b = parent.b;
                child.t = parent.t;
                child.item_type_id = -1;
                parent.children.push_back(bin.nodes.size());
                bin.nodes.push_back(child);
                //std::cout << "new node " << child << std::endl;
            } else if (parent.t != brother.t) {
                SolutionNode child;
                child.f = parent_id;
                child.d = parent.d + 1;
                child.l = parent.l;
                child.r = parent.r;
                child.b = brother.t;
                child.t = parent.t;
                child.item_type_id = -1;
                parent.children.push_back(bin.nodes.size());
                bin.nodes.push_back(child);
                //std::cout << "new node " << child << std::endl;
            }
        }
        parent_id = bin.nodes[parent_id].f;
        //std::cout << "parent_id " << parent_id << " depth " << bin.nodes[parent_id].d << std::endl;
    }

    SolutionNode& parent = bin.nodes[parent_id];
    if (parent.item_type_id >= 0) {
        throw std::logic_error(
                "rectangleguillotine::SolutionBuilder::add_node: "
                "cannot add a child to a node with an item.");
    }
    parent.item_type_id = -2;

    SolutionNode child;
    child.f = parent_id;
    child.d = parent.d + 1;
    if ((bin.first_cut_orientation == CutOrientation::Vertical
                && depth % 2 == 1)
            || (bin.first_cut_orientation == CutOrientation::Horizontal
                && depth % 2 == 0)) {
        child.l = (parent.children.empty())? parent.l: bin.nodes[parent.children.back()].r + cut_thickness;
        child.r = cut_position;
        child.b = parent.b;
        child.t = parent.t;
        if (child.r <= child.l) {
            throw std::logic_error(
                    "rectangleguillotine::SolutionBuilder::add_last_node_child: "
                    "'cut_position' is too small");
        }
        if (child.r > parent.r) {
            throw std::logic_error(
                    "rectangleguillotine::SolutionBuilder::add_last_node_child: "
                    "'cut_position' is too large.");
        }
    } else {
        child.l = parent.l;
        child.r = parent.r;
        child.b = (parent.children.empty())? parent.b: bin.nodes[parent.children.back()].t + cut_thickness;
        child.t = cut_position;
        if (child.t <= child.b) {
            throw std::logic_error(
                    "rectangleguillotine::SolutionBuilder::add_last_node_child: "
                    "'cut_position' is too small.");
        }
        if (child.t > parent.t) {
            throw std::logic_error(
                    "rectangleguillotine::SolutionBuilder::add_last_node_child: "
                    "'cut_position' is too large.");
        }
    }
    child.item_type_id = -1;
    parent.children.push_back(bin.nodes.size());
    bin.nodes.push_back(child);
}

Solution SolutionBuilder::build()
{
    // Finish bins.
    for (BinPos bin_pos = 0;
            bin_pos < solution_.number_of_different_bins();
            ++bin_pos) {
        SolutionBin& bin = solution_.bins_[bin_pos];
        SolutionNodeId parent_id = bin.nodes.size() - 1;
        while (parent_id != -1) {
            SolutionNode& parent = bin.nodes[parent_id];
            // Add a final child to the last node if necessary.
            if (!parent.children.empty()) {
                const SolutionNode& brother = bin.nodes[parent.children.back()];
                if (parent.r != brother.r) {
                    SolutionNode child;
                    child.f = parent_id;
                    child.d = parent.d + 1;
                    child.l = brother.r;
                    child.r = parent.r;
                    child.b = parent.b;
                    child.t = parent.t;
                    child.item_type_id = -1;
                    parent.children.push_back(bin.nodes.size());
                    bin.nodes.push_back(child);
                    //std::cout << "new node " << child << std::endl;
                } else if (parent.t != brother.t) {
                    SolutionNode child;
                    child.f = parent_id;
                    child.d = parent.d + 1;
                    child.l = parent.l;
                    child.r = parent.r;
                    child.b = brother.t;
                    child.t = parent.t;
                    child.item_type_id = -1;
                    parent.children.push_back(bin.nodes.size());
                    bin.nodes.push_back(child);
                    //std::cout << "new node " << child << std::endl;
                }
            }
            parent_id = bin.nodes[parent_id].f;
            //std::cout << "parent_id " << parent_id << " depth " << bin.nodes[parent_id].d << std::endl;
        }
    }
    // Set residual.
    if (solution_.number_of_different_bins() > 0) {
        SolutionNode& node = solution_.bins_.back().nodes.back();
        if (node.item_type_id == -1 && node.d == 1)
            node.item_type_id = -3;
    }

    // Compute indicators.
    for (BinPos bin_pos = 0;
            bin_pos < solution_.number_of_different_bins();
            ++bin_pos) {
        solution_.update_indicators(bin_pos);
    }

    // Check the size of nodes containing items.
    // TODO

    // Check min waste constraint.
    // TODO

    // Check consecutive cuts constraints.
    // TODO

    // Check stack order.
    // TODO

    return std::move(solution_);
}
