#include "packingsolver/boxstacks/instance_builder.hpp"
#include "boxstacks/sequential_onedimensional_rectangle.hpp"
#include "boxstacks/solution_builder.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver::boxstacks;
namespace fs = boost::filesystem;

struct BoxStacksSequentialOneDimensionalRectangleTestParams
{
    fs::path items_path;
    fs::path bins_path;
    fs::path parameters_path;
    fs::path certificate_path;
};

inline std::ostream& operator<<(std::ostream& os, const BoxStacksSequentialOneDimensionalRectangleTestParams& test_params)
{
    os << test_params.items_path;
    return os;
}

class BoxStacksSequentialOneDimensionalRectangleTest: public testing::TestWithParam<BoxStacksSequentialOneDimensionalRectangleTestParams> { };

TEST_P(BoxStacksSequentialOneDimensionalRectangleTest, BoxStacksSequentialOneDimensionalRectangle)
{
    BoxStacksSequentialOneDimensionalRectangleTestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    instance_builder.read_item_types(test_params.items_path.string());
    instance_builder.read_bin_types(test_params.bins_path.string());
    instance_builder.read_parameters(test_params.parameters_path.string());
    Instance instance = instance_builder.build();

    // These instances enter the axle weight repair loop, which used to read
    // 'fixed_items_solutions' one element past the end and throw
    // 'std::bad_array_new_length'/'std::bad_alloc'.
    SequentialOneDimensionalRectangleParameters sodr_parameters;
    SequentialOneDimensionalRectangleOutput output = sequential_onedimensional_rectangle(instance, sodr_parameters);

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
        BoxStacksSequentialOneDimensionalRectangle,
        BoxStacksSequentialOneDimensionalRectangleTest,
        testing::ValuesIn(std::vector<BoxStacksSequentialOneDimensionalRectangleTestParams>{
            {
                // The repair returns an empty solution for both instances:
                // called directly (unlike through 'optimize()'), the
                // algorithm has no later tree search pass to refine the
                // result, so this only pins down that it returns at all
                // instead of throwing.
                fs::path("data") / "boxstacks" / "tests" / "semi_trailer_truck_middle_axle_bin_packing" / "items.csv",
                fs::path("data") / "boxstacks" / "tests" / "semi_trailer_truck_middle_axle_bin_packing" / "bins.csv",
                fs::path("data") / "boxstacks" / "tests" / "semi_trailer_truck_middle_axle_bin_packing" / "parameters.csv",
                fs::path("data") / "boxstacks" / "tests" / "semi_trailer_truck_middle_axle_bin_packing" / "solution.csv",
            }, {
                fs::path("data") / "boxstacks" / "tests" / "semi_trailer_truck_middle_axle_knapsack" / "items.csv",
                fs::path("data") / "boxstacks" / "tests" / "semi_trailer_truck_middle_axle_knapsack" / "bins.csv",
                fs::path("data") / "boxstacks" / "tests" / "semi_trailer_truck_middle_axle_knapsack" / "parameters.csv",
                fs::path("data") / "boxstacks" / "tests" / "semi_trailer_truck_middle_axle_knapsack" / "solution.csv",
            }}));
