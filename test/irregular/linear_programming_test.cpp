#include "packingsolver/irregular/instance_builder.hpp"
#include "irregular/linear_programming.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver;
using namespace packingsolver::irregular;
namespace fs = boost::filesystem;

struct TestParams
{
    fs::path instance_path;
    fs::path initial_solution_path;
    Corner anchor_corner;
    fs::path expected_solution_path;
};

inline std::ostream& operator<<(std::ostream& os, const TestParams& test_params)
{
    os << test_params.instance_path;
    return os;
}

class LinearProgrammingTest: public testing::TestWithParam<TestParams> { };

TEST_P(LinearProgrammingTest, LinearProgramming)
{
    TestParams test_params = GetParam();
    std::cout << "Instance path: " << test_params.instance_path << std::endl;
    InstanceBuilder instance_builder;
    instance_builder.read(test_params.instance_path.string());
    Instance instance = instance_builder.build();

    std::cout << "Initial solution path: " << test_params.initial_solution_path << std::endl;
    Solution initial_solution(instance, test_params.initial_solution_path.string());

    std::cout << "Expected solution path: " << test_params.expected_solution_path << std::endl;
    Solution expected_solution(instance, test_params.expected_solution_path.string());

    LinearProgrammingAnchorToCornerParameters lp_parameters;
    lp_parameters.anchor_corner = test_params.anchor_corner;
    LinearProgrammingAnchorToCornerOutput lp_output = linear_programming_anchor_to_corner(
            initial_solution,
            lp_parameters);
    const Solution& solution = lp_output.solution_pool.best();

    std::cout << std::endl
        << "Expected solution" << std::endl
        << "-----------------" << std::endl;
    expected_solution.format(std::cout);

    std::cout << std::endl
        << "LP solution" << std::endl
        << "-----------" << std::endl;
    solution.format(std::cout);

    EXPECT_EQ(solution.number_of_different_bins(), expected_solution.number_of_different_bins());
    for (BinPos bin_pos = 0;
            bin_pos < (BinPos)expected_solution.number_of_different_bins();
            ++bin_pos) {
        const auto& sol_bin = solution.bin(bin_pos);
        const auto& exp_bin = expected_solution.bin(bin_pos);
        EXPECT_EQ(sol_bin.items.size(), exp_bin.items.size());
        for (ItemPos item_pos = 0;
                item_pos < (ItemPos)exp_bin.items.size();
                ++item_pos) {
            const SolutionItem& sol_item = sol_bin.items[item_pos];
            const SolutionItem& exp_item = exp_bin.items[item_pos];
            EXPECT_EQ(sol_item.item_type_id, exp_item.item_type_id);
            EXPECT_NEAR(sol_item.bl_corner.x, exp_item.bl_corner.x, 1e-3);
            EXPECT_NEAR(sol_item.bl_corner.y, exp_item.bl_corner.y, 1e-3);
            EXPECT_EQ(sol_item.angle, exp_item.angle);
            EXPECT_EQ(sol_item.mirror, exp_item.mirror);
        }
    }
}

INSTANTIATE_TEST_SUITE_P(
        Irregular,
        LinearProgrammingTest,
        testing::ValuesIn(std::vector<TestParams>{
            {
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor_to_corner/lp_single_item.json",
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor_to_corner/lp_single_item_initial.json",
                Corner::BottomLeft,
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor_to_corner/lp_single_item_expected.json"
            }, {
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor_to_corner/lp_two_items.json",
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor_to_corner/lp_two_items_initial.json",
                Corner::BottomLeft,
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor_to_corner/lp_two_items_expected.json"
            }, {
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor_to_corner/lp_rotation_90.json",
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor_to_corner/lp_rotation_90_initial.json",
                Corner::BottomLeft,
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor_to_corner/lp_rotation_90_expected.json"
            }, {
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor_to_corner/lp_anchor_top_right.json",
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor_to_corner/lp_anchor_top_right_initial.json",
                Corner::TopRight,
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor_to_corner/lp_anchor_top_right_expected.json"
            }, {
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor_to_corner/lp_test.json",
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor_to_corner/lp_test_initial.json",
                Corner::BottomLeft,
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor_to_corner/lp_test_expected.json"
            }}));
