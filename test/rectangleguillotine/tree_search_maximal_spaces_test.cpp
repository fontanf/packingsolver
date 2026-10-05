#include "packingsolver/rectangleguillotine/instance_builder.hpp"
#include "rectangleguillotine/tree_search_maximal_spaces.hpp"
#include "rectangleguillotine/solution_builder.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;
namespace fs = boost::filesystem;

struct RectangleGuillotineTreeSearchMaximalSpacesTestParams
{
    fs::path items_path;
    fs::path bins_path;
    fs::path parameters_path;
    fs::path certificate_path;
};

inline std::ostream& operator<<(
        std::ostream& os,
        const RectangleGuillotineTreeSearchMaximalSpacesTestParams& test_params)
{
    os << test_params.items_path;
    return os;
}

class RectangleGuillotineTreeSearchMaximalSpacesTest:
    public testing::TestWithParam<RectangleGuillotineTreeSearchMaximalSpacesTestParams> { };

TEST_P(
        RectangleGuillotineTreeSearchMaximalSpacesTest,
        RectangleGuillotineTreeSearchMaximalSpaces)
{
    RectangleGuillotineTreeSearchMaximalSpacesTestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    instance_builder.read_item_types(test_params.items_path.string());
    instance_builder.read_bin_types(test_params.bins_path.string());
    instance_builder.read_parameters(test_params.parameters_path.string());
    Instance instance = instance_builder.build();

    TreeSearchMaximalSpacesParameters tree_search_maximal_spaces_parameters;
    tree_search_maximal_spaces_parameters.optimization_mode = OptimizationMode::NotAnytimeSequential;
    TreeSearchMaximalSpacesOutput output = tree_search_maximal_spaces(instance, tree_search_maximal_spaces_parameters);

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
        RectangleGuillotineTreeSearchMaximalSpaces,
        RectangleGuillotineTreeSearchMaximalSpacesTest,
        testing::ValuesIn(std::vector<RectangleGuillotineTreeSearchMaximalSpacesTestParams>{
            // Unlimited stages, rotation allowed, cut thickness 4.
            // Items 1 (610x140) and 0 rotated (2050x136) are placed side by
            // side, so item 0's space is taller than the item by exactly the
            // cut thickness. A trim cut must still separate the item from the
            // gap (regression test for issue #660).
            {
                fs::path("data") / "rectangleguillotine" / "tests" / "bin_packing_ur_cut_thickness_small_gap" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "bin_packing_ur_cut_thickness_small_gap" / "bins.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "bin_packing_ur_cut_thickness_small_gap" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "bin_packing_ur_cut_thickness_small_gap" / "solution.csv",
            }}));
