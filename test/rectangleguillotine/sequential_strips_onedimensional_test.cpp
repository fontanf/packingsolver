#include "packingsolver/rectangleguillotine/instance_builder.hpp"
#include "rectangleguillotine/sequential_strips_onedimensional.hpp"
#include "rectangleguillotine/solution_builder.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver::rectangleguillotine;
namespace fs = boost::filesystem;

struct RectangleGuillotineSequentialStripsOnedimensionalTestParams
{
    fs::path items_path;
    fs::path bins_path;
    fs::path parameters_path;
    fs::path certificate_path;
};

inline std::ostream& operator<<(std::ostream& os, const RectangleGuillotineSequentialStripsOnedimensionalTestParams& test_params)
{
    os << test_params.items_path;
    return os;
}

class RectangleGuillotineSequentialStripsOnedimensionalTest: public testing::TestWithParam<RectangleGuillotineSequentialStripsOnedimensionalTestParams> { };

TEST_P(RectangleGuillotineSequentialStripsOnedimensionalTest, RectangleGuillotineSequentialStripsOnedimensional)
{
    RectangleGuillotineSequentialStripsOnedimensionalTestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    instance_builder.read_item_types(test_params.items_path.string());
    instance_builder.read_bin_types(test_params.bins_path.string());
    instance_builder.read_parameters(test_params.parameters_path.string());
    Instance instance = instance_builder.build();

    SequentialStripsOnedimensionalParameters sso_parameters;
    SequentialStripsOnedimensionalOutput output = sequential_strips_onedimensional(instance, sso_parameters);

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
        RectangleGuillotineSequentialStripsOnedimensional,
        RectangleGuillotineSequentialStripsOnedimensionalTest,
        testing::ValuesIn(std::vector<RectangleGuillotineSequentialStripsOnedimensionalTestParams>{
            {
                fs::path("data") / "rectangleguillotine" / "tests" / "bin_packing_3nvr" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "bin_packing_3nvr" / "bins.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "bin_packing_3nvr" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "bin_packing_3nvr" / "solution.csv",
            }, {
                // 3 item types (1x10x20, 2x5x10, 1x10x10) that tile a 20x20
                // bin exactly, but only with a genuine 3-stage guillotine cut
                // (a 2-stage pattern needs 2 bins instead of 1).
                fs::path("data") / "rectangleguillotine" / "tests" / "bin_packing_3nvo" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "bin_packing_3nvo" / "bins.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "bin_packing_3nvo" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "bin_packing_3nvo" / "solution.csv",
            }
        }));
