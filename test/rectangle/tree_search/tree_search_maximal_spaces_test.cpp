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
};

inline std::ostream& operator<<(std::ostream& os, const RectangleTreeSearchMaximalSpacesTestParams& test_params)
{
    os << test_params.items_path;
    return os;
}

class RectangleTreeSearchMaximalSpacesTest: public testing::TestWithParam<RectangleTreeSearchMaximalSpacesTestParams> { };

TEST_P(RectangleTreeSearchMaximalSpacesTest, RectangleTreeSearchMaximalSpaces)
{
    RectangleTreeSearchMaximalSpacesTestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    instance_builder.read_item_types(test_params.items_path.string());
    instance_builder.read_bin_types(test_params.bins_path.string());
    instance_builder.read_defects(test_params.defects_path.string());
    instance_builder.read_parameters(test_params.parameters_path.string());
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
            }}));
