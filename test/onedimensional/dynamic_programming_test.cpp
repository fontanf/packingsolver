#include "packingsolver/onedimensional/instance_builder.hpp"
#include "onedimensional/dynamic_programming.hpp"
#include "onedimensional/solution_builder.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver;
using namespace packingsolver::onedimensional;
namespace fs = boost::filesystem;

struct OneDimensionalDynamicProgrammingTestParams
{
    fs::path items_path;
    fs::path bins_path;
    fs::path parameters_path;
    fs::path certificate_path;

    /** Maximum size of the dynamic programming of the subset sum problems. */
    Counter subset_sum_maximum_size;

    /** Maximum number of states of the primal-dual dynamic programming. */
    Counter knapsack_maximum_number_of_states;

    /** True iff the dynamic programming is expected to be exact. */
    bool exact;
};

inline std::ostream& operator<<(std::ostream& os, const OneDimensionalDynamicProgrammingTestParams& test_params)
{
    os << test_params.items_path
        << " " << test_params.subset_sum_maximum_size
        << " " << test_params.knapsack_maximum_number_of_states;
    return os;
}

class OneDimensionalDynamicProgrammingTest: public testing::TestWithParam<OneDimensionalDynamicProgrammingTestParams> { };

TEST_P(OneDimensionalDynamicProgrammingTest, OneDimensionalDynamicProgramming)
{
    OneDimensionalDynamicProgrammingTestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    instance_builder.read_item_types(test_params.items_path.string());
    instance_builder.read_bin_types(test_params.bins_path.string());
    instance_builder.read_parameters(test_params.parameters_path.string());
    Instance instance = instance_builder.build();

    DynamicProgrammingParameters dp_parameters;
    dp_parameters.verbosity_level = 0;
    dp_parameters.subset_sum_maximum_size = test_params.subset_sum_maximum_size;
    dp_parameters.knapsack_maximum_number_of_states = test_params.knapsack_maximum_number_of_states;
    DynamicProgrammingOutput dp_output = dynamic_programming(instance, dp_parameters);

    SolutionBuilder solution_builder(instance);
    solution_builder.read(test_params.certificate_path.string());
    Solution solution = solution_builder.build();

    // The solution is feasible and the bound is valid.
    EXPECT_TRUE(dp_output.solution_pool.best().feasible());
    EXPECT_FALSE(strictly_greater_profit(
                dp_output.solution_pool.best().profit(),
                solution.profit()));
    EXPECT_FALSE(strictly_greater_profit(
                solution.profit(),
                dp_output.knapsack_bound));
    if (test_params.exact) {
        EXPECT_TRUE(equal_profit(
                    dp_output.solution_pool.best().profit(),
                    solution.profit()));
        EXPECT_TRUE(equal_profit(
                    dp_output.knapsack_bound,
                    solution.profit()));
    }
}

INSTANTIATE_TEST_SUITE_P(
        OneDimensional,
        OneDimensionalDynamicProgrammingTest,
        testing::ValuesIn(std::vector<OneDimensionalDynamicProgrammingTestParams>{
            {
                // Subset sum problem solved with the word RAM dynamic
                // programming.
                fs::path("data") / "onedimensional" / "tests" / "knapsack_subset_sum" / "items.csv",
                fs::path("data") / "onedimensional" / "tests" / "knapsack_subset_sum" / "bins.csv",
                fs::path("data") / "onedimensional" / "tests" / "knapsack_subset_sum" / "parameters.csv",
                fs::path("data") / "onedimensional" / "tests" / "knapsack_subset_sum" / "solution.csv",
                (1 << 23),
                (1 << 20),
                true,
            }, {
                // Subset sum problem above the maximum size, solved with the
                // primal-dual dynamic programming.
                fs::path("data") / "onedimensional" / "tests" / "knapsack_subset_sum" / "items.csv",
                fs::path("data") / "onedimensional" / "tests" / "knapsack_subset_sum" / "bins.csv",
                fs::path("data") / "onedimensional" / "tests" / "knapsack_subset_sum" / "parameters.csv",
                fs::path("data") / "onedimensional" / "tests" / "knapsack_subset_sum" / "solution.csv",
                8,
                (1 << 20),
                true,
            }, {
                // Same, the primal-dual dynamic programming stopping because
                // of the maximum number of states.
                fs::path("data") / "onedimensional" / "tests" / "knapsack_subset_sum" / "items.csv",
                fs::path("data") / "onedimensional" / "tests" / "knapsack_subset_sum" / "bins.csv",
                fs::path("data") / "onedimensional" / "tests" / "knapsack_subset_sum" / "parameters.csv",
                fs::path("data") / "onedimensional" / "tests" / "knapsack_subset_sum" / "solution.csv",
                8,
                1,
                false,
            }}));
