#include "packingsolver/box/optimize.hpp"
#include "packingsolver/box/instance_builder.hpp"

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

using namespace packingsolver;
using namespace packingsolver::box;
namespace po = boost::program_options;
namespace fs = boost::filesystem;

void read_args(
        packingsolver::Parameters<Instance, Solution>& parameters,
        const po::variables_map& vm)
{
    parameters.timer.set_sigint_handler();
    parameters.messages_to_stdout = true;
    if (vm.count("time-limit"))
        parameters.timer.set_time_limit(vm["time-limit"].as<double>());
    if (vm.count("verbosity-level"))
        parameters.verbosity_level = vm["verbosity-level"].as<int>();
    if (vm.count("log2stderr"))
        parameters.log_to_stderr = vm["log-to-stderr"].as<bool>();
    if (vm.count("log"))
        parameters.log_path = vm["log"].as<std::string>();
    parameters.log_to_stderr = vm.count("log-to-stderr");
    bool only_write_at_the_end = vm.count("only-write-at-the-end");
    if (!only_write_at_the_end) {

        std::string certificate_path = "";
        if (vm.count("certificate"))
            certificate_path = vm["certificate"].as<std::string>();

        std::string json_output_path = "";
        if (vm.count("output"))
            json_output_path = vm["output"].as<std::string>();

        parameters.new_solution_callback = [
            json_output_path,
            certificate_path](
                    const packingsolver::Output<Instance, Solution>& output)
        {
            if (!json_output_path.empty())
                output.write_json_output(json_output_path);
            if (!certificate_path.empty())
                output.solution_pool.best().write(certificate_path);
        };
    }
}

int main(int argc, char *argv[])
{

    // Parse program options
    po::options_description desc("Allowed options");
    desc.add_options()
        (",h", "Produce help message")

        ("items,i", po::value<std::string>()->required(), "Items path")
        ("bins,b", po::value<std::string>(), "Bins path")
        ("parameters", po::value<std::string>(), "Parameters path")

        ("bin-infinite-x", "")
        ("bin-infinite-y", "")
        ("bin-infinite-copies", "")
        ("bin-unweighted", "")
        ("item-infinite-copies", "")
        ("item-profits-auto", "")
        ("unweighted", "")
        ("no-item-rotation", "")

        ("objective,f", po::value<Objective>(), "Objective")

        ("output,o", po::value<std::string>(), "Output path")
        ("certificate,c", po::value<std::string>(), "Certificate path")
        ("log,l", po::value<std::string>(), "Log path")
        ("time-limit,t", po::value<double>(), "Time limit in seconds")
        ("seed,s", po::value<Seed>(), "Seed (not used)")
        ("only-write-at-the-end,e", "Only write output and certificate files at the end")
        ("verbosity-level,v", po::value<int>(), "Verbosity level")
        ("log-to-stderr,w", "Write log in stderr")

        ("linear-programming-solver,", po::value<columngenerationsolver::SolverName>(), "set linear programming solver")
        ("optimization-mode,", po::value<OptimizationMode>(), "set optimization mode")
        ;
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    if (vm.count("help")) {
        std::cout << desc << std::endl;;
        return 1;
    }
    try {
        po::notify(vm);
    } catch (const po::required_option& e) {
        std::cout << desc << std::endl;;
        return 1;
    }

    // Build instance.

    InstanceBuilder instance_builder;

    std::string instance_path = vm["items"].as<std::string>();
    if (fs::is_regular_file(instance_path)) {
        instance_path = "";
    } else if (fs::is_regular_file(instance_path + "_items.csv")) {
        instance_path = instance_path + "_";
    } else if (fs::is_regular_file(instance_path + "items.csv")) {
        instance_path = instance_path;
    } else if (fs::is_regular_file(instance_path + "/items.csv")) {
        instance_path = instance_path + "/";
    } else {
        throw std::invalid_argument("");
    }

    if (instance_path.empty()) {
        instance_builder.read_item_types(vm["items"].as<std::string>());
    } else {
        instance_builder.read_item_types(instance_path + "items.csv");
    }

    if (vm.count("bins")) {
        instance_builder.read_bin_types(vm["bins"].as<std::string>());
    } else {
        instance_builder.read_bin_types(instance_path + "bins.csv");
    }

    if (vm.count("bin-infinite-x"))
        instance_builder.set_bin_types_infinite_x();
    if (vm.count("bin-infinite-y"))
        instance_builder.set_bin_types_infinite_y();
    if (vm.count("bin-infinite-copies"))
        instance_builder.set_bin_types_infinite_copies();
    if (vm.count("item-infinite-copies"))
        instance_builder.set_item_types_infinite_copies();
    if (vm.count("no-item-rotation"))
        instance_builder.set_item_types_oriented();
    if (vm.count("unweighted"))
        instance_builder.set_item_types_unweighted();
    if (vm.count("bin-unweighted"))
        instance_builder.set_bin_types_unweighted();
    if (vm.count("item-profits-auto"))
        instance_builder.set_item_types_profits_auto();

    if (vm.count("parameters")) {
        instance_builder.read_parameters(vm["parameters"].as<std::string>());
    } else if (fs::is_regular_file(instance_path + "parameters.csv")) {
        instance_builder.read_parameters(instance_path + "parameters.csv");
    }

    if (vm.count("objective"))
        instance_builder.set_objective(vm["objective"].as<Objective>());

    Instance instance = instance_builder.build();

    // Read algorithm parameters.

#if XPRESS_FOUND
    if (optimize_parameters.solver_name
            == columngenerationsolver::SolverName::Xpress)
        XPRSinit(NULL);
#endif

    OptimizeParameters parameters;
    read_args(parameters, vm);
    if (vm.count("linear-programming-solver"))
        parameters.linear_programming_solver_name = vm["linear-programming-solver"].as<columngenerationsolver::SolverName>();
    if (vm.count("optimization-mode"))
        parameters.optimization_mode = vm["optimization-mode"].as<OptimizationMode>();

    const box::Output output = optimize(instance, parameters);

    if (vm.count("certificate"))
        output.solution_pool.best().write(vm["certificate"].as<std::string>());
    if (vm.count("output"))
        output.write_json_output(vm["output"].as<std::string>());

#if XPRESS_FOUND
    if (optimize_parameters.solver_name
            == columngenerationsolver::SolverName::Xpress)
        XPRSfree();
#endif

    return 0;
}
