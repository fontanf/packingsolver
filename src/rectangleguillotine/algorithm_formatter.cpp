#include "packingsolver/rectangleguillotine/algorithm_formatter.hpp"

#include <iomanip>
#include <sstream>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

void AlgorithmFormatter::start()
{
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
                << std::setw(12) << "# bins"
                << std::setw(12) << "Waste"
                << std::setw(12) << "Waste (%)"
                << std::setw(32) << "Comment"
                << std::endl
                << std::setw(12) << "----"
                << std::setw(12) << "------"
                << std::setw(12) << "-----"
                << std::setw(12) << "---------"
                << std::setw(32) << "-------"
                << std::endl;
        break;
    } case Objective::OpenDimensionX: {
        *os_
                << std::setw(12) << "Time"
                << std::setw(12) << "X"
                << std::setw(32) << "Comment"
                << std::endl
                << std::setw(12) << "----"
                << std::setw(12) << "-----"
                << std::setw(32) << "-------"
                << std::endl;
        break;
    } case Objective::OpenDimensionY: {
        *os_
                << std::setw(12) << "Time"
                << std::setw(12) << "Y"
                << std::setw(32) << "Comment"
                << std::endl
                << std::setw(12) << "----"
                << std::setw(12) << "-----"
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
        ss << "Problem type '" << Instance::type() << "' does not support objective \""
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
                << std::setw(12) << output_.solution_pool.best().number_of_bins()
                << std::setw(12) << output_.solution_pool.best().waste()
                << std::setw(12) << std::fixed << std::setprecision(2) << 100 * output_.solution_pool.best().waste_percentage() << std::defaultfloat << std::setprecision(precision)
                << std::setw(32) << s
                << std::endl;
        break;
    } case Objective::OpenDimensionX: {
        *os_
                << std::setw(12) << std::fixed << std::setprecision(3) << output_.time << std::defaultfloat << std::setprecision(precision)
                << std::setw(12) << output_.solution_pool.best().width()
                << std::setw(32) << s
                << std::endl;
        break;
    } case Objective::OpenDimensionY: {
        *os_
                << std::setw(12) << std::fixed << std::setprecision(3) << output_.time << std::defaultfloat << std::setprecision(precision)
                << std::setw(12) << output_.solution_pool.best().height()
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
        ss << "Problem type '" << Instance::type() << "' does not support objective \""
            << instance_.objective() << "\"";
        throw std::logic_error(ss.str());
    }
    }
}

void AlgorithmFormatter::update_solution(
        const Solution& solution,
        const std::string& s)
{
    mutex_.lock();
    output_.time = parameters_.timer.elapsed_time();
    int new_best = output_.solution_pool.add(solution);
    if (new_best == 1) {
        print(s);
        output_.json["IntermediaryOutputs"].push_back(output_.to_json());
        parameters_.new_solution_callback(output_);
    }
    mutex_.unlock();
}

void AlgorithmFormatter::end()
{
    output_.time = parameters_.timer.elapsed_time();
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
