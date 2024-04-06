#pragma once

#include "packingsolver/algorithms/common.hpp"

#include <fstream>
#include <iomanip>

namespace packingsolver
{

struct SemiTrailerTruckData
{
    /** Boolean indicating if the bin is a semi-trailer truck. */
    bool is = false;

    /** Weight of the tractor. */
    Weight tractor_weight = 0;

    /** Distance between the front and middle axles of the tractor. */
    Length front_axle_middle_axle_distance = 0;

    /**
     * Distance between the front axle and the center of gravity of the
     * tractor.
     */
    Length front_axle_tractor_gravity_center_distance = 0;

    /** Distance between the front axle and the harness of the tractor. */
    Length front_axle_harness_distance = 0;

    /** Weight of the empty trailer. */
    Weight empty_trailer_weight = 0;

    /** Distance between the harness and the rear axle of the trailer. */
    Length harness_rear_axle_distance = 0;

    /**
     * Distance between the center of gravity of the trailer and the rear axle.
     */
    Length trailer_gravity_center_rear_axle_distance = 0;

    /** Distance between the start of the trailer and the harness. */
    Length trailer_start_harness_distance = 0;

    /** Maximum weight on the rear axle of the trailer. */
    Weight rear_axle_maximum_weight = std::numeric_limits<Weight>::max();

    /** Maximum weight on the middle axle of the trailer. */
    Weight middle_axle_maximum_weight = std::numeric_limits<Weight>::max();

    std::pair<Weight, Weight> compute_axle_weights(
            double weight_weighted_sum,
            double weight) const
    {
        if (!is)
            return {0, 0};
        double stacks_gravity_center_trailer_start_distance  // eje
            = weight_weighted_sum
            / weight;  // tmt
        //std::cout << "stacks_gravity_center_trailer_start_distance "
        //    << stacks_gravity_center_trailer_start_distance
        //    << std::endl;
        double stacks_gravity_center_rear_axle_distance  // ejr
            = trailer_start_harness_distance // EJeh
            + harness_rear_axle_distance  // EJhr
            - stacks_gravity_center_trailer_start_distance;  // eje
        //std::cout << "stacks_gravity_center_rear_axle_distance "
        //    << stacks_gravity_center_rear_axle_distance
        //    << std::endl;
        double harness_weight  // emh
            = (weight  // tmt
                    * stacks_gravity_center_rear_axle_distance  // ejr
                    + empty_trailer_weight  // EM
                    * trailer_gravity_center_rear_axle_distance)  // EJcr
            / harness_rear_axle_distance;  // EJhr
        //std::cout << "harness_weight " << harness_weight << std::endl;
        double rear_axle_weight  // emr
            = weight  // tmt
            + empty_trailer_weight  // EM
            - harness_weight;  // emh
        double middle_axle_weight  // emm
            = (tractor_weight  // CM
                    * front_axle_tractor_gravity_center_distance  // CJfc
                    + harness_weight  // emh
                    * front_axle_harness_distance)  // CJfh
            / front_axle_middle_axle_distance; // CJfm
        return {middle_axle_weight, rear_axle_weight};
    }

    void read(std::string label, std::string value)
    {
        if (label == "IS_SEMI_TRAILER_TRUCK") {
            is = std::stol(value);
        } else if (label == "TRACTOR_WEIGHT") {
            tractor_weight = (Weight)std::stod(value);
        } else if (label == "FRONT_AXLE_MIDDLE_AXLE_DISTANCE") {
            front_axle_middle_axle_distance = (Length)std::stol(value);
        } else if (label == "FRONT_AXLE_TRACTOR_GRAVITY_CENTER_DISTANCE") {
            front_axle_tractor_gravity_center_distance = (Length)std::stol(value);
        } else if (label == "FRONT_AXLE_HARNESS_DISTANCE") {
            front_axle_harness_distance = (Length)std::stol(value);
        } else if (label == "EMPTY_TRAILER_WEIGHT") {
            empty_trailer_weight = (Weight)std::stod(value);
        } else if (label == "HARNESS_REAR_AXLE_DISTANCE") {
            harness_rear_axle_distance = (Length)std::stol(value);
        } else if (label == "TRAILER_GRAVITY_CENTER_REAR_AXLE_DISTANCE") {
            trailer_gravity_center_rear_axle_distance = (Length)std::stol(value);
        } else if (label == "TRAILER_START_HARNESS_DISTANCE") {
            trailer_start_harness_distance = (Length)std::stol(value);
        } else if (label == "REAR_AXLE_MAXIMUM_WEIGHT") {
            rear_axle_maximum_weight = (Weight)std::stod(value);
        } else if (label == "MIDDLE_AXLE_MAXIMUM_WEIGHT") {
            middle_axle_maximum_weight = (Weight)std::stod(value);
        }
    }

    static void write_header(std::ofstream& os)
    {
        os << "IS_SEMI_TRAILER_TRUCK,"
            "TRACTOR_WEIGHT,"
            "FRONT_AXLE_MIDDLE_AXLE_DISTANCE,"
            "FRONT_AXLE_TRACTOR_GRAVITY_CENTER_DISTANCE,"
            "FRONT_AXLE_HARNESS_DISTANCE,"
            "EMPTY_TRAILER_WEIGHT,"
            "HARNESS_REAR_AXLE_DISTANCE,"
            "TRAILER_GRAVITY_CENTER_REAR_AXLE_DISTANCE,"
            "TRAILER_START_HARNESS_DISTANCE,"
            "REAR_AXLE_MAXIMUM_WEIGHT,"
            "MIDDLE_AXLE_MAXIMUM_WEIGHT";
    }

    void write(std::ofstream& os) const
    {
        os << is << ","
            << tractor_weight << ","
            << front_axle_middle_axle_distance << ","
            << front_axle_tractor_gravity_center_distance << ","
            << front_axle_harness_distance << ","
            << empty_trailer_weight << ","
            << harness_rear_axle_distance << ","
            << trailer_gravity_center_rear_axle_distance << ","
            << trailer_start_harness_distance << ","
            << rear_axle_maximum_weight << ","
            << middle_axle_maximum_weight;
    }

    static void print_header_1(std::ostream& os)
    {
        os
            << std::setw(6) << "IsSTT"
            << std::setw(8) << "TtW"
            << std::setw(8) << "FaMaD"
            << std::setw(8) << "FaTtGcD"
            << std::setw(8) << "FaHD"
            << std::setw(8) << "ETlW"
            << std::setw(8) << "HRaD"
            << std::setw(8) << "TlGcRaD"
            << std::setw(8) << "TlSHD"
            << std::setw(10) << "MaMawWe"
            << std::setw(10) << "RaMaxWe"
            ;
    }

    static void print_header_2(std::ostream& os)
    {
        os
            << std::setw(6) << "-----"
            << std::setw(8) << "---"
            << std::setw(8) << "-----"
            << std::setw(8) << "-------"
            << std::setw(8) << "----"
            << std::setw(8) << "----"
            << std::setw(8) << "----"
            << std::setw(8) << "-------"
            << std::setw(8) << "-----"
            << std::setw(10) << "-------"
            << std::setw(10) << "-------"
            ;
    }

    void format(std::ostream& os) const
    {
        os
            << std::setw(6) << is
            << std::setw(8) << tractor_weight
            << std::setw(8) << front_axle_middle_axle_distance
            << std::setw(8) << front_axle_tractor_gravity_center_distance
            << std::setw(8) << front_axle_harness_distance
            << std::setw(8) << empty_trailer_weight
            << std::setw(8) << harness_rear_axle_distance
            << std::setw(8) << trailer_gravity_center_rear_axle_distance
            << std::setw(8) << trailer_start_harness_distance
            << std::setw(10) << middle_axle_maximum_weight
            << std::setw(10) << rear_axle_maximum_weight
            << std::endl;
    }

};

}
