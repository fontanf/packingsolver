#include "rectangle/solution_builder.hpp"

#include "optimizationtools/utils/utils.hpp"

using namespace packingsolver;
using namespace packingsolver::rectangle;

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
        bool rotate)
{
    const Instance& instance = this->solution_.instance();

    if (bin_pos >= this->solution_.number_of_different_bins()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'bin_pos'; "
                "bin_pos: " + std::to_string(bin_pos) + "; "
                "number_of_different_bins(): " + std::to_string(this->solution_.number_of_different_bins()) + ".");
    }

    if (item_type_id < 0
            || item_type_id >= instance.number_of_item_types()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'item_type_id'; "
                "item_type_id: " + std::to_string(item_type_id) + ".");
    }

    SolutionBin& bin = this->solution_.bins_[bin_pos];

    const BinType& bin_type = instance.bin_type(bin.bin_type_id);
    const ItemType& item_type = instance.item_type(item_type_id);

    if (rotate && item_type.oriented) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "item type " + std::to_string(item_type_id) + " cannot be rotated.");
    }

    SolutionItem item;
    item.item_type_id = item_type_id;
    item.bl_corner = bl_corner;
    item.rotate = rotate;
    bin.items.push_back(item);
}

void SolutionBuilder::read(
        const std::string& certificate_path)
{
    const Instance& instance = this->solution_.instance();

    std::ifstream file(certificate_path);
    if (!file.good()) {
        throw std::runtime_error(
                FUNC_SIGNATURE + ": "
                "unable to open file \"" + certificate_path + "\".");
    }

    std::string tmp;
    std::vector<std::string> line;
    std::vector<std::string> labels;

    getline(file, tmp);
    labels = optimizationtools::split(tmp, ',');
    while (getline(file, tmp)) {
        line = optimizationtools::split(tmp, ',');

        std::string type = "";
        ItemTypeId id = -1;
        BinPos copies = -1;
        BinPos bin_pos = -1;
        Length x = -1;
        Length y = -1;
        Length lx = -1;
        Length ly = -1;

        for (Counter i = 0; i < (Counter)line.size(); ++i) {
            if (labels[i] == "TYPE") {
                type = line[i];
            } else if (labels[i] == "ID") {
                id = (ItemTypeId)std::stol(line[i]);
            } else if (labels[i] == "COPIES") {
                copies = (Length)std::stol(line[i]);
            } else if (labels[i] == "BIN") {
                bin_pos = (BinPos)std::stol(line[i]);
            } else if (labels[i] == "X") {
                x = (Length)std::stol(line[i]);
            } else if (labels[i] == "Y") {
                y = (Length)std::stol(line[i]);
            } else if (labels[i] == "LX") {
                lx = (Length)std::stol(line[i]);
            } else if (labels[i] == "LY") {
                ly = (Length)std::stol(line[i]);
            }
        }

        if (type == "BIN") {
            add_bin(id, copies);
        } else if (type == "ITEM") {
            const ItemType& item_type = instance.item_type(id);
            add_item(
                    bin_pos,
                    id,
                    {x, y},
                    (lx != item_type.rect.x));
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
