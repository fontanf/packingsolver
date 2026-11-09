#include "irregular/solution_builder.hpp"

#include "nlohmann/json.hpp"

#include <fstream>

using namespace packingsolver;
using namespace packingsolver::irregular;

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

void SolutionBuilder::add_item(
        BinPos bin_pos,
        ItemTypeId item_type_id,
        Point bl_corner,
        Angle angle,
        bool mirror)
{
    const Instance& instance = this->solution_.instance();

    if (item_type_id < 0
            || item_type_id >= instance.number_of_item_types()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'item_type_id'; "
                "item_type_id: " + std::to_string(item_type_id) + ".");
    }

    const ItemType& item_type = instance.item_type(item_type_id);

    if (!item_type.is_rotation_allowed(angle, mirror)) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid (angle, mirror) combination; "
                "angle: " + std::to_string(angle) + "; "
                "mirror: " + std::to_string(mirror) + "; "
                "item_type_id: " + std::to_string(item_type_id) + ".");
    }

    SolutionItem item;
    item.item_type_id = item_type_id;
    item.bl_corner = bl_corner;
    item.angle = angle;
    item.mirror = mirror;
    this->solution_.bins_[bin_pos].items.push_back(item);
}

void SolutionBuilder::add_fixed_item(
        BinPos bin_pos,
        ItemPos fixed_item_pos)
{
    const Instance& instance = this->solution_.instance();
    SolutionBin& bin = this->solution_.bins_[bin_pos];
    const BinType& bin_type = instance.bin_type(bin.bin_type_id);

    if (fixed_item_pos < 0
            || fixed_item_pos >= (ItemPos)bin_type.fixed_items.size()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'fixed_item_pos'; "
                "fixed_item_pos: " + std::to_string(fixed_item_pos) + "; "
                "bin_type_id: " + std::to_string(bin.bin_type_id) + "; "
                "number of fixed items: " + std::to_string(bin_type.fixed_items.size()) + ".");
    }
    for (const SolutionItem& item: bin.items) {
        if (item.fixed_item_pos == fixed_item_pos) {
            throw std::invalid_argument(
                    FUNC_SIGNATURE + ": "
                    "fixed item already added to the bin; "
                    "bin_pos: " + std::to_string(bin_pos) + "; "
                    "fixed_item_pos: " + std::to_string(fixed_item_pos) + ".");
        }
    }

    const FixedItem& fixed_item = bin_type.fixed_items[fixed_item_pos];
    SolutionItem item;
    item.item_type_id = fixed_item.item_type_id;
    item.bl_corner = fixed_item.bl_corner;
    item.angle = fixed_item.angle;
    item.mirror = fixed_item.mirror;
    item.fixed_item_pos = fixed_item_pos;
    bin.items.push_back(item);
}

void SolutionBuilder::read(
        const std::string& certificate_path)
{
    std::ifstream file(certificate_path);
    if (!file.good()) {
        throw std::runtime_error(
                FUNC_SIGNATURE + ": "
                "unable to open file \"" + certificate_path + "\".");
    }

    nlohmann::json j;
    file >> j;

    for (const auto& json_bin: j["bins"]) {
        BinPos bin_pos = add_bin(json_bin["id"], json_bin["copies"]);
        for (const auto& json_item: json_bin["items"]) {
            if (json_item.contains("fixed_item_pos")) {
                add_fixed_item(bin_pos, json_item["fixed_item_pos"]);
                continue;
            }
            bool mirror = false;
            if (json_item.contains("mirror"))
                mirror = json_item["mirror"];
            add_item(
                    bin_pos,
                    json_item["id"],
                    {json_item["x"], json_item["y"]},
                    json_item["angle"],
                    mirror);
        }
    }
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
