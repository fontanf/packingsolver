#include "packingsolver/rectangleguillotine/solution.hpp"

#include "optimizationtools/utils/utils.hpp"

#include <fstream>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////// Node /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

std::ostream& print(
        std::ostream& os,
        const std::vector<SolutionNode>& res,
        SolutionNodeId id,
        std::string tab)
{
    os << tab << res[id] << std::endl;
    for (SolutionNodeId c: res[id].children)
        print(os, res, c, tab + "  ");
    return os;
}

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream& os,
        const SolutionNode& node)
{
    os
        << " f " << node.f
        << " d " << node.d
        << " l " << node.l
        << " r " << node.r
        << " b " << node.b
        << " t " << node.t
        << " item_type_id " << node.item_type_id;
    return os;
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// Solution ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void Solution::update_indicators(
        BinPos bin_pos)
{
    SolutionBin& bin = bins_[bin_pos];
    const BinType& bin_type = instance().bin_type(bin.bin_type_id);

    number_of_bins_ += bin.copies;
    bin_copies_[bin.bin_type_id] += bin.copies;
    cost_ += bin.copies * bin_type.cost;
    full_area_ += bin.copies * bin_type.area();
    area_ = full_area_;
    second_leftover_value_ = 0;

    width_ = 0;
    height_ = 0;
    Counter subplate1curr_number_of_2_cuts = 0;
    Length subpalte1curr_end = -1;
    for (const SolutionNode& node: bin.nodes) {
        if (node.f != -1 && node.item_type_id >= 0) {
            number_of_items_ += bin.copies;
            item_area_ += bin.copies * instance().item_type(node.item_type_id).area();
            profit_ += bin.copies * instance().item_type(node.item_type_id).profit;
            item_copies_[node.item_type_id] += bin.copies;
            if (item_copies_[node.item_type_id]
                    > instance().item_type(node.item_type_id).copies) {
                item_copies_feasible_ = false;
                feasible_ = false;
            }
        }

        // Subtract residual area.
        if (node.item_type_id == -3)
            area_ -= (node.t - node.b) * (node.r - node.l);

        // Update width_ and height_.
        if (node.d > 0 && node.item_type_id != -3) {
            if (width_ < node.r)
                width_ = node.r;
            if (height_ < node.t)
                height_ = node.t;
        }

        // Update second_leftover_value_.
        if (node.d == 1 && node.item_type_id != -3)
            second_leftover_value_ = 0;
        if (node.d == 2 && node.item_type_id != -1)
            second_leftover_value_ = 0;
        if (node.d == 2 && node.item_type_id == -1)
            second_leftover_value_ = (node.r - node.l) * (node.t - node.b);

        // Check minimum waste length.
        if (node.d >= 1
                && node.item_type_id < 0) {
            if ((node.r - node.l
                        < instance().parameters().minimum_waste_length)
                    || (node.t - node.b
                        < instance().parameters().minimum_waste_length)) {
                std::cout << "minimum_waste_length_feasible_ = false" << std::endl;
                std::cout << "bin_pos " << bin_pos << " node " << node << std::endl;
                minimum_waste_length_feasible_ = false;
                feasible_ = false;
            }
        }

        // Check minimum distance between 1-cuts.
        if (node.d == 1
                && node.item_type_id == -2) {
            if ((bin.first_cut_orientation == CutOrientation::Vertical
                        && node.r - node.l
                        < instance().parameters().minimum_distance_1_cuts)
                    || (bin.first_cut_orientation == CutOrientation::Horizontal
                        && node.t - node.b
                        < instance().parameters().minimum_distance_1_cuts)) {
                //std::cout << "minimum_distance_1_cuts = false" << std::endl;
                minimum_distance_1_cuts_feasible_ = false;
                feasible_ = false;
            }
        }

        // Check maximum distance between 1-cuts.
        if (instance().parameters().maximum_distance_1_cuts >= 0) {
            if (node.d == 1
                    && node.item_type_id == -2) {
                if ((bin.first_cut_orientation == CutOrientation::Vertical
                            && node.r - node.l
                            > instance().parameters().maximum_distance_1_cuts)
                        || (bin.first_cut_orientation == CutOrientation::Horizontal
                            && node.t - node.b
                            > instance().parameters().maximum_distance_1_cuts)) {
                    //std::cout << "maximum_distance_1_cuts = false" << std::endl;
                    maximum_distance_1_cuts_feasible_ = false;
                    feasible_ = false;
                }
            }
        }

        // Check minimum distance between 2-cuts.
        if (node.d == 2
                && node.item_type_id == -2) {
            if ((bin.first_cut_orientation == CutOrientation::Vertical
                        && node.t - node.b
                        < instance().parameters().minimum_distance_2_cuts)
                    || (bin.first_cut_orientation == CutOrientation::Horizontal
                        && node.r - node.l
                        < instance().parameters().minimum_distance_2_cuts)) {
                //std::cout << "minimum_distance_2_cuts = false" << std::endl;
                minimum_distance_2_cuts_feasible_ = false;
                feasible_ = false;
            }
        }

        // Check maximum distance between 2-cuts.
        if (instance().parameters().maximum_distance_2_cuts >= 0) {
            if (node.d == 2
                    && node.item_type_id == -2) {
                if ((bin.first_cut_orientation == CutOrientation::Vertical
                            && node.t - node.b
                            > instance().parameters().maximum_distance_2_cuts)
                        || (bin.first_cut_orientation == CutOrientation::Horizontal
                            && node.r - node.l
                            > instance().parameters().maximum_distance_2_cuts)) {
                    //std::cout << "maximum_distance_2_cuts = false" << std::endl;
                    maximum_distance_2_cuts_feasible_ = false;
                    feasible_ = false;
                }
            }
        }

        // Check maximum number of 2-cuts.
        if (instance().parameters().maximum_number_2_cuts >= 0) {
            if (node.d == 1) {
                subpalte1curr_end = (bin.first_cut_orientation == CutOrientation::Vertical)?
                    node.t:
                    node.r;
                subplate1curr_number_of_2_cuts = 0;
            }
            if (node.d == 2) {
                if ((bin.first_cut_orientation == CutOrientation::Vertical
                            && node.t != subpalte1curr_end)
                        || (bin.first_cut_orientation == CutOrientation::Horizontal
                            && node.r != subpalte1curr_end)) {
                    subplate1curr_number_of_2_cuts++;
                    if (subplate1curr_number_of_2_cuts
                            > instance().parameters().maximum_number_2_cuts) {
                        //std::cout << "maximum_number_2_cuts = false" << std::endl;
                        maximum_number_2_cuts_feasible_ = false;
                        feasible_ = false;
                    }
                }
            }
        }

        // Check stacks.
        if (node.d >= 1
                && node.item_type_id >= 0) {
            const ItemType& item_type = instance().item_type(node.item_type_id);
            if (item_type.stack_pos > 0) {
                ItemTypeId item_type_id_pred = instance().item(
                        item_type.stack_id,
                        item_type.stack_pos - 1);
                const ItemType& item_type_pred = instance().item_type(item_type_id_pred);
                if (item_copies(item_type_id_pred) != item_type_pred.copies) {
                    //std::cout << "stacks_feasible = false" << std::endl;
                    //std::cout << "item_type_id " << node.item_type_id
                    //    << " stack_id " << item_type.stack_id
                    //    << " stack_pos " << item_type.stack_pos
                    //    << std::endl;
                    //std::cout << "item_type_id_pred " << item_type_id_pred
                    //    << " stack_id " << item_type_pred.stack_id
                    //    << " stack_pos " << item_type_pred.stack_pos
                    //    << " copies " << item_copies(item_type_id_pred)
                    //    << " / " << item_type_pred.copies
                    //    << std::endl;
                    stacks_feasible_ = false;
                    feasible_ = false;
                }
            }
        }

        // Check defect intersections.
        if (node.d >= 1
                && node.item_type_id >= 0) {
            DefectId k = instance().rect_intersects_defect(
                    node.l,
                    node.r,
                    node.b,
                    node.t,
                    bin_type);
            if (k != -1) {
                //std::cout << "defects_feasible = false" << std::endl;
                defects_feasible_ = false;
                feasible_ = false;
            }
        }

        // Check cuts through defects.
        if (!instance().parameters().cut_through_defects
                && node.d >= 1) {
            DefectId kl = instance().x_intersects_defect(
                    node.l,
                    node.b,
                    node.t,
                    bin_type);
            DefectId kr = instance().x_intersects_defect(
                    node.r,
                    node.b,
                    node.t,
                    bin_type);
            DefectId kb = instance().y_intersects_defect(
                    node.l,
                    node.r,
                    node.b,
                    bin_type);
            DefectId kt = instance().y_intersects_defect(
                    node.l,
                    node.r,
                    node.t,
                    bin_type);
            if (kl != -1 || kr != -1 || kb != -1 || kt != -1) {
                cut_through_defects_feasible_ = false;
                feasible_ = false;
            }
        }
    }
    // Staging analysis.
    //
    // First pass: compute the maximum depth of any CUT node in this bin.
    // This is the "real" staged count as if all cuts were Exact.
    for (const SolutionNode& node: bin.nodes) {
        if (node.f != -1) {
            const SolutionNode& parent = bin.nodes[node.f];
            if (node.l == parent.l && node.r == parent.r && node.b == parent.b && node.t == parent.t)
                continue;
        }
        bin.number_of_stages_real = std::max(
                bin.number_of_stages_real, (Counter)node.d);
    }

    // Second pass: classify the deepest-level CUT nodes.
    //
    // A pattern is non-exact if ALL depth-k CUT nodes (where k =
    // number_of_stages_real) contain no cuts or a single cut separating one
    // item from one waste (i.e. ≤ 2 non-residual children, exactly 1 item).
    // A pattern is Roadef2018-style if ALL depth-k CUT nodes are valid
    // Roadef2018 extra cuts (≥ 2 item children, ≤ 3 non-residual children
    // total).  If any depth-k CUT node is a regular intermediate stage the
    // flags remain false.
    if (bin.number_of_stages_real > 0) {
        bool all_non_exact = true;
        bool all_roadef2018_compatible = true;
        bool any_roadef2018_style = false;
        bool found = false;

        for (SolutionNodeId node_id = 0;
                node_id < (SolutionNodeId)bin.nodes.size();
                ++node_id) {
            const SolutionNode& node = bin.nodes[node_id];
            if (node.item_type_id != -2 || node.d != bin.number_of_stages_real - 1)
                continue;
            found = true;
            Counter item_children = 0;
            Counter total_children = 0;
            for (SolutionNodeId child_id: node.children) {
                const SolutionNode& child = bin.nodes[child_id];
                if (child.item_type_id == -3)  // residual — ignore
                    continue;
                ++total_children;
                if (child.item_type_id >= 0)
                    ++item_children;
            }
            if (item_children == 1 && total_children <= 2) {
                // Non-exact style: one item + at most one waste.
            } else if (item_children >= 2 && total_children <= 3) {
                // Roadef2018 style: two items + at most one waste.
                any_roadef2018_style = true;
                all_non_exact = false;
            } else {
                // Regular intermediate stage: too many children or no items.
                all_non_exact = false;
                all_roadef2018_compatible = false;
            }
        }

        if (found) {
            bin.cut_type_is_non_exact = all_non_exact;
            bin.cut_type_is_roadef2018 = all_roadef2018_compatible && any_roadef2018_style;
        }
    }

    // Compute the effective number of stages for this bin.
    // If the deepest CUT level consists entirely of valid non-exact or
    // Roadef2018 extra cuts, that level does not count as a full stage.
    bool extra_cut_applies
        = (instance().parameters().cut_type == CutType::NonExact
                && bin.cut_type_is_non_exact)
        || (instance().parameters().cut_type == CutType::Roadef2018
                && (bin.cut_type_is_non_exact || bin.cut_type_is_roadef2018));
    bin.number_of_stages = extra_cut_applies?
        bin.number_of_stages_real - 1:
        bin.number_of_stages_real;

    // Aggregate staging information to the solution level.
    this->number_of_stages_ = std::max(this->number_of_stages_, bin.number_of_stages);

    // Number-of-stages feasibility check.
    if (this->number_of_stages_ > instance().parameters().number_of_stages) {
        this->number_of_stages_feasible_ = false;
        this->feasible_ = false;
    }

    if (!minimum_waste_length_feasible()) {
        for (const SolutionNode& node: bin.nodes) {
            std::cout << node << std::endl;
        }
        write("solution_rectangleguillotine.csv");
        exit(1);
    }
}

void Solution::append(
        const Solution& solution,
        BinPos bin_pos,
        BinPos copies,
        const std::vector<BinTypeId>& bin_type_ids,
        const std::vector<ItemTypeId>& item_type_ids)
{
    if (number_of_different_bins() > 0) {
        SolutionNode& node = bins_.back().nodes.back();
        if (node.item_type_id == -3) {
            node.item_type_id = -1;
            area_ -= (node.t - node.b) * (node.r - node.l);
        }
    }
    const SolutionBin& bin_old = solution.bin(bin_pos);
    BinTypeId bin_type_id = (bin_type_ids.empty())?
        bin_old.bin_type_id:
        bin_type_ids[bin_old.bin_type_id];
    SolutionBin bin;
    bin.bin_type_id = bin_type_id;
    bin.copies = copies;
    bin.first_cut_orientation = bin_old.first_cut_orientation;
    for (SolutionNode node: bin_old.nodes) {
        if (node.t == -4) {
        } else if (node.f == -1) {
            node.item_type_id = bin_type_id;
            const BinType& new_bin_type = instance().bin_type(bin_type_id);
            node.r = new_bin_type.rect.w;
            node.t = new_bin_type.rect.h;
        } else if (node.item_type_id >= 0) {
            node.item_type_id = (item_type_ids.empty())?
                node.item_type_id:
                item_type_ids[node.item_type_id];
        }
        bin.nodes.push_back(node);
    }
    bins_.push_back(bin);
    update_indicators(bins_.size() - 1);
}

void Solution::append(
        const Solution& solution,
        const std::vector<BinTypeId>& bin_type_ids,
        const std::vector<ItemTypeId>& item_type_ids)
{
    for (BinPos bin_pos = 0; bin_pos < solution.number_of_bins(); ++bin_pos)
        append(solution, bin_pos, 1, bin_type_ids, item_type_ids);
}

bool Solution::operator<(const Solution& solution) const
{
    // Check feasibility.
    if (!solution.feasible_)
        return false;
    if (!feasible_)
        return true;

    switch (instance().objective()) {
    case Objective::Default: {
        if (solution.profit() < profit())
            return false;
        if (solution.profit() > profit())
            return true;
        return solution.waste() < waste();
    } case Objective::BinPacking: {
        if (!solution.full())
            return false;
        if (!full())
            return true;
        return solution.number_of_bins() < number_of_bins();
    } case Objective::BinPackingWithLeftovers: {
        if (!solution.full())
            return false;
        if (!full())
            return true;
        if (solution.waste() != waste())
            return solution.waste() < waste();
        return solution.second_leftover_value() > second_leftover_value();
    } case Objective::OpenDimensionX: {
        if (!solution.full())
            return false;
        if (!full())
            return true;
        return solution.width() < width();
    } case Objective::OpenDimensionY: {
        if (!solution.full())
            return false;
        if (!full())
            return true;
        return solution.height() < height();
    } case Objective::Knapsack: {
        return strictly_greater(solution.profit(), profit());
    } case Objective::Feasibility: {
        if (solution.full() != full())
            return solution.full();
        return strictly_greater(solution.profit(), profit());
    } case Objective::VariableSizedBinPacking: {
        if (!solution.full())
            return false;
        if (!full())
            return true;
        return strictly_lesser(solution.cost(), cost());
    } default: {
        std::stringstream ss;
        ss << FUNC_SIGNATURE << ": "
            << "solution rectangleguillotine::Solution does not support objective \""
            << instance().objective() << "\"";
        throw std::logic_error(ss.str());
    }
    }
}

void Solution::write(
        const std::string& certificate_path) const
{
    if (certificate_path.empty())
        return;
    std::ofstream file{certificate_path};
    if (!file.good()) {
        throw std::runtime_error(
                FUNC_SIGNATURE + ": "
                "unable to open file \"" + certificate_path + "\".");
    }

    file << "PLATE_ID,COPIES,NODE_ID,X,Y,WIDTH,HEIGHT,TYPE,CUT,PARENT" << std::endl;
    SolutionNodeId offset = 0;
    for (BinPos bin_pos = 0; bin_pos < number_of_different_bins(); ++bin_pos) {
        const SolutionBin& solution_bin = bins_[bin_pos];
        BinTypeId bin_type_id = solution_bin.bin_type_id;
        const BinType& bin_type = instance().bin_type(bin_type_id);
        for (SolutionNodeId node_id = 0;
                node_id < (SolutionNodeId)solution_bin.nodes.size();
                ++node_id) {
            const SolutionNode& n = solution_bin.nodes[node_id];
            file
                << bin_pos << ","
                << solution_bin.copies << ","
                << offset + node_id << ","
                << n.l << ","
                << n.b << ","
                << n.r - n.l << ","
                << n.t - n.b << ","
                << n.item_type_id << ","
                << n.d << ",";
            if (n.f != -1)
                file << offset + n.f;
            file << std::endl;
        }
        offset += solution_bin.nodes.size();
        for (const Defect& defect: bin_type.defects) {
            file
                << bin_pos << ","
                << solution_bin.copies << ","
                << -1 << ","
                << defect.pos.x << ","
                << defect.pos.y << ","
                << defect.rect.w << ","
                << defect.rect.h << ","
                << -4 << ","
                << -1 << ","
                << std::endl;
        }
    }
}

nlohmann::json Solution::to_json() const
{
    return nlohmann::json {
        {"NumberOfItems", number_of_items()},
        {"ItemArea", item_area()},
        {"ItemProfit", profit()},
        {"NumberOfBins", number_of_bins()},
        {"NumberOfDifferentBins", number_of_different_bins()},
        {"BinArea", full_area()},
        {"BinCost", cost()},
        {"Waste", waste()},
        {"WastePercentage", waste_percentage()},
        {"FullWaste", full_waste()},
        {"FullWastePercentage", full_waste_percentage()},
        {"Width", width()},
        {"Height", height()},
        {"SecondLeftoverValue", second_leftover_value()},
        {"NumberOfStages", number_of_stages()},
        {"Feasible", {
            {"Feasible", feasible()},
            {"NumberOfStages", number_of_stages_feasible()},
            {"MinimumWasteLength", minimum_waste_length_feasible()},
            {"MinimumDistance1Cuts", minimum_distance_1_cuts_feasible()},
            {"MaximumDistance1Cuts", maximum_distance_1_cuts_feasible()},
            {"MinimumDistance2Cuts", minimum_distance_2_cuts_feasible()},
            {"MaximumDistance2Cuts", maximum_distance_2_cuts_feasible()},
            {"MaximumNumber2Cuts", maximum_number_2_cuts_feasible()},
            {"Stacks", stacks_feasible()},
            {"Defects", defects_feasible()},
            {"CutThroughDefects", cut_through_defects_feasible()},
            {"ItemCopies", item_copies_feasible()},
        }},
    };
}

void Solution::format(
        std::ostream& os,
        int verbosity_level) const
{
    if (verbosity_level >= 1) {
        os
            << "Number of items:           " << optimizationtools::Ratio<ItemPos>(number_of_items(), instance().number_of_items()) << std::endl
            << "Item area:                 " << optimizationtools::Ratio<Area>(item_area(), instance().item_area()) << std::endl
            << "Item profit:               " << optimizationtools::Ratio<Profit>(profit(), instance().item_profit()) << std::endl
            << "Number of bins:            " << optimizationtools::Ratio<BinPos>(number_of_bins(), instance().number_of_bins()) << std::endl
            << "Number of different bins:  " << number_of_different_bins() << std::endl
            << "Bin area:                  " << optimizationtools::Ratio<BinPos>(full_area(), instance().bin_area()) << std::endl
            << "Bin cost:                  " << cost() << std::endl
            << "Waste:                     " << waste() << " (" << 100 * waste_percentage() << "%)" << std::endl
            << "Full waste:                " << full_waste() << " (" << 100 * full_waste_percentage() << "%)" << std::endl
            << "Width:                     " << width() << std::endl
            << "Height:                    " << height() << std::endl
            << "Second leftover value:     " << second_leftover_value() << std::endl
            << "Number of stages:          " << number_of_stages() << std::endl
            << "Feasible:                  " << feasible() << std::endl
            << "    Number of stages:      " << number_of_stages_feasible() << std::endl
            << "    Min. waste length:     " << minimum_waste_length_feasible() << std::endl
            << "    Min. dist. 1-cuts:     " << minimum_distance_1_cuts_feasible() << std::endl
            << "    Max. dist. 1-cuts:     " << maximum_distance_1_cuts_feasible() << std::endl
            << "    Min. dist. 2-cuts:     " << minimum_distance_2_cuts_feasible() << std::endl
            << "    Max. dist. 2-cuts:     " << maximum_distance_2_cuts_feasible() << std::endl
            << "    Max. no. 2-cuts:       " << maximum_number_2_cuts_feasible() << std::endl
            << "    Stacks:                " << stacks_feasible() << std::endl
            << "    Defects:               " << defects_feasible() << std::endl
            << "    Cut through defects:   " << cut_through_defects_feasible() << std::endl
            << "    Item copies:           " << item_copies_feasible() << std::endl
            ;
    }

    if (verbosity_level >= 2) {
        os
            << std::right << std::endl
            << std::setw(12) << "Bin"
            << std::setw(12) << "Type"
            << std::setw(12) << "Copies"
            << std::endl
            << std::setw(12) << "---"
            << std::setw(12) << "----"
            << std::setw(12) << "------"
            << std::endl;
        for (BinPos bin_pos = 0;
                bin_pos < number_of_different_bins();
                ++bin_pos) {
            os
                << std::setw(12) << bin_pos
                << std::setw(12) << bin(bin_pos).bin_type_id
                << std::setw(12) << bin(bin_pos).copies
                << std::endl;
        }
    }

    if (verbosity_level >= 3) {
        os
            << std::right << std::endl
            << std::setw(12) << "Bin"
            << std::setw(12) << "Node"
            << std::setw(12) << "Parent"
            << std::setw(12) << "Depth"
            << std::setw(12) << "Left"
            << std::setw(12) << "Right"
            << std::setw(12) << "Bottom"
            << std::setw(12) << "Top"
            << std::setw(12) << "Item"
            << std::endl
            << std::setw(12) << "---"
            << std::setw(12) << "----"
            << std::setw(12) << "------"
            << std::setw(12) << "-----"
            << std::setw(12) << "----"
            << std::setw(12) << "-----"
            << std::setw(12) << "------"
            << std::setw(12) << "---"
            << std::setw(12) << "----"
            << std::endl;
        for (BinPos bin_pos = 0; bin_pos < number_of_different_bins(); ++bin_pos) {
            const SolutionBin& solution_bin = bins_[bin_pos];
            for (SolutionNodeId node_id = 0;
                    node_id < (SolutionNodeId)solution_bin.nodes.size();
                    ++node_id) {
                const SolutionNode& node = solution_bin.nodes[node_id];
                //const BinType& bin_type = instance().bin_type(bin(bin_pos).i);
                os
                    << std::setw(12) << bin_pos
                    << std::setw(12) << node_id
                    << std::setw(12) << node.f
                    << std::setw(12) << node.d
                    << std::setw(12) << node.l
                    << std::setw(12) << node.r
                    << std::setw(12) << node.b
                    << std::setw(12) << node.t
                    << std::setw(12) << node.item_type_id
                    << std::endl;
            }
        }
    }

    if (verbosity_level >= 3) {
        os
            << std::right << std::endl
            << std::setw(12) << "Item type"
            << std::setw(12) << "Width"
            << std::setw(12) << "Height"
            << std::setw(12) << "Copies"
            << std::endl
            << std::setw(12) << "---------"
            << std::setw(12) << "-----"
            << std::setw(12) << "------"
            << std::setw(12) << "------"
            << std::endl;
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance().number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = instance().item_type(item_type_id);
            os
                << std::setw(12) << item_type_id
                << std::setw(12) << item_type.rect.w
                << std::setw(12) << item_type.rect.h
                << std::setw(12) << item_copies(item_type_id)
                << std::endl;
        }
    }
}

bool packingsolver::rectangleguillotine::operator==(
        const SolutionNode& solution_node_1,
        const SolutionNode& solution_node_2)
{
    return solution_node_1.f == solution_node_2.f
        && solution_node_1.d == solution_node_2.d
        && solution_node_1.l == solution_node_2.l
        && solution_node_1.r == solution_node_2.r
        && solution_node_1.b == solution_node_2.b
        && solution_node_1.t == solution_node_2.t
        && solution_node_1.children == solution_node_2.children
        && solution_node_1.item_type_id == solution_node_2.item_type_id;
}

bool packingsolver::rectangleguillotine::operator!=(
        const SolutionNode& solution_node_1,
        const SolutionNode& solution_node_2)
{
    return !(solution_node_1 == solution_node_2);
}

bool packingsolver::rectangleguillotine::operator==(
        const SolutionBin& solution_bin_1,
        const SolutionBin& solution_bin_2)
{
    return solution_bin_1.bin_type_id == solution_bin_2.bin_type_id
        && solution_bin_1.first_cut_orientation == solution_bin_2.first_cut_orientation
        && solution_bin_1.nodes == solution_bin_2.nodes;
}

bool packingsolver::rectangleguillotine::operator!=(
        const SolutionBin& solution_bin_1,
        const SolutionBin& solution_bin_2)
{
    return !(solution_bin_1 == solution_bin_2);
}
