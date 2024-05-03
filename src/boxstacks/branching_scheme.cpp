#include "packingsolver/boxstacks/branching_scheme.hpp"

#include "knapsacksolver/multiple_choice_subset_sum/instance_builder.hpp"
#include "knapsacksolver/multiple_choice_subset_sum/algorithms/dynamic_programming_bellman.hpp"

#include <string>
#include <cmath>
#include <unordered_set>

using namespace packingsolver;
using namespace packingsolver::boxstacks;

struct StackHasher
{
    std::hash<ItemTypeId> hasher;

    size_t operator()(const std::vector<ItemTypeId>& stack) const
    {
        size_t hash = 0;
        for (ItemTypeId item_type_id: stack)
            optimizationtools::hash_combine(hash, hasher(item_type_id));
        return hash;
    }
};

using StackSet = std::unordered_set<std::vector<ItemTypeId>, StackHasher>;

std::vector<StackSet> generate_all_stacks(
        const Instance& instance)
{
    std::vector<StackSet> item_type_stacks(instance.number_of_item_types());
    Counter number_of_generated_stacks = 0;

    std::vector<std::unordered_map<StackabilityId, std::vector<ItemTypeId>>> stackability_id_item_types(instance.number_of_groups());
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        auto it = stackability_id_item_types[item_type.group_id].find(item_type.stackability_id);
        if (it == stackability_id_item_types[item_type.group_id].end()) {
            stackability_id_item_types[item_type.group_id].insert({
                    item_type.stackability_id,
                    {item_type_id}});
        } else {
            it->second.push_back(item_type_id);
        }
    }

    for (GroupId group_id = 0; group_id < instance.number_of_groups(); ++group_id) {
        for (auto it: stackability_id_item_types[group_id]) {
            const auto& item_types = it.second;

            const BinType& bin_type = instance.bin_type(0);
            Weight w = bin_type.maximum_stack_density * instance.item_type(item_types[0]).area();
            Weight stack_maximum_weight = (std::min)(bin_type.maximum_weight, w);

            std::vector<ItemTypeId> node_stack;
            for (ItemTypeId item_type_id_new: item_types) {
                node_stack.push_back(-1);
                node_stack.push_back(item_type_id_new);
            }

            std::vector<ItemPos> current_stack_item_number_of_copies(instance.number_of_item_types(), 0);
            std::vector<ItemTypeId> current_stack_item_type_ids;
            std::vector<Length> current_stack_height = {0};
            std::vector<Weight> current_stack_weight = {0};
            std::vector<Weight> current_stack_remaining_weight = {std::numeric_limits<Weight>::infinity()};
            std::vector<ItemPos> current_stack_maximum_number_of_items = {instance.number_of_items()};

            while (!node_stack.empty()) {

                ItemTypeId item_type_id = node_stack.back();
                node_stack.pop_back();

                if (item_type_id == -1) {
                    current_stack_item_number_of_copies[current_stack_item_type_ids.back()]--;
                    current_stack_item_type_ids.pop_back();
                    current_stack_height.pop_back();
                    current_stack_weight.pop_back();
                    current_stack_remaining_weight.pop_back();
                    current_stack_maximum_number_of_items.pop_back();
                } else {

                    // Update current stack.
                    const ItemType& item_type = instance.item_type(item_type_id);
                    Length zj = instance.z(item_type, 0) - item_type.nesting_height;
                    current_stack_item_number_of_copies[item_type_id]++;
                    current_stack_item_type_ids.push_back(item_type_id);
                    current_stack_height.push_back(current_stack_height.back() + zj);
                    current_stack_weight.push_back(current_stack_weight.back() + item_type.weight);
                    current_stack_maximum_number_of_items.push_back((std::min)(
                                current_stack_maximum_number_of_items.back(),
                                item_type.maximum_stackability));
                    current_stack_remaining_weight.push_back((std::min)(
                                current_stack_remaining_weight.back() - item_type.weight,
                                item_type.maximum_weight_above));

                    // Update set of feasible stacks.
                    for (ItemTypeId item_type_id_tmp: current_stack_item_type_ids) {
                        item_type_stacks[item_type_id_tmp].insert(current_stack_item_type_ids);
                    }

                    // Initialize node_stack with stacks of one item.
                    number_of_generated_stacks++;
                    if (number_of_generated_stacks > 1e3) {
                        return {};
                    }

                    //for (ItemTypeId item_type_id: current_stack_item_type_ids)
                    //    std::cout << " " << item_type_id;
                    //std::cout << std::endl;

                    // Generate children.
                    for (ItemTypeId item_type_id: item_types) {
                        // Check feasibility.
                        if (current_stack_item_number_of_copies[item_type_id]
                                == instance.item_type(item_type_id).copies) {
                            continue;
                        }

                        const ItemType& item_type = instance.item_type(item_type_id);
                        Length zj = instance.z(item_type, 0) - item_type.nesting_height;
                        Length zi = instance.z(bin_type);

                        // Check bin z.
                        if (current_stack_height.back() + zj > zi) {
                            continue;
                        }

                        // Check maximum weight.
                        Weight weight = current_stack_weight.back() + item_type.weight;
                        if (weight > stack_maximum_weight * PSTOL) {
                            continue;
                        }

                        // Check maximum stackability.
                        ItemPos last_stack_maximum_number_of_items = (std::min)(
                                current_stack_maximum_number_of_items.back(),
                                item_type.maximum_stackability);
                        if ((ItemPos)current_stack_item_type_ids.size() + 1
                                > last_stack_maximum_number_of_items) {
                            continue;
                        }

                        // Check maximum weight above.
                        if (item_type.weight > current_stack_remaining_weight.back()) {
                            continue;
                        }

                        // Update node stack.
                        //std::cout << "add " << item_type_id << std::endl;
                        node_stack.push_back(-1);
                        node_stack.push_back(item_type_id);
                    }
                }

            }
        }
    }

    //for (ItemTypeId item_type_id = 0;
    //        item_type_id < instance.number_of_item_types();
    //        ++item_type_id) {
    //    std::cout << "item_type_id: " << item_type_id << " size " << item_type_stacks[item_type_id].size() << std::endl;
    //    for (const std::vector<ItemTypeId>& stack: item_type_stacks[item_type_id]) {
    //        for (ItemTypeId item_type_id: stack)
    //            std::cout << " " << item_type_id;
    //        std::cout << std::endl;
    //    }
    //}

    return item_type_stacks;
}

bool may_dominate(
        const Instance& instance,
        const std::vector<StackSet>& item_type_stacks,
        ItemTypeId item_type_id_1,
        ItemTypeId item_type_id_2)
{
    //std::cout << "may_dominate " << item_type_id_1 << " " << item_type_id_2 << std::endl;
    const ItemType& item_type_1 = instance.item_type(item_type_id_1);
    const ItemType& item_type_2 = instance.item_type(item_type_id_2);
    for (const std::vector<ItemTypeId>& stack_2: item_type_stacks[item_type_id_2]) {

        ItemPos c = 1;
        for (ItemTypeId item_type_id_tmp: stack_2)
            if (item_type_id_tmp == item_type_id_1)
                ++c;

        for (; c <= item_type_1.copies; ++c) {
            auto stack_1 = stack_2;
            ItemPos cc = 0;
            for (ItemPos pos = 0; pos < (ItemPos)stack_2.size(); ++pos) {
                if (stack_2[pos] == item_type_id_2) {
                    stack_1[pos] = item_type_id_1;
                    cc++;
                    if (cc == c)
                        break;
                }
            }
            if (cc != c)
                break;
            //for (ItemTypeId item_type_id: stack_1)
            //    std::cout << " " << item_type_id;
            //std::cout << std::endl;
            if (item_type_stacks[item_type_id_1].find(stack_1)
                    == item_type_stacks[item_type_id_1].end()) {
                //std::cout << "false" << std::endl;
                return false;
            }
        }
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// BranchingScheme ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

BranchingScheme::BranchingScheme(
        const Instance& instance,
        const Parameters& parameters):
    instance_(instance),
    parameters_(parameters),
    predecessors_(instance.number_of_item_types())
{
    // Compute dominated items.
    std::vector<StackSet> item_type_stacks = generate_all_stacks(instance);
    if (!item_type_stacks.empty()) {
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = instance.item_type(item_type_id);
            for (ItemTypeId item_type_id_2 = 0;
                    item_type_id_2 < instance.number_of_item_types();
                    ++item_type_id_2) {
                if (item_type_id_2 == item_type_id)
                    continue;
                const ItemType& item_type_2 = instance.item_type(item_type_id_2);

                // Check if item_type dominates item_type_2.
                if (item_type_2.group_id != item_type.group_id)
                    continue;
                if (item_type_2.stackability_id != item_type.stackability_id)
                    continue;
                if (!((item_type_2.box.x == item_type.box.x
                                && item_type_2.box.y == item_type.box.y)
                            || (item_type_2.box.x == item_type.box.y
                                && item_type_2.box.y == item_type.box.x))) {
                    continue;
                }
                if (!may_dominate(instance, item_type_stacks, item_type_id, item_type_id_2)) {
                    continue;
                }
                if (!may_dominate(instance, item_type_stacks, item_type_id_2, item_type_id)) {
                    continue;
                }
                if (item_type.weight > item_type_2.weight)
                    continue;
                if (item_type.weight == item_type_2.weight
                        && item_type_id > item_type_id_2) {
                    continue;
                }
                //std::cout << item_type_id << " dominates " << item_type_id_2 << std::endl;

                predecessors_[item_type_id_2].push_back(item_type_id);
            }
        }
    }

    // Lift length.
    // Build Multiple-Choice Subset Sum instance.
    const BinType& bin_type = instance.bin_type(0);
    Direction o = parameters_.direction;
    knapsacksolver::multiple_choice_subset_sum::InstanceBuilder mcss_instance_builder;
    mcss_instance_builder.set_capacity(instance.x(bin_type, o));
    ItemPos mcss_pos = 0;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        for (ItemPos copies = 0; copies < item_type.copies; ++copies) {
            for (int rotation = 0; rotation < 6; ++rotation) {
                if (instance.item_type(item_type_id).can_rotate(rotation)) {
                    Length xj = instance.x(item_type, rotation, o);
                    mcss_instance_builder.add_item(mcss_pos, xj);
                }
            }
            mcss_pos++;
        }
    }
    knapsacksolver::multiple_choice_subset_sum::Instance mcss_instance = mcss_instance_builder.build();
    //mcss_instance.print(std::cout, 2);
    knapsacksolver::multiple_choice_subset_sum::Parameters mcss_parameters;
    mcss_parameters.verbosity_level = 0;
    auto mscc_output = knapsacksolver::multiple_choice_subset_sum::dynamic_programming_bellman_array(
            mcss_instance,
            mcss_parameters);
    //auto mscc_output = multiple_choice_subset_sumsolver::dynamic_programming_bellman_word_ram(mcss_instance);
    Length xi = mscc_output.bound;

    // Compute is_item_type_selected_.
    std::vector<ItemTypeId> item_types(instance.number_of_item_types());
    std::iota(item_types.begin(), item_types.end(), 0);
    sort(
            item_types.begin(),
            item_types.end(),
            [&instance](
                ItemTypeId item_type_id_1,
                ItemTypeId item_type_id_2) -> bool
            {
                return instance.item_type(item_type_id_1).group_id
                    > instance.item_type(item_type_id_2).group_id;
            });

    std::vector<Weight> group_average_weights(instance.number_of_groups(), 0);
    std::vector<std::vector<Weight>> sorted_group_weights(instance.number_of_groups());
    std::vector<ItemPos> group_number_of_selected_items(instance.number_of_groups(), 0);
    Weight weight = 0.0;
    bool stop = false;
    for (GroupId group_id = instance.number_of_groups() - 1; group_id >= 0; --group_id) {
        for (ItemTypeId j: instance.group(group_id).item_types) {
            const ItemType& item_type = instance.item_type(j);
            group_average_weights[group_id] += item_type.copies * item_type.weight;
        }
        group_average_weights[group_id] /= instance.group(group_id).number_of_items;
        for (ItemPos pos = 0; pos < instance.group(group_id).number_of_items; ++pos) {
            if (weight + group_average_weights[group_id] > instance.bin_weight()) {
                stop = true;
                break;
            }
            if (parameters.maximum_number_of_selected_items != -1
                    && number_of_selected_items_ + 1
                    > parameters.maximum_number_of_selected_items) {
                stop = true;
                break;
            }
            weight += group_average_weights[group_id];
            group_number_of_selected_items[group_id]++;
            number_of_selected_items_++;
        }
        for (ItemTypeId item_type_id: instance.group(group_id).item_types) {
            const ItemType& item_type = instance.item_type(item_type_id);
            for (ItemPos c = 0; c < item_type.copies; ++c)
                sorted_group_weights[group_id].push_back(item_type.weight);
        }
        sort(
                sorted_group_weights[group_id].begin(),
                sorted_group_weights[group_id].end());
        while ((ItemPos)sorted_group_weights[group_id].size() > group_number_of_selected_items[group_id])
            sorted_group_weights[group_id].pop_back();

        //std::cout
        //    << "group " << group_id
        //    << " average_weight " << group_average_weights[group_id]
        //    << " number_of_selected_items " << group_number_of_selected_items[group_id]
        //    << " weight " << weight
        //    << std::endl;
        if (stop)
            break;
    }

    // Compute item_distribution_parameter_.
    double parameter_lower_bound = 0.9;
    double parameter_upper_bound = 10.0;
    for (;;) {
        item_distribution_parameter_ = (parameter_lower_bound + parameter_upper_bound) / 2;
        //std::cout << "item_distribution_parameter_ " << item_distribution_parameter_ << std::endl;
        bool ok = true;
        double weight = 0.0;
        double weight_weighted_sum = 0.0;
        ItemPos pos = 0;
        for (GroupId group_id = instance.number_of_groups() - 1; group_id >= 0; --group_id) {
            //std::cout << "group " << group_id
            //    << " size " << group_number_of_selected_items[group_id]
            //    << std::endl;
            //std::cout
            //    << " group " << group_id
            //    << " average_weight " << average_weights[group_id]
            //    << " weight " << weight
            //    << std::endl;
            for (ItemPos p = 0; p < group_number_of_selected_items[group_id]; ++p) {
                double x1 = xi * std::pow(
                        (double)1.0 - std::pow(1.0 - (double)pos / number_of_selected_items_, item_distribution_parameter_),
                        (double)1.0 / item_distribution_parameter_);
                double x2 = xi * std::pow(
                        (double)1.0 - std::pow(1.0 - (double)(pos + 1) / number_of_selected_items_, item_distribution_parameter_),
                        (double)1.0 / item_distribution_parameter_);
                //std::cout << "    pos " << pos
                //    << " x1 " << x1
                //    << " x2 " << x2
                //    << " xi " << xi
                //    << " xc " << ((double)x1 + (double)(x2 - x1) / 2)
                //    << std::endl;
                Weight w = group_average_weights[group_id];
                //Weight w = sorted_group_weights[group_id][pos];
                weight += w;
                weight_weighted_sum
                    += ((double)x1 + (double)(x2 - x1) / 2) * w;
                pos++;
            }
            //std::cout << "  weight " << weight
            //    << " weight_weighted_sum " << weight_weighted_sum
            //    << std::endl;
            if (!instance.check_weight_constraints(group_id))
                continue;
            std::pair<double, double> axle_weights = bin_type.semi_trailer_truck_data.compute_axle_weights(
                    weight_weighted_sum, weight);
            //std::cout << "  middle_axle_weight " << axle_weights.first
            //    << " / " << bin_type.middle_axle_maximum_weight
            //    << " rear_axle_weight " << axle_weights.second
            //    << " / " << bin_type.rear_axle_maximum_weight
            //    << std::endl;
            if (axle_weights.first > bin_type.semi_trailer_truck_data.middle_axle_maximum_weight * PSTOL) {
                //std::cout << "group " << group_id
                //    << " middle_axle_weight " << axle_weights.first
                //    << std::endl;
                ok = false;
                break;
            }
            //if (axle_weights.second > bin_type.rear_axle_maximum_weight) {
            //    std::cout << "group " << group_id
            //        << " rear_axle_weight " << axle_weights.second
            //        << std::endl;
            //    ok = false;
            //    break;
            //}
        }
        if (ok) {
            parameter_upper_bound = item_distribution_parameter_;
        } else {
            parameter_lower_bound = item_distribution_parameter_;
        }
        if (parameter_upper_bound - parameter_lower_bound < 1e-4)
            break;
    }
    item_distribution_parameter_ = parameter_upper_bound;
    if (item_distribution_parameter_ < 1)
        item_distribution_parameter_ = (item_distribution_parameter_ + 1) / 2;
    //item_distribution_parameter_ = 1.1;
    //item_distribution_parameter_ = 1.0;
    //item_distribution_parameter_ = 0.9;
    //std::cout << item_distribution_parameter_ << std::endl;
    weight = 0.0;
    double weight_weighted_sum = 0.0;
    ItemPos pos = 0;
    expected_lengths_ = std::vector<double>(instance.number_of_items() + 1, 0);
    expected_axle_weights_ = std::vector<std::pair<double, double>>(instance.number_of_items() + 1, {0, 0});
    for (GroupId group_id = instance.number_of_groups() - 1; group_id >= 0; --group_id) {
        //std::cout
        //    << "group " << group_id
        //    << " average_weight " << average_weight << std::endl;
        for (ItemPos p = 0; p < group_number_of_selected_items[group_id]; ++p) {
            double x1 = xi * std::pow(
                    (double)1.0 - std::pow(1.0 - (double)pos / number_of_selected_items_, item_distribution_parameter_),
                    (double)1.0 / item_distribution_parameter_);
            double x2 = xi * std::pow(
                    (double)1.0 - std::pow(1.0 - (double)(pos + 1) / number_of_selected_items_, item_distribution_parameter_),
                    (double)1.0 / item_distribution_parameter_);
            //std::cout << "pos " << pos
            //    << " x1 " << x1
            //    << " x2 " << x2
            //    << " xi " << xi
            //    << std::endl;
            Weight w = group_average_weights[group_id];
            //Weight w = sorted_group_weights[group_id][pos];
            weight += w;
            weight_weighted_sum += ((double)x1 + (double)(x2 - x1) / 2) * w;
            expected_axle_weights_[pos + 1] = bin_type.semi_trailer_truck_data.compute_axle_weights(
                        weight_weighted_sum, weight);
            expected_lengths_[pos + 1] = x2;
            //std::cout << "pos " << pos
            //    << " expected_axle_weight " << expected_axle_weights_[pos].first
            //    << " expected_length " << expected_lengths_[pos]
            //    << std::endl;
            pos++;
        }
    }
    for (; pos < instance.number_of_items(); ++pos) {
        expected_lengths_[pos] = xi;
        expected_axle_weights_[pos] = {
            bin_type.semi_trailer_truck_data.middle_axle_maximum_weight,
            bin_type.semi_trailer_truck_data.rear_axle_maximum_weight};
    }
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// children ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<BranchingScheme::Node> BranchingScheme::child(
        const std::shared_ptr<Node>& pfather,
        const Insertion& insertion) const
{
    const Node& father = *pfather;
    auto pnode = std::shared_ptr<Node>(new BranchingScheme::Node());
    Node& node = static_cast<Node&>(*pnode);

    node.father = pfather;

    node.item_type_id = insertion.item_type_id;
    node.rotation = insertion.rotation;
    node.x = insertion.x;
    node.y = insertion.y;
    node.new_stack = (insertion.uncovered_item_pos == -1);

    // Update number_of_bins and last_bin_direction.
    if (insertion.new_bin > 0) {  // New bin.
        node.number_of_bins = father.number_of_bins + 1;
        node.last_bin_direction = (insertion.new_bin == 1)?
            Direction::X:
            Direction::Y;
    } else {  // Same bin.
        node.number_of_bins = father.number_of_bins;
        node.last_bin_direction = father.last_bin_direction;
    }

    BinPos i = node.number_of_bins - 1;
    Direction o = node.last_bin_direction;
    BinTypeId bin_type_id = instance().bin_type_id(i);
    const BinType& bin_type = instance().bin_type(bin_type_id);

    const ItemType& item_type = instance().item_type(insertion.item_type_id);
    Length xj = instance().x(item_type, insertion.rotation, o);
    Length yj = instance().y(item_type, insertion.rotation, o);
    Length zj = instance().z(item_type, insertion.rotation);
    if (!node.new_stack)
        zj -= item_type.nesting_height;
    Length xs = insertion.x;
    Length xe = insertion.x + xj;
    Length ys = insertion.y;
    Length ye = insertion.y + yj;
    Length yi = instance().y(bin_type, o);
    Length zi = bin_type.box.z;
    Volume item_volume = xj * yj * zj;
    if (insertion.z + zj > zi) {
        throw std::runtime_error(
                "insertion.z: " + std::to_string(insertion.z)
                + "; zj: " + std::to_string(zj)
                + "; zi: " + std::to_string(zi)
                + ".");
    }

    // Update uncovered_items.
    if (!node.new_stack) {
        node.uncovered_items = father.uncovered_items;
        node.uncovered_items[insertion.uncovered_item_pos].item_type_ids.push_back(insertion.item_type_id);
        node.uncovered_items[insertion.uncovered_item_pos].ze += zj;
        node.uncovered_items[insertion.uncovered_item_pos].weight += item_type.weight;
        node.uncovered_items[insertion.uncovered_item_pos].maximum_number_of_items = std::min(
                father.uncovered_items[insertion.uncovered_item_pos].maximum_number_of_items,
                item_type.maximum_stackability);
        node.uncovered_items[insertion.uncovered_item_pos].remaiing_weight = std::min(
                father.uncovered_items[insertion.uncovered_item_pos].remaiing_weight - item_type.weight,
                item_type.maximum_weight_above);
    } else if (insertion.new_bin > 0) {  // New bin.
        if (ys > 0) {
            UncoveredItem uncovered_item;
            uncovered_item.xs = 0;
            uncovered_item.xe = 0;
            uncovered_item.ys = 0;
            uncovered_item.ye = ys;
            node.uncovered_items.push_back(uncovered_item);
        }
        {
            UncoveredItem uncovered_item;
            uncovered_item.item_type_ids.push_back(insertion.item_type_id);
            uncovered_item.weight = item_type.weight;
            uncovered_item.maximum_number_of_items = item_type.maximum_stackability;
            uncovered_item.remaiing_weight = item_type.maximum_weight_above;
            uncovered_item.ze = zj;
            uncovered_item.xs = xs;
            uncovered_item.xe = xe;
            uncovered_item.ys = ys;
            uncovered_item.ye = ye;
            node.uncovered_items.push_back(uncovered_item);
        }
        if (ye < yi) {
            UncoveredItem uncovered_item;
            uncovered_item.xs = 0;
            uncovered_item.xe = 0;
            uncovered_item.ys = ye;
            uncovered_item.ye = yi;
            node.uncovered_items.push_back(uncovered_item);
        }
    } else {  // Same bin.
        for (const UncoveredItem& uncovered_item: father.uncovered_items) {
            if (uncovered_item.ye <= ys) {
                UncoveredItem new_uncovered_item = uncovered_item;
                node.uncovered_items.push_back(new_uncovered_item);
            } else if (uncovered_item.ys <= ys) {
                if (uncovered_item.ys < ys) {
                    UncoveredItem new_uncovered_item = uncovered_item;
                    new_uncovered_item.ye = ys;
                    node.uncovered_items.push_back(new_uncovered_item);
                }

                UncoveredItem new_uncovered_item_2;
                new_uncovered_item_2.item_type_ids.push_back(insertion.item_type_id);
                new_uncovered_item_2.weight = item_type.weight;
                new_uncovered_item_2.maximum_number_of_items = item_type.maximum_stackability;
                new_uncovered_item_2.remaiing_weight = item_type.maximum_weight_above;
                new_uncovered_item_2.ze = zj;
                new_uncovered_item_2.xs = xs;
                new_uncovered_item_2.xe = xe;
                new_uncovered_item_2.ys = ys;
                new_uncovered_item_2.ye = ye;
                node.uncovered_items.push_back(new_uncovered_item_2);

                if (uncovered_item.ye > ye) {
                    UncoveredItem new_uncovered_item = uncovered_item;
                    new_uncovered_item.ys = ye;
                    node.uncovered_items.push_back(new_uncovered_item);
                }
            } else if (uncovered_item.ys >= ye) {
                UncoveredItem new_uncovered_item = uncovered_item;
                node.uncovered_items.push_back(new_uncovered_item);
            } else {
                if (uncovered_item.ye > ye) {
                    UncoveredItem new_uncovered_item = uncovered_item;
                    new_uncovered_item.ys = ye;
                    node.uncovered_items.push_back(new_uncovered_item);
                }
            }
        }
    }

    // Compute item_number_of_copies, number_of_items, items_area,
    // squared_item_volume and profit.
    node.item_number_of_copies = father.item_number_of_copies;
    node.item_number_of_copies[insertion.item_type_id]++;
    node.number_of_items = father.number_of_items + 1;
    node.number_of_stacked_items = (node.new_stack)?
        father.number_of_stacked_items + 1: father.number_of_stacked_items;
    node.item_volume = father.item_volume + item_volume;
    node.item_weight = father.item_weight + item_type.weight;
    node.squared_item_volume = father.squared_item_volume + item_volume * item_volume;
    node.profit = father.profit + item_type.profit;
    node.guide_item_volume = father.guide_item_volume
        + item_volume * (item_type.group_id + 1);
    node.guide_profit = father.guide_profit + (item_type.group_id + 1);
    node.groups = father.groups;

    if (insertion.new_bin != 0) {
        for (GroupId group_id = 0; group_id < instance().number_of_groups(); ++group_id) {
            node.groups[group_id].last_bin_weight = 0;
            node.groups[group_id].last_bin_weight_weighted_sum = 0;
        }
    }
    for (GroupId group_id = 0; group_id <= item_type.group_id; ++group_id) {
        node.groups[group_id].last_bin_weight += item_type.weight;
        node.groups[group_id].last_bin_weight_weighted_sum
            += ((double)xs + (double)(xe - xs) / 2) * item_type.weight;
    }

    for (GroupId group_id = 0; group_id < instance().number_of_groups(); ++group_id) {
        if (!instance().check_weight_constraints(group_id))
            continue;
        if (node.groups[group_id].last_bin_weight == 0)
            continue;
        std::pair<double, double> axle_weights = bin_type.semi_trailer_truck_data.compute_axle_weights(
                node.groups[group_id].last_bin_weight_weighted_sum, node.groups[group_id].last_bin_weight);
        if (axle_weights.first > bin_type.semi_trailer_truck_data.middle_axle_maximum_weight * PSTOL)
            node.middle_axle_overweight += axle_weights.first - bin_type.semi_trailer_truck_data.middle_axle_maximum_weight;
        if (axle_weights.second > bin_type.semi_trailer_truck_data.rear_axle_maximum_weight * PSTOL)
            node.rear_axle_overweight += axle_weights.second - bin_type.semi_trailer_truck_data.rear_axle_maximum_weight;
        //if (father.father == nullptr)
        //    std::cout << "j " << node.j
        //        << " group_id " << group_id
        //        << " weight " << node.last_bin_weight[group_id]
        //        << " violation " << node.middle_axle_weight_violation << std::endl;
    }
    std::pair<double, double> axle_weights = bin_type.semi_trailer_truck_data.compute_axle_weights(
            node.groups.front().last_bin_weight_weighted_sum, node.groups.front().last_bin_weight);
    node.last_bin_middle_axle_weight = axle_weights.first;
    node.last_bin_rear_axle_weight = axle_weights.second;

    // Compute current_volume, guide_volume and width using uncovered_items.
    node.xs_max = (insertion.new_bin == 0)?
        std::max(father.xs_max, insertion.x):
        insertion.x;
    node.current_volume = instance_.previous_bin_volume(i);
    node.guide_volume = instance_.previous_bin_volume(i) + node.xs_max * yi * zi;
    for (const UncoveredItem& uncovered_item: node.uncovered_items) {
        node.current_volume += uncovered_item.xs
            * (uncovered_item.ye - uncovered_item.ys)
            * zi;
        node.current_volume += (uncovered_item.xe - uncovered_item.xs)
            * (uncovered_item.ye - uncovered_item.ys)
            * uncovered_item.ze;
        if (node.xe_max < uncovered_item.xe)
            node.xe_max = uncovered_item.xe;
        if (uncovered_item.xe > node.xs_max)
            node.guide_volume += (uncovered_item.xe - node.xs_max)
                * (uncovered_item.ye - uncovered_item.ys)
                * uncovered_item.ze;
    }
    node.waste = node.current_volume - node.item_volume;

    if (instance().unloading_constraint() == rectangle::UnloadingConstraint::IncreasingX
            || instance().unloading_constraint() == rectangle::UnloadingConstraint::IncreasingY) {
        if (node.groups[item_type.group_id].x_min > insertion.x)
            node.groups[item_type.group_id].x_min = insertion.x;
        if (node.groups[item_type.group_id].x_max < insertion.x)
            node.groups[item_type.group_id].x_max = insertion.x;
    }
    node.groups[item_type.group_id].number_of_items++;

    pnode->id = node_id_++;

    //std::cout << "node.number_of_items " << node.number_of_items << std::endl;
    //std::cout << "node.xs_max " << node.xs_max
    //    << " expected_length " << expected_lengths_[node.number_of_items] << std::endl;
    //std::cout << "node.last_bin_middle_axle_weight " << node.last_bin_middle_axle_weight
    //    << " expected_axle_weight " << expected_axle_weights_[node.number_of_items].first << std::endl;

    return pnode;
}

std::vector<std::shared_ptr<BranchingScheme::Node>> BranchingScheme::children(
        const std::shared_ptr<Node>& father) const
{
    auto is = insertions(father);
    std::vector<std::shared_ptr<Node>> cs;
    for (const Insertion& insertion: is) {
        cs.push_back(child(father, insertion));
        //Solution solution = to_solution(cs.back());
        //solution.write("solution_" + std::to_string(father->id) + "_" + std::to_string(cs.back()->id) + "_.csv");
    }

    return cs;
}

std::vector<BranchingScheme::Insertion> BranchingScheme::insertions(
        const std::shared_ptr<Node>& father) const
{
    //std::cout << "insertions " << *father;
    if (leaf(father))
        return {};

    std::vector<Insertion> insertions;

    std::vector<bool> ok(instance_.number_of_item_types(), true);
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        for (ItemTypeId item_type_id_pred: predecessors_[item_type_id])
            if (father->item_number_of_copies[item_type_id_pred]
                    != instance_.item_type(item_type_id_pred).copies)
                ok[item_type_id] = false;
    }

    if (father->number_of_bins > 0) {
        BinTypeId bin_type_id = instance().bin_type_id(father->number_of_bins - 1);
        const BinType& bin_type = instance().bin_type(bin_type_id);

        // Insert above a previous item.
        for (ItemPos uncovered_item_pos = 0;
                uncovered_item_pos < (ItemPos)father->uncovered_items.size();
                ++uncovered_item_pos) {
            if (father->uncovered_items[uncovered_item_pos].item_type_ids.empty())
                continue;
            for (ItemTypeId item_type_id = 0;
                    item_type_id < instance_.number_of_item_types();
                    ++item_type_id) {

                if (!ok[item_type_id])
                    continue;

                const ItemType& item_type = instance_.item_type(item_type_id);
                if (father->item_number_of_copies[item_type_id] == item_type.copies)
                    continue;
                for (int rotation = 0; rotation < 6; ++rotation)
                    if (instance().item_type(item_type_id).can_rotate(rotation))
                        insertion_item_above(
                                father,
                                insertions,
                                item_type_id,
                                rotation,
                                uncovered_item_pos);
            }
        }

        // Insert in the current bin.

        // Items.
        for (ItemPos uncovered_item_pos = 0;
                uncovered_item_pos < (ItemPos)father->uncovered_items.size();
                ++uncovered_item_pos) {
            for (ItemTypeId item_type_id = 0;
                    item_type_id < instance_.number_of_item_types();
                    ++item_type_id) {

                if (!ok[item_type_id])
                    continue;

                const ItemType& item_type = instance_.item_type(item_type_id);
                if (father->item_number_of_copies[item_type_id] == item_type.copies)
                    continue;
                for (int rotation = 0; rotation < 6; ++rotation) {
                    if (!instance().item_type(item_type_id).can_rotate(rotation))
                        continue;
                    insertion_item(
                            father,
                            insertions,
                            item_type_id,
                            rotation,
                            0,  // new_bin
                            uncovered_item_pos,
                            -1);  // defect_id
                    insertion_item_left(
                            father,
                            insertions,
                            item_type_id,
                            rotation,
                            uncovered_item_pos);
                }
            }
        }

        // Defects.
        for (const rectangle::Defect& defect: bin_type.defects) {
            // Check if left of defect is after the uncovered items.
            Length xs = instance().x_start(defect, father->last_bin_direction);
            Length ys = instance().y_start(defect, father->last_bin_direction);
            Length ye = instance().y_end(defect, father->last_bin_direction);
            bool ok2 = true;
            for (const UncoveredItem& uncovered_item: father->uncovered_items) {
                if (uncovered_item.ye < ys)
                    continue;
                if (uncovered_item.ys > ye)
                    continue;
                if (uncovered_item.xe > xs) {
                    ok2 = false;
                    break;
                }
            }
            if (!ok2)
                continue;

            for (ItemTypeId item_type_id = 0;
                    item_type_id < instance_.number_of_item_types();
                    ++item_type_id) {

                if (!ok[item_type_id])
                    continue;

                const ItemType& item_type = instance_.item_type(item_type_id);
                if (father->item_number_of_copies[item_type_id] == item_type.copies)
                    continue;
                for (int rotation = 0; rotation < 6; ++rotation)
                    if (instance().item_type(item_type_id).can_rotate(rotation))
                        insertion_item(
                                father,
                                insertions,
                                item_type_id,
                                rotation,
                                0,  // new_bin
                                -1,  // uncovered_item_pos
                                defect.id);
            }
        }
    }

    // Insert in a new bin.
    if (insertions.empty() && father->number_of_bins < instance().number_of_bins()) {
        BinTypeId bin_type_id = instance().bin_type_id(father->number_of_bins);
        const BinType& bin_type = instance().bin_type(bin_type_id);
        //std::cout << "new bin" << std::endl;

        int new_bin = 0;
        if (parameters_.direction == Direction::X) {
            new_bin = 1;
        } else if (parameters_.direction == Direction::Y) {
            new_bin = 2;
        } else {
            if (bin_type.box.x >= bin_type.box.y) {
                new_bin = 1;
            } else {
                new_bin = 2;
            }
        }

        // Items.
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance_.number_of_item_types();
                ++item_type_id) {
            //std::cout << "item_type_id " << item_type_id << std::endl;

            if (!ok[item_type_id])
                continue;

            const ItemType& item_type = instance_.item_type(item_type_id);
            if (father->item_number_of_copies[item_type_id] == item_type.copies)
                continue;
            for (int rotation = 0; rotation < 6; ++rotation)
                if (instance().item_type(item_type_id).can_rotate(rotation))
                    insertion_item(
                            father,
                            insertions,
                            item_type_id,
                            rotation,
                            new_bin,
                            0,  // uncovered_item_pos
                            -1);  // defect_id
        }

        // Defects.
        for (const rectangle::Defect& defect: bin_type.defects) {
            for (ItemTypeId item_type_id = 0;
                    item_type_id < instance_.number_of_item_types();
                    ++item_type_id) {

                if (!ok[item_type_id])
                    continue;

                const ItemType& item_type = instance_.item_type(item_type_id);
                if (father->item_number_of_copies[item_type_id] == item_type.copies)
                    continue;
                for (int rotation = 0; rotation < 6; ++rotation)
                    if (instance().item_type(item_type_id).can_rotate(rotation))
                        insertion_item(
                                father,
                                insertions,
                                item_type_id,
                                rotation,
                                new_bin,
                                -1,  // uncovered_item_pos
                                defect.id);
            }
        }
    }

    return insertions;
}

void BranchingScheme::insertion_item_above(
        const std::shared_ptr<Node>& father,
        std::vector<Insertion>& insertions,
        ItemTypeId item_type_id,
        int rotation,
        ItemPos uncovered_item_pos) const
{
    const ItemType& item_type = instance_.item_type(item_type_id);
    BinTypeId bin_type_id = instance().bin_type_id(father->number_of_bins - 1);
    const BinType& bin_type = instance().bin_type(bin_type_id);
    Direction o = father->last_bin_direction;
    Length xj = instance().x(item_type, rotation, o);
    Length yj = instance().y(item_type, rotation, o);
    Length zj = instance().z(item_type, rotation) - item_type.nesting_height;
    Length zi = instance().z(bin_type);
    const UncoveredItem& uncovered_item = father->uncovered_items[uncovered_item_pos];

    const ItemType& item_type_below = instance_.item_type(uncovered_item.item_type_ids.back());

    // Check if the item type can be packed above the previous item.
    if (xj != uncovered_item.xe - uncovered_item.xs
            || yj != uncovered_item.ye - uncovered_item.ys) {
        return;
    }
    if (item_type.stackability_id != item_type_below.stackability_id) {
        return;
    }
    // Check bin z.
    if (uncovered_item.ze + zj > zi) {
        return;
    }
    // Check maximum weight.
    double last_bin_weight = father->groups.front().last_bin_weight + item_type.weight;
    if (last_bin_weight > bin_type.maximum_weight * PSTOL) {
        return;
    }
    // Check maximum stack density.
    double stack_density = (double)(uncovered_item.weight + item_type.weight) / xj / yj;
    if (stack_density > bin_type.maximum_stack_density * PSTOL) {
        return;
    }
    // Check maximum stackability.
    ItemPos last_stack_maximum_number_of_items = std::min(
            uncovered_item.maximum_number_of_items,
            item_type.maximum_stackability);
    if ((ItemPos)uncovered_item.item_type_ids.size() + 1 > last_stack_maximum_number_of_items) {
        return;
    }
    // Check maximum weight above.
    if (item_type.weight > uncovered_item.remaiing_weight) {
        return;
    }
    // Check unloading constraints.
    if (item_type.group_id != item_type_below.group_id) {
        return;
    }
    // Check weight constraints.
    Length xs = father->x;
    Length xe = xs + xj;
    for (GroupId group_id = 0; group_id <= item_type.group_id; ++group_id) {
        if (!instance().check_weight_constraints(group_id))
            continue;
        if (father->groups[group_id].number_of_items + 1 == instance().group(group_id).number_of_items) {
            double last_bin_weight
                = father->groups[group_id].last_bin_weight
                + item_type.weight;
            double last_bin_weight_weighted_sum
                = father->groups[group_id].last_bin_weight_weighted_sum
                + ((double)xs + (double)(xe - xs) / 2) * item_type.weight;
            std::pair<double, double> axle_weights = bin_type.semi_trailer_truck_data.compute_axle_weights(
                    last_bin_weight_weighted_sum, last_bin_weight);
            if (axle_weights.first > bin_type.semi_trailer_truck_data.middle_axle_maximum_weight * PSTOL) {
                return;
            }
            if (axle_weights.second > bin_type.semi_trailer_truck_data.rear_axle_maximum_weight * PSTOL) {
                return;
            }
        }
    }

    Insertion insertion;
    insertion.item_type_id = item_type_id;
    insertion.rotation = rotation;
    insertion.x = uncovered_item.xs;
    insertion.y = uncovered_item.ys;
    insertion.z = uncovered_item.ze;
    insertion.uncovered_item_pos = uncovered_item_pos;
    insertion.new_bin = 0;
    insertions.push_back(insertion);
}

void BranchingScheme::insertion_item(
        const std::shared_ptr<Node>& father,
        std::vector<Insertion>& insertions,
        ItemTypeId item_type_id,
        int rotation,
        int8_t new_bin,
        ItemPos uncovered_item_pos,
        DefectId defect_id) const
{
    const ItemType& item_type = instance_.item_type(item_type_id);
    BinTypeId bin_type_id = (new_bin == 0)?
        instance().bin_type_id(father->number_of_bins - 1):
        instance().bin_type_id(father->number_of_bins);
    const BinType& bin_type = instance().bin_type(bin_type_id);
    Direction o = (new_bin == 0)?
        father->last_bin_direction:
        ((new_bin == 1)? Direction::X: Direction::Y);
    Length xj = instance().x(item_type, rotation, o);
    Length yj = instance().y(item_type, rotation, o);
    Length zj = instance().z(item_type, rotation);
    Length xi = instance().x(bin_type, o);
    Length yi = instance().y(bin_type, o);
    Length zi = instance().z(bin_type);
    Length ys;
    if (uncovered_item_pos > 0) {
        ys = father->uncovered_items[uncovered_item_pos].ys;
    } else if (defect_id != -1) {
        ys = instance().y_end(bin_type.defects[defect_id], o);
    } else {  // new bin.
        ys = 0;
    }
    Length ye = ys + yj;
    // Check bin y.
    if (ye > yi) {
        //std::cout << "y" << std::endl;
        return;
    }
    // Check maximum weight.
    double last_bin_weight = (new_bin == 0)?  // tmt
        father->groups.front().last_bin_weight + item_type.weight:
        item_type.weight;
    if (last_bin_weight > bin_type.maximum_weight * PSTOL) {
        //std::cout << "maximum_weight" << std::endl;
        return;
    }
    // Check maximum stack density.
    double stack_density = (double)(item_type.weight) / xj / yj;
    if (stack_density > bin_type.maximum_stack_density * PSTOL) {
        //std::cout << "maximum_stack_density" << std::endl;
        return;
    }
    // Compute xl.
    Length xs = 0;
    if (new_bin == 0) {
        for (const UncoveredItem& uncovered_item: father->uncovered_items) {
            if (uncovered_item.ye <= ys || uncovered_item.ys >= ye)
                continue;
            if (xs < uncovered_item.xe)
                xs = uncovered_item.xe;
        }
    }

    // Check unloading constraints.
    switch (instance().unloading_constraint()) {
    case rectangle::UnloadingConstraint::None: {
        break;
    } case rectangle::UnloadingConstraint::OnlyXMovements: case rectangle::UnloadingConstraint::OnlyYMovements: {
        // Check if an item from the uncovered item with higher group is not
        // blocked by the new item.
        for (const UncoveredItem& uncovered_item: father->uncovered_items) {
            if (uncovered_item.ye <= ys || uncovered_item.ys >= ye)
                continue;
            const ItemType& item_type_pred = instance().item_type(uncovered_item.item_type_ids.back());
            if (item_type.group_id > item_type_pred.group_id)
                return;
        }
        break;
    } case rectangle::UnloadingConstraint::IncreasingX: case rectangle::UnloadingConstraint::IncreasingY: {
        for (GroupId group = item_type.group_id + 1; group < instance().number_of_groups(); ++group)
            if (xs < father->groups[group].x_max)
                return;
        for (GroupId group = 0; group < item_type.group_id; ++group)
            if (xs > father->groups[group].x_min)
                return;
        break;
    }
    }

    // Defects
    // While the item intersects a defect, move it to the right.
    for (;;) {
        bool stop = true;
        for (const rectangle::Defect& defect: bin_type.defects) {
            if (instance().x_start(defect, o) >= xs + xj)
                continue;
            if (xs >= instance().x_end(defect, o))
                continue;
            if (instance().y_start(defect, o) >= ye)
                continue;
            if (ys >= instance().y_end(defect, o))
                continue;
            xs = instance().x_end(defect, o);
            stop = false;
        }
        if (stop)
            break;
    }

    Length xe = xs + xj;
    // Check bin x.
    if (xe > xi)
        return;

    // Check bin z.
    if (zj > zi)
        return;

    if (uncovered_item_pos > 0) {
        if (xe <= father->uncovered_items[uncovered_item_pos - 1].xs)
            return;
    } else if (defect_id != -1) {
        if (xe <= instance().x_start(bin_type.defects[defect_id], o))
            return;
        if (xs >= instance().x_end(bin_type.defects[defect_id], o))
            return;
    }

    // Check weight constraints.
    for (GroupId group_id = 0; group_id <= item_type.group_id; ++group_id) {
        if (!instance().check_weight_constraints(group_id))
            continue;
        if (father->groups[group_id].number_of_items + 1 == instance().group(group_id).number_of_items) {
            double last_bin_weight = (new_bin == 0)?  // tmt
                father->groups[group_id].last_bin_weight + item_type.weight:
                item_type.weight;
            double last_bin_weight_weighted_sum = (new_bin == 0)?
                father->groups[group_id].last_bin_weight_weighted_sum
                + ((double)xs + (double)(xe - xs) / 2) * item_type.weight:
                ((double)xs + (double)(xe - xs) / 2) * item_type.weight;
            std::pair<double, double> axle_weights = bin_type.semi_trailer_truck_data.compute_axle_weights(
                    last_bin_weight_weighted_sum, last_bin_weight);
            if (axle_weights.second > bin_type.semi_trailer_truck_data.rear_axle_maximum_weight * PSTOL)
                return;
            if (axle_weights.first > bin_type.semi_trailer_truck_data.middle_axle_maximum_weight * PSTOL)
                return;
        }
    }

    Insertion insertion;
    insertion.item_type_id = item_type_id;
    insertion.rotation = rotation;
    insertion.x = xs;
    insertion.y = ys;
    insertion.z = 0;
    insertion.uncovered_item_pos = -1;
    insertion.new_bin = new_bin;
    insertions.push_back(insertion);
}

void BranchingScheme::insertion_item_left(
        const std::shared_ptr<Node>& father,
        std::vector<Insertion>& insertions,
        ItemTypeId item_type_id,
        int rotation,
        ItemPos uncovered_item_pos) const
{
    //std::cout << "insertion_item_left " << uncovered_item_pos << std::endl;
    const ItemType& item_type = instance_.item_type(item_type_id);
    BinTypeId bin_type_id = instance().bin_type_id(father->number_of_bins - 1);
    const BinType& bin_type = instance().bin_type(bin_type_id);
    Direction o = father->last_bin_direction;
    Length xj = instance().x(item_type, rotation, o);
    Length yj = instance().y(item_type, rotation, o);
    Length zj = instance().z(item_type, rotation);
    Length xi = instance().x(bin_type, o);
    Length yi = instance().y(bin_type, o);
    Length zi = instance().z(bin_type);
    Length ys = father->uncovered_items[uncovered_item_pos].ye;
    Length xs = father->uncovered_items[uncovered_item_pos].xe;
    Length ye = ys + yj;
    // Check bin y.
    if (ye > yi) {
        //std::cout << "y" << std::endl;
        return;
    }
    // Check maximum weight.
    double last_bin_weight = father->groups.front().last_bin_weight + item_type.weight;
    if (last_bin_weight > bin_type.maximum_weight * PSTOL) {
        //std::cout << "maximum_weight" << std::endl;
        return;
    }
    // Check maximum stack density.
    double stack_density = (double)(item_type.weight) / xj / yj;
    if (stack_density > bin_type.maximum_stack_density * PSTOL) {
        //std::cout << "maximum_stack_density" << std::endl;
        return;
    }
    // Check intersection with other items.
    for (const UncoveredItem& uncovered_item: father->uncovered_items) {
        if (uncovered_item.ye <= ys || uncovered_item.ys >= ye)
            continue;
        if (xs < uncovered_item.xe) {
            //std::cout << "intersection " << uncovered_item << std::endl;
            return;
        }
    }

    // Check unloading constraints.
    switch (instance().unloading_constraint()) {
    case rectangle::UnloadingConstraint::None: {
        break;
    } case rectangle::UnloadingConstraint::OnlyXMovements: case rectangle::UnloadingConstraint::OnlyYMovements: {
        // Check if an item from the uncovered item with higher group is not
        // blocked by the new item.
        for (const UncoveredItem& uncovered_item: father->uncovered_items) {
            if (uncovered_item.ye <= ys || uncovered_item.ys >= ye)
                continue;
            const ItemType& item_type_pred = instance().item_type(uncovered_item.item_type_ids.back());
            if (item_type.group_id > item_type_pred.group_id)
                return;
        }
        break;
    } case rectangle::UnloadingConstraint::IncreasingX: case rectangle::UnloadingConstraint::IncreasingY: {
        for (GroupId group = item_type.group_id + 1; group < instance().number_of_groups(); ++group)
            if (xs < father->groups[group].x_max)
                return;
        for (GroupId group = 0; group < item_type.group_id; ++group)
            if (xs > father->groups[group].x_min)
                return;
        break;
    }
    }

    // Defects
    // While the item intersects a defect, move it to the right.
    for (const rectangle::Defect& defect: bin_type.defects) {
        if (instance().x_start(defect, o) >= xs + xj)
            continue;
        if (xs >= instance().x_end(defect, o))
            continue;
        if (instance().y_start(defect, o) >= ye)
            continue;
        if (ys >= instance().y_end(defect, o))
            continue;
        //std::cout << "defect " << defect << std::endl;
        return;
    }

    Length xe = xs + xj;
    // Check bin x.
    if (xe > xi) {
        //std::cout << "length" << std::endl;
        return;
    }

    // Check bin z.
    if (zj > zi) {
        //std::cout << "height" << std::endl;
        return;
    }

    if (uncovered_item_pos > 0) {
        if (xe <= father->uncovered_items[uncovered_item_pos - 1].xs) {
            //std::cout << "toto" << std::endl;
            return;
        }
    }

    // Check weight constraints.
    for (GroupId group_id = 0; group_id <= item_type.group_id; ++group_id) {
        if (!instance().check_weight_constraints(group_id))
            continue;
        if (father->groups[group_id].number_of_items + 1 == instance().group(group_id).number_of_items) {
            double last_bin_weight = father->groups[group_id].last_bin_weight + item_type.weight;
            double last_bin_weight_weighted_sum
                = father->groups[group_id].last_bin_weight_weighted_sum
                + ((double)xs + (double)(xe - xs) / 2) * item_type.weight;
            std::pair<double, double> axle_weights = bin_type.semi_trailer_truck_data.compute_axle_weights(
                    last_bin_weight_weighted_sum, last_bin_weight);
            if (axle_weights.second > bin_type.semi_trailer_truck_data.rear_axle_maximum_weight * PSTOL) {
                //std::cout << "rear_axle_weight" << std::endl;
                return;
            }
            if (axle_weights.first > bin_type.semi_trailer_truck_data.middle_axle_maximum_weight * PSTOL) {
                //std::cout << "middle_axle_weight" << std::endl;
                return;
            }
        }
    }

    Insertion insertion;
    insertion.item_type_id = item_type_id;
    insertion.rotation = rotation;
    insertion.x = xs;
    insertion.y = ys;
    insertion.z = 0;
    insertion.uncovered_item_pos = -1;
    insertion.new_bin = 0;
    insertions.push_back(insertion);
    //std::cout << "success" << std::endl;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

const std::shared_ptr<BranchingScheme::Node> BranchingScheme::root() const
{
    BranchingScheme::Node node;
    node.item_number_of_copies = std::vector<ItemPos>(instance_.number_of_item_types(), 0);
    node.groups = std::vector<NodeGroup>(instance().number_of_groups());
    for (GroupId group_id = 0; group_id < instance().number_of_groups(); ++group_id) {
        node.groups[group_id].x_min = std::numeric_limits<Length>::max();
    }
    node.id = node_id_++;
    return std::shared_ptr<Node>(new BranchingScheme::Node(node));
}

bool BranchingScheme::better(
        const std::shared_ptr<Node>& node_1,
        const std::shared_ptr<Node>& node_2) const
{
    if (node_1->middle_axle_overweight > 0
            || node_1->rear_axle_overweight > 0)
        return false;
    if (node_2->middle_axle_overweight > 0
            || node_2->rear_axle_overweight > 0)
        return true;
    switch (instance_.objective()) {
    case Objective::Default: {
        if (node_2->profit > node_1->profit)
            return false;
        if (node_2->profit < node_1->profit)
            return true;
        return node_2->waste > node_1->waste;
    } case Objective::BinPacking: case Objective::VariableSizedBinPacking: {
        if (!leaf(node_1))
            return false;
        if (!leaf(node_2))
            return true;
        return node_2->number_of_bins > node_1->number_of_bins;
    } case Objective::BinPackingWithLeftovers: {
        if (!leaf(node_1))
            return false;
        if (!leaf(node_2))
            return true;
        return node_2->waste > node_1->waste;
    } case Objective::OpenDimensionX: {
        if (!leaf(node_1))
            return false;
        if (!leaf(node_2))
            return true;
        return node_2->xe_max > node_1->xe_max;
    } case Objective::OpenDimensionY: {
        if (!leaf(node_1))
            return false;
        if (!leaf(node_2))
            return true;
        return node_2->xe_max > node_1->xe_max;
    } case Objective::Knapsack: {
        return node_2->profit < node_1->profit;
    } default: {
        std::stringstream ss;
        ss << "Branching scheme 'boxstacks::BranchingScheme'"
            << "does not support objective '" << instance_.objective() << "'.";
        throw std::logic_error(ss.str());
        return false;
    }
    }
}

bool BranchingScheme::bound(
        const std::shared_ptr<Node>& node_1,
        const std::shared_ptr<Node>& node_2) const
{
    switch (instance_.objective()) {
    case Objective::Default: {
        if (!leaf(node_2)) {
            return (ubkp(*node_1) <= node_2->profit);
        } else {
            if (ubkp(*node_1) != node_2->profit)
                return (ubkp(*node_1) <= node_2->profit);
            return node_1->waste >= node_2->waste;
        }
    } case Objective::BinPacking: case Objective::VariableSizedBinPacking: {
        if (!leaf(node_2))
            return false;
        BinPos bin_pos = -1;
        Area a = instance_.item_volume() + node_1->waste;
        while (a > 0) {
            bin_pos++;
            if (bin_pos >= instance_.number_of_bins())
                return true;
            BinTypeId bin_type_id = instance().bin_type_id(bin_pos);
            const BinType& bin_type = instance().bin_type(bin_type_id);
            a -= bin_type.volume();
        }
        return (bin_pos + 1 >= node_2->number_of_bins);
    } case Objective::BinPackingWithLeftovers: {
        if (!leaf(node_2))
            return false;
        return node_1->waste >= node_2->waste;
    } case Objective::Knapsack: {
        if (leaf(node_2))
            return true;
        return false;
    } case Objective::OpenDimensionX: case Objective::OpenDimensionY: {
        if (!leaf(node_2))
            return false;
        return (std::max(
                    node_1->xe_max,
                    node_1->waste + instance_.item_volume() - 1)
                / (instance().x(instance_.bin_type(0), Direction::X) + 1))
            >= node_2->xe_max;
    } default: {
        std::stringstream ss;
        ss << "Branching scheme 'boxstacks::BranchingScheme'"
            << "does not support objective '" << instance_.objective() << "'.";
        throw std::logic_error(ss.str());
        return false;
    }
    }
}

////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////// export ////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

Solution BranchingScheme::to_solution(
        const std::shared_ptr<Node>& node) const
{
    std::vector<std::shared_ptr<Node>> descendents;
    for (std::shared_ptr<Node> current_node = node;
            current_node->father != nullptr;
            current_node = current_node->father) {
        descendents.push_back(current_node);
    }
    std::reverse(descendents.begin(), descendents.end());

    Solution solution(instance());
    BinPos bin_pos = -1;
    std::map<std::tuple<BinPos, Length, Length>, StackId> coord2stack;
    for (auto current_node: descendents) {
        if (current_node->number_of_bins > solution.number_of_bins())
            bin_pos = solution.add_bin(
                    instance().bin_type_id(current_node->number_of_bins - 1),
                    1);
        const ItemType& item_type = instance().item_type(current_node->item_type_id);
        Length xj = instance().x(
                item_type,
                current_node->rotation,
                current_node->last_bin_direction);
        Length yj = instance().y(
                item_type,
                current_node->rotation,
                current_node->last_bin_direction);
        //std::cout
        //    << " item_type_id " << current_node->item_type_id
        //    << " x " << current_node->x
        //    << " y " << current_node->y
        //    << " z " << current_node->z
        //    << " xj " << xj
        //    << " yj " << yj
        //    << " rot " << current_node->rotation
        //    << " o " << current_node->last_bin_direction
        //    << std::endl;
        if (current_node->new_stack) {
            StackId stack_id = -1;
            if (current_node->last_bin_direction == Direction::X) {
                stack_id = solution.add_stack(
                        bin_pos,
                        current_node->x,
                        current_node->x + xj,
                        current_node->y,
                        current_node->y + yj);
            } else {
                stack_id = solution.add_stack(
                        bin_pos,
                        current_node->y,
                        current_node->y + yj,
                        current_node->x,
                        current_node->x + xj);
            }
            coord2stack[{bin_pos, current_node->x, current_node->y}] = stack_id;
        }
        StackId stack_id = coord2stack[{bin_pos, current_node->x, current_node->y}];
        solution.add_item(
                bin_pos,
                stack_id,
                current_node->item_type_id,
                current_node->rotation);
    }
    //if (!solution.feasible()) {
    //    std::cout.precision(std::numeric_limits<double>::max_digits10);
    //    for (GroupId group_id = 0; group_id < instance().number_of_groups(); ++group_id) {
    //        const BinType& bin_type = instance().bin_type(solution.bin(0).bin_type_id);
    //        auto axle_weights = bin_type.semi_trailer_truck_data.compute_axle_weights(
    //                solution.bin(0).weight_weighted_sum[group_id],
    //                solution.bin(0).weight[group_id]);
    //        std::cout << " group_id " << group_id << std::endl;
    //        std::cout << "wws node " << node->groups[group_id].last_bin_weight_weighted_sum
    //            << " sol " << solution.bin(0).weight_weighted_sum[group_id]
    //            << std::endl;
    //        std::cout << "weight node " << node->groups[group_id].last_bin_weight
    //            << " sol " << solution.bin(0).weight[group_id]
    //            << " / " << bin_type.maximum_weight
    //            << std::endl;
    //        std::cout << "middle " << axle_weights.first
    //            << " / " << bin_type.semi_trailer_truck_data.middle_axle_maximum_weight
    //            << std::endl;
    //        std::cout << "rear " << axle_weights.second
    //            << " / " << bin_type.semi_trailer_truck_data.rear_axle_maximum_weight
    //            << std::endl;
    //    }
    //    //throw std::runtime_error("to_solution: infeasible solution");
    //}
    return solution;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

std::ostream& packingsolver::boxstacks::operator<<(
        std::ostream& os,
        const BranchingScheme::UncoveredItem& uncovered_item)
{
    os << "items";
    for (ItemTypeId item_type_id: uncovered_item.item_type_ids)
        os << " " << item_type_id;
    os << " xs " << uncovered_item.xs
        << " xe " << uncovered_item.xe
        << " ys " << uncovered_item.ys
        << " ye " << uncovered_item.ye
        ;
    return os;
}

bool BranchingScheme::UncoveredItem::operator==(
        const UncoveredItem& uncovered_item) const
{
    return ((item_type_ids == uncovered_item.item_type_ids)
            && (xs == uncovered_item.xs)
            && (xe == uncovered_item.xe)
            && (ys == uncovered_item.ys)
            && (ye == uncovered_item.ye)
            );
}

bool BranchingScheme::Insertion::operator==(
        const Insertion& insertion) const
{
    return ((item_type_id == insertion.item_type_id)
            && (rotation == insertion.rotation)
            && (new_bin == insertion.new_bin)
            && (x == insertion.x)
            && (y == insertion.y)
            && (z == insertion.z)
            );
}

std::ostream& packingsolver::boxstacks::operator<<(
        std::ostream& os,
        const BranchingScheme::Insertion& insertion)
{
    os << "item_type_id " << insertion.item_type_id
        << " rotation " << insertion.rotation
        << " new_bin " << (int)insertion.new_bin
        << " x " << insertion.x
        << " y " << insertion.y
        << " z " << insertion.z
        ;
    return os;
}

std::ostream& packingsolver::boxstacks::operator<<(
        std::ostream& os,
        const BranchingScheme::Node& node)
{
    os << "id " << node.id
        << " number_of_items " << node.number_of_items
        << " number_of_bins " << node.number_of_bins
        << std::endl;
    os << "item_volume " << node.item_volume
        << " current_volume " << node.current_volume
        << std::endl;
    os << "waste " << node.waste
        << " profit " << node.profit
        << std::endl;
    os << "item_type_id " << node.item_type_id << std::endl;

    // item_number_of_copies
    os << "item_number_of_copies" << std::flush;
    for (ItemPos j_pos: node.item_number_of_copies)
        os << " " << j_pos;
    os << std::endl;

    return os;
}

