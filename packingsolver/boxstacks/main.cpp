#include "packingsolver/boxstacks/optimize.hpp"
#include "packingsolver/boxstacks/instance_builder.hpp"

#include <boost/program_options.hpp>

using namespace packingsolver;
using namespace packingsolver::boxstacks;
namespace po = boost::program_options;

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
        ("defects,d", po::value<std::string>(), "Defects path")
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

        ("unloading-constraint,", po::value<rectangle::UnloadingConstraint>(), "")

        ("output,o", po::value<std::string>(), "Output path")
        ("certificate,c", po::value<std::string>(), "Certificate path")
        ("log,l", po::value<std::string>(), "Log path")
        ("time-limit,t", po::value<double>(), "Time limit in seconds")
        ("seed,s", po::value<Seed>(), "Seed (not used)")
        ("only-write-at-the-end,e", "Only write output and certificate files at the end")
        ("verbosity-level,v", po::value<int>(), "Verbosity level")
        ("log2stderr,w", "Write log in stderr")
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

    InstanceBuilder instance_builder;

    std::string items_path = vm["items"].as<std::string>();
    if (!std::ifstream(items_path).good())
        if (std::ifstream(items_path + "_items.csv").good())
            items_path = items_path + "_items.csv";
    instance_builder.read_item_types(items_path);

    std::string bins_path = (vm.count("bins"))?
        vm["bins"].as<std::string>():
        vm["items"].as<std::string>() + "_bins.csv";
    instance_builder.read_bin_types(bins_path);

    std::string defects_path = (vm.count("defects"))?
        vm["defects"].as<std::string>():
        (std::ifstream(vm["items"].as<std::string>() + "_defects.csv").good())?
        vm["items"].as<std::string>() + "_defects.csv":
        "";
    if (!defects_path.empty())
        instance_builder.read_defects(defects_path);

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

    std::string parameters_path = (vm.count("parameters"))?
        vm["parameters"].as<std::string>():
        (std::ifstream(vm["items"].as<std::string>() + "_parameters.csv").good())?
        vm["items"].as<std::string>() + "_parameters.csv":
        "";
    if (!parameters_path.empty())
        instance_builder.read_parameters(parameters_path);

    if (vm.count("objective"))
        instance_builder.set_objective(vm["objective"].as<Objective>());
    if (vm.count("unloading-constraint"))
        instance_builder.set_unloading_constraint(vm["unloading-constraint"].as<rectangle::UnloadingConstraint>());

    Instance instance = instance_builder.build();

#if XPRESS_FOUND
    if (optimize_parameters.linear_programming_solver
            == columngenerationsolver::LinearProgrammingSolver::Xpress)
        XPRSinit(NULL);
#endif

    OptimizeParameters parameters;
    read_args(parameters, vm);
    const boxstacks::Output output = optimize(instance, parameters);

    if (vm.count("certificate"))
        output.solution_pool.best().write(vm["certificate"].as<std::string>());
    if (vm.count("output"))
        output.write_json_output(vm["output"].as<std::string>());

#if XPRESS_FOUND
    if (optimize_parameters.linear_programming_solver
            == columngenerationsolver::LinearProgrammingSolver::Xpress)
        XPRSfree();
#endif

    return 0;
}
