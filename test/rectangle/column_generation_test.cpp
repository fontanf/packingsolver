#include "algorithms/column_generation.hpp"
#include "packingsolver/rectangle/instance_builder.hpp"
#include "packingsolver/rectangle/optimize.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

#include <unordered_set>

using namespace packingsolver;
using namespace packingsolver::rectangle;
namespace fs = boost::filesystem;

////////////////////////////////////////////////////////////////////////////////
///////////////////// RectangleColumnGenerationBinPackingTest ////////////////////
////////////////////////////////////////////////////////////////////////////////

struct RectangleColumnGenerationBinPackingTestParams
{
    fs::path items_path;
    fs::path bins_path;
    fs::path parameters_path;
    BinPos expected_number_of_bins;
};

inline std::ostream& operator<<(std::ostream& os, const RectangleColumnGenerationBinPackingTestParams& test_params)
{
    os << test_params.items_path;
    return os;
}

class RectangleColumnGenerationBinPackingTest: public testing::TestWithParam<RectangleColumnGenerationBinPackingTestParams> { };

TEST_P(RectangleColumnGenerationBinPackingTest, RectangleColumnGenerationBinPacking)
{
    // Exercises 'column_generation''s sequential feasibility scheme (see
    // 'ColumnGenerationParameters::use_sequential_feasibility' in
    // 'algorithms/column_generation.hpp'), used by default for the
    // 'BinPacking' objective and required (regardless of the parameter) once
    // there is more than one bin type - column generation's usual bound
    // conversion (dividing the master's cost-minimizing LP bound by
    // bin_type(0).cost) is not valid there, so it can only be solved to
    // optimality this way.
    RectangleColumnGenerationBinPackingTestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    instance_builder.read_item_types(test_params.items_path.string());
    instance_builder.read_bin_types(test_params.bins_path.string());
    instance_builder.read_parameters(test_params.parameters_path.string());
    Instance instance = instance_builder.build();

    OptimizeParameters parameters;
    parameters.use_tree_search = false;
    parameters.use_tree_search_maximal_spaces = false;
    parameters.use_sequential_value_correction = false;
    parameters.use_column_generation = true;
    parameters.use_dichotomic_search = false;
    parameters.use_sequential_single_knapsack = false;
    parameters.use_benders_decomposition = false;
    parameters.verbosity_level = 0;
    rectangle::Output output = optimize(instance, parameters);
    const Solution& solution = output.solution_pool.best();
    EXPECT_TRUE(solution.full());
    EXPECT_EQ(solution.number_of_bins(), test_params.expected_number_of_bins);
    EXPECT_EQ(output.bin_packing_bound, test_params.expected_number_of_bins);
}

INSTANTIATE_TEST_SUITE_P(
        Rectangle,
        RectangleColumnGenerationBinPackingTest,
        testing::ValuesIn(std::vector<RectangleColumnGenerationBinPackingTestParams>{
            {
                // Two bin types of different costs (50 and 80): column
                // generation's usual bound conversion is unsound here, so
                // this can only be solved to optimality via the sequential
                // feasibility scheme. Optimal solution uses 3 bins (2 of the
                // cheaper type, 1 of the costlier one).
                fs::path("data") / "rectangle" / "tests" / "bin_packing_multiple_bin_types_heterogeneous_cost" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_multiple_bin_types_heterogeneous_cost" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_multiple_bin_types_heterogeneous_cost" / "parameters.csv",
                3,
            }, {
                // Reused from 'benders_decomposition_test.cpp': two bin
                // types (same cost here, but still more than one, so this
                // also goes through the sequential feasibility scheme by
                // default), 3 copies of a single item type. Reference
                // solution (see its own 'solution.csv') uses 1 bin of the
                // first type and 2 of the second, 3 bins total.
                fs::path("data") / "rectangle" / "tests" / "bin_packing_two_bin_types" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_two_bin_types" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_two_bin_types" / "parameters.csv",
                3,
            }, {
                // Reused from 'benders_decomposition_test.cpp': a single bin
                // type but two different item sizes that must be mixed
                // within a bin to reach the optimum of 2 bins (see its own
                // 'solution.csv') - exercises column generation's regular
                // item-mix pricing rather than the sequential feasibility
                // scheme (only one bin type here).
                fs::path("data") / "rectangle" / "tests" / "bin_packing_mixed_items_two_bins" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_mixed_items_two_bins" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_mixed_items_two_bins" / "parameters.csv",
                2,
            }}));

////////////////////////////////////////////////////////////////////////////////
///////////////// RectangleColumnGenerationVariableSizedBinPackingTest ///////////
////////////////////////////////////////////////////////////////////////////////

struct RectangleColumnGenerationVariableSizedBinPackingTestParams
{
    fs::path items_path;
    fs::path bins_path;
    fs::path parameters_path;
    BinPos expected_number_of_bins;
};

inline std::ostream& operator<<(std::ostream& os, const RectangleColumnGenerationVariableSizedBinPackingTestParams& test_params)
{
    os << test_params.items_path;
    return os;
}

class RectangleColumnGenerationVariableSizedBinPackingTest: public testing::TestWithParam<RectangleColumnGenerationVariableSizedBinPackingTestParams> { };

TEST_P(RectangleColumnGenerationVariableSizedBinPackingTest, RectangleColumnGenerationVariableSizedBinPacking)
{
    // Regression check that the 'VariableSizedBinPacking' objective's own
    // bound conversion (which multiplies rather than divides by a bin cost -
    // see 'column_generation''s 'new_bound_callback') is unaffected by the
    // 'BinPacking'-only sequential feasibility scheme added alongside it.
    RectangleColumnGenerationVariableSizedBinPackingTestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    instance_builder.read_item_types(test_params.items_path.string());
    instance_builder.read_bin_types(test_params.bins_path.string());
    instance_builder.read_parameters(test_params.parameters_path.string());
    Instance instance = instance_builder.build();

    OptimizeParameters parameters;
    parameters.use_tree_search = false;
    parameters.use_tree_search_maximal_spaces = false;
    parameters.use_sequential_value_correction = false;
    parameters.use_column_generation = true;
    parameters.use_dichotomic_search = false;
    parameters.use_sequential_single_knapsack = false;
    parameters.use_benders_decomposition = false;
    parameters.verbosity_level = 0;
    rectangle::Output output = optimize(instance, parameters);
    const Solution& solution = output.solution_pool.best();
    EXPECT_TRUE(solution.full());
    EXPECT_EQ(solution.number_of_bins(), test_params.expected_number_of_bins);
}

INSTANTIATE_TEST_SUITE_P(
        Rectangle,
        RectangleColumnGenerationVariableSizedBinPackingTest,
        testing::ValuesIn(std::vector<RectangleColumnGenerationVariableSizedBinPackingTestParams>{
            {
                // Two bin types, one copy of each: a cheap 15x15 bin too
                // small to fit both 10x10 items together, and a costlier
                // 20x30 bin that fits both at once. Using the cheap bin
                // still requires the costly one for the second item (total
                // cost 1 + 3 = 4), so the optimum (see its own
                // 'solution.csv') instead uses only the costly bin alone
                // (cost 3), for 1 bin total.
                fs::path("data") / "rectangle" / "tests" / "variable_sized_bin_packing" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "variable_sized_bin_packing" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "variable_sized_bin_packing" / "parameters.csv",
                1,
            }}));

////////////////////////////////////////////////////////////////////////////////
///////////////////// RectangleSubsetRowCardinality5Test /////////////////////////
////////////////////////////////////////////////////////////////////////////////

namespace
{

using CgsColumn = columngenerationsolver::Column;
using CgsModel = columngenerationsolver::Model;
using CgsSolution = columngenerationsolver::Solution;
using CgsSolutionBuilder = columngenerationsolver::SolutionBuilder;

std::shared_ptr<const CgsColumn> make_column(
        BinTypeId bin_type_id,
        const std::vector<ItemTypeId>& item_type_ids,
        BinTypeId number_of_bin_types)
{
    auto column = std::make_shared<CgsColumn>();
    column->elements.push_back({bin_type_id, 1.0});
    for (ItemTypeId item_type_id: item_type_ids)
        column->elements.push_back({number_of_bin_types + item_type_id, 1.0});
    return column;
}

}

TEST(RectangleSubsetRowCardinality5Test, SeparatesCardinality5Cut)
{
    // Reproduces, item-type-id for item-type-id, a fractional LP node from
    // a real run: 18 item types (mapped here to item type ids 0..17, in the
    // same order as the original row ids
    // 12,14,16,19,22,24,27,32,38,40,42,45,51,53,57,58,60,62), 16 fractional
    // columns. Several candidate triples over these columns are violated
    // (confirmed independently by exhaustive search over the raw numbers),
    // the strongest by 0.384615 - but several 5-item-type subsets are
    // violated by a full 0.5, more than any triple, so the single cut
    // 'separate_cuts' returns (capped at 1 per round) should be one of
    // those quintuples, not the best triple. This checks that ranking
    // actually happens, not just that the math works out on paper.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10);
    for (int item = 0; item < 18; ++item)
        instance_builder.add_item_type(1, 1, true);
    Instance instance = instance_builder.build();
    rectangle::Output output(instance);
    ColumnGenerationPricingFunction<Instance, InstanceBuilder, Solution, rectangle::Output> pricing_function
        = [](const Instance&, PricingType) -> rectangle::Output
        {
            throw std::logic_error("not used by this test");
        };
    ColumnGenerationPricingSolver<Instance, InstanceBuilder, Solution, rectangle::Output> pricing_solver(
            instance, output, pricing_function);

    CgsModel model;
    model.rows.resize(instance.number_of_bin_types() + instance.number_of_item_types());

    struct ColumnSpec
    {
        std::vector<ItemTypeId> item_type_ids;
        Value value;
    };
    std::vector<ColumnSpec> column_specs = {
        {{2, 6, 8, 9, 15}, 0.5},
        {{9, 12, 14}, 3.0 / 13.0},
        {{11, 14, 16}, 0.5},
        {{1, 7, 17}, 9.0 / 26.0},
        {{0, 4, 9, 14}, 3.0 / 13.0},
        {{1, 5, 7}, 3.0 / 26.0},
        {{11, 13, 16}, 0.5},
        {{0, 3, 5, 7}, 7.0 / 26.0},
        {{5, 7, 10, 17}, 3.0 / 13.0},
        {{3, 4, 10, 12}, 7.0 / 26.0},
        {{2, 6, 8, 13, 15}, 0.5},
        {{3, 4, 5, 17}, 5.0 / 13.0},
        {{0, 4, 7, 12, 17}, 1.0 / 26.0},
        {{0, 1, 10, 12}, 6.0 / 13.0},
        {{1, 3, 4}, 1.0 / 13.0},
        {{9, 10, 14}, 1.0 / 26.0},
    };
    CgsSolutionBuilder solution_builder;
    solution_builder.set_model(model);
    for (const ColumnSpec& column_spec: column_specs) {
        solution_builder.add_column(
                make_column(bin_type_id, column_spec.item_type_ids, instance.number_of_bin_types()),
                column_spec.value);
    }
    CgsSolution solution = solution_builder.build();

    std::vector<std::shared_ptr<const columngenerationsolver::Cut>> cuts
        = pricing_solver.separate_cuts(solution);

    ASSERT_FALSE(cuts.empty());
    const SubsetRowCutExtra& extra
        = *std::static_pointer_cast<SubsetRowCutExtra>(cuts[0]->extra);
    // The best triple over these columns is violated by only 0.384615 (by
    // exhaustive search over the raw numbers, independent of this
    // template's own heuristics), less than the 0.5 several quintuples -
    // and, tied with them, several 7-item-type subsets too - reach, so the
    // single returned cut should be cardinality 5 or 7, never 3.
    ASSERT_TRUE(extra.item_type_ids.size() == 5U || extra.item_type_ids.size() == 7U)
        << "size: " << extra.item_type_ids.size();
    EXPECT_EQ(cuts[0]->upper_bound, (Value)(extra.item_type_ids.size() / 2));

    // Recompute the cut's violation directly against 'column_specs' (not
    // relying on any of 'separate_cuts''s own machinery) as the real
    // correctness check: several subsets of this particular fractional
    // point tie at the maximum violation of 0.5 across both cardinalities
    // (confirmed by exhaustive search), so this checks the *magnitude*
    // 'separate_cuts' actually found rather than pinning down which of the
    // tied subsets its greedy search happened to land on.
    std::unordered_set<ItemTypeId> subset(extra.item_type_ids.begin(), extra.item_type_ids.end());
    Value lhs = 0.0;
    for (const ColumnSpec& column_spec: column_specs) {
        int count = 0;
        for (ItemTypeId item_type_id: column_spec.item_type_ids) {
            if (subset.count(item_type_id) > 0)
                ++count;
        }
        lhs += (Value)(count / 2) * column_spec.value;
    }
    EXPECT_NEAR(lhs - (Value)(extra.item_type_ids.size() / 2), 0.5, 1e-6);
}

////////////////////////////////////////////////////////////////////////////////
///////////////////// RectangleSubsetRowCardinality7Test /////////////////////////
////////////////////////////////////////////////////////////////////////////////

TEST(RectangleSubsetRowCardinality7Test, SeparatesCardinality7Cut)
{
    // 'column_items' below (item type ids and LP values) of a real
    // fractional node from 'martello1998/Class_07.2bp_80_7' (BinPacking,
    // no rotation, infinite bin copies), captured with cutting planes
    // enabled at every search node: neither the cardinality-3 nor the
    // cardinality-5 pass finds any violated cut here (confirmed
    // independently by exhaustive search over the raw numbers), but
    // several 7-item-type subsets are violated by a full 0.5 - e.g.
    // {26, 39, 44, 46, 52, 55, 56}: seven columns each contain exactly 2 of
    // the 7 ({44,56,59}, {39,50,56}, {26,37,46,57}, {15,26,37,52,57},
    // {39,55,60}, {46,55,60}, {44,52,59}, each valued 0.5), so
    // floor(2/2) = 1 per column, summing to 3.5 > floor(7/2) = 3. This
    // checks 'separate_cuts' actually reaches cardinality 7, not just that
    // the math works out on paper.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10);
    for (int item = 0; item < 61; ++item)
        instance_builder.add_item_type(1, 1, true);
    Instance instance = instance_builder.build();
    rectangle::Output output(instance);
    ColumnGenerationPricingFunction<Instance, InstanceBuilder, Solution, rectangle::Output> pricing_function
        = [](const Instance&, PricingType) -> rectangle::Output
        {
            throw std::logic_error("not used by this test");
        };
    ColumnGenerationPricingSolver<Instance, InstanceBuilder, Solution, rectangle::Output> pricing_solver(
            instance, output, pricing_function);

    CgsModel model;
    model.rows.resize(instance.number_of_bin_types() + instance.number_of_item_types());

    struct ColumnSpec
    {
        std::vector<ItemTypeId> item_type_ids;
        Value value;
    };
    std::vector<ColumnSpec> column_specs = {
        {{3, 22}, 0.286667},
        {{1, 49}, 0.166667},
        {{2, 40}, 0.113333},
        {{44, 56, 59}, 0.5},
        {{39, 50, 56}, 0.5},
        {{25, 50, 53}, 0.113333},
        {{31, 47, 53}, 0.16},
        {{1, 43, 47, 49}, 0.413333},
        {{26, 37, 46, 57}, 0.5},
        {{3, 11, 22}, 0.26},
        {{0, 12}, 0.5},
        {{49, 50}, 0.306667},
        {{12, 15, 22, 40}, 0.34},
        {{2, 11, 41, 47}, 0.24},
        {{11, 47}, 0.0266667},
        {{1, 3, 40, 43}, 0.0733333},
        {{15, 26, 37, 52, 57}, 0.5},
        {{39, 55, 60}, 0.5},
        {{2, 25}, 0.3},
        {{3, 53}, 0.0266667},
        {{31, 41, 53}, 0.413333},
        {{46, 55, 60}, 0.5},
        {{3, 43}, 0.273333},
        {{41}, 0.3},
        {{11, 53}, 0.286667},
        {{1, 2, 31}, 0.346667},
        {{44, 52, 59}, 0.5},
        {{0}, 0.5},
        {{12, 15, 25, 40, 43, 47}, 0.16},
        {{11, 25, 40}, 0.14},
        {{22, 25, 49}, 0.113333},
        {{25, 40}, 0.173333},
        {{11, 41}, 0.0466667},
        {{3, 31, 43, 50}, 0.08},
    };
    CgsSolutionBuilder solution_builder;
    solution_builder.set_model(model);
    for (const ColumnSpec& column_spec: column_specs) {
        solution_builder.add_column(
                make_column(bin_type_id, column_spec.item_type_ids, instance.number_of_bin_types()),
                column_spec.value);
    }
    CgsSolution solution = solution_builder.build();

    std::vector<std::shared_ptr<const columngenerationsolver::Cut>> cuts
        = pricing_solver.separate_cuts(solution);

    ASSERT_FALSE(cuts.empty());
    const SubsetRowCutExtra& extra
        = *std::static_pointer_cast<SubsetRowCutExtra>(cuts[0]->extra);
    // Neither cardinality 3 nor 5 has any violated candidate here, so the
    // cut found can only come from the cardinality-7 extension.
    ASSERT_EQ(extra.item_type_ids.size(), 7U);
    EXPECT_EQ(cuts[0]->upper_bound, 3.0);

    // Recompute the cut's violation directly against 'column_specs', the
    // real correctness check (same reasoning as the cardinality-5 test
    // above): several 7-item-type subsets tie at the maximum violation of
    // 0.5, so this checks the magnitude found rather than pinning down
    // which tied subset the beam search happened to land on.
    std::unordered_set<ItemTypeId> subset(extra.item_type_ids.begin(), extra.item_type_ids.end());
    Value lhs = 0.0;
    for (const ColumnSpec& column_spec: column_specs) {
        int count = 0;
        for (ItemTypeId item_type_id: column_spec.item_type_ids) {
            if (subset.count(item_type_id) > 0)
                ++count;
        }
        lhs += (Value)(count / 2) * column_spec.value;
    }
    EXPECT_NEAR(lhs - 3.0, 0.5, 1e-6);
}
