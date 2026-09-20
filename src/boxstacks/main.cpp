#include "packingsolver/boxstacks/optimize.hpp"
#include "packingsolver/boxstacks/post_process.hpp"
#include "packingsolver/boxstacks/instance_builder.hpp"

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

using namespace packingsolver;
using namespace packingsolver::boxstacks;
namespace po = boost::program_options;
namespace fs = boost::filesystem;

void read_args(
        packingsolver::Parameters<Instance, Solution, boxstacks::Output>& parameters,
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
    if (vm.count("output"))
        parameters.write_json_output = true;
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
    try {
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
            ("log-to-stderr,w", "Write log in stderr")

            ("memory-limit,", po::value<Megabytes>(), "Memory limit in mebibytes (default: unlimited)")

            ("linear-programming-solver,", po::value<columngenerationsolver::SolverName>(), "set linear programming solver")
            ("optimization-mode,", po::value<OptimizationMode>(), "set optimization mode")
            ("reduce,", po::value<bool>(), "enable/disable instance reduction (preprocessing)")
            ("use-box-bounds,", po::value<bool>(), "enable/disable the box relaxation bound")
            ("use-sequential-single-knapsack,", po::value<bool>(), "enable sequential-single-knapsack")
            ("use-sequential-value-correction,", po::value<bool>(), "enable sequential-value-correction")
            ("many-items-in-bins-threshold,", po::value<Counter>(), "")
            ("many-items-in-bins-threshold-2,", po::value<Counter>(), "")
            ("many-item-type-copies-factor,", po::value<Counter>(), "")
            ("sequential-value-correction-subproblem-tree-search-queue-size,", po::value<NodeId>(), "set sequential value correction subproblem queue size")
            ("column-generation-subproblem-tree-search-queue-size,", po::value<NodeId>(), "set column generation subproblem queue size")
            ("anytime-tree-search-initial-queue-size,", po::value<NodeId>(), "")
            ("anytime-sequential-onedimensional-rectangle-rectangle-initial-queue-size,", po::value<NodeId>(), "")
            ("not-anytime-tree-search-queue-size,", po::value<NodeId>(), "")
            ("not-anytime-sequential-single-knapsack-subproblem-tree-search-queue-size,", po::value<NodeId>(), "")
            ("not-anytime-sequential-single-knapsack-subproblem-rectangle-tree-search-queue-size,", po::value<NodeId>(), "")
            ("not-anytime-sequential-onedimensional-rectangle-rectangle-tree-search-queue-size,", po::value<NodeId>(), "")
            ("not-anytime-sequential-value-correction-number-of-iterations,", po::value<Counter>(), "")

            ("group-identical-bins,", po::value<bool>(), "")
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
            throw std::invalid_argument(FUNC_SIGNATURE);
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

        if (vm.count("defects")) {
            instance_builder.read_defects(vm["defects"].as<std::string>());
        } else if (fs::is_regular_file(instance_path + "defects.csv")) {
            instance_builder.read_defects(instance_path + "defects.csv");
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
        if (vm.count("unloading-constraint"))
            instance_builder.set_unloading_constraint(vm["unloading-constraint"].as<rectangle::UnloadingConstraint>());

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
        if (vm.count("memory-limit"))
            parameters.memory_limit_megabytes = vm["memory-limit"].as<Megabytes>();
        if (vm.count("reduce"))
            parameters.reduction_parameters.reduce = vm["reduce"].as<bool>();
        if (vm.count("use-box-bounds"))
            parameters.use_box_bounds = vm["use-box-bounds"].as<bool>();
        if (vm.count("use-sequential-single-knapsack"))
            parameters.use_sequential_single_knapsack = vm["use-sequential-single-knapsack"].as<bool>();
        if (vm.count("use-sequential-value-correction"))
            parameters.use_sequential_value_correction = vm["use-sequential-value-correction"].as<bool>();
        if (vm.count("many-items-in-bins-threshold"))
            parameters.many_items_in_bins_threshold = vm["many-items-in-bins-threshold"].as<Counter>();
        if (vm.count("many-items-in-bins-threshold-2"))
            parameters.many_items_in_bins_threshold_2 = vm["many-items-in-bins-threshold-2"].as<Counter>();
        if (vm.count("many-item-type-copies-factor"))
            parameters.many_item_type_copies_factor = vm["many-item-type-copies-factor"].as<Counter>();
        if (vm.count("sequential-value-correction-subproblem-tree-search-queue-size"))
            parameters.sequential_value_correction_subproblem_tree_search_queue_size = vm["sequential-value-correction-subproblem-tree-search-queue-size"].as<NodeId>();
        if (vm.count("column-generation-subproblem-tree-search-queue-size"))
            parameters.column_generation_subproblem_tree_search_queue_size = vm["column-generation-subproblem-tree-search-queue-size"].as<NodeId>();
        if (vm.count("anytime-tree-search-initial-queue-size"))
            parameters.anytime_tree_search_initial_queue_size = vm["anytime-tree-search-initial-queue-size"].as<NodeId>();
        if (vm.count("anytime-sequential-onedimensional-rectangle-rectangle-initial-queue-size"))
            parameters.anytime_sequential_onedimensional_rectangle_rectangle_initial_queue_size = vm["anytime-sequential-onedimensional-rectangle-rectangle-initial-queue-size"].as<NodeId>();
        if (vm.count("not-anytime-tree-search-queue-size"))
            parameters.not_anytime_tree_search_queue_size = vm["not-anytime-tree-search-queue-size"].as<NodeId>();
        if (vm.count("not-anytime-sequential-single-knapsack-subproblem-tree-search-queue-size"))
            parameters.not_anytime_sequential_single_knapsack_subproblem_tree_search_queue_size = vm["not-anytime-sequential-single-knapsack-subproblem-tree-search-queue-size"].as<NodeId>();
        if (vm.count("not-anytime-sequential-single-knapsack-subproblem-rectangle-tree-search-queue-size"))
            parameters.not_anytime_sequential_single_knapsack_subproblem_rectangle_tree_search_queue_size = vm["not-anytime-sequential-single-knapsack-subproblem-rectangle-tree-search-queue-size"].as<NodeId>();
        if (vm.count("not-anytime-sequential-onedimensional-rectangle-rectangle-tree-search-queue-size"))
            parameters.not_anytime_sequential_onedimensional_rectangle_rectangle_tree_search_queue_size = vm["not-anytime-sequential-onedimensional-rectangle-rectangle-tree-search-queue-size"].as<NodeId>();
        if (vm.count("not-anytime-sequential-value-correction-number-of-iterations"))
            parameters.not_anytime_sequential_value_correction_number_of_iterations = vm["not-anytime-sequential-value-correction-number-of-iterations"].as<Counter>();

        const boxstacks::Output output = optimize(instance, parameters);

        if (vm.count("output"))
            output.write_json_output(vm["output"].as<std::string>());

        Solution solution = output.solution_pool.best();
        if (vm.count("group-identical-bins")) {
            GroupIdenticalBinsOutput gib_output = group_identical_bins(solution);
            solution = gib_output.solution_pool.best();
        }

        if (vm.count("certificate"))
            solution.write(vm["certificate"].as<std::string>());

#if XPRESS_FOUND
        if (optimize_parameters.solver_name
                == columngenerationsolver::SolverName::Xpress)
            XPRSfree();
#endif

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
