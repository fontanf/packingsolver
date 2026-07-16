#include "boxstacks/solution_builder.hpp"

using namespace packingsolver;
using namespace packingsolver::boxstacks;

BinPos SolutionBuilder::add_bin(
        BinTypeId bin_type_id,
        BinPos copies)
{
    const Instance& instance = this->solution_.instance();
    const BinType& bin_type = instance.bin_type(bin_type_id);

    SolutionBin bin;
    bin.bin_type_id = bin_type_id;
    bin.copies = copies;
    this->solution_.bins_.push_back(bin);

    return this->solution_.bins_.size() - 1;
}

StackId SolutionBuilder::add_stack(
        BinPos bin_pos,
        Length x_start,
        Length x_end,
        Length y_start,
        Length y_end)
{
    if (bin_pos >= this->solution_.number_of_different_bins()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "bin_pos: " + std::to_string(bin_pos) + "; "
                "number_of_different_bins(): " + std::to_string(this->solution_.number_of_different_bins()) + ".");
    }

    SolutionStack stack;
    stack.x_start = x_start;
    stack.x_end = x_end;
    stack.y_start = y_start;
    stack.y_end = y_end;
    this->solution_.bins_[bin_pos].stacks.push_back(stack);

    return this->solution_.bins_[bin_pos].stacks.size() - 1;
}

void SolutionBuilder::add_item(
        BinPos bin_pos,
        StackId stack_id,
        ItemTypeId item_type_id,
        Rotation rotation)
{
    const Instance& instance = this->solution_.instance();

    if (bin_pos >= this->solution_.number_of_different_bins()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "bin_pos: " + std::to_string(bin_pos) + "; "
                "number_of_different_bins(): " + std::to_string(this->solution_.number_of_different_bins()) + ".");
    }
    if (item_type_id < 0 || item_type_id >= instance.number_of_item_types()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "item_type_id: " + std::to_string(item_type_id)
                + " / " + std::to_string(instance.number_of_item_types()) + ".");
    }

    SolutionStack& stack = this->solution_.bins_[bin_pos].stacks[stack_id];
    const ItemType& item_type = instance.item_type(item_type_id);
    Length xj = item_type.x(rotation);
    Length yj = item_type.y(rotation);

    if (!item_type.can_rotate(rotation)) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "forbidden rotation; "
                "item_type_id: " + std::to_string(item_type_id) + "; "
                "rotation: " + to_string(rotation) + "; "
                "xj: " + std::to_string(xj) + "; "
                "yj: " + std::to_string(yj) + "; "
                "stack.x_start: " + std::to_string(stack.x_start) + "; "
                "stack.x_end: " + std::to_string(stack.x_end) + "; "
                "stack.y_start: " + std::to_string(stack.y_start) + "; "
                "stack.y_end: " + std::to_string(stack.y_end) + ".");
    }
    if (xj != stack.x_end - stack.x_start) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "item_type_id: " + std::to_string(item_type_id) + "; "
                "rotation: " + to_string(rotation) + "; "
                "xj: " + std::to_string(xj) + "; "
                "yj: " + std::to_string(yj) + "; "
                "stack.x_start: " + std::to_string(stack.x_start) + "; "
                "stack.x_end: " + std::to_string(stack.x_end) + "; "
                "stack.y_start: " + std::to_string(stack.y_start) + "; "
                "stack.y_end: " + std::to_string(stack.y_end) + ".");
    }
    if (yj != stack.y_end - stack.y_start) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "item_type_id: " + std::to_string(item_type_id) + "; "
                "rotation: " + to_string(rotation) + "; "
                "xj: " + std::to_string(xj) + "; "
                "yj: " + std::to_string(yj) + "; "
                "stack.x_start: " + std::to_string(stack.x_start) + "; "
                "stack.x_end: " + std::to_string(stack.x_end) + "; "
                "stack.y_start: " + std::to_string(stack.y_start) + "; "
                "stack.y_end: " + std::to_string(stack.y_end) + ".");
    }

    SolutionItem item;
    item.item_type_id = item_type_id;
    item.rotation = rotation;
    stack.items.push_back(item);
}

////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////// Build /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

Solution SolutionBuilder::build()
{
    // Compute indicators.
    for (BinPos bin_pos = 0;
            bin_pos < solution_.number_of_different_bins();
            ++bin_pos) {
        solution_.update_indicators(bin_pos);
    }

    return std::move(solution_);
}
