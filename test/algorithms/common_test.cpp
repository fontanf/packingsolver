#include "packingsolver/algorithms/common.hpp"

#include <gtest/gtest.h>

using namespace packingsolver;

struct PowerOfTwoTestParams
{
    double value;
    double expected_largest_power_of_two_lesser_or_equal;
    double expected_smallest_power_of_two_greater_or_equal;
};

void PrintTo(const PowerOfTwoTestParams& params, std::ostream* os)
{
    *os << "value " << params.value << "\n";
    *os << "expected_largest_power_of_two_lesser_or_equal " << params.expected_largest_power_of_two_lesser_or_equal << "\n";
    *os << "expected_smallest_power_of_two_greater_or_equal " << params.expected_smallest_power_of_two_greater_or_equal << "\n";
}

class PowerOfTwoTest: public testing::TestWithParam<PowerOfTwoTestParams> { };

TEST_P(PowerOfTwoTest, PowerOfTwo)
{
    PowerOfTwoTestParams test_params = GetParam();
    PrintTo(test_params, &std::cout);
    // Powers of two are exact in floating-point arithmetic.
    EXPECT_EQ(
            largest_power_of_two_lesser_or_equal(test_params.value),
            test_params.expected_largest_power_of_two_lesser_or_equal);
    EXPECT_EQ(
            smallest_power_of_two_greater_or_equal(test_params.value),
            test_params.expected_smallest_power_of_two_greater_or_equal);
    // The helpers are also called with integer lengths.
    if (test_params.value >= 1 && test_params.value == (Length)test_params.value) {
        Length length = (Length)test_params.value;
        EXPECT_EQ(
                largest_power_of_two_lesser_or_equal(length),
                test_params.expected_largest_power_of_two_lesser_or_equal);
        EXPECT_EQ(
                smallest_power_of_two_greater_or_equal(length),
                test_params.expected_smallest_power_of_two_greater_or_equal);
    }
}

INSTANTIATE_TEST_SUITE_P(
        Algorithms,
        PowerOfTwoTest,
        testing::ValuesIn(std::vector<PowerOfTwoTestParams>{
            {0.25, 0.25, 0.25},
            {0.3, 0.25, 0.5},
            {0.5, 0.5, 0.5},
            {0.75, 0.5, 1},
            {1, 1, 1},
            {3, 2, 4},
            {4, 4, 4},
            {5, 4, 8},
            {1000, 512, 1024},
            {1024, 1024, 1024},
        }),
        [](const testing::TestParamInfo<PowerOfTwoTest::ParamType>& info) {
            return std::to_string(info.index);
        });
