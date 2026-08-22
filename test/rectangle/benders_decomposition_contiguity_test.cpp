#include "packingsolver/rectangle/instance_builder.hpp"
#include "rectangle/benders_decomposition_contiguity.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver;
using namespace packingsolver::rectangle;
namespace fs = boost::filesystem;

TEST(RectangleBendersDecompositionContiguity, UnsupportedObjectiveThrows)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::OpenDimensionX);
    instance_builder.add_bin_type(10, 10);
    instance_builder.add_item_type(5, 5, true);
    Instance instance = instance_builder.build();

    BendersDecompositionContiguityParameters parameters;
    parameters.verbosity_level = 0;
    EXPECT_THROW(benders_decomposition_contiguity(instance, parameters), std::invalid_argument);
}

TEST(RectangleBendersDecompositionContiguity, SeveralBinTypesThrows)
{
    // The master (a P|cont|Cmax-style MILP over a single bin's columns) is
    // only defined for one bin used once - see the "Scope" paragraph in
    // 'benders_decomposition_contiguity.hpp'.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Knapsack);
    instance_builder.add_bin_type(10, 10);
    instance_builder.add_bin_type(6, 6);
    instance_builder.add_item_type(5, 5, true);
    Instance instance = instance_builder.build();

    BendersDecompositionContiguityParameters parameters;
    parameters.verbosity_level = 0;
    EXPECT_THROW(benders_decomposition_contiguity(instance, parameters), std::invalid_argument);
}

TEST(RectangleBendersDecompositionContiguity, BinTypeCopiesNotOneThrows)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Knapsack);
    BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10);
    instance_builder.set_bin_type_copies(bin_type_id, 2);
    instance_builder.add_item_type(5, 5, true);
    Instance instance = instance_builder.build();

    BendersDecompositionContiguityParameters parameters;
    parameters.verbosity_level = 0;
    EXPECT_THROW(benders_decomposition_contiguity(instance, parameters), std::invalid_argument);
}

TEST(RectangleBendersDecompositionContiguity, UnsupportedPenalizeResourceCapacityThrows)
{
    // 'onedimentional_contiguity::add_resource_constraints' (used by
    // 'milp', not 'tree_search' - see
    // 'BendersDecompositionContiguityParameters::use_tree_search') only
    // supports a 'penalize' resource shaped like a Jepsen et al.
    // (2008)-style "at least 2 of a set of item-type units" cut: capacity
    // == 1 (see 'RectangleResourceTest.
    // BendersDecompositionContiguityPenalizeResource' in
    // 'resource_test.cpp' for a supported one) - any other capacity throws
    // rather than silently ignoring or mishandling it.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Knapsack);
    BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10);
    ResourceId resource_id = instance_builder.add_bin_type_resource(bin_type_id, 2.0, true, 100.0);
    ItemTypeId item_type_id = instance_builder.add_item_type(5, 5, true);
    instance_builder.set_item_type_copies(item_type_id, 2);
    instance_builder.add_resource_consumption(bin_type_id, resource_id, item_type_id, 0, 1.0);
    instance_builder.add_resource_consumption(bin_type_id, resource_id, item_type_id, 1, 1.0);
    instance_builder.add_resource_consumption(bin_type_id, resource_id, item_type_id, 2, 0.0);
    Instance instance = instance_builder.build();

    BendersDecompositionContiguityParameters parameters;
    parameters.verbosity_level = 0;
    parameters.use_tree_search = false;
    EXPECT_THROW(benders_decomposition_contiguity(instance, parameters), std::invalid_argument);
}

TEST(RectangleBendersDecompositionContiguity, UnsupportedPenalizeResourceScheduleShapeThrows)
{
    // A uniform (non-'threshold_schedule(N)') consumption schedule - see
    // 'RectangleResourceTest.BendersDecompositionContiguityPenalizeResource'
    // in 'resource_test.cpp' for why this shape is required for 'milp' (not
    // 'tree_search' - see
    // 'BendersDecompositionContiguityParameters::use_tree_search').
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Knapsack);
    BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10);
    ResourceId resource_id = instance_builder.add_bin_type_resource(bin_type_id, 1.0, true, 100.0);
    ItemTypeId item_type_id = instance_builder.add_item_type(5, 5, true);
    instance_builder.set_item_type_copies(item_type_id, 2);
    instance_builder.add_resource_consumption(bin_type_id, resource_id, item_type_id, 0, 1.0);
    Instance instance = instance_builder.build();

    BendersDecompositionContiguityParameters parameters;
    parameters.verbosity_level = 0;
    parameters.use_tree_search = false;
    EXPECT_THROW(benders_decomposition_contiguity(instance, parameters), std::invalid_argument);
}

struct RectangleBendersDecompositionContiguityTestParams
{
    fs::path items_path;
    fs::path bins_path;
    fs::path parameters_path;

    // Meaningful only for a 'Knapsack' instance: the true optimal profit
    // (not merely a bound - unlike 'bar_relaxation_test.cpp''s parametrized
    // suite, this algorithm is exact).
    Profit expected_knapsack_profit = 0;

    // Meaningful only for a 'Feasibility' instance.
    bool expected_is_proven_infeasible = false;
};

inline std::ostream& operator<<(
        std::ostream& os,
        const RectangleBendersDecompositionContiguityTestParams& test_params)
{
    os << test_params.items_path;
    return os;
}

class RectangleBendersDecompositionContiguityTest: public testing::TestWithParam<RectangleBendersDecompositionContiguityTestParams> { };

TEST_P(RectangleBendersDecompositionContiguityTest, RectangleBendersDecompositionContiguity)
{
    RectangleBendersDecompositionContiguityTestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    instance_builder.read_item_types(test_params.items_path.string());
    instance_builder.read_bin_types(test_params.bins_path.string());
    instance_builder.read_parameters(test_params.parameters_path.string());
    Instance instance = instance_builder.build();

    BendersDecompositionContiguityParameters parameters;
    parameters.verbosity_level = 0;
    BendersDecompositionContiguityOutput output = benders_decomposition_contiguity(instance, parameters);

    if (instance.objective() == Objective::Knapsack) {
        EXPECT_TRUE(output.solution_pool.best().feasible());
        EXPECT_TRUE(equal_profit(output.solution_pool.best().profit(), test_params.expected_knapsack_profit));
        EXPECT_TRUE(equal_profit(output.knapsack_bound, test_params.expected_knapsack_profit));
    } else {
        EXPECT_EQ(output.is_proven_infeasible, test_params.expected_is_proven_infeasible);
        if (!test_params.expected_is_proven_infeasible) {
            EXPECT_TRUE(output.solution_pool.best().feasible());
            EXPECT_TRUE(output.solution_pool.best().full());
        }
    }
}

INSTANTIATE_TEST_SUITE_P(
        Rectangle,
        RectangleBendersDecompositionContiguityTest,
        testing::ValuesIn(std::vector<RectangleBendersDecompositionContiguityTestParams>{
            {
                // Same fixture (and expected profit) as
                // 'benders_decomposition_test.cpp': item1 (profit 190) is
                // pairwise-incompatible with the others, so the optimum
                // uses only the 4 copies of item0 (profit 50 each).
                fs::path("data") / "rectangle" / "tests" / "knapsack_incompatible_item_pair" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_incompatible_item_pair" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_incompatible_item_pair" / "parameters.csv",
                200,
            }, {
                // Same fixture as 'benders_decomposition_test.cpp' and
                // 'bar_relaxation_test.cpp' ('SquareGridBoundIsTight'-style
                // exact tiling): the 5 items tile the 20x10 bin exactly, so
                // the optimum is the sum of their (area-default) profits.
                fs::path("data") / "rectangle" / "tests" / "knapsack_all_items_fit_exactly" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_all_items_fit_exactly" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_all_items_fit_exactly" / "parameters.csv",
                200,
            }, {
                // Same fixture as 'benders_decomposition_test.cpp': all 9
                // copies of the lower-profit item0 (50 each) tile the bin
                // exactly (3x3 grid), strictly beating the single high-
                // profit item1 (199) alone.
                fs::path("data") / "rectangle" / "tests" / "knapsack_high_profit_item_unpacked" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_high_profit_item_unpacked" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_high_profit_item_unpacked" / "parameters.csv",
                450,
            }, {
                // Same fixture as 'benders_decomposition_test.cpp': the 5
                // copies of the thin, high-total-profit item0 (5 each) beat
                // the single larger-area item1 (profit 10) once geometry -
                // not just area - is accounted for.
                fs::path("data") / "rectangle" / "tests" / "knapsack_area_domination_mismatch" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_area_domination_mismatch" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_area_domination_mismatch" / "parameters.csv",
                25,
            }, {
                // Same fixture as 'bar_relaxation_test.cpp': both copies of
                // the rotatable item fit side by side in the non-square
                // bin - the bar relaxation bound (2) is already tight here.
                fs::path("data") / "rectangle" / "tests" / "knapsack_non_square_bin_rotatable_items_fit_side_by_side" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_non_square_bin_rotatable_items_fit_side_by_side" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_non_square_bin_rotatable_items_fit_side_by_side" / "parameters.csv",
                2,
            }, {
                // Same fixture as 'bar_relaxation_test.cpp': only 3 of the
                // 4 copies actually fit once placed (a fourth would
                // overlap) - the bar relaxation bound (40/11) is loose
                // here, exercising that this algorithm finds the true,
                // strictly lower, exact optimum instead of the bound.
                fs::path("data") / "rectangle" / "tests" / "knapsack_non_square_bin_too_many_rotatable_items" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_non_square_bin_too_many_rotatable_items" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_non_square_bin_too_many_rotatable_items" / "parameters.csv",
                3,
            }, {
                // Same fixture as 'bar_relaxation_test.cpp': 4 copies of a
                // 5x5 item tile a 10x10 bin exactly - genuinely feasible.
                fs::path("data") / "rectangle" / "tests" / "feasibility_exact_tiling_feasible" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "feasibility_exact_tiling_feasible" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "feasibility_exact_tiling_feasible" / "parameters.csv",
                0,
                false,
            }, {
                // Same fixture as 'bar_relaxation_test.cpp': the item
                // (20x20) does not fit the bin (10x10) in either
                // orientation, so no candidate position exists for it at
                // all - the BMP itself is infeasible, without ever needing
                // a cut.
                fs::path("data") / "rectangle" / "tests" / "feasibility_item_too_big_infeasible" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "feasibility_item_too_big_infeasible" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "feasibility_item_too_big_infeasible" / "parameters.csv",
                0,
                true,
            }}));
