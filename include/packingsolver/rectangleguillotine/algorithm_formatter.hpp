#pragma once

#include "packingsolver/rectangleguillotine/solution.hpp"

#include <mutex>

namespace packingsolver
{
namespace rectangleguillotine
{

class AlgorithmFormatter
{

public:

    /** Constructor. */
    AlgorithmFormatter(
            const Instance& instance,
            const packingsolver::Parameters<Instance, Solution>& parameters,
            packingsolver::Output<Instance, Solution>& output):
        instance_(instance),
        parameters_(parameters),
        output_(output),
        os_(parameters.create_os())
    {
        // Check optimality.
        if (instance_.objective() == Objective::BinPacking) {
            if (output_.solution_pool.best().full()
                    && output_.bin_packing_bound == output_.solution_pool.best().number_of_bins()) {
                end_ = true;
            }
        }
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

    /** Update the knapsack bound. */
    void update_knapsack_bound(
            Profit profit);

    /** Update the bin packing bound. */
    void update_bin_packing_bound(
            BinPos number_of_bins);

    /** Update the bin packing bound. */
    void update_variable_sized_bin_packing_bound(
            Profit cost);

    /** Method to call at the end of the algorithm. */
    void end();

    /** Get end boolean. */
    bool& end_boolean() { return end_; };

    const packingsolver::Output<Instance, Solution>& output() const { return output_; }

private:

    /** Instance. */
    const Instance& instance_;

    /** Parameters. */
    const packingsolver::Parameters<Instance, Solution>& parameters_;

    /** Output. */
    packingsolver::Output<Instance, Solution>& output_;

    /** Output stream. */
    std::unique_ptr<optimizationtools::ComposeStream> os_;

    /** End boolean. */
    bool end_ = false;

    /** Mutex. */
    std::mutex mutex_;

};

}
}
