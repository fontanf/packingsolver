#include "packingsolver/irregular/instance_builder.hpp"
#include "irregular/global_nonlinear_programming.hpp"
#include "irregular/solution_builder.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver;
using namespace packingsolver::irregular;
namespace fs = boost::filesystem;

namespace
{

fs::path instance_path(const std::string& name)
{
    return fs::path("data") / "irregular" / "tests" / (name + ".json");
}

fs::path solution_path(const std::string& name)
{
    return fs::path("data") / "irregular" / "tests" / (name + "_solution.json");
}

Instance read_instance(const std::string& name)
{
    InstanceBuilder instance_builder;
    instance_builder.read(instance_path(name).string());
    return instance_builder.build();
}

}

class IrregularGlobalNonlinearProgrammingTest: public testing::TestWithParam<std::string> { };

TEST_P(IrregularGlobalNonlinearProgrammingTest, IrregularGlobalNonlinearProgramming)
{
    // The instance is feasible iff it has a solution file; then the
    // algorithm must find a solution with all the items, otherwise it must
    // prove that the instance is infeasible.
    std::string name = GetParam();
    std::cout << "Instance: " << name << std::endl;
    Instance instance = read_instance(name);
    bool feasible = fs::exists(solution_path(name));
    if (feasible) {
        SolutionBuilder solution_builder(instance);
        solution_builder.read(solution_path(name).string());
        Solution solution = solution_builder.build();
        ASSERT_EQ(solution.number_of_items(), instance.number_of_items());
    }

    EXPECT_TRUE(global_nonlinear_programming_supports(instance));
    GlobalNonlinearProgrammingParameters parameters;
    parameters.verbosity_level = 0;
    GlobalNonlinearProgrammingOutput output = global_nonlinear_programming(instance, parameters);
    EXPECT_EQ(output.is_proven_infeasible, !feasible);
    if (feasible) {
        const Solution& solution = output.solution_pool.best();
        EXPECT_EQ(solution.number_of_items(), instance.number_of_items());
        EXPECT_TRUE(solution.feasible());
    }
}

INSTANTIATE_TEST_SUITE_P(
        Irregular,
        IrregularGlobalNonlinearProgrammingTest,
        testing::ValuesIn(std::vector<std::string>{
                // A 13 x 1 rectangle fits in a 10 x 10 bin rotated by about 45
                // degrees.
                "feasibility_continuous_rotations_diagonal_rectangle",
                // A 13.5 x 1 rectangle doesn't.
                "feasibility_continuous_rotations_rectangle_too_long",
                // Two squares of side a fit in a square of side s iff
                // s >= 2 a, whatever their rotations.
                "feasibility_continuous_rotations_two_squares",
                // Non-convex items: the pairs, then the subset of the three
                // items, are solved.
                "feasibility_continuous_rotations_l_shapes",
                // A 4 x 6 item which can only be rotated by 0 or 90 degrees,
                // and a 4 x 4 item: they only fit in the 11 x 5 bin with the
                // first one rotated by 90 degrees.
                "feasibility_discrete_rotations_two_ranges",
                }));

TEST(IrregularGlobalNonlinearProgramming, UnsupportedObjective)
{
    InstanceBuilder instance_builder;
    instance_builder.read(instance_path("free_rotation").string());
    Instance instance = instance_builder.build();
    EXPECT_FALSE(global_nonlinear_programming_supports(instance));
    GlobalNonlinearProgrammingParameters parameters;
    parameters.verbosity_level = 0;
    EXPECT_THROW(global_nonlinear_programming(instance, parameters), std::invalid_argument);
}

TEST(IrregularGlobalNonlinearProgramming, NonConvexBin)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Feasibility);
    Shape bin_shape;
    std::vector<Point> points = {{0, 0}, {10, 0}, {10, 4}, {4, 4}, {4, 10}, {0, 10}};
    for (size_t pos = 0; pos < points.size(); ++pos)
        bin_shape.elements.push_back(build_line_segment(points[pos], points[(pos + 1) % points.size()]));
    instance_builder.add_bin_type(bin_shape);
    ItemShape item_shape;
    item_shape.shape_orig.shape = build_square(2);
    instance_builder.add_item_type({item_shape});
    Instance instance = instance_builder.build();
    EXPECT_FALSE(global_nonlinear_programming_supports(instance));
    GlobalNonlinearProgrammingParameters parameters;
    parameters.verbosity_level = 0;
    EXPECT_THROW(global_nonlinear_programming(instance, parameters), std::invalid_argument);
}
