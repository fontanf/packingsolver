#include "packingsolver/irregular/instance_builder.hpp"
#include "packingsolver/irregular/optimize.hpp"
#include "irregular/solution_builder.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver::irregular;
namespace fs = boost::filesystem;

struct IrregularTreeSearchPeriodicPackingTestParams
{
    fs::path instance_path;
    fs::path certificate_path;
};

inline std::ostream& operator<<(std::ostream& os, const IrregularTreeSearchPeriodicPackingTestParams& test_params)
{
    os << test_params.instance_path;
    return os;
}

class IrregularTreeSearchPeriodicPackingTest: public testing::TestWithParam<IrregularTreeSearchPeriodicPackingTestParams> { };

TEST_P(IrregularTreeSearchPeriodicPackingTest, IrregularTreeSearchPeriodicPacking)
{
    IrregularTreeSearchPeriodicPackingTestParams test_params = GetParam();
    std::cout << "Instance path: " << test_params.instance_path << std::endl;
    std::cout << "Certificate path: " << test_params.certificate_path << std::endl;

    InstanceBuilder instance_builder;
    instance_builder.read(test_params.instance_path.string());
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    optimize_parameters.not_anytime_maximum_approximation_ratio = 0.01;
    optimize_parameters.use_tree_search_periodic_packing = true;
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
        Irregular,
        IrregularTreeSearchPeriodicPackingTest,
        testing::ValuesIn(std::vector<IrregularTreeSearchPeriodicPackingTestParams>{
            {
                fs::path("data") / "irregular" / "users" / "2025-12-08.json",
                fs::path("data") / "irregular" / "users" / "2025-12-08_solution.json"
            }, {
                fs::path("data") / "irregular" / "users" / "2026-04-08_sub_2.json",
                fs::path("data") / "irregular" / "users" / "2026-04-08_sub_2_solution.json"
            }, {
                fs::path("data") / "irregular" / "users" / "2026-04-08_sub_3.json",
                fs::path("data") / "irregular" / "users" / "2026-04-08_sub_3_solution.json"
            }, {
                fs::path("data") / "irregular" / "users" / "2026-04-08_sub_4.json",
                fs::path("data") / "irregular" / "users" / "2026-04-08_sub_4_solution.json"
            }, {
                fs::path("data") / "irregular" / "users" / "2026-04-23_sub.json",
                fs::path("data") / "irregular" / "users" / "2026-04-23_sub_solution.json"
            }, {
                fs::path("data") / "irregular" / "users" / "2026-04-23_sub_2.json",
                fs::path("data") / "irregular" / "users" / "2026-04-23_sub_2_solution.json"
            }}));
