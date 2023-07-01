#include "packingsolver/onedimensional/optimize.hpp"

#include <boost/program_options.hpp>

using namespace packingsolver;
using namespace packingsolver::onedimensional;
namespace po = boost::program_options;

int main(int argc, char *argv[])
{

    // Parse program options
    std::string items_path = "";
    std::string bins_path = "";
    std::string parameters_path = "";
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

        ("items,i", po::value<std::string>(&items_path)->required(), "Items path")
        ("bins,b", po::value<std::string>(&bins_path), "Bins path")
        ("parameters", po::value<std::string>(&parameters_path), "Parameters path")

        ("bin-infinite-x", "")
        ("bin-infinite-y", "")
        ("bin-infinite-copies", "")
        ("bin-unweighted", "")
        ("item-infinite-copies", "")
        ("unweighted", "")
        ("no-item-rotation", "")

        ("objective,f", po::value<Objective>(&objective), "Objective")

        ("bpp-algorithm,", po::value<Algorithm>(&optimize_parameters.bpp_algorithm), "Algorithm for Bin Packing problems")
        ("vbpp-algorithm,", po::value<Algorithm>(&optimize_parameters.vbpp_algorithm), "Algorithm for Variable-sized Bin Packing problems")

        ("tree-search-queue-size,", po::value<NodeId>(&optimize_parameters.tree_search_queue_size), "")
        ("tree-search-guides,", po::value<std::vector<GuideId>>(&optimize_parameters.tree_search_guides)->multitoken(), "")
        ("column-generation-vbpp2bpp-time-limit,", po::value<double>(&optimize_parameters.column_generation_vbpp2bpp_time_limit), "")
        ("column-generation-vbpp2bpp-queue-size,", po::value<NodeId>(&optimize_parameters.column_generation_vbpp2bpp_queue_size), "")
        ("column-generation-pricing-queue-size,", po::value<NodeId>(&optimize_parameters.column_generation_pricing_queue_size), "")
        ("linear-programming-solver,", po::value<columngenerationsolver::LinearProgrammingSolver>(&optimize_parameters.linear_programming_solver), "")
        ("dichotomic-search-queue-size,", po::value<NodeId>(&optimize_parameters.dichotomic_search_queue_size), "")

        ("output,o", po::value<std::string>(&output_path), "Output path")
        ("certificate,c", po::value<std::string>(&certificate_path), "Certificate path")
        ("log,l", po::value<std::string>(&log_path), "Log path")

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

    if (!vm.count("-b"))
        if (std::ifstream(items_path + "_bins.csv").good())
            bins_path = items_path + "_bins.csv";
    if (!vm.count("parameters"))
        if (std::ifstream(items_path + "_parameters.txt").good())
            parameters_path = items_path + "_parameters.txt";
    if (!std::ifstream(items_path).good())
        if (std::ifstream(items_path + "_items.csv").good())
            items_path = items_path + "_items.csv";

    instance_builder.set_objective(objective);
    instance_builder.read_item_types(items_path);
    instance_builder.read_bin_types(bins_path);

    if (vm.count("bin-infinite-copies"))
        instance_builder.set_bin_types_infinite_copies();
    if (vm.count("item-infinite-copies"))
        instance_builder.set_item_types_infinite_copies();
    if (vm.count("unweighted"))
        instance_builder.set_item_types_unweighted();
    if (vm.count("bin-unweighted"))
        instance_builder.set_bin_types_unweighted();

    instance_builder.read_parameters(parameters_path);

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

