#include "packingsolver/irregular/instance_builder.hpp"
#include "packingsolver/irregular/optimize.hpp"
#include "packingsolver/irregular/solution.hpp"
#include "irregular/solution_builder.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver::irregular;
namespace fs = boost::filesystem;

TEST(Irregular, BinCopies)
{
    Shape shape;
    ShapeElement shape_element;
    shape_element.type = ShapeElementType::LineSegment;
    shape_element.start = {0, 0};
    shape_element.end = {10, 0};
    shape.elements.push_back(shape_element);
    shape_element.start = {10, 0};
    shape_element.end = {10, 10};
    shape.elements.push_back(shape_element);
    shape_element.start = {10, 10};
    shape_element.end = {0, 10};
    shape.elements.push_back(shape_element);
    shape_element.start = {0, 10};
    shape_element.end = {0, 0};
    shape.elements.push_back(shape_element);

    ItemShape item_shape;
    item_shape.shape_orig.shape = shape;

    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::VariableSizedBinPacking);
    packingsolver::ItemTypeId item_type_id = instance_builder.add_item_type({item_shape});
    instance_builder.set_item_type_copies(item_type_id, 10);
    packingsolver::BinTypeId bin_type_id = instance_builder.add_bin_type(shape);
    instance_builder.set_bin_type_copies(bin_type_id, 10);
    const Instance instance = instance_builder.build();
    SolutionBuilder solution_builder(instance);
    solution_builder.add_bin(0, 2);
    Solution solution = solution_builder.build();
    EXPECT_EQ(solution.number_of_bins(), 2);
    EXPECT_EQ(solution.bin_copies(0), 2);
}

struct IrregularOptimizeTestParams
{
    fs::path instance_path;
    fs::path certificate_path;
};

inline std::ostream& operator<<(std::ostream& os, const IrregularOptimizeTestParams& test_params)
{
    os << test_params.instance_path;
    return os;
}

class IrregularOptimizeTest: public testing::TestWithParam<IrregularOptimizeTestParams> { };

TEST_P(IrregularOptimizeTest, IrregularOptimize)
{
    IrregularOptimizeTestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    instance_builder.read(test_params.instance_path.string());
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
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

    // The bound must not exceed the value of the reference solution, which
    // is feasible.
    switch (instance.objective()) {
    case packingsolver::Objective::BinPacking:
        EXPECT_LE(output.bin_packing_bound, solution.number_of_bins());
        break;
    case packingsolver::Objective::VariableSizedBinPacking:
        EXPECT_FALSE(packingsolver::strictly_lesser_cost(
                    solution.cost(),
                    output.variable_sized_bin_packing_bound));
        break;
    case packingsolver::Objective::Knapsack:
        EXPECT_FALSE(packingsolver::strictly_greater_profit(
                    solution.profit(),
                    output.knapsack_bound));
        break;
    default:
        break;
    }
}

INSTANTIATE_TEST_SUITE_P(
        Irregular,
        IrregularOptimizeTest,
        testing::ValuesIn(std::vector<IrregularOptimizeTestParams>{
            {
                // The lower bound of 'VariableSizedBinPacking' (the
                // one-dimensional trivial bound, which every problem type
                // uses through its one-dimensional relaxation) used to
                // require the optional bins alone to cover the items,
                // ignoring that the mandatory bins of a bin type with a
                // positive 'copies_min' hold items too. No two items fit in
                // the same bin: the optimum uses three bins of type 1
                // (cost 3), but the bound was 4.
                fs::path("data") / "irregular" / "tests" / "variable_sized_bin_packing_mandatory_bins.json",
                fs::path("data") / "irregular" / "tests" / "variable_sized_bin_packing_mandatory_bins_solution.json",
            }}));

TEST(Irregular, QualityRules)
{
    ItemShape item_shape;
    item_shape.shape_orig.shape = build_rectangle(0, 5, 0, 5);
    item_shape.quality_rule = 1;

    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::BinPacking);
    // Quality rule 0 allows defects of type 1; quality rule 1, of type 0.
    EXPECT_EQ(instance_builder.add_quality_rule({0, 1}), 0);
    EXPECT_EQ(instance_builder.add_quality_rule({1, 0}), 1);
    instance_builder.add_item_type({item_shape});
    instance_builder.add_bin_type(build_rectangle(0, 10, 0, 10));
    const Instance instance = instance_builder.build();

    EXPECT_FALSE(instance.can_contain(0, 0));
    EXPECT_TRUE(instance.can_contain(0, 1));
    EXPECT_TRUE(instance.can_contain(1, 0));
    EXPECT_FALSE(instance.can_contain(1, 1));
    // Out of range defect types and quality rules.
    EXPECT_FALSE(instance.can_contain(0, 2));
    EXPECT_FALSE(instance.can_contain(0, -1));
    EXPECT_FALSE(instance.can_contain(2, 0));
    EXPECT_FALSE(instance.can_contain(-1, 0));
}

TEST(Irregular, AddFixedItemInvalidIds)
{
    ItemShape item_shape;
    item_shape.shape_orig.shape = build_rectangle(0, 5, 0, 5);

    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::BinPacking);
    packingsolver::ItemTypeId item_type_id = instance_builder.add_item_type({item_shape});
    packingsolver::BinTypeId bin_type_id = instance_builder.add_bin_type(build_rectangle(0, 10, 0, 10));
    EXPECT_THROW(
            instance_builder.add_fixed_item(bin_type_id + 1, item_type_id, {0, 0}, 0, false),
            std::invalid_argument);
    EXPECT_THROW(
            instance_builder.add_fixed_item(-1, item_type_id, {0, 0}, 0, false),
            std::invalid_argument);
    EXPECT_THROW(
            instance_builder.add_fixed_item(bin_type_id, -1, {0, 0}, 0, false),
            std::invalid_argument);
    // The item type of a fixed item may be added after it, but must exist
    // when the instance is built.
    instance_builder.add_fixed_item(bin_type_id, item_type_id + 1, {0, 0}, 0, false);
    EXPECT_THROW(instance_builder.build(), std::invalid_argument);
}
