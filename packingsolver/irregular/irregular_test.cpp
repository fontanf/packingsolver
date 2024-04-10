#include "packingsolver/irregular/instance_builder.hpp"
#include "packingsolver/irregular/optimize.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver::irregular;
namespace fs = boost::filesystem;

TEST(Irregular, BinCopies)
{
    Shape shape;
    ShapeElement shape_element;
    shape_element.type = ShapeElementType::LineSegment;
    shape_element.start = {0, 0};
    shape_element.end = {10, 0};
    shape.elements.push_back(shape_element);
    shape_element.start = {10, 0};
    shape_element.end = {10, 10};
    shape.elements.push_back(shape_element);
    shape_element.start = {10, 10};
    shape_element.end = {0, 10};
    shape.elements.push_back(shape_element);
    shape_element.start = {0, 10};
    shape_element.end = {0, 0};
    shape.elements.push_back(shape_element);

    ItemShape item_shape;
    item_shape.shape = shape;

    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::VariableSizedBinPacking);
    instance_builder.add_item_type({item_shape}, -1, 10);
    instance_builder.add_bin_type(shape, -1, 10);
    const Instance instance = instance_builder.build();
    Solution solution(instance);
    solution.add_bin(0, 2);
    EXPECT_EQ(solution.number_of_bins(), 2);
    EXPECT_EQ(solution.bin_copies(0), 2);
}
