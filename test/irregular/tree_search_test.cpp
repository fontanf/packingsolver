#include "packingsolver/irregular/instance_builder.hpp"
#include "packingsolver/irregular/optimize.hpp"
#include "irregular/solution_builder.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver::irregular;
namespace fs = boost::filesystem;

struct IrregularTreeSearchTestParams
{
    fs::path instance_path;
    fs::path certificate_path;
};

inline std::ostream& operator<<(std::ostream& os, const IrregularTreeSearchTestParams& test_params)
{
    os << test_params.instance_path;
    return os;
}

class IrregularTreeSearchTest: public testing::TestWithParam<IrregularTreeSearchTestParams> { };

TEST_P(IrregularTreeSearchTest, IrregularTreeSearch)
{
    IrregularTreeSearchTestParams test_params = GetParam();
    std::cout << "Instance path: " << test_params.instance_path << std::endl;
    std::cout << "Certificate path: " << test_params.certificate_path << std::endl;

    InstanceBuilder instance_builder;
    instance_builder.read(test_params.instance_path.string());
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    optimize_parameters.not_anytime_maximum_approximation_ratio = 0.01;
    optimize_parameters.use_tree_search = true;
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
        IrregularTreeSearchTest,
        testing::ValuesIn(std::vector<IrregularTreeSearchTestParams>{
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
            //    fs::path("data") / "irregular" / "users" / "2024-07-15.json",
            //    fs::path("data") / "irregular" / "users" / "2024-07-15_solution.json"
            //}, {
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
            }, {
                fs::path("data") / "irregular" / "users" / "2024-08-25_3.json",
                fs::path("data") / "irregular" / "users" / "2024-08-25_3_solution.json"
            }, {
                fs::path("data") / "irregular" / "users" / "2024-08-25_4.json",
                fs::path("data") / "irregular" / "users" / "2024-08-25_4_solution.json"
            }, {
                fs::path("data") / "irregular" / "users" / "2024-09-11.json",
                fs::path("data") / "irregular" / "users" / "2024-09-11_solution.json"
            }, {
                fs::path("data") / "irregular" / "users" / "2024-09-21.json",
                fs::path("data") / "irregular" / "users" / "2024-09-21_solution.json"
            }, {
                fs::path("data") / "irregular" / "users" / "2025-01-12.json",
                fs::path("data") / "irregular" / "users" / "2025-01-12_solution.json"
            }, {
                fs::path("data") / "irregular" / "users" / "2025-05-04.json",
                fs::path("data") / "irregular" / "users" / "2025-05-04_solution.json"
            }, {
                fs::path("data") / "irregular" / "users" / "2025-05-05_1.json",
                fs::path("data") / "irregular" / "users" / "2025-05-05_1_solution.json"
            }, {
                fs::path("data") / "irregular" / "users" / "2025-05-05_2.json",
                fs::path("data") / "irregular" / "users" / "2025-05-05_2_solution.json"
            }, {
                fs::path("data") / "irregular" / "users" / "2025-05-07.json",
                fs::path("data") / "irregular" / "users" / "2025-05-07_solution.json"
            }, {
                fs::path("data") / "irregular" / "users" / "2025-05-08_1.json",
                fs::path("data") / "irregular" / "users" / "2025-05-08_1_solution.json"
            }, {
                fs::path("data") / "irregular" / "users" / "2025-05-08_2.json",
                fs::path("data") / "irregular" / "users" / "2025-05-08_2_solution.json"
            }, {
                fs::path("data") / "irregular" / "users" / "2025-05-08_3.json",
                fs::path("data") / "irregular" / "users" / "2025-05-08_3_solution.json"
            }, {
                fs::path("data") / "irregular" / "users" / "2025-05-09.json",
                fs::path("data") / "irregular" / "users" / "2025-05-09_solution.json"
            }, {
                fs::path("data") / "irregular" / "users" / "2025-05-12.json",
                fs::path("data") / "irregular" / "users" / "2025-05-12_solution.json"
            }, {
                fs::path("data") / "irregular" / "users" / "2025-09-01.json",
                fs::path("data") / "irregular" / "users" / "2025-09-01_solution.json"
            }, {
                fs::path("data") / "irregular" / "users" / "2025-09-02.json",
                fs::path("data") / "irregular" / "users" / "2025-09-02_solution.json"
            }, {
                fs::path("data") / "irregular" / "users" / "2025-09-01_2.json",
                fs::path("data") / "irregular" / "users" / "2025-09-01_2_solution.json"
            }, {
                fs::path("data") / "irregular" / "users" / "2025-09-01_3.json",
                fs::path("data") / "irregular" / "users" / "2025-09-01_3_solution.json"
            }, {
                fs::path("data") / "irregular" / "users" / "2025-09-01_4.json",
                fs::path("data") / "irregular" / "users" / "2025-09-01_4_solution.json"
            }, {
                fs::path("data") / "irregular" / "users" / "2025-12-05.json",
                fs::path("data") / "irregular" / "users" / "2025-12-05_solution.json"
            }, {
                fs::path("data") / "irregular" / "users" / "2026-02-03.json",
                fs::path("data") / "irregular" / "users" / "2026-02-03_solution.json"
            }, {
                fs::path("data") / "irregular" / "tests" / "open_dimension_xy_1.json",
                fs::path("data") / "irregular" / "tests" / "open_dimension_xy_1_solution.json"
            }, {
                fs::path("data") / "irregular" / "tests" / "open_dimension_xy_small.json",
                fs::path("data") / "irregular" / "tests" / "open_dimension_xy_small_solution.json"
            }, {
                fs::path("data") / "irregular" / "tests" / "item_defect_minimum_spacing.json",
                fs::path("data") / "irregular" / "tests" / "item_defect_minimum_spacing_solution.json"
            }, {
                fs::path("data") / "irregular" / "users" / "2026-02-05_1.json",
                fs::path("data") / "irregular" / "users" / "2026-02-05_1_solution.json"
            }, {
                fs::path("data") / "irregular" / "users" / "2026-02-05_2.json",
                fs::path("data") / "irregular" / "users" / "2026-02-05_2_solution.json"
            }, {
                fs::path("data") / "irregular" / "users" / "2026-03-22.json",
                fs::path("data") / "irregular" / "users" / "2026-03-22_solution.json"
            }, {
                // Issue #740: with 'leftover_all_bins', the free space of the
                // first bin is in a single region at the top right.
                fs::path("data") / "irregular" / "users" / "2026-10-23.json",
                fs::path("data") / "irregular" / "users" / "2026-10-23_solution.json"
            }, {
                // The tree search re-optimizes the last bin with guides
                // {2, 3} in a post-process. It used to rebuild the solution
                // from all the bins but the last two (instead of the last
                // one), so with several bins the rebuilt solution missed the
                // items of the second-to-last bin and was never kept. Here,
                // the re-optimized last bin increases the leftover value from
                // about 3238 to about 3543.
                fs::path("data") / "irregular" / "tests" / "bin_packing_with_leftovers_last_bin_post_process.json",
                fs::path("data") / "irregular" / "tests" / "bin_packing_with_leftovers_last_bin_post_process_solution.json"
            }, {
                fs::path("data") / "irregular" / "tests" / "free_rotation.json",
                fs::path("data") / "irregular" / "tests" / "free_rotation_solution.json"
            }, {
                fs::path("data") / "irregular" / "tests" / "fixed_item.json",
                fs::path("data") / "irregular" / "tests" / "fixed_item_solution.json"
            }, {
                fs::path("data") / "irregular" / "tests" / "circle.json",
                fs::path("data") / "irregular" / "tests" / "circle_solution.json"
            }, {
                fs::path("data") / "irregular" / "tests" / "circular_bin.json",
                fs::path("data") / "irregular" / "tests" / "circular_bin_solution.json"
            }}));

#include "irregular/tree_search.hpp"

TEST(IrregularBranchingScheme, DominanceCopyDependentResourceConsumption)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::BinPacking);
    packingsolver::BinTypeId bin_type_id = instance_builder.add_bin_type(shape::build_rectangle(100, 100));
    instance_builder.set_bin_type_copies(bin_type_id, 3);
    ItemShape item_shape;
    item_shape.shape_orig.shape = shape::build_rectangle(1, 1);
    packingsolver::ItemTypeId item_type_id_a = instance_builder.add_item_type({item_shape});
    instance_builder.set_item_type_copies(item_type_id_a, 5);
    packingsolver::ItemTypeId item_type_id_b = instance_builder.add_item_type({item_shape});
    packingsolver::ResourceId resource_id = instance_builder.add_bin_type_resource(bin_type_id, 2);
    instance_builder.add_resource_consumption(bin_type_id, resource_id, item_type_id_a, {1.0, 1.0, 0.0});
    instance_builder.add_resource_consumption(bin_type_id, resource_id, item_type_id_b, {1.0});
    const Instance instance = instance_builder.build();
    BranchingScheme branching_scheme(instance, BranchingScheme::Parameters());
    // Child of 'parent' with one more copy of 'item_type_id', in a new bin
    // or in the current one; 'nullptr' if there is none.
    auto insert = [&branching_scheme](
            const std::shared_ptr<BranchingScheme::Node>& parent,
            packingsolver::ItemTypeId item_type_id,
            bool new_bin) -> std::shared_ptr<BranchingScheme::Node>
    {
        if (parent == nullptr)
            return nullptr;
        for (const auto& child: branching_scheme.children(parent)) {
            if (child->number_of_bins == parent->number_of_bins + (new_bin? 1: 0)
                    && child->item_number_of_copies[item_type_id]
                    == parent->item_number_of_copies[item_type_id] + 1) {
                return child;
            }
        }
        return nullptr;
    };
    // A resource of capacity 2: the first two copies of item type A in a bin
    // consume 1 each, the next ones nothing; item type B consumes 1. Two
    // nodes with the same geometry, whose last bins both consume 2: one with
    // a copy of A and a copy of B, the other with two copies of A. Another
    // copy of A fits in the second one (its third copy consumes nothing),
    // not in the first one (its second copy consumes 1), so the first one
    // doesn't dominate the second one, although it doesn't consume more.
    auto root = branching_scheme.root();
    auto node = insert(insert(root, item_type_id_a, true), item_type_id_a, false);
    ASSERT_NE(node, nullptr);
    auto node_1 = std::make_shared<BranchingScheme::Node>(*node);
    node_1->last_bin_resource_consumption = {2};
    node_1->last_bin_item_number_of_copies = {1, 1};
    auto node_2 = std::make_shared<BranchingScheme::Node>(*node);
    node_2->last_bin_resource_consumption = {2};
    node_2->last_bin_item_number_of_copies = {2, 0};
    EXPECT_FALSE(branching_scheme.dominates(node_1, node_2));
    EXPECT_FALSE(branching_scheme.dominates(node_2, node_1));
    // With the same copies, the consumptions decide.
    node_1->last_bin_item_number_of_copies = {2, 0};
    node_1->last_bin_resource_consumption = {1};
    EXPECT_TRUE(branching_scheme.dominates(node_1, node_2));
}
