#include "packingsolver/rectangleguillotine/optimize.hpp"

#include <boost/program_options.hpp>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;
namespace po = boost::program_options;

int main(int argc, char *argv[])
{

    // Parse program options
    std::string items_path = "";
    std::string bins_path = "";
    std::string defects_path = "";
    std::string parameters_path = "";
    Parameters parameters;
    Objective objective = Objective::Default;
    std::string predefined = "";
    std::string certificate_path = "";
    std::string output_path = "";
    std::string log_path = "";
    int verbosity_level = 0;
    int log_levelmax = 999;
    double time_limit = std::numeric_limits<double>::infinity();
    Seed seed = 0;

    po::options_description desc("Allowed options");
    desc.add_options()
        (",h", "Produce help message")

        ("items,i", po::value<std::string>(&items_path)->required(), "Items path")
        ("bins,b", po::value<std::string>(&bins_path), "Bins path")
        ("defects,d", po::value<std::string>(&defects_path), "Defects path")
        ("parameters", po::value<std::string>(&parameters_path), "Parameters path")

        ("bin-infinite-x", "")
        ("bin-infinite-y", "")
        ("bin-infinite-copies", "")
        ("bin-unweighted", "")
        ("item-infinite-copies", "")
        ("unweighted", "")
        ("no-item-rotation", "")

        ("objective,f", po::value<Objective>(&objective), "Objective")

        ("predefined,p", po::value<std::string>(&predefined), "")

        ("cut-type-1,", po::value<rectangleguillotine::CutType1>(&parameters.cut_type_1), "")
        ("cut-type-2,", po::value<rectangleguillotine::CutType2>(&parameters.cut_type_2), "")
        ("first-stage-orientation,", po::value<rectangleguillotine::CutOrientation>(&parameters.first_stage_orientation), "")
        ("min1cut,", po::value<Length>(&parameters.min1cut), "")
        ("max1cut,", po::value<Length>(&parameters.max1cut), "")
        ("min2cut,", po::value<Length>(&parameters.min2cut), "")
        ("max2cut,", po::value<Length>(&parameters.max2cut), "")
        ("min-waste,", po::value<Length>(&parameters.min_waste), "")
        ("one2cut,", po::value<bool>(&parameters.one2cut), "")
        ("cut-through-defects", po::value<bool>(&parameters.cut_through_defects), "")

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

    Instance instance;

    if (!vm.count("-d"))
        if (std::ifstream(items_path + "_defects.csv").good())
            defects_path = items_path + "_defects.csv";
    if (!vm.count("-b"))
        if (std::ifstream(items_path + "_bins.csv").good())
            bins_path = items_path + "_bins.csv";
    if (!vm.count("parameters"))
        if (std::ifstream(items_path + "_parameters.txt").good())
            parameters_path = items_path + "_parameters.txt";
    if (!std::ifstream(items_path).good())
        if (std::ifstream(items_path + "_items.csv").good())
            items_path = items_path + "_items.csv";

    instance.set_objective(objective);
    instance.read_item_types(items_path);
    instance.read_bin_types(bins_path);
    instance.read_defects(defects_path);

    if (vm.count("bin-infinite-x"))
        instance.set_bin_infinite_x();
    if (vm.count("bin-infinite-y"))
        instance.set_bin_infinite_y();
    if (vm.count("bin-infinite-copies"))
        instance.set_bin_infinite_copies();
    if (vm.count("no-item-rotation"))
        instance.set_no_item_rotation();
    if (vm.count("item-infinite-copies"))
        instance.set_item_infinite_copies();
    if (vm.count("unweighted"))
        instance.set_unweighted();
    if (vm.count("bin-unweighted"))
        instance.set_bin_unweighted();

    instance.read_parameters(parameters_path);
    instance.set_predefined(predefined);

    if (vm.count("cut-type-1"))
        instance.set_cut_type_1(parameters.cut_type_1);
    if (vm.count("cut-type-2"))
        instance.set_cut_type_2(parameters.cut_type_2);
    if (vm.count("first-stage-orientation"))
        instance.set_first_stage_orientation(parameters.first_stage_orientation);
    if (vm.count("min1cut"))
        instance.set_min1cut(parameters.min1cut);
    if (vm.count("max1cut"))
        instance.set_max1cut(parameters.max1cut);
    if (vm.count("min2cut"))
        instance.set_min2cut(parameters.min2cut);
    if (vm.count("max2cut"))
        instance.set_max2cut(parameters.max2cut);
    if (vm.count("min-waste"))
        instance.set_min_waste(parameters.min_waste);
    if (vm.count("one2cut"))
        instance.set_one2cut(parameters.one2cut);
    if (vm.count("cut-through-defects"))
        instance.set_cut_through_defects(parameters.cut_through_defects);

    Info info = optimizationtools::Info()
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

    OptimizeOptionalParameters parameters_opt;
    parameters_opt.info = info;
    optimize(instance, parameters_opt);

    return 0;
}

