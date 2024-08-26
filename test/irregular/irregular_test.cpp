#include "packingsolver/irregular/instance_builder.hpp"
#include "packingsolver/irregular/optimize.hpp"

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
    item_shape.shape = shape;

    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::VariableSizedBinPacking);
    instance_builder.add_item_type({item_shape}, -1, 10);
    instance_builder.add_bin_type(shape, -1, 10);
    const Instance instance = instance_builder.build();
    Solution solution(instance);
    solution.add_bin(0, 2);
    EXPECT_EQ(solution.number_of_bins(), 2);
    EXPECT_EQ(solution.bin_copies(0), 2);
}

struct TestParams
{
    fs::path instance_path;
    fs::path certificate_path;
};

inline std::ostream& operator<<(std::ostream& os, const TestParams& test_params)
{
    os << test_params.instance_path;
    return os;
}

class AlgorithmTest: public testing::TestWithParam<TestParams> { };

TEST_P(AlgorithmTest, Algorithm)
{
    TestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    instance_builder.read(test_params.instance_path.string());
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    Output output = optimize(instance, optimize_parameters);

    Solution solution(instance, test_params.certificate_path.string());
    EXPECT_EQ(!(output.solution_pool.best() < solution), true);
    EXPECT_EQ(!(solution < output.solution_pool.best()), true);
}

INSTANTIATE_TEST_SUITE_P(
        Irregular,
        AlgorithmTest,
        testing::ValuesIn(std::vector<TestParams>{
            {
                fs::path("data") / "irregular" / "tests" / "rectangles_non_guillotine.json",
                fs::path("data") / "irregular" / "tests" / "rectangles_non_guillotine_solution.json"
            }, {
                fs::path("data") / "irregular" / "tests" / "direction_y.json",
                fs::path("data") / "irregular" / "tests" / "direction_y_solution.json"
            }, {
                fs::path("data") / "irregular" / "users" / "2024-06-25.json",
                fs::path("data") / "irregular" / "users" / "2024-06-25_solution.json"
            }, {
                fs::path("data") / "irregular" / "tests" / "non_rectangular_bin.json",
                fs::path("data") / "irregular" / "tests" / "non_rectangular_bin_solution.json"
            }, {
                fs::path("data") / "irregular" / "tests" / "polygon_with_hole.json",
                fs::path("data") / "irregular" / "tests" / "polygon_with_hole_solution.json"
            }, {
                fs::path("data") / "irregular" / "users" / "2024-07-01.json",
                fs::path("data") / "irregular" / "users" / "2024-07-01_solution.json"
            }, {
                fs::path("data") / "irregular" / "tests" / "rotations.json",
                fs::path("data") / "irregular" / "tests" / "rotations_solution.json"
            }, {
                fs::path("data") / "irregular" / "tests" / "multiple_bins.json",
                fs::path("data") / "irregular" / "tests" / "multiple_bins_solution.json"
            }, {
                fs::path("data") / "irregular" / "users" / "2024-07-15.json",
                fs::path("data") / "irregular" / "users" / "2024-07-15_solution.json"
            }, {
                fs::path("data") / "irregular" / "tests" / "trapezoidal_insertions.json",
                fs::path("data") / "irregular" / "tests" / "trapezoidal_insertions_solution.json"
            }, {
                fs::path("data") / "irregular" / "tests" / "trapezoidal_insertions_2.json",
                fs::path("data") / "irregular" / "tests" / "trapezoidal_insertions_2_solution.json"
            }, {
                fs::path("data") / "irregular" / "tests" / "trapezoidal_insertions_3.json",
                fs::path("data") / "irregular" / "tests" / "trapezoidal_insertions_3_solution.json"
            }, {
                fs::path("data") / "irregular" / "tests" / "trapezoidal_insertions_4.json",
                fs::path("data") / "irregular" / "tests" / "trapezoidal_insertions_4_solution.json"
            }, {
                fs::path("data") / "irregular" / "users" / "2024-08-05.json",
                fs::path("data") / "irregular" / "users" / "2024-08-05_solution.json"
            }, {
                fs::path("data") / "irregular" / "users" / "2024-08-08_sub_1.json",
                fs::path("data") / "irregular" / "users" / "2024-08-08_sub_1_solution.json"
            }, {
                fs::path("data") / "irregular" / "users" / "2024-08-08_sub_2.json",
                fs::path("data") / "irregular" / "users" / "2024-08-08_sub_2_solution.json"
            }, {
                fs::path("data") / "irregular" / "users" / "2024-08-08_sub_3.json",
                fs::path("data") / "irregular" / "users" / "2024-08-08_sub_3_solution.json"
            }, {
                fs::path("data") / "irregular" / "users" / "2024-08-08.json",
                fs::path("data") / "irregular" / "users" / "2024-08-08_solution.json"
            }, {
                fs::path("data") / "irregular" / "users" / "2024-08-13.json",
                fs::path("data") / "irregular" / "users" / "2024-08-13_solution.json"
            }, {
                fs::path("data") / "irregular" / "tests" / "mirror.json",
                fs::path("data") / "irregular" / "tests" / "mirror_solution.json"
            }, {
                fs::path("data") / "irregular" / "users" / "2024-08-25_1.json",
                fs::path("data") / "irregular" / "users" / "2024-08-25_1_solution.json"
            }, {
                fs::path("data") / "irregular" / "users" / "2024-08-25_2.json",
                fs::path("data") / "irregular" / "users" / "2024-08-25_2_solution.json"
            }}));
