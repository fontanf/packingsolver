#include "packingsolver/onedimensional/instance_builder.hpp"
#include "packingsolver/onedimensional/optimize.hpp"
#include "onedimensional/solution_builder.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver::onedimensional;
namespace fs = boost::filesystem;

struct OneDimensionalMilpAssignmentTestParams
{
    fs::path items_path;
    fs::path bins_path;
    fs::path parameters_path;

    /** Path to the reference solution; if empty, the instance is expected to be proven infeasible. */
    fs::path certificate_path;

    /**
     * Path to a JSON instance file; if non-empty, used instead of
     * 'items_path'/'bins_path'/'parameters_path' (needed for features with
     * no CSV representation, e.g. resources).
     */
    fs::path instance_path;
};

inline std::ostream& operator<<(
        std::ostream& os,
        const OneDimensionalMilpAssignmentTestParams& test_params)
{
    os << (!test_params.instance_path.empty()? test_params.instance_path: test_params.items_path);
    return os;
}

class OneDimensionalMilpAssignmentTest: public testing::TestWithParam<OneDimensionalMilpAssignmentTestParams> { };

TEST_P(OneDimensionalMilpAssignmentTest, OneDimensionalMilpAssignment)
{
    OneDimensionalMilpAssignmentTestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    if (!test_params.instance_path.empty()) {
        instance_builder.read(test_params.instance_path.string());
    } else {
        instance_builder.read_item_types(test_params.items_path.string());
        instance_builder.read_bin_types(test_params.bins_path.string());
        instance_builder.read_parameters(test_params.parameters_path.string());
    }
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    optimize_parameters.use_milp_assignment = true;
    optimize_parameters.use_tree_search = false;
    optimize_parameters.use_sequential_single_knapsack = false;
    optimize_parameters.use_sequential_value_correction = false;
    optimize_parameters.use_dichotomic_search = false;
    optimize_parameters.use_column_generation = false;
    Output output = optimize(instance, optimize_parameters);

    if (test_params.certificate_path.empty()) {
        EXPECT_EQ(output.is_proven_infeasible, true);
        return;
    }

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
        OneDimensional,
        OneDimensionalMilpAssignmentTest,
        testing::ValuesIn(std::vector<OneDimensionalMilpAssignmentTestParams>{
            {
                fs::path("data") / "onedimensional" / "tests" / "variable_sized_bin_packing_mandatory_bin_type" / "items.csv",
                fs::path("data") / "onedimensional" / "tests" / "variable_sized_bin_packing_mandatory_bin_type" / "bins.csv",
                fs::path("data") / "onedimensional" / "tests" / "variable_sized_bin_packing_mandatory_bin_type" / "parameters.csv",
                fs::path("data") / "onedimensional" / "tests" / "variable_sized_bin_packing_mandatory_bin_type" / "solution.csv",
            }, {
                // Single bin type: exercises reusing
                // 'compute_bin_instance_upper_bound''s packing as a MILP
                // warm start (see 'output.solution_pool.best()' in
                // 'milp_assignment.cpp'). Two items of length 6 and two of
                // length 4 pair up exactly (6 + 4 = 10) in a length-10 bin,
                // so the optimum is 2 bins.
                fs::path("data") / "onedimensional" / "tests" / "variable_sized_bin_packing_single_bin_type" / "items.csv",
                fs::path("data") / "onedimensional" / "tests" / "variable_sized_bin_packing_single_bin_type" / "bins.csv",
                fs::path("data") / "onedimensional" / "tests" / "variable_sized_bin_packing_single_bin_type" / "parameters.csv",
                fs::path("data") / "onedimensional" / "tests" / "variable_sized_bin_packing_single_bin_type" / "solution.csv",
            }, {
                fs::path("data") / "onedimensional" / "tests" / "knapsack_multiple_bins" / "items.csv",
                fs::path("data") / "onedimensional" / "tests" / "knapsack_multiple_bins" / "bins.csv",
                fs::path("data") / "onedimensional" / "tests" / "knapsack_multiple_bins" / "parameters.csv",
                fs::path("data") / "onedimensional" / "tests" / "knapsack_multiple_bins" / "solution.csv",
            }, {
                fs::path("data") / "onedimensional" / "tests" / "feasibility_multiple_bin_types" / "items.csv",
                fs::path("data") / "onedimensional" / "tests" / "feasibility_multiple_bin_types" / "bins.csv",
                fs::path("data") / "onedimensional" / "tests" / "feasibility_multiple_bin_types" / "parameters.csv",
                fs::path("data") / "onedimensional" / "tests" / "feasibility_multiple_bin_types" / "solution.csv",
            }, {
                fs::path("data") / "onedimensional" / "tests" / "feasibility_infeasible" / "items.csv",
                fs::path("data") / "onedimensional" / "tests" / "feasibility_infeasible" / "bins.csv",
                fs::path("data") / "onedimensional" / "tests" / "feasibility_infeasible" / "parameters.csv",
                fs::path(""),
            }, {
                // Two item types, one copy each, that can't share a bin
                // instance (6 + 8 = 14 > 10), against a bin type with 3
                // instances: exercises the bin-instance symmetry-breaking
                // variable elimination in 'build_milp_model' across
                // multiple item types sharing one bin type.
                fs::path("data") / "onedimensional" / "tests" / "feasibility_incompatible_items" / "items.csv",
                fs::path("data") / "onedimensional" / "tests" / "feasibility_incompatible_items" / "bins.csv",
                fs::path("data") / "onedimensional" / "tests" / "feasibility_incompatible_items" / "parameters.csv",
                fs::path("data") / "onedimensional" / "tests" / "feasibility_incompatible_items" / "solution.csv",
            }, {
                // A single item type with 4 copies, each bin instance
                // fitting at most 3 by length, against a bin type with 3
                // instances: exercises the same symmetry-breaking variable
                // elimination with multiple copies of one item type spread
                // across more than one bin instance.
                fs::path("data") / "onedimensional" / "tests" / "feasibility_item_copies_across_bins" / "items.csv",
                fs::path("data") / "onedimensional" / "tests" / "feasibility_item_copies_across_bins" / "bins.csv",
                fs::path("data") / "onedimensional" / "tests" / "feasibility_item_copies_across_bins" / "parameters.csv",
                fs::path("data") / "onedimensional" / "tests" / "feasibility_item_copies_across_bins" / "solution.csv",
            }, {
                fs::path("data") / "onedimensional" / "tests" / "bin_packing_type_order" / "items.csv",
                fs::path("data") / "onedimensional" / "tests" / "bin_packing_type_order" / "bins.csv",
                fs::path("data") / "onedimensional" / "tests" / "bin_packing_type_order" / "parameters.csv",
                fs::path("data") / "onedimensional" / "tests" / "bin_packing_type_order" / "solution.csv",
            }, {
                // Single bin type, 'BinPacking' objective: exercises the
                // same MILP warm start as
                // 'variable_sized_bin_packing_single_bin_type', but through
                // the 'BinPacking'-objective code path (fixed bin counts,
                // no 'compute_bin_instance_upper_bound' loop over bin
                // types). Same 6+4=10 pairing, optimum is 2 bins.
                fs::path("data") / "onedimensional" / "tests" / "bin_packing_single_bin_type" / "items.csv",
                fs::path("data") / "onedimensional" / "tests" / "bin_packing_single_bin_type" / "bins.csv",
                fs::path("data") / "onedimensional" / "tests" / "bin_packing_single_bin_type" / "parameters.csv",
                fs::path("data") / "onedimensional" / "tests" / "bin_packing_single_bin_type" / "solution.csv",
            }, {
                fs::path("data") / "onedimensional" / "tests" / "bin_packing_weight_capacity" / "items.csv",
                fs::path("data") / "onedimensional" / "tests" / "bin_packing_weight_capacity" / "bins.csv",
                fs::path("data") / "onedimensional" / "tests" / "bin_packing_weight_capacity" / "parameters.csv",
                fs::path("data") / "onedimensional" / "tests" / "bin_packing_weight_capacity" / "solution.csv",
            }, {
                fs::path(""),
                fs::path(""),
                fs::path(""),
                fs::path("data") / "onedimensional" / "tests" / "bin_packing_resource_capacity" / "solution.csv",
                fs::path("data") / "onedimensional" / "tests" / "bin_packing_resource_capacity" / "instance.json",
            }, {
                fs::path(""),
                fs::path(""),
                fs::path(""),
                fs::path("data") / "onedimensional" / "tests" / "bin_packing_eligibility" / "solution.csv",
                fs::path("data") / "onedimensional" / "tests" / "bin_packing_eligibility" / "instance.json",
            }}));
