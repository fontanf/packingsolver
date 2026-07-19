#pragma once

#include "packingsolver/rectangle/optimize.hpp"

#include <mutex>

namespace packingsolver
{
namespace rectangle
{

class AlgorithmFormatter
{

public:

    /** Constructor. */
    AlgorithmFormatter(
            const Instance& instance,
            const packingsolver::Parameters<Instance, Solution, Output>& parameters,
            Output& output):
        instance_(instance),
        parameters_(parameters),
        output_(output),
        os_(parameters.create_os())
    {
        if (output_.is_proven_optimal())
            end_ = true;
    }

    /** Print the header. */
    void start();

    /** Print the header. */
    void print_header();

    /** Print current state. */
    void print(
            const std::string& s);

    /** Update the solution. */
    void update_solution(
            const Solution& solution,
            const std::string& s);

    /** Mark the instance as proven infeasible (Feasibility objective only). */
    void update_is_proven_infeasible();

    /** Update the knapsack bound. */
    void update_knapsack_bound(
            Profit profit);

    /** Update the bin packing bound. */
    void update_bin_packing_bound(
            BinPos number_of_bins);

    /** Update the variable-sized bin packing bound. */
    void update_variable_sized_bin_packing_bound(
            Profit cost);

    /** Update the open dimension X bound. */
    void update_open_dimension_x_bound(
            Length x);

    /** Update the open dimension Y bound. */
    void update_open_dimension_y_bound(
            Length y);

    /** Update all applicable bounds from another output. */
    void update_bounds(
            const Output& output);

    /** Method to call at the end of the algorithm. */
    void end();

    /** Get end boolean. */
    bool& end_boolean() { return end_; };

private:

    /** Instance. */
    const Instance& instance_;

    /** Parameters. */
    const packingsolver::Parameters<Instance, Solution, Output>& parameters_;

    /** Output. */
    Output& output_;

    /** Output stream. */
    std::unique_ptr<optimizationtools::ComposeStream> os_;

    /** End boolean. */
    bool end_ = false;

    /** Mutex. */
    std::mutex mutex_;

};

}
}
