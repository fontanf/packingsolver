#include "packingsolver/rectangle/instance_builder.hpp"
#include "rectangle/onedimentional_contiguity/sweep.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

#include <functional>
#include <random>
#include <sstream>

using namespace packingsolver;
using namespace packingsolver::rectangle;
using namespace packingsolver::rectangle::onedimentional_contiguity;
namespace fs = boost::filesystem;

namespace
{

SweepOutput run(
        const Instance& instance,
        Counter maximum_number_of_operations = -1)
{
    SweepParameters parameters;
    parameters.maximum_number_of_operations = maximum_number_of_operations;
    return sweep(instance, parameters);
}

/** Item of a random instance. */
struct RandomItem
{
    Length width;
    Length height;
    bool oriented;
};

Instance build_instance(
        Length bin_width,
        Length bin_height,
        const std::vector<RandomItem>& items)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Feasibility);
    instance_builder.add_bin_type(bin_width, bin_height);
    for (const RandomItem& item: items)
        instance_builder.add_item_type(item.width, item.height, item.oriented);
    return instance_builder.build();
}

std::string to_string(
        Length bin_width,
        Length bin_height,
        const std::vector<RandomItem>& items)
{
    std::stringstream ss;
    ss << "bin " << bin_width << "x" << bin_height << ", items";
    for (const RandomItem& item: items)
        ss << " " << item.width << "x" << item.height << (item.oriented? "": "r");
    return ss.str();
}

/**
 * Return 'true' iff the relaxation is feasible, by enumerating all the
 * integer positions of the items. An item which can be rotated has its
 * smallest length as width and height, as in 'sweep'.
 */
bool brute_force_feasible(
        Length bin_width,
        Length bin_height,
        const std::vector<RandomItem>& items)
{
    std::vector<Length> widths;
    std::vector<Length> heights;
    for (const RandomItem& item: items) {
        if (item.oriented) {
            widths.push_back(item.width);
            heights.push_back(item.height);
        } else {
            widths.push_back((std::min)(item.width, item.height));
            heights.push_back((std::min)(item.width, item.height));
        }
    }
    std::vector<Length> loads(bin_width, 0);
    std::function<bool(size_t)> search = [&](size_t item_pos)
    {
        if (item_pos == widths.size())
            return true;
        for (Length position = 0; position + widths[item_pos] <= bin_width; ++position) {
            bool fits = true;
            for (Length column = position; column < position + widths[item_pos]; ++column)
                if (loads[column] + heights[item_pos] > bin_height)
                    fits = false;
            if (!fits)
                continue;
            for (Length column = position; column < position + widths[item_pos]; ++column)
                loads[column] += heights[item_pos];
            bool found = search(item_pos + 1);
            for (Length column = position; column < position + widths[item_pos]; ++column)
                loads[column] -= heights[item_pos];
            if (found)
                return true;
        }
        return false;
    };
    return search(0);
}

}

TEST(RectangleOnedimentionalContiguitySweep, UnsupportedObjectiveThrows)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Knapsack);
    instance_builder.add_bin_type(10, 10);
    instance_builder.add_item_type(5, 5, true);
    Instance instance = instance_builder.build();

    EXPECT_THROW(run(instance), std::invalid_argument);
}

TEST(RectangleOnedimentionalContiguitySweep, ItemTooBig)
{
    // Bin 10x10, item 5x11.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Feasibility);
    instance_builder.add_bin_type(10, 10);
    instance_builder.add_item_type(5, 11, true);
    Instance instance = instance_builder.build();

    EXPECT_EQ(run(instance).status, OnedimensionalContiguityResult::Status::Infeasible);
}

TEST(RectangleOnedimentionalContiguitySweep, ItemsTooHighToShareAColumn)
{
    // Bin 10x10, items 6x6 (2 copies): they can't share a column, and don't
    // fit side by side.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Feasibility);
    instance_builder.add_bin_type(10, 10);
    ItemTypeId item_type_id = instance_builder.add_item_type(6, 6, true);
    instance_builder.set_item_type_copies(item_type_id, 2);
    Instance instance = instance_builder.build();

    EXPECT_EQ(run(instance).status, OnedimensionalContiguityResult::Status::Infeasible);
}

TEST(RectangleOnedimentionalContiguitySweep, HifiA2Subproblem)
{
    // Subproblem of the Benders decomposition on the 'hifi1997a/A2' knapsack
    // instance: the area of the items fits in the bin, but their widths and
    // heights can't be combined to fill the columns.
    fs::path directory = fs::path("data") / "rectangle" / "tests" / "feasibility_hifi_a2_subproblem_infeasible";
    InstanceBuilder instance_builder;
    instance_builder.read_item_types((directory / "items.csv").string());
    instance_builder.read_bin_types((directory / "bins.csv").string());
    instance_builder.read_parameters((directory / "parameters.csv").string());
    Instance instance = instance_builder.build();

    EXPECT_EQ(run(instance).status, OnedimensionalContiguityResult::Status::Infeasible);
}

TEST(RectangleOnedimentionalContiguitySweep, RandomInstancesMatchBruteForce)
{
    // Random small instances, with item areas summing to between 75% and
    // 100% of the area of the bin: the sweep must conclude, as the
    // enumeration of all the positions.
    std::mt19937_64 generator(0);
    std::uniform_int_distribution<Length> bin_distribution(4, 9);
    std::uniform_real_distribution<double> fill_distribution(0.75, 1.0);
    std::bernoulli_distribution rotation_distribution(0.25);
    Counter number_of_feasible_instances = 0;
    Counter number_of_infeasible_instances = 0;
    for (Counter instance_id = 0; instance_id < 2000; ++instance_id) {
        Length bin_width = bin_distribution(generator);
        Length bin_height = bin_distribution(generator);
        Area target_area = fill_distribution(generator) * bin_width * bin_height;
        std::uniform_int_distribution<Length> width_distribution(1, (2 * bin_width + 2) / 3);
        std::uniform_int_distribution<Length> height_distribution(1, (2 * bin_height + 2) / 3);
        std::vector<RandomItem> items;
        Area area = 0;
        while (items.size() < 7) {
            RandomItem item;
            item.width = width_distribution(generator);
            item.height = height_distribution(generator);
            item.oriented = !rotation_distribution(generator);
            if (area + item.width * item.height > target_area)
                break;
            area += item.width * item.height;
            items.push_back(item);
        }
        if (items.empty())
            continue;

        bool feasible = brute_force_feasible(bin_width, bin_height, items);
        if (feasible) {
            number_of_feasible_instances++;
        } else {
            number_of_infeasible_instances++;
        }
        SweepOutput output = run(build_instance(bin_width, bin_height, items));
        EXPECT_EQ(
                output.status,
                (feasible)?
                OnedimensionalContiguityResult::Status::Feasible:
                OnedimensionalContiguityResult::Status::Infeasible)
            << to_string(bin_width, bin_height, items);
        if (testing::Test::HasFailure())
            break;
    }
    EXPECT_GE(number_of_feasible_instances, 100);
    EXPECT_GE(number_of_infeasible_instances, 100);
}

TEST(RectangleOnedimentionalContiguitySweep, PackedInstancesNotInfeasible)
{
    // Random instances built by cutting the bin recursively, so that their
    // items fill the bin: the sweep must never conclude that they are
    // infeasible. Filling the bin exactly can take many operations, so the
    // search is limited and may be inconclusive.
    std::mt19937_64 generator(0);
    std::uniform_int_distribution<Length> bin_distribution(10, 40);
    std::bernoulli_distribution rotation_distribution(0.25);
    for (Counter instance_id = 0; instance_id < 500; ++instance_id) {
        Length bin_width = bin_distribution(generator);
        Length bin_height = bin_distribution(generator);
        std::vector<RandomItem> items;
        std::function<void(Length, Length, int)> cut = [&](Length width, Length height, int depth)
        {
            bool cut_width = (width >= 2 && (height < 2 || std::bernoulli_distribution(0.5)(generator)));
            bool cut_height = (!cut_width && height >= 2);
            if (depth >= 4 || (!cut_width && !cut_height)) {
                items.push_back({width, height, !rotation_distribution(generator)});
                return;
            }
            if (cut_width) {
                Length position = std::uniform_int_distribution<Length>(1, width - 1)(generator);
                cut(position, height, depth + 1);
                cut(width - position, height, depth + 1);
            } else {
                Length position = std::uniform_int_distribution<Length>(1, height - 1)(generator);
                cut(width, position, depth + 1);
                cut(width, height - position, depth + 1);
            }
        };
        cut(bin_width, bin_height, 0);
        SweepOutput output = run(build_instance(bin_width, bin_height, items), 1000000);
        EXPECT_NE(output.status, OnedimensionalContiguityResult::Status::Infeasible)
            << to_string(bin_width, bin_height, items);
        if (testing::Test::HasFailure())
            break;
    }
}
