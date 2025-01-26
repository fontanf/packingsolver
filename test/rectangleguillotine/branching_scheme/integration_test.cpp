#include "packingsolver/rectangleguillotine/instance_builder.hpp"
#include "rectangleguillotine/branching_scheme.hpp"

#include "treesearchsolver/iterative_beam_search_2.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;
namespace fs = boost::filesystem;

TEST(RectangleGuillotineBranchingScheme, ConvertionDefect)
{
    /**
     * Test added to fix a bug when converting nodes to solutions with
     * two-staged guillotine cuts and defects.
     *
     * |-------------------------|------------------------| 3210
     * |                         |                        |
     * |                         |                        | 3000
     * |                         |                        |
     * |                         |                        |
     * |                         |                        |
     * |                         |                        |
     * |                         |                        |
     * |                         | x                      |
     * |                         |------------------------| 500
     * |                         |                        |
     * |-------------------------|------------------------|
     *                         3000                     6000
     */

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    instance_builder.set_number_of_stages(2);
    instance_builder.add_item_type(3000, 3210, -1, 1, false, 0);
    instance_builder.add_item_type(3000, 500, -1, 1, false, 1);
    instance_builder.add_bin_type(6000, 3210);
    instance_builder.add_defect(0, 3100, 600, 2, 2);
    Instance instance = instance_builder.build();

    BranchingScheme::Parameters branching_scheme_parameters;
    BranchingScheme branching_scheme(instance, branching_scheme_parameters);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 = {0, -1, -2, 3210, 3000, 3210, 3210, 6000, 0, 0};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);

    BranchingScheme::Insertion i1 = {1, -1, 1, 3210, 6000, 500, 3210, 6000, 0, 0};
    std::vector<BranchingScheme::Insertion> is1 = branching_scheme.insertions(node_1);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    auto node_2 = branching_scheme.child(node_1, i1);

    auto solution = branching_scheme.to_solution(node_2);
}

TEST(RectangleGuillotineBranchingScheme, IntegrationC1)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    fs::path directory = fs::path("data") / "rectangle" / "tests";
    instance_builder.read_item_types((directory / "C1" / "items.csv").string());
    instance_builder.read_bin_types((directory / "C1" / "bins.csv").string());
    instance_builder.read_defects((directory / "C1" / "defects.csv").string());
    instance_builder.set_roadef2018();
    Instance instance = instance_builder.build();

    BranchingScheme::Parameters branching_scheme_parameters;
    branching_scheme_parameters.guide_id = 6;
    BranchingScheme branching_scheme(instance, branching_scheme_parameters);
    auto output = treesearchsolver::iterative_beam_search_2(branching_scheme);
    EXPECT_EQ(branching_scheme.to_solution(output.solution_pool.best()).waste(), 0);
}

TEST(RectangleGuillotineBranchingScheme, IntegrationC2)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    fs::path directory = fs::path("data") / "rectangle" / "tests";
    instance_builder.read_item_types((directory / "C2" / "items.csv").string());
    instance_builder.read_bin_types((directory / "C2" / "bins.csv").string());
    instance_builder.read_defects((directory / "C2" / "defects.csv").string());
    instance_builder.set_roadef2018();
    Instance instance = instance_builder.build();

    BranchingScheme::Parameters branching_scheme_parameters;
    branching_scheme_parameters.guide_id = 6;
    BranchingScheme branching_scheme(instance, branching_scheme_parameters);
    auto output = treesearchsolver::iterative_beam_search_2(branching_scheme);
    EXPECT_EQ(branching_scheme.to_solution(output.solution_pool.best()).waste(), 210 * 5700);
}

TEST(RectangleGuillotineBranchingScheme, IntegrationC3)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    fs::path directory = fs::path("data") / "rectangle" / "tests";
    instance_builder.read_item_types((directory / "C3" / "items.csv").string());
    instance_builder.read_bin_types((directory / "C3" / "bins.csv").string());
    instance_builder.read_defects((directory / "C3" / "defects.csv").string());
    instance_builder.set_roadef2018();
    Instance instance = instance_builder.build();

    BranchingScheme::Parameters branching_scheme_parameters;
    branching_scheme_parameters.guide_id = 6;
    BranchingScheme branching_scheme(instance, branching_scheme_parameters);
    SolutionPool<Instance, Solution> solution_pool(instance, 1);
    auto output = treesearchsolver::iterative_beam_search_2(branching_scheme);
    EXPECT_EQ(branching_scheme.to_solution(output.solution_pool.best()).waste(), 0);
}

TEST(RectangleGuillotineBranchingScheme, IntegrationC11)
{
    /*
     *
     * |--------------|---------------| |------------------------------| 3210
     * |              |    x     x    | |                              |
     * |              |---------------| |                              | 3000
     * |              |               | |                              |
     * |              |               | |                              |
     * |              |               | |                              |
     * |              |               | |---|                          | 1520
     * |              |               | |   |                          |
     * |     0        |               | | 2 |                          |
     * |              |               | |   |                          |
     * |              |       1       | |---|                          | 20
     * |              |               | | x                            |
     * |--------------|---------------| |------------------------------|
     *              3000           6000 0  210
     *                   4000   5000     10
     *
     */

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    fs::path directory = fs::path("data") / "rectangle" / "tests";
    instance_builder.read_item_types((directory / "C11" / "items.csv").string());
    instance_builder.read_bin_types((directory / "C11" / "bins.csv").string());
    instance_builder.read_defects((directory / "C11" / "defects.csv").string());
    instance_builder.set_roadef2018();
    instance_builder.set_cut_type(CutType::Exact);
    Instance instance = instance_builder.build();

    BranchingScheme::Parameters branching_scheme_parameters;
    branching_scheme_parameters.guide_id = 6;
    BranchingScheme branching_scheme(instance, branching_scheme_parameters);
    auto output = treesearchsolver::iterative_beam_search_2(branching_scheme);
    EXPECT_EQ(branching_scheme.to_solution(output.solution_pool.best()).waste(), 3210 * (6000 + 210) - instance.item_area());
}

TEST(RectangleGuillotineBranchingScheme, IntegrationDefect1)
{
    /**
     *
     * |-------------------------|------------------------| 3210
     * |-------------------------|                        |
     * |                         |                        |
     * |            1            |                        |
     * |                         |                        |
     * |-------------------------|                        | 1500
     * |                 xxx                              |
     * |----------|      xxx                              | 1000
     * |          |      xxx                              |
     * |     0    |                                       |
     * |          |                                       |
     * |----------|---------------------------------------|
     *           500    1000   1500                     6000
     */

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    instance_builder.add_item_type(500, 1000, -1, 1, false, 0);
    instance_builder.add_item_type(1500, 1600, -1, 1, false, 0);
    instance_builder.add_bin_type(6000, 3210);
    instance_builder.add_defect(0, 950, 950, 100, 100);
    Instance instance = instance_builder.build();

    BranchingScheme::Parameters branching_scheme_parameters;
    branching_scheme_parameters.guide_id = 6;
    BranchingScheme branching_scheme(instance, branching_scheme_parameters);

    auto output = treesearchsolver::iterative_beam_search_2(branching_scheme);
    EXPECT_EQ(branching_scheme.to_solution(output.solution_pool.best()).waste(), 3210 * 1500 - instance.item_area());
}

TEST(RectangleGuillotineBranchingScheme, IntegrationDefect2)
{
    /**
     *
     * |-------------------------|------------------------| 3210
     * |-------------------------|                        |
     * |                         |                        |
     * |            2            |                        |
     * |                         |                        |
     * |------------------|------|                        | 1500
     * |                 x|      |                        | 1250
     * |----------|       |      |                        | 1000
     * |          |       |  1   |                        |
     * |     0    |       |      |                        |
     * |          |       |      |                        |
     * |----------|-------|------|------------------------|
     *           500    1000   1500                     6000
     */

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    instance_builder.add_item_type(500, 1000, -1, 1, false, 0);
    instance_builder.add_item_type(500, 1500, -1, 1, false, 0);
    instance_builder.add_item_type(1500, 1600, -1, 1, false, 0);
    instance_builder.add_bin_type(6000, 3210);
    instance_builder.add_defect(0, 950, 1250, 50, 50);
    Instance instance = instance_builder.build();

    BranchingScheme::Parameters branching_scheme_parameters;
    branching_scheme_parameters.guide_id = 6;
    BranchingScheme branching_scheme(instance, branching_scheme_parameters);

    auto output = treesearchsolver::iterative_beam_search_2(branching_scheme);
    EXPECT_EQ(branching_scheme.to_solution(output.solution_pool.best()).waste(), 3210 * 1500 - instance.item_area());
}

TEST(RectangleGuillotineBranchingScheme, IntegrationDefect3)
{
    /**
     *
     * |----------------------------|---------------------| 3210
     * |                            |                     |
     * |                            |                     |
     * |                            |                     |
     * |                            |                     |
     * |                            |                     |
     * |                          x |         2           |
     * |-----------------------|    |                     | 1000
     * |           1           |    |                     |
     * |-----------------------|-|  |                     | 500
     * |            0            |  |                     |
     * |-------------------------|--|---------------------|
     *                         2900 3050                6000
     *                       2800
     */

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    instance_builder.add_item_type(500, 2900, -1, 1, false, 0);
    instance_builder.add_item_type(500, 2800, -1, 1, false, 0);
    instance_builder.add_item_type(2950, 3210, -1, 1, false, 0);
    instance_builder.add_bin_type(6000, 3210);
    instance_builder.add_defect(0, 2950, 1500, 100, 100);
    Instance instance = instance_builder.build();

    BranchingScheme::Parameters branching_scheme_parameters;
    branching_scheme_parameters.guide_id = 6;
    BranchingScheme branching_scheme(instance, branching_scheme_parameters);

    auto output = treesearchsolver::iterative_beam_search_2(branching_scheme);
    EXPECT_EQ(branching_scheme.to_solution(output.solution_pool.best()).waste(), 3210 * 6000 - instance.item_area());
}

TEST(RectangleGuillotineBranchingScheme, IntegrationDefect4)
{
    /**
     * Three-staged exact patterns.
     * |--------------|-------------|---------------------| 3210
     * |      x       |             |                     |
     * |--------------|             |                     | 3100
     * |              |             |                     |
     * |      1       |             |                     |
     * |              |      2      |                     |
     * |-----------|--|             |                     | 1600
     * |           |  |             |                     |
     * |           |  |             |                     |
     * |     0     |  |-------------|                     |
     * |           |  |      x      |                     |
     * |-----------|--|-------------|---------------------|
     *              1500           3000                 6000
     */

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    instance_builder.set_cut_type(CutType::Exact);
    instance_builder.add_item_type(1400, 1600, -1, 1, false, 0);
    instance_builder.add_item_type(1500, 1500, -1, 1, false, 0);
    instance_builder.add_item_type(1500, 3000, -1, 1, false, 0);
    instance_builder.add_bin_type(6000, 3210);
    instance_builder.add_defect(0, 1000, 3100, 10, 10);
    instance_builder.add_defect(0, 2000, 200, 10, 10);
    Instance instance = instance_builder.build();

    BranchingScheme::Parameters branching_scheme_parameters;
    branching_scheme_parameters.guide_id = 6;
    BranchingScheme branching_scheme(instance, branching_scheme_parameters);

    auto output = treesearchsolver::iterative_beam_search_2(branching_scheme);
    EXPECT_EQ(branching_scheme.to_solution(output.solution_pool.best()).waste(), 3210 * 3000 - instance.item_area());
}
