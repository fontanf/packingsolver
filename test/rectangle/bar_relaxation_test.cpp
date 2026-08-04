#include "packingsolver/rectangle/instance_builder.hpp"
#include "rectangle/bar_relaxation.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver;
using namespace packingsolver::rectangle;
namespace fs = boost::filesystem;

TEST(RectangleBarRelaxation, UnsupportedObjectiveThrows)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::OpenDimensionX);
    instance_builder.add_bin_type(10, 10);
    instance_builder.add_item_type(5, 5, true);
    Instance instance = instance_builder.build();

    BarRelaxationParameters parameters;
    parameters.verbosity_level = 0;
    EXPECT_THROW(bar_relaxation(instance, parameters), std::invalid_argument);
}

TEST(RectangleBarRelaxation, BinPackingWithSeveralBinTypesThrows)
{
    // The reported cost bound is converted to a bin *count* bound by
    // dividing by a single bin type's cost - not meaningful with several,
    // differently-costed bin types.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    instance_builder.add_bin_type(10, 10);
    instance_builder.add_bin_type(6, 6);
    instance_builder.add_item_type(5, 5, true);
    Instance instance = instance_builder.build();

    BarRelaxationParameters parameters;
    parameters.verbosity_level = 0;
    EXPECT_THROW(bar_relaxation(instance, parameters), std::invalid_argument);
}

TEST(RectangleBarRelaxation, DefectIsIgnoredSoundly)
{
    // A defect only forbids a specific position within the bin, which the
    // bar relaxation cannot express (it only tracks bar counts, never
    // positions) - so it is simply not modeled at all, rather than rejected:
    // dropping a placement constraint can only relax the feasible region,
    // so the bound stays a valid (if unaffected-by-the-defect, hence
    // possibly looser) upper bound on achievable profit. Checked here by
    // observing that adding a defect changes nothing about the computed
    // bound, since the model never looks at it: it is exactly the same
    // instance as 'SquareGridBoundIsTight' below, plus one defect.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Knapsack);
    BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10);
    instance_builder.add_defect(bin_type_id, 0, 0, 1, 1);
    ItemTypeId item_type_id = instance_builder.add_item_type(5, 5, true);
    instance_builder.set_item_type_profit(item_type_id, 1);
    instance_builder.set_item_type_copies(item_type_id, 10);
    Instance instance = instance_builder.build();

    BarRelaxationParameters parameters;
    parameters.verbosity_level = 0;
    BarRelaxationOutput output = bar_relaxation(instance, parameters);

    EXPECT_TRUE(equal_profit(output.knapsack_bound, 4.0));
}

TEST(RectangleBarRelaxation, SquareGridBoundIsTight)
{
    // Bin 10x10, item 5x5 (oriented, so orientation plays no role here):
    // four copies tile the bin exactly (two row-bars of two items each,
    // matching two column-bars of two items each), so the bar relaxation -
    // which is exact whenever a grid of bar-patterns can reconstruct an
    // actual packing - should find the true optimum of 4, not merely an
    // over-estimate.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Knapsack);
    instance_builder.add_bin_type(10, 10);
    ItemTypeId item_type_id = instance_builder.add_item_type(5, 5, true);
    instance_builder.set_item_type_profit(item_type_id, 1);
    instance_builder.set_item_type_copies(item_type_id, 10);
    Instance instance = instance_builder.build();

    BarRelaxationParameters parameters;
    parameters.verbosity_level = 0;
    BarRelaxationOutput output = bar_relaxation(instance, parameters);

    EXPECT_TRUE(equal_profit(output.knapsack_bound, 4.0));
}

TEST(RectangleBarRelaxation, TwoBinTypes)
{
    // Two independent bin types (all test data above only ever has one),
    // exercising that rows and static columns are correctly kept separate
    // per bin type. Bin type 0 (10x10) tiles 4 copies of the 5x5 item
    // exactly, same as 'SquareGridBoundIsTight' above. Bin type 1 (6x6) can
    // only ever fit a single real copy (two would need 10 > 6 along either
    // axis), but the relaxation cannot see that: capacity 6 along either
    // bar direction allows one 5-wide/tall item per bar, so the row-cap and
    // column-cap rows (<= 6 bars each) only bound it to 6/5 = 1.2 relaxed
    // copies. Total: 4 + 1.2 = 5.2.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Knapsack);
    instance_builder.add_bin_type(10, 10);
    instance_builder.add_bin_type(6, 6);
    ItemTypeId item_type_id = instance_builder.add_item_type(5, 5, true);
    instance_builder.set_item_type_profit(item_type_id, 1);
    instance_builder.set_item_type_copies(item_type_id, 10);
    Instance instance = instance_builder.build();

    BarRelaxationParameters parameters;
    parameters.verbosity_level = 0;
    BarRelaxationOutput output = bar_relaxation(instance, parameters);

    EXPECT_TRUE(equal_profit(output.knapsack_bound, 5.2));
}

TEST(RectangleBarRelaxation, BinPackingExactTilingIsTight)
{
    // Same exact tiling as 'SquareGridBoundIsTight': 4 copies of the 5x5
    // item fit a single 10x10 bin exactly, so the true minimum is 1 bin -
    // and, since the relaxation is exact on this instance, that is also
    // what it should find. The bin cost (7, i.e. not 1) exercises that the
    // cost bound is correctly converted back to a bin *count* by dividing
    // it out, not just happening to equal the cost bound itself.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10);
    instance_builder.set_bin_type_cost(bin_type_id, 7);
    instance_builder.set_bin_type_copies(bin_type_id, -1);
    ItemTypeId item_type_id = instance_builder.add_item_type(5, 5, true);
    instance_builder.set_item_type_copies(item_type_id, 4);
    Instance instance = instance_builder.build();

    BarRelaxationParameters parameters;
    parameters.verbosity_level = 0;
    BarRelaxationOutput output = bar_relaxation(instance, parameters);

    EXPECT_EQ(output.bin_packing_bound, 1);
}

TEST(RectangleBarRelaxation, VariableSizedBinPackingPrefersCheaperBinType)
{
    // Same two bin types as 'TwoBinTypes' (10x10 and 6x6), 5 copies of the
    // 5x5 item to pack in full (VariableSizedBinPacking, unlike Knapsack,
    // requires every copy to be packed). Bin type 0 relaxes to 4
    // items/10 cost = 0.4 items per unit cost; bin type 1 (cost 1) relaxes
    // to 1.2 items/1 cost = 1.2 items per unit cost - strictly better - so
    // an all-bin-type-1 solution is cheapest in the relaxation: covering
    // all 5 copies needs k_1 = 5 / 1.2 = 25/6 (relaxed) bins, for a total
    // cost of 25/6.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::VariableSizedBinPacking);
    BinTypeId bin_type_id_0 = instance_builder.add_bin_type(10, 10);
    instance_builder.set_bin_type_cost(bin_type_id_0, 10);
    instance_builder.set_bin_type_copies(bin_type_id_0, -1);
    BinTypeId bin_type_id_1 = instance_builder.add_bin_type(6, 6);
    instance_builder.set_bin_type_cost(bin_type_id_1, 1);
    instance_builder.set_bin_type_copies(bin_type_id_1, -1);
    ItemTypeId item_type_id = instance_builder.add_item_type(5, 5, true);
    instance_builder.set_item_type_copies(item_type_id, 5);
    Instance instance = instance_builder.build();

    BarRelaxationParameters parameters;
    parameters.verbosity_level = 0;
    BarRelaxationOutput output = bar_relaxation(instance, parameters);

    EXPECT_TRUE(equal_cost(output.variable_sized_bin_packing_bound, 25.0 / 6.0));
}

struct RectangleBarRelaxationBoundTestParams
{
    fs::path items_path;
    fs::path bins_path;
    fs::path parameters_path;
    // Meaningful only for a 'Knapsack' instance.
    Profit expected_knapsack_bound = 0;
    // Meaningful only for a 'Feasibility' instance.
    bool expected_is_proven_infeasible = false;
};

inline std::ostream& operator<<(
        std::ostream& os,
        const RectangleBarRelaxationBoundTestParams& test_params)
{
    os << test_params.items_path;
    return os;
}

class RectangleBarRelaxationBoundTest: public testing::TestWithParam<RectangleBarRelaxationBoundTestParams> { };

TEST_P(RectangleBarRelaxationBoundTest, RectangleBarRelaxationBound)
{
    RectangleBarRelaxationBoundTestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    instance_builder.read_item_types(test_params.items_path.string());
    instance_builder.read_bin_types(test_params.bins_path.string());
    instance_builder.read_parameters(test_params.parameters_path.string());
    Instance instance = instance_builder.build();

    BarRelaxationParameters parameters;
    parameters.verbosity_level = 0;
    BarRelaxationOutput output = bar_relaxation(instance, parameters);

    if (instance.objective() == Objective::Knapsack) {
        EXPECT_TRUE(equal_profit(output.knapsack_bound, test_params.expected_knapsack_bound));
    } else {
        EXPECT_EQ(output.is_proven_infeasible, test_params.expected_is_proven_infeasible);
    }
}

INSTANTIATE_TEST_SUITE_P(
        Rectangle,
        RectangleBarRelaxationBoundTest,
        testing::ValuesIn(std::vector<RectangleBarRelaxationBoundTestParams>{
            {
                // Items 12x6, 8x3, 16x4, 4x7, 4x3 tile the 20x10 bin
                // exactly (total area 200 == bin area): the bar relaxation
                // is exact whenever bar-patterns can reconstruct an actual
                // packing, so it finds the true optimum of 200 (the sum of
                // all item profits, which default to their area), not
                // merely an over-estimate.
                fs::path("data") / "rectangle" / "tests" / "knapsack_all_items_fit_exactly" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_all_items_fit_exactly" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_all_items_fit_exactly" / "parameters.csv",
                200,
            }, {
                fs::path("data") / "rectangle" / "tests" / "knapsack_area_domination_mismatch" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_area_domination_mismatch" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_area_domination_mismatch" / "parameters.csv",
                25,
            }, {
                // A loose bound (true optimum is 450, achieved by tree
                // search): the bar relaxation is generally not tight, this
                // just pins down its actual value on this instance.
                fs::path("data") / "rectangle" / "tests" / "knapsack_high_profit_item_unpacked" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_high_profit_item_unpacked" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_high_profit_item_unpacked" / "parameters.csv",
                1597.0 / 3.0,
            }, {
                // Also loose (true optimum is 200): the two item types
                // cannot both fit the bin at once, a constraint the bar
                // relaxation - which never tracks actual positions - does
                // not capture.
                fs::path("data") / "rectangle" / "tests" / "knapsack_incompatible_item_pair" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_incompatible_item_pair" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_incompatible_item_pair" / "parameters.csv",
                270,
            }, {
                fs::path("data") / "rectangle" / "tests" / "knapsack_non_square_bin_rotatable_items_fit_side_by_side" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_non_square_bin_rotatable_items_fit_side_by_side" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_non_square_bin_rotatable_items_fit_side_by_side" / "parameters.csv",
                2,
            }, {
                // Loose again (true optimum is 3, achieved by tree search):
                // the bar relaxation cannot see that a fourth copy would
                // overlap the other three once actually placed.
                fs::path("data") / "rectangle" / "tests" / "knapsack_non_square_bin_too_many_rotatable_items" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_non_square_bin_too_many_rotatable_items" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_non_square_bin_too_many_rotatable_items" / "parameters.csv",
                40.0 / 11.0,
            }, {
                // Exact tiling (4 copies of a 5x5 item in a 10x10 bin, same
                // as 'SquareGridBoundIsTight'), genuinely feasible: the
                // relaxation can only ever prove infeasibility, never
                // feasibility, so it must not raise a false positive here.
                fs::path("data") / "rectangle" / "tests" / "feasibility_exact_tiling_feasible" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "feasibility_exact_tiling_feasible" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "feasibility_exact_tiling_feasible" / "parameters.csv",
                0,
                false,
            }, {
                // The item (20x20) is wider and taller than the bin (10x10)
                // in both orientations, so it is not eligible for any bin
                // type at all: its item row (exact-match for Feasibility)
                // can then only be satisfied by a dummy column - a sound,
                // easy-to-prove infeasibility.
                fs::path("data") / "rectangle" / "tests" / "feasibility_item_too_big_infeasible" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "feasibility_item_too_big_infeasible" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "feasibility_item_too_big_infeasible" / "parameters.csv",
                0,
                true,
            }}));
