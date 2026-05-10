#define PACKINGSOLVER_PERIODIC_PACKING_TEST_ENABLE_DEBUG

#include "irregular/periodic_packing.hpp"

#ifdef PACKINGSOLVER_PERIODIC_PACKING_TEST_ENABLE_DEBUG
#include "shape/writer.hpp"
#endif

#include <gtest/gtest.h>

using namespace packingsolver;
using namespace packingsolver::irregular;


struct ComputePeriodicPackingsSingleShapeTestParams
{
    std::string name;
    ShapeWithHoles shape;
    std::vector<PeriodicPacking> expected_output;
};

inline std::ostream& operator<<(
        std::ostream& os,
        const ComputePeriodicPackingsSingleShapeTestParams& p)
{
    os << p.name;
    return os;
}

class ComputePeriodicPackingsSingleShapeTest:
    public testing::TestWithParam<ComputePeriodicPackingsSingleShapeTestParams> { };

TEST_P(ComputePeriodicPackingsSingleShapeTest, OutputMatchesExpected)
{
    const ComputePeriodicPackingsSingleShapeTestParams& params = GetParam();

#ifdef PACKINGSOLVER_PERIODIC_PACKING_TEST_ENABLE_DEBUG
    Writer writer;
    writer.add_shape_with_holes(params.shape, "Input");
    writer.write_json("periodic_packing_input.json");
#endif

    std::vector<PeriodicPacking> output = compute_periodic_packings(params.shape);

#ifdef PACKINGSOLVER_PERIODIC_PACKING_TEST_ENABLE_DEBUG
    for (shape::ShapePos pos = 0;
            pos < (shape::ShapePos)output.size();
            ++pos) {
        const PeriodicPacking& periodic_packing = output[pos];
        Writer writer;
        for (int i = 0; i < 2; ++i) {
            for (int j = 0; j < 2; ++j) {
                ShapeWithHoles shape = params.shape;
                shape.shift(periodic_packing.positions[0]);
                shape.shift(i * periodic_packing.vector_1);
                shape.shift(j * periodic_packing.vector_2);
                writer.add_shape_with_holes(shape, "Output");
            }
        }
        writer.write_json("periodic_packing_output_" + std::to_string(pos) + ".json");
    }
#endif

    ASSERT_EQ((int)output.size(), (int)params.expected_output.size())
        << "Size mismatch for " << params.name
        << ": expected " << params.expected_output.size()
        << ", got " << output.size();

    for (int idx = 0; idx < (int)output.size(); ++idx) {
        EXPECT_TRUE(equal(output[idx], params.expected_output[idx]))
            << "Packing " << idx << " mismatch for " << params.name
            << ": v1=(" << output[idx].vector_1.x << "," << output[idx].vector_1.y << ")"
            << " v2=(" << output[idx].vector_2.x << "," << output[idx].vector_2.y << ")";
    }
}

INSTANTIATE_TEST_SUITE_P(
        PeriodicPacking,
        ComputePeriodicPackingsSingleShapeTest,
        testing::Values(
            ComputePeriodicPackingsSingleShapeTestParams{
                "Rectangle",
                {shape::build_rectangle(2.0, 3.0)},
                {
                    {{{0, 0}}, {2.0, 0}, {0, 3.0}},
                }
            },
            ComputePeriodicPackingsSingleShapeTestParams{
                "ArrowRight",
                {shape::build_shape({{1, 0}, {3, 0}, {2, 1}, {3, 2}, {1, 2}, {0, 1}})},
                {
                    {{{0, 0}}, {2, 0}, {0, 2}},
                }
            },
            ComputePeriodicPackingsSingleShapeTestParams{
                "ArrowTop",
                {shape::build_shape({{0, 0}, {1, 1}, {2, 0}, {2, 3}, {1, 4}, {0, 3}})},
                {
                    {{{0, 0}}, {2, 0}, {0, 3}},
                }
            }
        ),
        testing::PrintToStringParamName()
);


struct ComputePeriodicPackingsTwoShapesTestParams
{
    std::string name;
    ShapeWithHoles shape_0;
    ShapeWithHoles shape_r;
    std::vector<PeriodicPacking> expected_output;
};

inline std::ostream& operator<<(
        std::ostream& os,
        const ComputePeriodicPackingsTwoShapesTestParams& p)
{
    os << p.name;
    return os;
}

class ComputePeriodicPackingsTwoShapesTest:
    public testing::TestWithParam<ComputePeriodicPackingsTwoShapesTestParams> { };

TEST_P(ComputePeriodicPackingsTwoShapesTest, OutputMatchesExpected)
{
    const ComputePeriodicPackingsTwoShapesTestParams& test_params = GetParam();

#ifdef PACKINGSOLVER_PERIODIC_PACKING_TEST_ENABLE_DEBUG
    Writer writer;
    writer.add_shape_with_holes(test_params.shape_0, "Input 1");
    writer.add_shape_with_holes(test_params.shape_r, "Input 2");
    writer.write_json("periodic_packing_input.json");
#endif

    std::vector<PeriodicPacking> output = compute_periodic_packings(test_params.shape_0, test_params.shape_r);

    std::cout << "output" << std::endl;
    for (const PeriodicPacking& periodic_packing: output)
        std::cout << "- " << periodic_packing << std::endl;

#ifdef PACKINGSOLVER_PERIODIC_PACKING_TEST_ENABLE_DEBUG
    for (shape::ShapePos pos = 0;
            pos < (shape::ShapePos)output.size();
            ++pos) {
        const PeriodicPacking& periodic_packing = output[pos];
        Writer writer;
        for (int i = 0; i < 2; ++i) {
            for (int j = 0; j < 2; ++j) {
                ShapeWithHoles shape_0 = test_params.shape_0;
                shape_0.shift(periodic_packing.positions[0]);
                shape_0.shift(i * periodic_packing.vector_1);
                shape_0.shift(j * periodic_packing.vector_2);
                writer.add_shape_with_holes(shape_0, "Output 1");
                ShapeWithHoles shape_r = test_params.shape_r;
                shape_r.shift(periodic_packing.positions[1]);
                shape_r.shift(i * periodic_packing.vector_1);
                shape_r.shift(j * periodic_packing.vector_2);
                writer.add_shape_with_holes(shape_r, "Output 2");
            }
        }
        writer.write_json("periodic_packing_output_" + std::to_string(pos) + ".json");
    }
#endif

    ASSERT_EQ(output.size(), test_params.expected_output.size());
    for (const PeriodicPacking& expected_periodic_packing: test_params.expected_output) {
        EXPECT_NE(std::find_if(
                      output.begin(),
                      output.end(),
                      [&expected_periodic_packing](const PeriodicPacking& periodic_packing) { return equal(periodic_packing, expected_periodic_packing); }),
                  output.end());
    }
}

INSTANTIATE_TEST_SUITE_P(
        PeriodicPacking,
        ComputePeriodicPackingsTwoShapesTest,
        testing::Values(
            ComputePeriodicPackingsTwoShapesTestParams{
                "RightTriangle_0and180",
                {shape::build_triangle({0, 0}, {1.0, 0}, {0, 1.0})},
                {shape::build_triangle({1.0, 1.0}, {0.0, 1.0}, {1.0, 0.0})},
                {
                    {{{0, 0}, {0, 0}}, {1.0, 0}, {0, 1.0}},
                }
            },
            ComputePeriodicPackingsTwoShapesTestParams{
                "Triangle",
                {shape::build_triangle({0, 0}, {2, 0}, {1, 1})},
                {shape::build_triangle({1, 0}, {2, 1}, {0, 1})},
                {
                    {{{0, 0}, {1, 0}}, {2, 0}, {0, 1}},
                    {{{0, 0}, {0, 1}}, {2, 0}, {0, 2}},
                    {{{0, 0}, {0, 1}}, {0, 2}, {1, 1}},
                }
            }
        ),
        testing::PrintToStringParamName()
);
