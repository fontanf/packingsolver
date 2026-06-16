#include "packingsolver/rectangleguillotine/instance_builder.hpp"
#include "rectangleguillotine/dynamic_programming_infinite_copies_array.hpp"
#include "rectangleguillotine/solution_builder.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver::rectangleguillotine;
namespace fs = boost::filesystem;

struct RectangleGuillotineDynamicProgrammingInfiniteCopiesArrayTestParams
{
    fs::path items_path;
    fs::path bins_path;
    fs::path parameters_path;
    fs::path certificate_path;
};

inline std::ostream& operator<<(
        std::ostream& os,
        const RectangleGuillotineDynamicProgrammingInfiniteCopiesArrayTestParams& test_params)
{
    os << test_params.items_path;
    return os;
}

class RectangleGuillotineDynamicProgrammingInfiniteCopiesArrayTest:
    public testing::TestWithParam<RectangleGuillotineDynamicProgrammingInfiniteCopiesArrayTestParams> { };

TEST_P(
        RectangleGuillotineDynamicProgrammingInfiniteCopiesArrayTest,
        RectangleGuillotineDynamicProgrammingInfiniteCopiesArray)
{
    RectangleGuillotineDynamicProgrammingInfiniteCopiesArrayTestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    instance_builder.read_item_types(test_params.items_path.string());
    instance_builder.read_bin_types(test_params.bins_path.string());
    instance_builder.read_parameters(test_params.parameters_path.string());
    Instance instance = instance_builder.build();

    DynamicProgrammingInfiniteCopiesArrayParameters dp_parameters;
    DynamicProgrammingInfiniteCopiesArrayOutput output
        = dynamic_programming_infinite_copies_array(instance, dp_parameters);

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
        RectangleGuillotineDynamicProgrammingInfiniteCopiesArray,
        RectangleGuillotineDynamicProgrammingInfiniteCopiesArrayTest,
        testing::ValuesIn(std::vector<RectangleGuillotineDynamicProgrammingInfiniteCopiesArrayTestParams>{
            // Unlimited stages, infinite copies, oriented items.
            // Single item that exactly fills the bin (no cuts needed).
            {
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo" / "bins.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo" / "solution.csv",
            }, {
                // Two items placed side by side (x-split).
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo_x" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo_x" / "bins.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo_x" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo_x" / "solution.csv",
            }, {
                // Two items stacked vertically (y-split).
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo_y" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo_y" / "bins.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo_y" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo_y" / "solution.csv",
            }, {
                // Two items with cut thickness consuming one unit between them.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo_cut_thickness" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo_cut_thickness" / "bins.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo_cut_thickness" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo_cut_thickness" / "solution.csv",
            }, {
                // Unlimited stages, infinite copies, rotation allowed.
                // Item that fits only after a 90-degree rotation.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_ur" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_ur" / "bins.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_ur" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_ur" / "solution.csv",
            }, {
                // Unlimited stages, infinite copies, oriented items, hard trims.
                // 20x15 bin with left=2/bottom=2 hard trims and right=3/top=3 soft trims,
                // giving a 15x10 usable area.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo_trims" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo_trims" / "bins.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo_trims" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo_trims" / "solution.csv",
            }}));
