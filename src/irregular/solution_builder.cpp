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
        bool mirror,
        bool is_fixed)
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
    item.is_fixed = is_fixed;
    this->solution_.bins_[bin_pos].items.push_back(item);
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

    const Instance& instance = this->solution_.instance();
    for (const auto& json_bin: j["bins"]) {
        BinPos bin_pos = add_bin(json_bin["id"], json_bin["copies"]);
        // The fixed items are written as the other items: an item is fixed
        // if it is at the position of a fixed item of the bin type.
        const BinType& bin_type = instance.bin_type(json_bin["id"]);
        std::vector<bool> fixed_items_found(bin_type.fixed_items.size(), false);
        for (const auto& json_item: json_bin["items"]) {
            ItemTypeId item_type_id = json_item["id"];
            Point bl_corner = {json_item["x"], json_item["y"]};
            Angle angle = json_item["angle"];
            bool mirror = false;
            if (json_item.contains("mirror"))
                mirror = json_item["mirror"];
            bool is_fixed = false;
            for (ItemPos fixed_item_pos = 0;
                    fixed_item_pos < (ItemPos)bin_type.fixed_items.size();
                    ++fixed_item_pos) {
                const FixedItem& fixed_item = bin_type.fixed_items[fixed_item_pos];
                if (!fixed_items_found[fixed_item_pos]
                        && fixed_item.item_type_id == item_type_id
                        && equal(fixed_item.bl_corner.x, bl_corner.x)
                        && equal(fixed_item.bl_corner.y, bl_corner.y)
                        && equal(fixed_item.angle, angle)
                        && fixed_item.mirror == mirror) {
                    fixed_items_found[fixed_item_pos] = true;
                    is_fixed = true;
                    break;
                }
            }
            add_item(
                    bin_pos,
                    item_type_id,
                    bl_corner,
                    angle,
                    mirror,
                    is_fixed);
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
