#include "packingsolver/irregular/instance_builder.hpp"
#include "packingsolver/irregular/optimize.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver::irregular;
namespace fs = boost::filesystem;

struct IrregularInvalidInstanceTestParams
{
    fs::path instance_path;
    std::string expected_error_substring;
};

inline std::ostream& operator<<(std::ostream& os, const IrregularInvalidInstanceTestParams& test_params)
{
    os << test_params.instance_path;
    return os;
}

class IrregularInvalidInstanceTest: public testing::TestWithParam<IrregularInvalidInstanceTestParams> { };

TEST_P(IrregularInvalidInstanceTest, IrregularInvalidInstance)
{
    IrregularInvalidInstanceTestParams test_params = GetParam();
    std::cout << "Instance path: " << test_params.instance_path << std::endl;

    InstanceBuilder instance_builder;
    try {
        instance_builder.read(test_params.instance_path.string());
        FAIL()
            << "InstanceBuilder::read did not throw for \""
            << test_params.instance_path << "\".";
    } catch (const std::invalid_argument& e) {
        EXPECT_NE(
                std::string(e.what()).find(test_params.expected_error_substring),
                std::string::npos)
            << "Exception message \"" << e.what()
            << "\" does not contain \"" << test_params.expected_error_substring << "\".";
    }
}

INSTANTIATE_TEST_SUITE_P(
        Irregular,
        IrregularInvalidInstanceTest,
        testing::ValuesIn(std::vector<IrregularInvalidInstanceTestParams>{
            {
                fs::path("data") / "irregular" / "tests" / "missing_objective.json",
                "missing \"objective\" field"
            }, {
                fs::path("data") / "irregular" / "tests" / "invalid_objective.json",
                "unrecognized \"objective\" value"
            }}));

struct IrregularTrivialSingleItemTestParams
{
    fs::path instance_path;
};

inline std::ostream& operator<<(std::ostream& os, const IrregularTrivialSingleItemTestParams& test_params)
{
    os << test_params.instance_path;
    return os;
}

class IrregularTrivialSingleItemTest: public testing::TestWithParam<IrregularTrivialSingleItemTestParams> { };

// Regression test: a single-item, single-bin instance whose item's only
// allowed rotation at its first (angle, mirror) entry requires mirroring
// used to make 'trivial_single_item' crash, since it hardcoded
// 'mirror = false' instead of reading it from that entry, producing an
// (angle, mirror) combination the item doesn't actually allow.
TEST_P(IrregularTrivialSingleItemTest, IrregularTrivialSingleItem)
{
    IrregularTrivialSingleItemTestParams test_params = GetParam();
    std::cout << "Instance path: " << test_params.instance_path << std::endl;

    InstanceBuilder instance_builder;
    instance_builder.read(test_params.instance_path.string());
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    optimize_parameters.verbosity_level = 0;
    Output output = optimize(instance, optimize_parameters);

    EXPECT_EQ(
            output.solution_pool.best().number_of_items(),
            instance.number_of_items());
}

INSTANTIATE_TEST_SUITE_P(
        Irregular,
        IrregularTrivialSingleItemTest,
        testing::ValuesIn(std::vector<IrregularTrivialSingleItemTestParams>{
            {
                fs::path("data") / "irregular" / "tests" / "mirror_only_rotation.json"
            }}));
