#include "packingsolver/rectangleguillotine/optimize.hpp"
#include "packingsolver/rectangleguillotine/instance_builder.hpp"

#include <boost/program_options.hpp>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;
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
        ("item-multiply-copies", po::value<ItemPos>(), "")
        ("unweighted", "")
        ("no-item-rotation", "")

        ("objective,f", po::value<Objective>(), "Objective")

        ("predefined,p", po::value<std::string>(), "")

        ("number-of-stages,", po::value<Counter>(), "")
        ("cut-type,", po::value<rectangleguillotine::CutType>(), "")
        ("first-stage-orientation,", po::value<rectangleguillotine::CutOrientation>(), "")
        ("min1cut,", po::value<Length>(), "")
        ("minimum-distance-1-cuts,", po::value<Length>(), "")
        ("max1cut,", po::value<Length>(), "")
        ("maximum-distance-1-cuts,", po::value<Length>(), "")
        ("min2cut,", po::value<Length>(), "")
        ("minimum-distance-2-cuts,", po::value<Length>(), "")
        ("min-waste,", po::value<Length>(), "")
        ("minimum-waste-length,", po::value<Length>(), "")
        ("maximum-number-2-cuts,", po::value<bool>(), "")
        ("cut-through-defects", po::value<bool>(), "")
        ("cut-thickness", po::value<Length>(), "")

        ("output,o", po::value<std::string>(), "Output path")
        ("certificate,c", po::value<std::string>(), "Certificate path")
        ("log,l", po::value<std::string>(), "Log path")
        ("time-limit,t", po::value<double>(), "Time limit in seconds")
        ("seed,s", po::value<Seed>(), "Seed (not used)")
        ("only-write-at-the-end,e", "Only write output and certificate files at the end")
        ("verbosity-level,v", po::value<int>(), "Verbosity level")
        ("log2stderr,w", "Write log in stderr")

        ("optimization-mode,", po::value<OptimizationMode>(), "set optimization mode")
        ("use-tree-search,", po::value<bool>(), "enable tree search algorithm")
        ("use-sequential-single-knapsack,", po::value<bool>(), "enable sequential-single-knapsack")
        ("use-sequential-value-correction,", po::value<bool>(), "enable sequential-value-correction")
        ("use-column-generation,", po::value<bool>(), "enable column-generation")
        ("use-dichotomic-search,", po::value<bool>(), "enable dichotomic search")
        ("sequential-value-correction-subproblem-queue-size,", po::value<NodeId>(), "set sequential value correction subproblem queue size")
        ("column-generation-subproblem-queue-size,", po::value<NodeId>(), "set column generation subproblem queue size")
        ("not-anytime-tree-search-queue-size,", po::value<Counter>(), "")
        ("not-anytime-sequential-single-knapsack-subproblem-queue-size,", po::value<Counter>(), "")
        ("not-anytime-sequential-value-correction-number-of-iterations,", po::value<Counter>(), "")
        ("not-anytime-dichotomic-search-subproblem-queue-size,", po::value<Counter>(), "")
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

    if (vm.count("item-multiply-copies"))
        instance_builder.multiply_item_types_copies(vm["item-multiply-copies"].as<ItemPos>());
    if (vm.count("bin-infinite-x"))
        instance_builder.set_bin_types_infinite_x();
    if (vm.count("bin-infinite-y"))
        instance_builder.set_bin_types_infinite_y();
    if (vm.count("bin-infinite-copies"))
        instance_builder.set_bin_types_infinite_copies();
    if (vm.count("no-item-rotation"))
        instance_builder.set_item_types_oriented();
    if (vm.count("item-infinite-copies"))
        instance_builder.set_item_types_infinite_copies();
    if (vm.count("unweighted"))
        instance_builder.set_item_types_unweighted();
    if (vm.count("bin-unweighted"))
        instance_builder.set_bin_types_unweighted();

    std::string parameters_path = (vm.count("parameters"))?
        vm["parameters"].as<std::string>():
        (std::ifstream(vm["items"].as<std::string>() + "_parameters.csv").good())?
        vm["items"].as<std::string>() + "_parameters.csv":
        "";
    if (!parameters_path.empty())
        instance_builder.read_parameters(parameters_path);

    if (vm.count("objective"))
        instance_builder.set_objective(vm["objective"].as<Objective>());
    if (vm.count("predefined"))
        instance_builder.set_predefined(vm["predefined"].as<std::string>());

    if (vm.count("number-of-stages"))
        instance_builder.set_number_of_stages(vm["number-of-stages"].as<Counter>());
    if (vm.count("cut-type"))
        instance_builder.set_cut_type(vm["cut-type"].as<CutType>());
    if (vm.count("first-stage-orientation"))
        instance_builder.set_first_stage_orientation(vm["first-stage-orientation"].as<CutOrientation>());
    if (vm.count("min1cut"))
        instance_builder.set_minimum_distance_1_cuts(vm["min1cut"].as<Length>());
    if (vm.count("minimum-distance-1-cuts"))
        instance_builder.set_minimum_distance_1_cuts(vm["minimum-distance-1-cuts"].as<Length>());
    if (vm.count("max1cut"))
        instance_builder.set_maximum_distance_1_cuts(vm["max1cut"].as<Length>());
    if (vm.count("maximum-distance-1-cuts"))
        instance_builder.set_maximum_distance_1_cuts(vm["maximum-distance-1-cuts"].as<Length>());
    if (vm.count("min2cut"))
        instance_builder.set_minimum_distance_2_cuts(vm["min2cut"].as<Length>());
    if (vm.count("minimum-distance-2-cuts"))
        instance_builder.set_minimum_distance_2_cuts(vm["minimum-distance-2-cuts"].as<Length>());
    if (vm.count("min-waste"))
        instance_builder.set_minimum_waste_length(vm["min-waste"].as<Length>());
    if (vm.count("minimum-waste-length"))
        instance_builder.set_minimum_waste_length(vm["minimum-waste-length"].as<Length>());
    if (vm.count("maximum-number-2-cuts"))
        instance_builder.set_maximum_number_2_cuts(vm["maximum-number-2-cuts"].as<bool>());
    if (vm.count("cut-through-defects"))
        instance_builder.set_cut_through_defects(vm["cut-through-defects"].as<bool>());
    if (vm.count("cut-thickness"))
        instance_builder.set_cut_thickness(vm["cut-thickness"].as<Length>());

    Instance instance = instance_builder.build();

    OptimizeParameters parameters;
    read_args(parameters, vm);
    if (vm.count("optimization-mode"))
        parameters.optimization_mode = vm["optimization-mode"].as<OptimizationMode>();

    if (vm.count("use-tree-search"))
        parameters.use_tree_search = vm["use-tree-search"].as<bool>();
    if (vm.count("use-sequential-single-knapsack"))
        parameters.use_sequential_single_knapsack = vm["use-sequential-single-knapsack"].as<bool>();
    if (vm.count("use-sequential-value-correction"))
        parameters.use_sequential_value_correction = vm["use-sequential-value-correction"].as<bool>();
    if (vm.count("use-column-generation"))
        parameters.use_column_generation = vm["use-column-generation"].as<bool>();
    if (vm.count("use-dichotomic-search"))
        parameters.use_dichotomic_search = vm["use-dichotomic-search"].as<bool>();

    if (vm.count("sequential-value-correction-subproblem-queue-size"))
        parameters.sequential_value_correction_subproblem_queue_size = vm["sequential-value-correction-subproblem-queue-size"].as<NodeId>();
    if (vm.count("column-generation-subproblem-queue-size"))
        parameters.column_generation_subproblem_queue_size = vm["column-generation-subproblem-queue-size"].as<NodeId>();
    if (vm.count("not-anytime-tree-search-queue-size"))
        parameters.not_anytime_tree_search_queue_size = vm["not-anytime-tree-search-queue-size"].as<Counter>();
    if (vm.count("not-anytime-sequential-single-knapsack-subproblem-queue-size"))
        parameters.not_anytime_sequential_single_knapsack_subproblem_queue_size = vm["not-anytime-sequential-single-knapsack-subproblem-queue-size"].as<Counter>();
    if (vm.count("not-anytime-sequential-value-correction-number-of-iterations"))
        parameters.not_anytime_sequential_value_correction_number_of_iterations = vm["not-anytime-sequential-value-correction-number-of-iterations"].as<Counter>();
    if (vm.count("not-anytime-dichotomic-search-subproblem-queue-size"))
        parameters.not_anytime_dichotomic_search_subproblem_queue_size = vm["not-anytime-dichotomic-search-subproblem-queue-size"].as<Counter>();
    const rectangleguillotine::Output output = optimize(instance, parameters);

    if (vm.count("certificate"))
        output.solution_pool.best().write(vm["certificate"].as<std::string>());
    if (vm.count("output"))
        output.write_json_output(vm["output"].as<std::string>());

    return 0;
}
