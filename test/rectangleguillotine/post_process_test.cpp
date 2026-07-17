#include "packingsolver/rectangleguillotine/instance_builder.hpp"
#include "packingsolver/rectangleguillotine/post_process.hpp"
#include "rectangleguillotine/solution_builder.hpp"

#include <gtest/gtest.h>

#include <functional>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

struct ItemDef
{
    Length w;
    Length h;
    int profit;
    int copies;
    bool oriented;
};

struct PostProcessTestParams
{
    std::string description;
    Length bin_width;
    Length bin_height;
    std::vector<ItemDef> items;
    CutOrientation first_stage_orientation;
    std::vector<ItemPlacement> placements;
    std::function<Solution(const Instance&)> build_expected_solution;
};

inline std::ostream& operator<<(
        std::ostream& os,
        const PostProcessTestParams& test_params)
{
    os << test_params.description;
    return os;
}

class RectangleGuillotinePostProcessTest:
    public testing::TestWithParam<PostProcessTestParams> { };

TEST_P(RectangleGuillotinePostProcessTest, BuildGuillotineSolution)
{
    PostProcessTestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Knapsack);
    instance_builder.add_bin_type(test_params.bin_width, test_params.bin_height);
    for (const ItemDef& item: test_params.items) {
        ItemTypeId item_type_id = instance_builder.add_item_type(item.w, item.h, item.oriented);
        instance_builder.set_item_type_profit(item_type_id, item.profit);
        instance_builder.set_item_type_copies(item_type_id, item.copies);
    }
    instance_builder.set_first_stage_orientation(test_params.first_stage_orientation);
    Instance instance = instance_builder.build();

    Solution actual_solution = build_guillotine_solution(instance, 0, test_params.placements);
    Solution expected_solution = test_params.build_expected_solution(instance);

    EXPECT_EQ(!(actual_solution < expected_solution), true);
    EXPECT_EQ(!(expected_solution < actual_solution), true);
}

INSTANTIATE_TEST_SUITE_P(
        RectangleGuillotinePostProcess,
        RectangleGuillotinePostProcessTest,
        testing::ValuesIn(std::vector<PostProcessTestParams>{
            {
                // Single item that exactly fills the bin (no cuts needed).
                "SingleItemWholePlate",
                10, 5,
                {ItemDef{10, 5, -1, 1, true}},
                CutOrientation::Vertical,
                {ItemPlacement{0, 0, 10, 0, 5}},
                [](const Instance& instance) {
                    SolutionBuilder builder(instance);
                    builder.add_bin(0, 1, CutOrientation::Vertical);
                    builder.add_node(1, 10);
                    builder.set_last_node_item(0);
                    return builder.build();
                },
            }, {
                // Bin 10 x 5, one item 4 x 2 placed at l=3, r=7, b=2, t=4, i.e.
                // there is waste on all four sides of the item.  The sub-plate
                // produced by the first cut therefore starts with a waste area in
                // the cut direction, which used to make build_region recurse
                // indefinitely.
                "LeadingWasteOnAllSides",
                10, 5,
                {ItemDef{4, 2, -1, 1, true}},
                CutOrientation::Vertical,
                {ItemPlacement{0, 3, 7, 2, 4}},
                [](const Instance& instance) {
                    SolutionBuilder builder(instance);
                    builder.add_bin(0, 1, CutOrientation::Vertical);
                    builder.add_node(1, 3);   // waste [0,3]x[0,5]
                    builder.add_node(1, 7);   // column [3,7]x[0,5]
                    builder.add_node(2, 2);   // waste [3,7]x[0,2]
                    builder.add_node(2, 4);   // item  [3,7]x[2,4]
                    builder.set_last_node_item(0);
                    return builder.build();
                },
            }, {
                // Bin 10 x 10. Left column: one item filling the full height
                // (0,5)x(0,10). Right column: two items stacked vertically,
                // (5,10)x(0,5) and (5,10)x(5,10).
                //
                // Forcing first_stage_orientation = Horizontal makes
                // build_guillotine_solution add a single depth-1 node spanning
                // the full plate height, then do the vertical split at depth 2
                // and the horizontal splits of the right column at depth 3.
                "ForcedOrientationMismatch",
                10, 10,
                {
                    ItemDef{5, 10, -1, 1, true},   // left column
                    ItemDef{5, 5, -1, 1, true},    // right-bottom
                    ItemDef{5, 5, -1, 1, true},    // right-top
                },
                CutOrientation::Horizontal,
                {
                    ItemPlacement{0, 0, 5, 0, 10},
                    ItemPlacement{1, 5, 10, 0, 5},
                    ItemPlacement{2, 5, 10, 5, 10},
                },
                [](const Instance& instance) {
                    SolutionBuilder builder(instance);
                    builder.add_bin(0, 1, CutOrientation::Horizontal);
                    builder.add_node(1, 10);   // depth-1: full-bin horizontal band
                    builder.add_node(2, 5);    // depth-2: left column  [0,5]x[0,10]
                    builder.set_last_node_item(0);
                    builder.add_node(2, 10);   // depth-2: right column [5,10]x[0,10]
                    builder.add_node(3, 5);    // depth-3: bottom-right [5,10]x[0,5]
                    builder.set_last_node_item(1);
                    builder.add_node(3, 10);   // depth-3: top-right    [5,10]x[5,10]
                    builder.set_last_node_item(2);
                    return builder.build();
                },
            },
        }));
