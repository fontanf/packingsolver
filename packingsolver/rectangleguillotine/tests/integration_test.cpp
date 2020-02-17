#include "packingsolver/rectangleguillotine/branching_scheme.hpp"
#include "packingsolver/algorithms/astar.hpp"

#include <gtest/gtest.h>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

TEST(RectangleGuillotineBranchingScheme, IntegrationC1)
{
    Info info = Info()
        //.set_log2stderr(true)
        ;
    Instance instance(Objective::BinPackingLeftovers,
            "data/rectangle/tests/C1_items.csv",
            "data/rectangle/tests/C1_bins.csv",
            "data/rectangle/tests/C1_defects.csv");
    BranchingScheme::Parameters p;
    p.set_roadef2018();
    BranchingScheme branching_scheme(instance, p);
    Solution solution(instance);
    AStar<Solution, BranchingScheme> astar(solution, branching_scheme, 0, 5, info);
    astar.run();
    EXPECT_EQ(solution.waste(), 0);
}

TEST(RectangleGuillotineBranchingScheme, IntegrationC2)
{
    Info info;
    Instance instance(Objective::BinPackingLeftovers,
            "data/rectangle/tests/C2_items.csv",
            "data/rectangle/tests/C2_bins.csv",
            "data/rectangle/tests/C2_defects.csv");
    BranchingScheme::Parameters p;
    p.set_roadef2018();
    BranchingScheme branching_scheme(instance, p);
    Solution solution(instance);
    AStar<Solution, BranchingScheme> astar(solution, branching_scheme, 0, 5, info);
    astar.run();
    EXPECT_EQ(solution.waste(), 210 * 5700);
}

TEST(RectangleGuillotineBranchingScheme, IntegrationC3)
{
    Info info;
    Instance instance(Objective::BinPackingLeftovers,
            "data/rectangle/tests/C3_items.csv",
            "data/rectangle/tests/C3_bins.csv",
            "data/rectangle/tests/C3_defects.csv");
    BranchingScheme::Parameters p;
    p.set_roadef2018();
    BranchingScheme branching_scheme(instance, p);
    Solution solution(instance);
    AStar<Solution, BranchingScheme> astar(solution, branching_scheme, 0, 5, info);
    astar.run();
    EXPECT_EQ(solution.waste(), 0);
}

