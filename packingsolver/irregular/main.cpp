#include "packingsolver/irregular/optimize.hpp"

#include <boost/program_options.hpp>

using namespace packingsolver;
using namespace packingsolver::irregular;
namespace po = boost::program_options;

int main(int argc, char *argv[])
{

    // Parse program options
    std::string instance_path = "";
    //Parameters parameters;
    Objective objective = Objective::Default;
    std::string certificate_path = "";
    std::string output_path = "";
    std::string log_path = "";
    int verbosity_level = 0;
    int log_levelmax = 999;
    double time_limit = std::numeric_limits<double>::infinity();
    Seed seed = 0;
    OptimizeOptionalParameters optimize_parameters;

    po::options_description desc("Allowed options");
    desc.add_options()
        (",h", "Produce help message")

        ("input,i", po::value<std::string>(&instance_path)->required(), "Input path")

        ("objective,f", po::value<Objective>(&objective), "Objective")

        ("algorithm,", po::value<Algorithm>(&optimize_parameters.algorithm), "Algorithm")

        ("output,o", po::value<std::string>(&output_path), "Output path")
        ("certificate,c", po::value<std::string>(&certificate_path), "Certificate path")
        ("log,l", po::value<std::string>(&log_path), "Log path")
        ("nl", po::value<std::string>(&optimize_parameters.output_nl_path), "Output .nl path")

        ("time-limit,t", po::value<double>(&time_limit), "Time limit in seconds")
        ("seed,s", po::value<Seed>(&seed), "Seed (not used)")

        ("only-write-at-the-end,e", "Only write output and certificate files at the end")
        ("verbosity-level,v", po::value<int>(&verbosity_level), "Verbosity level")
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
    instance_builder.read(instance_path);
    instance_builder.set_objective(objective);
    optimize_parameters.output_nl_path = instance_path;
    Instance instance = instance_builder.build();

    optimize_parameters.info = optimizationtools::Info()
        .set_verbosity_level(verbosity_level)
        .set_time_limit(time_limit)
        .set_certificate_path(certificate_path)
        .set_json_output_path(output_path)
        .set_only_write_at_the_end(vm.count("only-write-at-the-end"))
        .set_log_path(log_path)
        .set_log2stderr(vm.count("log2stderr"))
        .set_maximum_log_level(log_levelmax)
        .set_sigint_handler()
        ;

    optimize(instance, optimize_parameters);

    return 0;
}

