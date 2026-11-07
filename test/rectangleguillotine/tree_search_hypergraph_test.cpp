#include "packingsolver/rectangleguillotine/instance_builder.hpp"
#include "rectangleguillotine/tree_search_hypergraph.hpp"
#include "rectangleguillotine/solution_builder.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver::rectangleguillotine;
namespace fs = boost::filesystem;

struct RectangleGuillotineTreeSearchHypergraphTestParams
{
    fs::path items_path;
    fs::path bins_path;
    fs::path parameters_path;
    /** Certificate of an optimal solution; empty if the instance is infeasible. */
    fs::path certificate_path;
};

inline std::ostream& operator<<(
        std::ostream& os,
        const RectangleGuillotineTreeSearchHypergraphTestParams& test_params)
{
    os << test_params.items_path;
    return os;
}

class RectangleGuillotineTreeSearchHypergraphTest:
    public testing::TestWithParam<RectangleGuillotineTreeSearchHypergraphTestParams> { };

TEST_P(
        RectangleGuillotineTreeSearchHypergraphTest,
        RectangleGuillotineTreeSearchHypergraph)
{
    RectangleGuillotineTreeSearchHypergraphTestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    instance_builder.read_item_types(test_params.items_path.string());
    instance_builder.read_bin_types(test_params.bins_path.string());
    instance_builder.read_parameters(test_params.parameters_path.string());
    Instance instance = instance_builder.build();

    TreeSearchHypergraphParameters tree_search_hypergraph_parameters;
    TreeSearchHypergraphOutput output = tree_search_hypergraph(instance, tree_search_hypergraph_parameters);

    if (test_params.certificate_path.empty()) {
        EXPECT_TRUE(output.is_proven_infeasible);
        return;
    }

    EXPECT_TRUE(output.solution_pool.best().feasible());

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
        RectangleGuillotineTreeSearchHypergraph,
        RectangleGuillotineTreeSearchHypergraphTest,
        testing::ValuesIn(std::vector<RectangleGuillotineTreeSearchHypergraphTestParams>{
            // Unlimited stages, oriented items.
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
                // Rotation allowed.
                // Item that fits only after a 90-degree rotation.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_ur" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_ur" / "bins.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_ur" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_ur" / "solution.csv",
            }, {
                // Oriented items, hard trims.
                // 20x15 bin with left=2/bottom=2 hard trims and right=3/top=3 soft trims,
                // giving a 15x10 usable area.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo_trims" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo_trims" / "bins.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo_trims" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo_trims" / "solution.csv",
            }, {
                // Bounded item copies: the unbounded-copies relaxation would
                // select two copies of the high-profit item type, which is
                // infeasible here since it only has one copy available.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo_bounded_copies" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo_bounded_copies" / "bins.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo_bounded_copies" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo_bounded_copies" / "solution.csv",
            }, {
                // Two stages, non-exact cut type.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2nvo" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2nvo" / "bins.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2nvo" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2nvo" / "solution.csv",
            }, {
                // Two stages, horizontal first stage.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2nho" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2nho" / "bins.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2nho" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2nho" / "solution.csv",
            }, {
                // Two stages, exact cut type.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2evo" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2evo" / "bins.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2evo" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2evo" / "solution.csv",
            }, {
                // Two stages, exact cut type, rotations.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2evr" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2evr" / "bins.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2evr" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2evr" / "solution.csv",
            }, {
                // Two stages, non-exact cut type, rotations.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2nvr" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2nvr" / "bins.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2nvr" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2nvr" / "solution.csv",
            }, {
                // Two stages, roadef2018 cut type.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2rvo" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2rvo" / "bins.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2rvo" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2rvo" / "solution.csv",
            }, {
                // Two stages, roadef2018 cut type, cut thickness.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2rvo_cut_thickness" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2rvo_cut_thickness" / "bins.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2rvo_cut_thickness" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2rvo_cut_thickness" / "solution.csv",
            }, {
                // Two stages, roadef2018 cut type: two items in a second-level
                // sub-plate, separated by the extra cut.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2rvo_two_items" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2rvo_two_items" / "bins.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2rvo_two_items" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2rvo_two_items" / "solution.csv",
            }, {
                // Two stages, roadef2018 cut type, rotations.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2rvr" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2rvr" / "bins.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2rvr" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2rvr" / "solution.csv",
            }, {
                // Two stages, hard trims.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2nvo_trims" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2nvo_trims" / "bins.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2nvo_trims" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2nvo_trims" / "solution.csv",
            }, {
                // Three stages, exact cut type.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_3evo" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_3evo" / "bins.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_3evo" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_3evo" / "solution.csv",
            }, {
                // Three stages, non-exact cut type, rotations.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_3nvr" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_3nvr" / "bins.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_3nvr" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_3nvr" / "solution.csv",
            }, {
                // Three stages, roadef2018 cut type, rotations.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_3rvr" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_3rvr" / "bins.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_3rvr" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_3rvr" / "solution.csv",
            }, {
                // Three stages, roadef2018 cut type, horizontal first stage: two
                // items in a third-level sub-plate.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_3rho" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_3rho" / "bins.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_3rho" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_3rho" / "solution.csv",
            }, {
                // Four stages, exact cut type, horizontal first stage.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_4eho" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_4eho" / "bins.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_4eho" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_4eho" / "solution.csv",
            }, {
                // Four stages, non-exact cut type, horizontal first stage.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_4nho" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_4nho" / "bins.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_4nho" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_4nho" / "solution.csv",
            }, {
                // One stage, roadef2018 cut type.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_1rvr" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_1rvr" / "bins.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_1rvr" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_1rvr" / "solution.csv",
            }, {
                // Feasibility, unlimited stages.
                fs::path("data") / "rectangleguillotine" / "tests" / "feasibility_unlimited_stages" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "feasibility_unlimited_stages" / "bins.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "feasibility_unlimited_stages" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "feasibility_unlimited_stages" / "solution.csv",
            }, {
                // Feasibility, three stages, any first stage orientation, cut
                // thickness.
                fs::path("data") / "rectangleguillotine" / "tests" / "feasibility_3nao_cut_thickness" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "feasibility_3nao_cut_thickness" / "bins.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "feasibility_3nao_cut_thickness" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "feasibility_3nao_cut_thickness" / "solution.csv",
            }, {
                // Feasibility, two stages, any first stage orientation:
                // infeasible.
                fs::path("data") / "rectangleguillotine" / "tests" / "feasibility_2nao_infeasible" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "feasibility_2nao_infeasible" / "bins.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "feasibility_2nao_infeasible" / "parameters.csv",
                fs::path(),
            }}));
