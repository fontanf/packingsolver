#include "packingsolver/onedimensional/algorithm_formatter.hpp"

#include <iomanip>
#include <sstream>

using namespace packingsolver;
using namespace packingsolver::onedimensional;

void AlgorithmFormatter::start()
{
    if (parameters_.write_json_output)
        output_.json["Parameters"] = parameters_.to_json();

    if (parameters_.verbosity_level == 0)
        return;
    *os_
        << "=================================" << std::endl
        << "          PackingSolver          " << std::endl
        << "=================================" << std::endl
        << std::endl
        << "Problem type" << std::endl
        << "------------" << std::endl
        << Instance::type() << std::endl
        << std::endl
        << "Instance" << std::endl
        << "--------" << std::endl;
    output_.solution_pool.best().instance().format(*os_, parameters_.verbosity_level);
}

void AlgorithmFormatter::print_header()
{
    if (parameters_.verbosity_level == 0)
        return;
    *os_ << std::endl;
    switch (instance_.objective()) {
    case Objective::Default: {
        *os_
                << std::setw(12) << "Time"
                << std::setw(12) << "Profit"
                << std::setw(6) << "Full"
                << std::setw(12) << "Waste"
                << std::setw(32) << "Comment"
                << std::endl
                << std::setw(12) << "----"
                << std::setw(12) << "------"
                << std::setw(6) << "----"
                << std::setw(12) << "-----"
                << std::setw(32) << "-------"
                << std::endl;
        break;
    } case Objective::BinPacking: {
        *os_
                << std::setw(12) << "Time"
                << std::setw(8) << "Bins"
                << std::setw(16) << "Full waste (%)"
                << std::setw(32) << "Comment"
                << std::endl
                << std::setw(12) << "----"
                << std::setw(8) << "----"
                << std::setw(16) << "--------------"
                << std::setw(32) << "-------"
                << std::endl;
        break;
    } case Objective::BinPackingWithLeftovers: {
        *os_
                << std::setw(12) << "Time"
                << std::setw(12) << "Waste"
                << std::setw(12) << "Waste (%)"
                << std::setw(32) << "Comment"
                << std::endl
                << std::setw(12) << "----"
                << std::setw(12) << "-----"
                << std::setw(12) << "---------"
                << std::setw(32) << "-------"
                << std::endl;
        break;
    } case Objective::Knapsack: {
        *os_
                << std::setw(12) << "Time"
                << std::setw(14) << "Profit"
                << std::setw(10) << "# items"
                << std::setw(32) << "Comment"
                << std::endl
                << std::setw(12) << "----"
                << std::setw(14) << "------"
                << std::setw(10) << "-------"
                << std::setw(32) << "-------"
                << std::endl;
        break;
    } case Objective::Feasibility: {
        *os_
                << std::setw(12) << "Time"
                << std::setw(14) << "Profit"
                << std::setw(10) << "# items"
                << std::setw(32) << "Comment"
                << std::endl
                << std::setw(12) << "----"
                << std::setw(14) << "------"
                << std::setw(10) << "-------"
                << std::setw(32) << "-------"
                << std::endl;
        break;
    } case Objective::VariableSizedBinPacking: {
        *os_
                << std::setw(12) << "Time"
                << std::setw(14) << "Cost"
                << std::setw(8) << "# bins"
                << std::setw(16) << "Full waste (%)"
                << std::setw(32) << "Comment"
                << std::endl
                << std::setw(12) << "----"
                << std::setw(14) << "----"
                << std::setw(8) << "------"
                << std::setw(16) << "--------------"
                << std::setw(32) << "-------"
                << std::endl;
        break;
    } default: {
        std::stringstream ss;
        ss << FUNC_SIGNATURE << ": "
            << "problem type '" << Instance::type() << "' does not support objective \""
            << instance_.objective() << "\"";
        throw std::logic_error(ss.str());
    }
    }
}

void AlgorithmFormatter::print(
        const std::string& s)
{
    if (parameters_.verbosity_level == 0)
        return;
    std::streamsize precision = std::cout.precision();

    switch (instance_.objective()) {
    case Objective::Default: {
        *os_
                << std::setw(12) << std::fixed << std::setprecision(3) << output_.time << std::defaultfloat << std::setprecision(precision)
                << std::setw(12) << output_.solution_pool.best().profit()
                << std::setw(6) << output_.solution_pool.best().full()
                << std::setw(12) << output_.solution_pool.best().waste()
                << std::setw(32) << s
                << std::endl;
        break;
    } case Objective::BinPacking: {
        *os_
                << std::setw(12) << std::fixed << std::setprecision(3) << output_.time << std::defaultfloat << std::setprecision(precision)
                << std::setw(8) << output_.solution_pool.best().number_of_bins()
                << std::setw(16) << std::fixed << std::setprecision(2) << 100 * output_.solution_pool.best().full_waste_percentage() << std::defaultfloat << std::setprecision(precision)
                << std::setw(32) << s
                << std::endl;
        break;
    } case Objective::BinPackingWithLeftovers: {
        *os_
                << std::setw(12) << std::fixed << std::setprecision(3) << output_.time << std::defaultfloat << std::setprecision(precision)
                << std::setw(12) << output_.solution_pool.best().waste()
                << std::setw(12) << std::fixed << std::setprecision(2) << 100 * output_.solution_pool.best().waste_percentage() << std::defaultfloat << std::setprecision(precision)
                << std::setw(32) << s
                << std::endl;
        break;
    } case Objective::Knapsack: {
        *os_
                << std::setw(12) << std::fixed << std::setprecision(3) << output_.time << std::defaultfloat << std::setprecision(precision)
                << std::setw(14) << output_.solution_pool.best().profit()
                << std::setw(10) << output_.solution_pool.best().number_of_items()
                << std::setw(32) << s
                << std::endl;
        break;
    } case Objective::Feasibility: {
        *os_
                << std::setw(12) << std::fixed << std::setprecision(3) << output_.time << std::defaultfloat << std::setprecision(precision)
                << std::setw(14) << output_.solution_pool.best().profit()
                << std::setw(10) << output_.solution_pool.best().number_of_items()
                << std::setw(32) << s
                << std::endl;
        break;
    } case Objective::VariableSizedBinPacking: {
        *os_
                << std::setw(12) << std::fixed << std::setprecision(3) << output_.time << std::defaultfloat << std::setprecision(precision)
                << std::setw(14) << output_.solution_pool.best().cost()
                << std::setw(8) << output_.solution_pool.best().number_of_bins()
                << std::setw(16) << std::fixed << std::setprecision(2) << 100 * output_.solution_pool.best().full_waste_percentage() << std::defaultfloat << std::setprecision(precision)
                << std::setw(32) << s
                << std::endl;
        break;
    } default: {
        std::stringstream ss;
        ss << FUNC_SIGNATURE << ": "
            << "problem type '" << Instance::type() << "' does not support objective \""
            << instance_.objective() << "\"";
        throw std::logic_error(ss.str());
    }
    }
}

void AlgorithmFormatter::update_solution(
        const Solution& solution,
        const std::string& s)
{
    std::unique_lock<std::mutex> lock(mutex_);
    output_.time = parameters_.timer.elapsed_time();
    int new_best = output_.solution_pool.add(solution);
    bool callback = false;
    packingsolver::Output<Instance, Solution> output_snapshot(instance_);
    if (new_best == 1) {
        print(s);
        if (parameters_.write_json_output)
            output_.json["IntermediaryOutputs"].push_back(output_.to_json());

        // Check optimality.
        if (instance_.objective() == Objective::Knapsack) {
            if (equal(output_.knapsack_bound, output_.solution_pool.best().profit())) {
                end_ = true;
            }
       } else if (instance_.objective() == Objective::BinPacking) {
            if (output_.solution_pool.best().full()
                    && output_.bin_packing_bound == output_.solution_pool.best().number_of_bins()) {
                end_ = true;
            }
       } else if (instance_.objective() == Objective::VariableSizedBinPacking) {
            if (output_.solution_pool.best().full()
                    && equal(output_.variable_sized_bin_packing_bound, output_.solution_pool.best().cost())) {
                end_ = true;
            }
        } else if (instance_.objective() == Objective::Feasibility) {
            if (output_.solution_pool.best().full()) {
                end_ = true;
            }
        }

        // Take a snapshot of the output so the (potentially heavy, file-I/O
        // performing) callback can run outside the critical section.
        output_snapshot = output_;
        callback = true;
    }
    lock.unlock();

    // Invoke the callback outside the lock so that file I/O performed by it
    // does not serialize the worker threads.
    if (callback)
        parameters_.new_solution_callback(output_snapshot);
}

void AlgorithmFormatter::update_knapsack_bound(
        Profit profit)
{
    std::unique_lock<std::mutex> lock(mutex_);
    bool callback = false;
    packingsolver::Output<Instance, Solution> output_snapshot(instance_);
    if (profit < output_.knapsack_bound) {
        output_.knapsack_bound = profit;
        if (parameters_.write_json_output)
            output_.json["IntermediaryOutputs"].push_back(output_.to_json());

        // Check optimality.
        if (equal(output_.knapsack_bound, output_.solution_pool.best().profit())) {
            end_ = true;
        }

        output_snapshot = output_;
        callback = true;
    }
    lock.unlock();

    if (callback)
        parameters_.new_solution_callback(output_snapshot);
}

void AlgorithmFormatter::update_bin_packing_bound(
        BinPos number_of_bins)
{
    std::unique_lock<std::mutex> lock(mutex_);
    bool callback = false;
    packingsolver::Output<Instance, Solution> output_snapshot(instance_);
    if (number_of_bins > output_.bin_packing_bound) {
        output_.bin_packing_bound = number_of_bins;
        if (parameters_.write_json_output)
            output_.json["IntermediaryOutputs"].push_back(output_.to_json());

        // Check optimality.
        if (output_.solution_pool.best().full()
                && output_.bin_packing_bound == output_.solution_pool.best().number_of_bins()) {
            end_ = true;
        }

        output_snapshot = output_;
        callback = true;
    }
    lock.unlock();

    if (callback)
        parameters_.new_solution_callback(output_snapshot);
}

void AlgorithmFormatter::update_variable_sized_bin_packing_bound(
        Profit cost)
{
    std::unique_lock<std::mutex> lock(mutex_);
    bool callback = false;
    packingsolver::Output<Instance, Solution> output_snapshot(instance_);
    if (cost > output_.variable_sized_bin_packing_bound) {
        output_.variable_sized_bin_packing_bound = cost;
        if (parameters_.write_json_output)
            output_.json["IntermediaryOutputs"].push_back(output_.to_json());

        // Check optimality.
        if (output_.solution_pool.best().full()
                && equal(output_.variable_sized_bin_packing_bound, output_.solution_pool.best().cost())) {
            end_ = true;
        }

        output_snapshot = output_;
        callback = true;
    }
    lock.unlock();

    if (callback)
        parameters_.new_solution_callback(output_snapshot);
}

void AlgorithmFormatter::end()
{
    output_.time = parameters_.timer.elapsed_time();
    if (parameters_.write_json_output)
        output_.json["Output"] = output_.to_json();

    if (parameters_.verbosity_level == 0)
        return;
    *os_
        << std::endl
        << "Final statistics" << std::endl
        << "----------------" << std::endl;
    output_.format(*os_);
    *os_
        << std::endl
        << "Solution" << std::endl
        << "--------" << std::endl;
    output_.solution_pool.best().format(*os_, parameters_.verbosity_level);
}
