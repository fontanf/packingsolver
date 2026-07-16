#include "onedimensional/solution_builder.hpp"

#include "optimizationtools/utils/utils.hpp"

#include <fstream>

using namespace packingsolver;
using namespace packingsolver::onedimensional;

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
        ItemTypeId item_type_id)
{
    if (bin_pos >= this->solution_.number_of_different_bins()) {
        throw std::invalid_argument(FUNC_SIGNATURE);
    }

    SolutionItem item;
    item.item_type_id = item_type_id;
    this->solution_.bins_[bin_pos].items.push_back(item);
}

void SolutionBuilder::read(
        const std::string& certificate_path)
{
    std::ifstream f(certificate_path);
    if (!f.good()) {
        throw std::runtime_error(
                FUNC_SIGNATURE + ": "
                "unable to open file \"" + certificate_path + "\".");
    }

    std::string tmp;
    std::vector<std::string> line;
    std::vector<std::string> labels;

    // read bin file
    getline(f, tmp);
    labels = optimizationtools::split(tmp, ',');
    while (getline(f, tmp)) {
        line = optimizationtools::split(tmp, ',');

        std::string type;
        BinTypeId type_id = -1;
        BinPos copies = -1;
        BinPos bin_pos = -1;
        Length x = -1;
        Length lx = -1;

        for (Counter i = 0; i < (Counter)line.size(); ++i) {
            if (labels[i] == "TYPE") {
                type = line[i];
            } else if (labels[i] == "ID") {
                type_id = (BinTypeId)std::stol(line[i]);
            } else if (labels[i] == "COPIES") {
                copies = (BinPos)std::stol(line[i]);
            } else if (labels[i] == "BIN") {
                bin_pos = (BinPos)std::stol(line[i]);
            } else if (labels[i] == "X") {
                x = (Length)std::stod(line[i]);
            } else if (labels[i] == "LX") {
                lx = (Length)std::stod(line[i]);
            }
        }
        if (type == "") {
            throw std::runtime_error(
                    FUNC_SIGNATURE + ": "
                    "missing \"TYPE\" value in \"" + certificate_path + "\".");
        }
        if (type_id == -1) {
            throw std::runtime_error(
                    FUNC_SIGNATURE + ": "
                    "missing \"ID\" value in \"" + certificate_path + "\".");
        }
        if (copies == -1) {
            throw std::runtime_error(
                    FUNC_SIGNATURE + ": "
                    "missing \"COPIES\" value in \"" + certificate_path + "\".");
        }
        if (bin_pos == -1) {
            throw std::runtime_error(
                    FUNC_SIGNATURE + ": "
                    "missing \"BIN\" value in \"" + certificate_path + "\".");
        }
        if (x == -1) {
            throw std::runtime_error(
                    FUNC_SIGNATURE + ": "
                    "missing \"X\" value in \"" + certificate_path + "\".");
        }
        if (lx == -1) {
            throw std::runtime_error(
                    FUNC_SIGNATURE + ": "
                    "missing \"LX\" value in \"" + certificate_path + "\".");
        }

        if (type == "BIN") {
            add_bin(
                    type_id,
                    copies);
        } else if (type == "ITEM") {
            add_item(
                    bin_pos,
                    type_id);
        } else {
            throw std::runtime_error(
                    FUNC_SIGNATURE + ": "
                    "wrong \"TYPE\" value in \"" + certificate_path + "\".");
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
