#include "packingsolver/rectangle/instance_builder.hpp"
#include "packingsolver/rectangle/optimize.hpp"
#include "rectangle/solution_builder.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver::rectangle;
namespace fs = boost::filesystem;

struct RectangleTreeSearchMaximalSpacesTestParams
{
    fs::path items_path;
    fs::path bins_path;
    fs::path defects_path;
    fs::path parameters_path;
    fs::path certificate_path;

    /**
     * Path to a JSON instance file; if non-empty, used instead of
     * 'items_path'/'bins_path'/'defects_path'/'parameters_path' (needed for
     * features with no CSV representation, e.g. resources).
     */
    fs::path instance_path;
};

inline std::ostream& operator<<(std::ostream& os, const RectangleTreeSearchMaximalSpacesTestParams& test_params)
{
    os << (!test_params.instance_path.empty()? test_params.instance_path: test_params.items_path);
    return os;
}

class RectangleTreeSearchMaximalSpacesTest: public testing::TestWithParam<RectangleTreeSearchMaximalSpacesTestParams> { };

TEST_P(RectangleTreeSearchMaximalSpacesTest, RectangleTreeSearchMaximalSpaces)
{
    RectangleTreeSearchMaximalSpacesTestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    if (!test_params.instance_path.empty()) {
        instance_builder.read(test_params.instance_path.string());
    } else {
        instance_builder.read_item_types(test_params.items_path.string());
        instance_builder.read_bin_types(test_params.bins_path.string());
        instance_builder.read_defects(test_params.defects_path.string());
        instance_builder.read_parameters(test_params.parameters_path.string());
    }
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    optimize_parameters.use_tree_search_maximal_spaces = true;
    Output output = optimize(instance, optimize_parameters);

    SolutionBuilder solution_builder(instance);
    solution_builder.read(test_params.certificate_path.string());
    Solution solution = solution_builder.build();
    std::cout << std::endl
        << "Reference solution" << std::endl
        << "------------------" << std::endl;
    solution.format(std::cout);

    EXPECT_EQ(!(output.solution_pool.best() < solution), true);
    EXPECT_EQ(!(solution < output.solution_pool.best()), true);
}

INSTANTIATE_TEST_SUITE_P(
        Rectangle,
        RectangleTreeSearchMaximalSpacesTest,
        testing::ValuesIn(std::vector<RectangleTreeSearchMaximalSpacesTestParams>{
            {
                // Bin 20x10, a defect covering the 5-wide middle strip
                // (x in [5,10)), a 10x10 item with 2 copies (a single copy
                // would make the bin's tracked width collapse to 10 via the
                // "max reachable length" domain reduction - since one copy
                // could never reach past x=10 anyway - which would place
                // the defect at the truncated bin's far edge instead of its
                // middle, defeating the point of this test). Cutting the
                // initial empty space around the defect leaves a 5x10 space
                // (too narrow for the item) and a 10x10 space (an exact
                // fit) - and the 5x10 one, being closer to the bin's
                // origin, is exactly the kind of unusable-but-preferred
                // space 'remove_unusable_spaces' must discard, otherwise
                // the search would wrongly consider the root infertile
                // despite the still-usable 10x10 space (and, without the
                // defect cut being applied at all, would instead silently
                // pack both copies, one of them overlapping the defect).
                // Only 1 of the 2 copies fits.
                fs::path("data") / "rectangle" / "tests" / "knapsack_defect_blocks_middle" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_defect_blocks_middle" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_defect_blocks_middle" / "defects.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_defect_blocks_middle" / "parameters.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_defect_blocks_middle" / "solution.csv",
            }, {
                // Bin 12x4, three copies of a 4x4 item tile it exactly side
                // by side (3 * 4 = 12). A resource with capacity 2 and a
                // uniform consumption of 1 per copy caps the item type at 2
                // copies: 'tree_search_maximal_spaces' must reject the third
                // copy instead of silently ignoring the resource.
                fs::path(),
                fs::path(),
                fs::path(),
                fs::path(),
                fs::path("data") / "rectangle" / "tests" / "knapsack_resource_capacity_blocks_insertion" / "solution.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_resource_capacity_blocks_insertion" / "instance.json",
            }, {
                // Same layout as the previous case, but the resource is
                // 'penalize': it must not block the third copy from being
                // packed, and must subtract 'penalty' from the profit
                // exactly once (the first time consumption crosses
                // capacity), not once per copy over capacity - so the
                // reference solution packs all 3 items, and its profit
                // (3 * 10 - 5 = 25, via 'Solution::update_indicators')
                // matches what the algorithm reports.
                fs::path(),
                fs::path(),
                fs::path(),
                fs::path(),
                fs::path("data") / "rectangle" / "tests" / "knapsack_resource_penalty_reduces_profit" / "solution.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_resource_penalty_reduces_profit" / "instance.json",
            }, {
                // Bin 8x4, maximum_weight 5: two copies of a 4x4, weight-3
                // item tile it exactly side by side (2 * 4 = 8, geometrically
                // fine), but weigh 6 together, over the bin's own capacity.
                // Regression test for a bug where moving the weight check
                // from 'insertions'/'best_insertion' into 'apply_insertion's
                // 'valid_block_ids' pruning (both only ever accumulate, so
                // the prune is permanent once a block is ruled out) missed
                // that the *root* node never goes through 'apply_insertion'
                // at all - a block whose own weight already exceeds capacity
                // needs to be excluded at compute_blocks() time too (see
                // 'Block::weight''s own doc comment), or it would wrongly
                // stay offered as the very first insertion.
                fs::path("data") / "rectangle" / "tests" / "knapsack_weight_blocks_insertion" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_weight_blocks_insertion" / "bins.csv",
                fs::path(),
                fs::path("data") / "rectangle" / "tests" / "knapsack_weight_blocks_insertion" / "parameters.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_weight_blocks_insertion" / "solution.csv",
            }}));
