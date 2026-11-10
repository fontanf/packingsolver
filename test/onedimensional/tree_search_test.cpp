#include "packingsolver/onedimensional/instance_builder.hpp"
#include "packingsolver/onedimensional/optimize.hpp"
#include "onedimensional/solution_builder.hpp"
#include "onedimensional/tree_search.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver::onedimensional;
namespace fs = boost::filesystem;

struct OneDimensionalTreeSearchTestParams
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
        const OneDimensionalTreeSearchTestParams& test_params)
{
    os << (!test_params.instance_path.empty()? test_params.instance_path: test_params.items_path);
    return os;
}

class OneDimensionalTreeSearchTest: public testing::TestWithParam<OneDimensionalTreeSearchTestParams> { };

TEST_P(OneDimensionalTreeSearchTest, OneDimensionalTreeSearch)
{
    OneDimensionalTreeSearchTestParams test_params = GetParam();
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
    optimize_parameters.use_tree_search = true;
    optimize_parameters.use_sequential_single_knapsack = false;
    optimize_parameters.use_sequential_value_correction = false;
    optimize_parameters.use_dichotomic_search = false;
    optimize_parameters.use_column_generation = false;
    optimize_parameters.use_milp_assignment = false;
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

// Note: 'variable_sized_bin_packing_mandatory_bin_type' (used by the MILP
// assignment tests) is intentionally not reused here: tree search is
// disabled for 'VariableSizedBinPacking' instances with more than one bin
// type (see optimize.cpp), so forcing 'use_tree_search' on it would silently
// fall back to a different algorithm instead of exercising tree search.
INSTANTIATE_TEST_SUITE_P(
        OneDimensional,
        OneDimensionalTreeSearchTest,
        testing::ValuesIn(std::vector<OneDimensionalTreeSearchTestParams>{
            {
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
                fs::path("data") / "onedimensional" / "tests" / "bin_packing_type_order" / "items.csv",
                fs::path("data") / "onedimensional" / "tests" / "bin_packing_type_order" / "bins.csv",
                fs::path("data") / "onedimensional" / "tests" / "bin_packing_type_order" / "parameters.csv",
                fs::path("data") / "onedimensional" / "tests" / "bin_packing_type_order" / "solution.csv",
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
            }, {
                fs::path("data") / "onedimensional" / "users" / "2024-04-06_t3_items.csv",
                fs::path("data") / "onedimensional" / "users" / "2024-04-06_t3_bins.csv",
                fs::path("data") / "onedimensional" / "users" / "2024-04-06_t3_parameters.csv",
                fs::path("data") / "onedimensional" / "users" / "2024-04-06_t3_solution.csv",
            }}));

TEST(OneDimensionalTreeSearch, DominanceCopyDependentResourceConsumption)
{
    // A resource of capacity 2: the first two copies of item type A in a bin
    // consume 1 each, the next ones nothing; item type B consumes 1.
    // Node 1: bins [A, A] and [B, A]; node 2: bins [B, A] and [A, A]. Both
    // last bins consume 2, but another copy of A fits in the last bin of
    // node 2 (its third copy consumes nothing), not in the one of node 1
    // (its second copy consumes 1): node 1 doesn't dominate node 2.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::BinPacking);
    packingsolver::BinTypeId bin_type_id = instance_builder.add_bin_type(100);
    instance_builder.set_bin_type_copies(bin_type_id, 3);
    packingsolver::ResourceId resource_id = instance_builder.add_bin_type_resource(bin_type_id, 2);
    packingsolver::ItemTypeId item_type_id_a = instance_builder.add_item_type(1);
    instance_builder.set_item_type_copies(item_type_id_a, 5);
    packingsolver::ItemTypeId item_type_id_b = instance_builder.add_item_type(1);
    instance_builder.add_resource_consumption(bin_type_id, resource_id, item_type_id_a, {1.0, 1.0, 0.0});
    instance_builder.add_resource_consumption(bin_type_id, resource_id, item_type_id_b, {1.0});
    const Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance, BranchingScheme::Parameters());
    auto insert = [&branching_scheme](
            const std::shared_ptr<BranchingScheme::Node>& parent,
            packingsolver::ItemTypeId item_type_id,
            bool new_bin)
    {
        BranchingScheme::Insertion insertion;
        insertion.item_type_id = item_type_id;
        insertion.new_bin = new_bin;
        insertion.new_bin_pos = (new_bin)? parent->number_of_bins: -1;
        return branching_scheme.child(parent, insertion);
    };
    auto root = branching_scheme.root();
    auto node_1 = insert(insert(insert(insert(root,
                        item_type_id_a, true),
                    item_type_id_a, false),
                item_type_id_b, true),
            item_type_id_a, false);
    auto node_2 = insert(insert(insert(insert(root,
                        item_type_id_b, true),
                    item_type_id_a, false),
                item_type_id_a, true),
            item_type_id_a, false);
    EXPECT_EQ(node_1->last_bin_resource_consumption[resource_id], 2);
    EXPECT_EQ(node_2->last_bin_resource_consumption[resource_id], 2);

    // Another copy of A fits in the last bin of node 2 only.
    auto fits_in_last_bin = [&branching_scheme, item_type_id_a](
            const std::shared_ptr<BranchingScheme::Node>& node)
    {
        for (const BranchingScheme::Insertion& insertion: branching_scheme.insertions(node))
            if (insertion.item_type_id == item_type_id_a && !insertion.new_bin)
                return true;
        return false;
    };
    EXPECT_FALSE(fits_in_last_bin(node_1));
    EXPECT_TRUE(fits_in_last_bin(node_2));

    EXPECT_FALSE(branching_scheme.dominates(node_1, node_2));
}
