#include "packingsolver/rectangleguillotine/branching_scheme.hpp"
#include "packingsolver/algorithms/a_star.hpp"
#include "packingsolver/algorithms/dynamic_programming_a_star.hpp"

#include <gtest/gtest.h>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

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

    Info info;

    Instance instance(Objective::BinPackingWithLeftovers);
    instance.add_item_type(3000, 3210, -1, 1, false, true);
    instance.add_item_type(3000, 500, -1, 1, false, true);
    instance.add_bin_type(6000, 3210);
    instance.add_defect(0, 3100, 600, 2, 2);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    p.cut_type_1 = CutType1::TwoStagedGuillotine;
    BranchingScheme branching_scheme(instance, p);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 = {0, -1, -2, 3210, 3000, 3210, 3210, 6000, 0, 0};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root, info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);

    BranchingScheme::Insertion i1 = {1, -1, 1, 3210, 6000, 500, 3210, 6000, 0, 0};
    std::vector<BranchingScheme::Insertion> is1 = branching_scheme.insertions(node_1, info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    auto node_2 = branching_scheme.child(node_1, i1);

    auto solution = branching_scheme.to_solution(*node_2, Solution(instance));
}

TEST(RectangleGuillotineBranchingScheme, IntegrationC1)
{
    Info info = Info()
        //.set_log2stderr(true)
        ;
    Instance instance(Objective::BinPackingWithLeftovers,
            "data/rectangle/tests/C1_items.csv",
            "data/rectangle/tests/C1_bins.csv",
            "data/rectangle/tests/C1_defects.csv");
    BranchingScheme::Parameters p;
    p.set_roadef2018();
    p.guide_id = 6;
    BranchingScheme branching_scheme(instance, p);
    SolutionPool<Instance, Solution> solution_pool(instance, 1);
    a_star(branching_scheme, solution_pool);
    EXPECT_EQ(solution_pool.best().waste(), 0);
}

TEST(RectangleGuillotineBranchingScheme, IntegrationC2)
{
    Info info;
    Instance instance(Objective::BinPackingWithLeftovers,
            "data/rectangle/tests/C2_items.csv",
            "data/rectangle/tests/C2_bins.csv",
            "data/rectangle/tests/C2_defects.csv");
    BranchingScheme::Parameters p;
    p.set_roadef2018();
    p.guide_id = 6;
    BranchingScheme branching_scheme(instance, p);
    SolutionPool<Instance, Solution> solution_pool(instance, 1);
    a_star(branching_scheme, solution_pool);
    EXPECT_EQ(solution_pool.best().waste(), 210 * 5700);
}

TEST(RectangleGuillotineBranchingScheme, IntegrationC3)
{
    Info info;
    Instance instance(Objective::BinPackingWithLeftovers,
            "data/rectangle/tests/C3_items.csv",
            "data/rectangle/tests/C3_bins.csv",
            "data/rectangle/tests/C3_defects.csv");
    BranchingScheme::Parameters p;
    p.set_roadef2018();
    p.guide_id = 6;
    BranchingScheme branching_scheme(instance, p);
    SolutionPool<Instance, Solution> solution_pool(instance, 1);
    a_star(branching_scheme, solution_pool);
    EXPECT_EQ(solution_pool.best().waste(), 0);
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

    Info info;
    Instance instance(Objective::BinPackingWithLeftovers,
            "data/rectangle/tests/C11_items.csv",
            "data/rectangle/tests/C11_bins.csv",
            "data/rectangle/tests/C11_defects.csv");
    BranchingScheme::Parameters p;
    p.set_roadef2018();
    p.cut_type_2 = CutType2::Exact;
    p.guide_id = 6;
    BranchingScheme branching_scheme(instance, p);
    SolutionPool<Instance, Solution> solution_pool(instance, 1);
    a_star(branching_scheme, solution_pool);
    EXPECT_EQ(solution_pool.best().waste(), 3210 * (6000 + 210) - instance.item_area());
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

    Info info;

    Instance instance(Objective::BinPackingWithLeftovers);
    instance.add_item_type(500, 1000, -1, 1, false, true);
    instance.add_item_type(1500, 1600, -1, 1, false, false);
    instance.add_bin_type(6000, 3210);
    instance.add_defect(0, 950, 950, 100, 100);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    p.guide_id = 6;
    BranchingScheme branching_scheme(instance, p);

    SolutionPool<Instance, Solution> solution_pool(instance, 1);
    a_star(branching_scheme, solution_pool);
    EXPECT_EQ(solution_pool.best().waste(), 3210 * 1500 - instance.item_area());
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

    Info info;

    Instance instance(Objective::BinPackingWithLeftovers);
    instance.add_item_type(500, 1000, -1, 1, false, true);
    instance.add_item_type(500, 1500, -1, 1, false, false);
    instance.add_item_type(1500, 1600, -1, 1, false, false);
    instance.add_bin_type(6000, 3210);
    instance.add_defect(0, 950, 1250, 50, 50);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    p.guide_id = 6;
    BranchingScheme branching_scheme(instance, p);

    SolutionPool<Instance, Solution> solution_pool(instance, 1);
    a_star(branching_scheme, solution_pool);
    EXPECT_EQ(solution_pool.best().waste(), 3210 * 1500 - instance.item_area());
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

    Info info;

    Instance instance(Objective::BinPackingWithLeftovers);
    instance.add_item_type(500, 2900, -1, 1, false, true);
    instance.add_item_type(500, 2800, -1, 1, false, false);
    instance.add_item_type(2950, 3210, -1, 1, false, false);
    instance.add_bin_type(6000, 3210);
    instance.add_defect(0, 2950, 1500, 100, 100);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    p.guide_id = 6;
    BranchingScheme branching_scheme(instance, p);

    SolutionPool<Instance, Solution> solution_pool(instance, 1);
    a_star(branching_scheme, solution_pool);
    EXPECT_EQ(solution_pool.best().waste(), 3210 * 6000 - instance.item_area());
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

    Info info = Info()
        //.set_log2stderr(true)
        ;

    Instance instance(Objective::BinPackingWithLeftovers);
    instance.add_item_type(1400, 1600, -1, 1, false, true);
    instance.add_item_type(1500, 1500, -1, 1, false, false);
    instance.add_item_type(1500, 3000, -1, 1, false, false);
    instance.add_bin_type(6000, 3210);
    instance.add_defect(0, 1000, 3100, 10, 10);
    instance.add_defect(0, 2000, 200, 10, 10);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    p.cut_type_2 = CutType2::Exact;
    p.guide_id = 6;
    BranchingScheme branching_scheme(instance, p);

    SolutionPool<Instance, Solution> solution_pool(instance, 1);
    a_star(branching_scheme, solution_pool);
    EXPECT_EQ(solution_pool.best().waste(), 3210 * 3000 - instance.item_area());
}

static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

std::vector<std::string> split(std::string line)
{
    std::vector<std::string> v;
    std::stringstream ss(line);
    std::string tmp;
    char c = ',';
    if (line.find(';') != std::string::npos)
        c = ';';
    while (getline(ss, tmp, c)) {
        rtrim(tmp);
        v.push_back(tmp);
    }
    return v;
}

TEST(RectangleGuillotineBranchingScheme, IntegrationBest)
{

    for (std::string name: {
            "A1", "A2", "A3", "A4", "A5", "A6", "A7", "A8", "A9", "A10", "A11", "A12", "A13", "A14", "A15", "A16", "A17", "A18", "A19",
            "B1", "B2", "B3", "B4", "B5", "B6", "B7", "B8", "B9", "B10", "B11", "B12", "B13", "B14", /*"B15", */
            /*"X1", *//*"X2", */"X3", "X4", "X5", "X6", "X7", "X8", "X9", "X10", "X11", "X12", "X13", "X14", /*"X15", */
            }) {

        Info info;

        Instance instance(Objective::BinPackingWithLeftovers,
                "data/rectangle/roadef2018/" + name + "_items.csv",
                "data/rectangle/roadef2018/" + name + "_bins.csv",
                "data/rectangle/roadef2018/" + name + "_defects.csv");

        std::string solution_filepath = "data/rectangle_solutions/roadef2018/" + name + "_solution.csv";
        std::ifstream f(solution_filepath);
        EXPECT_EQ(f.good(), true);
        std::string tmp;
        getline(f, tmp);
        std::vector<std::string> line = split(tmp);
        Area waste = 0;
        std::vector<ItemTypeId> items;
        while (getline(f, tmp)) {
            line = split(tmp);
            ItemTypeId j = std::stol(line[6]);
            if (j == -1) {
                waste += std::stol(line[4]) * std::stol(line[5]);
            } else if (std::stol(line[6]) >= 0) {
                items.push_back(j);
            }
        }

        Instance instance_new(Objective::BinPackingWithLeftovers);
        for (BinTypeId i = 0; i < instance.bin_type_number(); ++i) {
            const BinType& bin = instance.bin_type(i);
            instance_new.add_bin_type(bin.rect.w, bin.rect.h, bin.copies);
        }
        for (DefectId k = 0; k < instance.defect_number(); ++k) {
            const Defect& defect = instance.defect(k);
            instance_new.add_defect(defect.bin_id, defect.pos.x, defect.pos.y, defect.rect.w, defect.rect.h);
        }
        bool new_stack = true;
        for (ItemTypeId j: items) {
            const ItemType& item = instance.item_type(j);
            instance_new.add_item_type(item.rect.w, item.rect.h, item.profit, item.copies, item.oriented, new_stack);
            new_stack = false;
        }

        BranchingScheme::Parameters p;
        p.set_roadef2018();
        BranchingScheme branching_scheme(instance_new, p);
        SolutionPool<Instance, Solution> solution_pool(instance_new, 1);
        dynamic_programming_a_star(branching_scheme, solution_pool);
        const Solution& solution = solution_pool.best();
        std::cout << name << " " << waste << std::endl;

        if (solution.waste() != waste) {
            std::cout << "PLATE_ID,NODE_ID,X,Y,WIDTH,HEIGHT,TYPE,CUT,PARENT" << std::endl;
            for (const Solution::Node& n: solution.nodes()) {
                std::cout
                    << n.i << ","
                    << n.id << ","
                    << n.l << ","
                    << n.b << ","
                    << n.r - n.l << ","
                    << n.t - n.b << ","
                    << ((n.j < 0)? n.j: items[n.j]) << ","
                    << n.d << ",";
                if (n.f != -1)
                    std::cout << n.f;
                std::cout << std::endl;
            }

        }
        EXPECT_EQ(solution.waste(), waste);
        info.set_certificate_path(name + "_solution.csv");

    }
}

