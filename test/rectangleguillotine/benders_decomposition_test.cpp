#include "packingsolver/rectangleguillotine/instance_builder.hpp"
#include "packingsolver/rectangleguillotine/optimize.hpp"
#include "rectangleguillotine/benders_decomposition.hpp"
#include "rectangleguillotine/solution_builder.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;
namespace fs = boost::filesystem;

struct RectangleGuillotineBendersDecompositionTestParams
{
    /** Items file, or JSON instance file. */
    fs::path items_path;
    fs::path bins_path;
    fs::path defects_path;
    fs::path parameters_path;
    /** Certificate of an optimal solution. */
    fs::path certificate_path;
};

inline std::ostream& operator<<(
        std::ostream& os,
        const RectangleGuillotineBendersDecompositionTestParams& test_params)
{
    os << test_params.items_path;
    return os;
}

class RectangleGuillotineBendersDecompositionTest:
    public testing::TestWithParam<RectangleGuillotineBendersDecompositionTestParams> { };

TEST_P(
        RectangleGuillotineBendersDecompositionTest,
        RectangleGuillotineBendersDecomposition)
{
    RectangleGuillotineBendersDecompositionTestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    if (test_params.items_path.extension() == ".json") {
        instance_builder.read(test_params.items_path.string());
    } else {
        instance_builder.read_item_types(test_params.items_path.string());
        instance_builder.read_bin_types(test_params.bins_path.string());
        instance_builder.read_defects(test_params.defects_path.string());
        instance_builder.read_parameters(test_params.parameters_path.string());
    }
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    optimize_parameters.optimization_mode = OptimizationMode::Exact;
    optimize_parameters.use_benders_decomposition = true;
    rectangleguillotine::Output output = optimize(instance, optimize_parameters);

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
        RectangleGuillotineBendersDecomposition,
        RectangleGuillotineBendersDecompositionTest,
        testing::ValuesIn(std::vector<RectangleGuillotineBendersDecompositionTestParams>{
            // Unlimited stages, oriented items.
            // Single item that exactly fills the bin (no cuts needed).
            {
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo" / "solution.csv",
            }, {
                // Two items placed side by side (x-split).
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo_x" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo_x" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo_x" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo_x" / "solution.csv",
            }, {
                // Two items stacked vertically (y-split).
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo_y" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo_y" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo_y" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo_y" / "solution.csv",
            }, {
                // Two items with cut thickness consuming one unit between them:
                // the relaxation ignores the cut thickness.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo_cut_thickness" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo_cut_thickness" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo_cut_thickness" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo_cut_thickness" / "solution.csv",
            }, {
                // Rotation allowed.
                // Item that fits only after a 90-degree rotation.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_ur" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_ur" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_ur" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_ur" / "solution.csv",
            }, {
                // Oriented items, hard and soft trims: the relaxation removes
                // the hard trims from the bin.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo_trims" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo_trims" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo_trims" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo_trims" / "solution.csv",
            }, {
                // Bounded item copies.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo_bounded_copies" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo_bounded_copies" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo_bounded_copies" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_uo_bounded_copies" / "solution.csv",
            }, {
                // Four 3x2 items in a 5x5 bin: they all fit in a pinwheel,
                // which is not a guillotine packing; with guillotine cuts, at
                // most 3 of them fit.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_ur_pinwheel" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_ur_pinwheel" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_ur_pinwheel" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_ur_pinwheel" / "solution.csv",
            }, {
                // Two stages, non-exact cut type.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2nvo" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2nvo" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2nvo" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2nvo" / "solution.csv",
            }, {
                // Two stages, horizontal first stage.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2nho" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2nho" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2nho" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2nho" / "solution.csv",
            }, {
                // Two stages, exact cut type.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2evo" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2evo" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2evo" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2evo" / "solution.csv",
            }, {
                // Two stages, exact cut type, rotations.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2evr" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2evr" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2evr" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2evr" / "solution.csv",
            }, {
                // Two stages, exact cut type: the 3 items fit with 3 stages
                // (a 4x10 strip, then a 6x10 strip with the 6x3 item above the
                // 3x7 item, trimmed by a third cut), but not with 2 stages,
                // although each pair of them fits, and although they fit
                // without guillotine cuts.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2evo_three_items" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2evo_three_items" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2evo_three_items" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2evo_three_items" / "solution.csv",
            }, {
                // The same items with 3 stages: they all fit.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_3evo_three_items" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_3evo_three_items" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_3evo_three_items" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_3evo_three_items" / "solution.csv",
            }, {
                // Two stages, roadef2018 cut type.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2rvo" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2rvo" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2rvo" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2rvo" / "solution.csv",
            }, {
                // Three stages, exact cut type.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_3evo" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_3evo" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_3evo" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_3evo" / "solution.csv",
            }, {
                // Three stages, non-exact cut type, rotations.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_3nvr" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_3nvr" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_3nvr" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_3nvr" / "solution.csv",
            }, {
                // Four stages, exact cut type, horizontal first stage.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_4eho" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_4eho" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_4eho" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_4eho" / "solution.csv",
            }, {
                // Two stages, defects: the relaxation ignores the defects.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2nvo_defects_2" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2nvo_defects_2" / "bins.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2nvo_defects_2" / "defects.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2nvo_defects_2" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2nvo_defects_2" / "solution.csv",
            }, {
                // A resource of capacity 1 consumed by item types 0 and 1:
                // they can't be packed together, item types 0 and 2 are.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_unlimited_stages_hard_resource" / "instance.json",
                fs::path(""),
                fs::path(""),
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_unlimited_stages_hard_resource" / "solution.csv",
            }, {
                // Bin packing, three stages, non-exact cut type.
                fs::path("data") / "rectangleguillotine" / "tests" / "bin_packing_3nvo" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "bin_packing_3nvo" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "bin_packing_3nvo" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "bin_packing_3nvo" / "solution.csv",
            }, {
                // Bin packing, three stages, non-exact cut type, rotations.
                fs::path("data") / "rectangleguillotine" / "tests" / "bin_packing_3nvr" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "bin_packing_3nvr" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "bin_packing_3nvr" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "bin_packing_3nvr" / "solution.csv",
            }, {
                // Bin packing, unlimited stages, hard trims.
                fs::path("data") / "rectangleguillotine" / "tests" / "bin_packing_unlimited_stages_hard_trims" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "bin_packing_unlimited_stages_hard_trims" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "bin_packing_unlimited_stages_hard_trims" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "bin_packing_unlimited_stages_hard_trims" / "solution.csv",
            }, {
                // Bin packing, the pinwheel above: the four items need 2
                // bins with guillotine cuts.
                fs::path("data") / "rectangleguillotine" / "tests" / "bin_packing_ur_pinwheel" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "bin_packing_ur_pinwheel" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "bin_packing_ur_pinwheel" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "bin_packing_ur_pinwheel" / "solution.csv",
            }, {
                // Variable-sized bin packing, unlimited stages.
                fs::path("data") / "rectangleguillotine" / "tests" / "variable_sized_bin_packing_unlimited_stages_single_bin" / "instance.json",
                fs::path(""),
                fs::path(""),
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "variable_sized_bin_packing_unlimited_stages_single_bin" / "solution.csv",
            }}));

TEST(RectangleGuillotineBendersDecomposition, PenalizedResourceNotSupported)
{
    // A penalized resource whose consumptions don't count the copies up to a
    // threshold: the master problem doesn't support it.
    InstanceBuilder instance_builder;
    instance_builder.read((fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_unlimited_stages_penalized_resource" / "instance.json").string());
    Instance instance = instance_builder.build();
    EXPECT_FALSE(benders_decomposition_supported(instance));
}
