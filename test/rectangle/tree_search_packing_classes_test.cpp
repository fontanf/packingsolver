#include "packingsolver/rectangle/instance_builder.hpp"
#include "rectangle/tree_search_packing_classes.hpp"
#include "rectangle/solution_builder.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

#include <algorithm>
#include <functional>
#include <random>
#include <set>
#include <sstream>

using namespace packingsolver;
using namespace packingsolver::rectangle;
namespace fs = boost::filesystem;

namespace
{

TreeSearchPackingClassesOutput run(const Instance& instance)
{
    TreeSearchPackingClassesParameters parameters;
    parameters.verbosity_level = 0;
    return tree_search_packing_classes(instance, parameters);
}

ItemTypeId add_item_type(
        InstanceBuilder& instance_builder,
        Length width,
        Length height,
        ItemPos copies,
        bool oriented)
{
    ItemTypeId item_type_id = instance_builder.add_item_type(width, height, oriented);
    instance_builder.set_item_type_copies(item_type_id, copies);
    return item_type_id;
}

/** Check that the output contains a packing of all the items. */
void expect_packing(
        const Instance& instance,
        const TreeSearchPackingClassesOutput& output)
{
    EXPECT_FALSE(output.is_proven_infeasible);
    const Solution& solution = output.solution_pool.best();
    EXPECT_TRUE(solution.feasible());
    EXPECT_EQ(solution.number_of_items(), instance.number_of_items());
}

}

TEST(RectangleTreeSearchPackingClasses, UnsupportedObjectiveThrows)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Knapsack);
    instance_builder.add_bin_type(10, 10);
    add_item_type(instance_builder, 5, 5, 1, true);
    Instance instance = instance_builder.build();

    EXPECT_THROW(run(instance), std::invalid_argument);
}

TEST(RectangleTreeSearchPackingClasses, SeveralBinTypesThrows)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Feasibility);
    instance_builder.add_bin_type(10, 10);
    instance_builder.add_bin_type(6, 6);
    add_item_type(instance_builder, 5, 5, 1, true);
    Instance instance = instance_builder.build();

    EXPECT_THROW(run(instance), std::invalid_argument);
}

TEST(RectangleTreeSearchPackingClasses, BinTypeCopiesNotOneThrows)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Feasibility);
    BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10);
    instance_builder.set_bin_type_copies(bin_type_id, 2);
    add_item_type(instance_builder, 5, 5, 1, true);
    Instance instance = instance_builder.build();

    EXPECT_THROW(run(instance), std::invalid_argument);
}

TEST(RectangleTreeSearchPackingClasses, MaximumWeightThrows)
{
    // The weight of the items packed is not checked by the search.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Feasibility);
    BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10);
    instance_builder.set_bin_type_maximum_weight(bin_type_id, 10);
    ItemTypeId item_type_id = add_item_type(instance_builder, 5, 5, 1, true);
    instance_builder.set_item_type_weight(item_type_id, 1);
    Instance instance = instance_builder.build();

    EXPECT_THROW(run(instance), std::invalid_argument);
}

struct RectangleTreeSearchPackingClassesTestParams
{
    fs::path items_path;
    fs::path bins_path;
    fs::path parameters_path;

    // Path to a reference solution certificate. Empty for an instance
    // expected to be proven infeasible.
    fs::path certificate_path;
};

inline std::ostream& operator<<(
        std::ostream& os,
        const RectangleTreeSearchPackingClassesTestParams& test_params)
{
    os << test_params.items_path;
    return os;
}

class RectangleTreeSearchPackingClassesTest: public testing::TestWithParam<RectangleTreeSearchPackingClassesTestParams> { };

TEST_P(RectangleTreeSearchPackingClassesTest, RectangleTreeSearchPackingClasses)
{
    RectangleTreeSearchPackingClassesTestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    instance_builder.read_item_types(test_params.items_path.string());
    instance_builder.read_bin_types(test_params.bins_path.string());
    instance_builder.read_parameters(test_params.parameters_path.string());
    Instance instance = instance_builder.build();

    TreeSearchPackingClassesOutput output = run(instance);

    if (test_params.certificate_path.empty()) {
        EXPECT_EQ(output.is_proven_infeasible, true);
    } else {
        SolutionBuilder solution_builder(instance);
        solution_builder.read(test_params.certificate_path.string());
        Solution solution = solution_builder.build();
        std::cout << std::endl
            << "Reference solution" << std::endl
            << "------------------" << std::endl;
        solution.format(std::cout);

        EXPECT_EQ(output.is_proven_infeasible, false);
        EXPECT_EQ(!(output.solution_pool.best() < solution), true);
        EXPECT_EQ(!(solution < output.solution_pool.best()), true);
    }
}

namespace
{

/**
 * Parameters of a test on the instance of a directory of 'data/rectangle/
 * tests'; the instance is expected to be infeasible iff the directory has no
 * 'solution.csv'.
 */
RectangleTreeSearchPackingClassesTestParams test_params(
        const std::string& name)
{
    fs::path directory = fs::path("data") / "rectangle" / "tests" / name;
    fs::path certificate_path = directory / "solution.csv";
    return {
        directory / "items.csv",
        directory / "bins.csv",
        directory / "parameters.csv",
        (fs::exists(certificate_path))? certificate_path: fs::path(""),
    };
}

}

INSTANTIATE_TEST_SUITE_P(
        Rectangle,
        RectangleTreeSearchPackingClassesTest,
        testing::ValuesIn(std::vector<RectangleTreeSearchPackingClassesTestParams>{
            // 4 copies of a 5x5 item tile a 10x10 bin.
            test_params("feasibility_exact_tiling_feasible"),
            // Items 6x4, 4x6, 4x4 and 6x6 tile a 10x10 bin, each one at a
            // corner.
            test_params("feasibility_four_corners_feasible"),
            // The item (20x20) doesn't fit in the bin (10x10).
            test_params("feasibility_item_too_big_infeasible"),
            // Bin 10x3, item 3x10, which only fits rotated.
            test_params("feasibility_rotation_needed_feasible"),
            // Two items 6x6 in a 10x10 bin: their area fits, but they
            // overlap along both axes.
            test_params("feasibility_items_overlapping_along_both_axes_infeasible"),
            // Bin 10x4, items 4x3 (3 copies): their area fits, but they
            // pairwise overlap along y, so they would have to be side by
            // side along x (12 > 10).
            test_params("feasibility_long_chain_infeasible"),
            // Bin 6x5, items 1x3 and 4x4 (rotatable), 3x1 and 1x3 (2
            // copies): the packing is lost if the propagation on the
            // induced cycles of length 4 makes a pair overlap instead of
            // being disjoint.
            test_params("feasibility_induced_cycle_feasible"),
            // Fixtures of 'benders_decomposition_contiguity_test.cpp'.
            test_params("feasibility_two_tight_column_stacks"),
            test_params("feasibility_fuzzer_found_column_partition"),
            // Subproblems of the Benders decomposition on the
            // 'christofides1977/cgcut2' and 'cung2000/CHL2' knapsack
            // instances.
            test_params("feasibility_cgcut2_subproblem_feasible"),
            test_params("feasibility_cgcut2_subproblem_infeasible"),
            test_params("feasibility_chl2_subproblem_infeasible"),
        }));

TEST(RectangleTreeSearchPackingClasses, NotEligibleItemInfeasible)
{
    // Bin 10x10 supporting eligibility 0, item with eligibility 1.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Feasibility);
    BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10);
    instance_builder.add_bin_type_eligibility(bin_type_id, 0);
    ItemTypeId item_type_id = add_item_type(instance_builder, 5, 5, 1, true);
    instance_builder.set_item_type_eligibility(item_type_id, 1);
    Instance instance = instance_builder.build();

    EXPECT_TRUE(run(instance).is_proven_infeasible);
}

namespace
{

/** Orientations (width, height) of an item. */
using BruteForceItem = std::vector<std::pair<Length, Length>>;

/**
 * Sums of lengths of subsets of items, each one in one of its orientations,
 * at most 'capacity'.
 */
std::set<Length> subset_sums(
        const std::vector<std::vector<Length>>& lengths,
        Length capacity)
{
    std::set<Length> sums = {0};
    for (const std::vector<Length>& item_lengths: lengths) {
        std::set<Length> new_sums = sums;
        for (Length sum: sums)
            for (Length length: item_lengths)
                if (sum + length <= capacity)
                    new_sums.insert(sum + length);
        sums = new_sums;
    }
    return sums;
}

/**
 * Return 'true' iff the items can be packed in the bin, by enumerating the
 * positions of the items on the normal patterns.
 */
bool brute_force_feasible(
        Length bin_width,
        Length bin_height,
        std::vector<BruteForceItem> items)
{
    // The largest items first, to detect the infeasible branches early.
    std::sort(
            items.begin(),
            items.end(),
            [](const BruteForceItem& item_1, const BruteForceItem& item_2)
            {
                return item_1[0].first * item_1[0].second
                    > item_2[0].first * item_2[0].second;
            });

    // Normal patterns of each item, along each axis.
    std::vector<std::array<std::set<Length>, 2>> patterns(items.size());
    for (size_t item_id = 0; item_id < items.size(); ++item_id) {
        std::array<std::vector<std::vector<Length>>, 2> other_lengths;
        for (size_t other_item_id = 0; other_item_id < items.size(); ++other_item_id) {
            if (other_item_id == item_id)
                continue;
            std::array<std::vector<Length>, 2> lengths;
            for (const auto& orientation: items[other_item_id]) {
                lengths[0].push_back(orientation.first);
                lengths[1].push_back(orientation.second);
            }
            other_lengths[0].push_back(lengths[0]);
            other_lengths[1].push_back(lengths[1]);
        }
        patterns[item_id][0] = subset_sums(other_lengths[0], bin_width);
        patterns[item_id][1] = subset_sums(other_lengths[1], bin_height);
    }

    struct Placement { Length x; Length y; Length width; Length height; };
    std::vector<Placement> placements;
    std::function<bool(size_t)> search = [&](size_t item_id)
    {
        if (item_id == items.size())
            return true;
        for (const auto& orientation: items[item_id]) {
            Length width = orientation.first;
            Length height = orientation.second;
            for (Length x: patterns[item_id][0]) {
                if (x + width > bin_width)
                    continue;
                for (Length y: patterns[item_id][1]) {
                    if (y + height > bin_height)
                        continue;
                    bool overlap = false;
                    for (const Placement& placement: placements) {
                        if (x < placement.x + placement.width
                                && placement.x < x + width
                                && y < placement.y + placement.height
                                && placement.y < y + height) {
                            overlap = true;
                            break;
                        }
                    }
                    if (overlap)
                        continue;
                    placements.push_back({x, y, width, height});
                    bool found = search(item_id + 1);
                    placements.pop_back();
                    if (found)
                        return true;
                }
            }
        }
        return false;
    };
    return search(0);
}

}

TEST(RectangleTreeSearchPackingClasses, RandomInstancesMatchBruteForce)
{
    // Random small instances, with item areas summing to between 75% and
    // 100% of the area of the bin: the search must find a packing iff the
    // brute force does, and otherwise prove the instance infeasible.
    std::mt19937_64 generator(0);
    std::uniform_int_distribution<Length> bin_distribution(5, 9);
    std::uniform_real_distribution<double> fill_distribution(0.75, 1.0);
    std::bernoulli_distribution rotation_distribution(0.25);
    Counter number_of_feasible_instances = 0;
    Counter number_of_infeasible_instances = 0;
    for (Counter instance_id = 0; instance_id < 5000; ++instance_id) {
        Length bin_width = bin_distribution(generator);
        Length bin_height = bin_distribution(generator);
        Area target_area = fill_distribution(generator) * bin_width * bin_height;
        std::uniform_int_distribution<Length> width_distribution(1, (2 * bin_width + 2) / 3);
        std::uniform_int_distribution<Length> height_distribution(1, (2 * bin_height + 2) / 3);

        InstanceBuilder instance_builder;
        instance_builder.set_objective(Objective::Feasibility);
        instance_builder.add_bin_type(bin_width, bin_height);
        std::vector<BruteForceItem> items;
        Area area = 0;
        std::stringstream ss;
        ss << "bin " << bin_width << "x" << bin_height << ", items";
        while (items.size() < 7) {
            Length width = width_distribution(generator);
            Length height = height_distribution(generator);
            if (area + width * height > target_area)
                break;
            area += width * height;
            bool oriented = !rotation_distribution(generator);
            add_item_type(instance_builder, width, height, 1, oriented);
            BruteForceItem item = {{width, height}};
            if (!oriented && width != height)
                item.push_back({height, width});
            items.push_back(item);
            ss << " " << width << "x" << height << (oriented? "": "r");
        }
        if (items.empty())
            continue;
        Instance instance = instance_builder.build();

        TreeSearchPackingClassesOutput output = run(instance);
        if (brute_force_feasible(bin_width, bin_height, items)) {
            number_of_feasible_instances++;
            expect_packing(instance, output);
        } else {
            number_of_infeasible_instances++;
            EXPECT_TRUE(output.is_proven_infeasible) << ss.str();
        }
        if (testing::Test::HasFailure()) {
            ADD_FAILURE() << ss.str();
            break;
        }
    }
    EXPECT_GE(number_of_feasible_instances, 50);
    EXPECT_GE(number_of_infeasible_instances, 20);
}
