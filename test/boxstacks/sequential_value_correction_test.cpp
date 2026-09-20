#include "packingsolver/boxstacks/instance_builder.hpp"
#include "packingsolver/boxstacks/optimize.hpp"
#include "boxstacks/solution_builder.hpp"

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>

using namespace packingsolver::boxstacks;
namespace fs = boost::filesystem;

struct BoxStacksSequentialValueCorrectionTestParams
{
    fs::path items_path;
    fs::path bins_path;
    fs::path parameters_path;
    fs::path certificate_path;
};

inline std::ostream& operator<<(std::ostream& os, const BoxStacksSequentialValueCorrectionTestParams& test_params)
{
    os << test_params.items_path;
    return os;
}

class BoxStacksSequentialValueCorrectionTest: public testing::TestWithParam<BoxStacksSequentialValueCorrectionTestParams> { };

TEST_P(BoxStacksSequentialValueCorrectionTest, BoxStacksSequentialValueCorrection)
{
    BoxStacksSequentialValueCorrectionTestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    instance_builder.read_item_types(test_params.items_path.string());
    instance_builder.read_bin_types(test_params.bins_path.string());
    instance_builder.read_parameters(test_params.parameters_path.string());
    Instance instance = instance_builder.build();

    // This instance is a multi-bin Knapsack instance (which always goes
    // through sequential value correction - see 'optimize.cpp') with more
    // item type 0 copies than fit and a single copy of item type 1 too
    // large to ever be worth including alongside it: the first iteration's
    // solution packs every bin with item type 0 alone, leaving item type 1
    // at 0 copies packed. Computing that item type's profit for the next
    // iteration used to divide by its (zero) packed copies, producing a
    // non-finite/non-positive profit that the next iteration's pricing
    // subproblem's own instance builder rejected with "Items must have
    // strictly positive profits." - see issue #587.
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
}

INSTANTIATE_TEST_SUITE_P(
        BoxStacksSequentialValueCorrection,
        BoxStacksSequentialValueCorrectionTest,
        testing::ValuesIn(std::vector<BoxStacksSequentialValueCorrectionTestParams>{
            {
                fs::path("data") / "boxstacks" / "tests" / "knapsack_multi_bin_svc_zero_copies_item_type" / "items.csv",
                fs::path("data") / "boxstacks" / "tests" / "knapsack_multi_bin_svc_zero_copies_item_type" / "bins.csv",
                fs::path("data") / "boxstacks" / "tests" / "knapsack_multi_bin_svc_zero_copies_item_type" / "parameters.csv",
                fs::path("data") / "boxstacks" / "tests" / "knapsack_multi_bin_svc_zero_copies_item_type" / "solution.csv",
            }}));
