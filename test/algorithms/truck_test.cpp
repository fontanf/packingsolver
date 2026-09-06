#include "packingsolver/algorithms/truck.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

using namespace packingsolver;

namespace
{

/** Semi-trailer truck with a complete geometry. */
SemiTrailerTruckData complete_truck()
{
    SemiTrailerTruckData semi_trailer_truck_data;
    semi_trailer_truck_data.is = true;
    semi_trailer_truck_data.tractor_weight = 8000;
    semi_trailer_truck_data.front_axle_middle_axle_distance = 380;
    semi_trailer_truck_data.front_axle_tractor_gravity_center_distance = 100;
    semi_trailer_truck_data.front_axle_harness_distance = 320;
    semi_trailer_truck_data.empty_trailer_weight = 6000;
    semi_trailer_truck_data.harness_rear_axle_distance = 800;
    semi_trailer_truck_data.trailer_gravity_center_rear_axle_distance = 400;
    semi_trailer_truck_data.trailer_start_harness_distance = 100;
    return semi_trailer_truck_data;
}

}

TEST(Truck, ComputeAxleWeights)
{
    SemiTrailerTruckData semi_trailer_truck_data = complete_truck();
    std::pair<Weight, Weight> axle_weights
        = semi_trailer_truck_data.compute_axle_weights(200000, 2000);
    EXPECT_NEAR(axle_weights.first, 6315.789473684211, 1e-6);
    EXPECT_NEAR(axle_weights.second, 3000.0, 1e-6);
}

TEST(Truck, CheckNotATruck)
{
    // A bin type that isn't a semi-trailer truck needs no truck data.
    SemiTrailerTruckData semi_trailer_truck_data;
    EXPECT_NO_THROW(semi_trailer_truck_data.check());
}

TEST(Truck, CheckWithoutHarnessRearAxleDistance)
{
    // Every axle weight is computed from 'harness_weight', which divides by
    // 'harness_rear_axle_distance', so a semi-trailer truck without it is an
    // inconsistent input, not a case to silently skip.
    SemiTrailerTruckData semi_trailer_truck_data = complete_truck();
    semi_trailer_truck_data.harness_rear_axle_distance = 0;
    EXPECT_THROW(semi_trailer_truck_data.check(), std::invalid_argument);
}

TEST(Truck, CheckWithoutFrontAxleMiddleAxleDistance)
{
    // The middle axle weight is computed from 'harness_weight', which
    // divides by 'front_axle_middle_axle_distance', so a semi-trailer truck
    // without it is an inconsistent input, not a case to silently skip.
    SemiTrailerTruckData semi_trailer_truck_data = complete_truck();
    semi_trailer_truck_data.front_axle_middle_axle_distance = 0;
    EXPECT_THROW(semi_trailer_truck_data.check(), std::invalid_argument);
}
