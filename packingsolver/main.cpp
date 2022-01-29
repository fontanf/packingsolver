#include "packingsolver/rectangleguillotine/branching_scheme.hpp"

#include "packingsolver/algorithms/depth_first_search.hpp"
#include "packingsolver/algorithms/a_star.hpp"
#include "packingsolver/algorithms/iterative_memory_bounded_a_star.hpp"
#include "packingsolver/algorithms/iterative_beam_search.hpp"
#include "packingsolver/algorithms/dynamic_programming_a_star.hpp"
#include "packingsolver/algorithms/column_generation.hpp"

#include <boost/program_options.hpp>

#include <iomanip>
#include <thread>

using namespace packingsolver;
namespace po = boost::program_options;

AStarOptionalParameters read_a_star_args(std::vector<char*> argv)
{
    AStarOptionalParameters parameters;
    po::options_description desc("Allowed options");
    desc.add_options()
        ;
    po::variables_map vm;
    po::store(po::parse_command_line((Counter)argv.size(), argv.data(), desc), vm);
    try {
        po::notify(vm);
    } catch (const po::required_option& e) {
        std::cout << desc << std::endl;;
        throw "";
    }
    return parameters;
}

DepthFirstSearchOptionalParameters read_depth_frist_search_args(std::vector<char*> argv)
{
    DepthFirstSearchOptionalParameters parameters;
    po::options_description desc("Allowed options");
    desc.add_options()
        ;
    po::variables_map vm;
    po::store(po::parse_command_line((Counter)argv.size(), argv.data(), desc), vm);
    try {
        po::notify(vm);
    } catch (const po::required_option& e) {
        std::cout << desc << std::endl;;
        throw "";
    }
    return parameters;
}

IterativeMemoryBoundedAStarOptionalParameters read_iterative_memory_bounded_a_star_args(std::vector<char*> argv)
{
    IterativeMemoryBoundedAStarOptionalParameters parameters;
    po::options_description desc("Allowed options");
    desc.add_options()
        ("growth-factor,f", po::value<double>(&parameters.growth_factor), "")
        ("queue-size-min,m", po::value<Counter>(&parameters.queue_size_min), "")
        ("queue-size-max,M", po::value<Counter>(&parameters.queue_size_max), "")
        ("maximum-number-of-nodes,n", po::value<Counter>(&parameters.maximum_number_of_nodes), "")
        ;
    po::variables_map vm;
    po::store(po::parse_command_line((Counter)argv.size(), argv.data(), desc), vm);
    try {
        po::notify(vm);
    } catch (const po::required_option& e) {
        std::cout << desc << std::endl;;
        throw "";
    }
    return parameters;
}

IterativeBeamSearchOptionalParameters read_iterative_beam_search_args(std::vector<char*> argv)
{
    IterativeBeamSearchOptionalParameters parameters;
    po::options_description desc("Allowed options");
    desc.add_options()
        ("growth-factor,f", po::value<double>(&parameters.growth_factor), "")
        ("queue-size-min,m", po::value<Counter>(&parameters.queue_size_min), "")
        ("queue-size-max,M", po::value<Counter>(&parameters.queue_size_max), "")
        ("maximum-number-of-nodes,n", po::value<Counter>(&parameters.maximum_number_of_nodes), "")
        ;
    po::variables_map vm;
    po::store(po::parse_command_line((Counter)argv.size(), argv.data(), desc), vm);
    try {
        po::notify(vm);
    } catch (const po::required_option& e) {
        std::cout << desc << std::endl;;
        throw "";
    }
    return parameters;
}

DynamicProgrammingAStarOptionalParameters read_dynamic_programming_a_star_args(std::vector<char*> argv)
{
    DynamicProgrammingAStarOptionalParameters parameters;
    po::options_description desc("Allowed options");
    desc.add_options()
        ;
    po::variables_map vm;
    po::store(po::parse_command_line((Counter)argv.size(), argv.data(), desc), vm);
    try {
        po::notify(vm);
    } catch (const po::required_option& e) {
        std::cout << desc << std::endl;;
        throw "";
    }
    return parameters;
}

rectangleguillotine::BranchingScheme::Parameters read_rg_branching_scheme_parameters(
        std::vector<std::string> args)
{
    std::vector<char*> argv;
    for(Counter i = 0; i < (Counter)args.size(); ++i)
        argv.push_back(const_cast<char*>(args[i].c_str()));

    rectangleguillotine::BranchingScheme::Parameters p0;
    std::string predefined = "";
    po::options_description desc("Allowed options");
    desc.add_options()
        ("predefined,p", po::value<std::string>(&predefined), "")
        ("cut-type-1,", po::value<rectangleguillotine::CutType1>(&p0.cut_type_1), "")
        ("cut-type-2,", po::value<rectangleguillotine::CutType2>(&p0.cut_type_2), "")
        ("first-stage-orientation,", po::value<rectangleguillotine::CutOrientation>(&p0.first_stage_orientation), "")
        ("min1cut,", po::value<Length>(&p0.min1cut), "")
        ("max1cut,", po::value<Length>(&p0.max1cut), "")
        ("min2cut,", po::value<Length>(&p0.min2cut), "")
        ("max2cut,", po::value<Length>(&p0.max2cut), "")
        ("min-waste,", po::value<Length>(&p0.min_waste), "")
        ("one2cut,", "")
        ("no-item-rotation,", "")
        ("cut-through-defects", "")
        ("guide,c", po::value<GuideId>(&p0.guide_id), "")
        ;
    po::variables_map vm;
    po::store(po::parse_command_line((Counter)argv.size(), argv.data(), desc), vm);
    try {
        po::notify(vm);
    } catch (const po::required_option& e) {
        std::cout << desc << std::endl;;
        throw "";
    }
    rectangleguillotine::BranchingScheme::Parameters p;
    if (vm.count("predefined")) {
        if (predefined == "roadef2018") {
            p.set_roadef2018();
        } else {
            p.set_predefined(predefined);
        }
    }
    if (vm.count("cut-type-1")) p.cut_type_1 = p0.cut_type_1;
    if (vm.count("cut-type-2")) p.cut_type_2 = p0.cut_type_2;
    if (vm.count("first-stage-orientation")) p.first_stage_orientation = p0.first_stage_orientation;
    if (vm.count("min1cut")) p.min1cut = p0.min1cut;
    if (vm.count("max1cut")) p.max1cut = p0.max1cut;
    if (vm.count("min2cut")) p.min2cut = p0.min2cut;
    if (vm.count("max2cut")) p.max2cut = p0.max2cut;
    if (vm.count("min-waste")) p.min_waste = p0.min_waste;
    if (vm.count("one2cut")) p.one2cut = true;
    if (vm.count("no-item-rotation")) p.no_item_rotation = true;
    if (vm.count("cut-through-defects")) p.cut_through_defects = true;
    if (vm.count("guide")) p.guide_id = p0.guide_id;
    return p;
}

template <typename Instance, typename Solution, typename BranchingScheme>
int run(
        std::string algorithm,
        BranchingScheme& branching_scheme,
        SolutionPool<Instance, Solution>& solution_pool,
        Counter thread_id,
        Info info)
{
    std::vector<std::string> algorithm_args = po::split_unix(algorithm);
    std::vector<char*> algorithm_argv;
    for(Counter i = 0; i < (Counter)algorithm_args.size(); ++i)
        algorithm_argv.push_back(const_cast<char*>(algorithm_args[i].c_str()));

    if (algorithm_args[0] == "A*") {
        AStarOptionalParameters parameters;
        parameters.thread_id = thread_id;
        parameters.info = info;
        a_star(branching_scheme, solution_pool, parameters);
    } else if (algorithm_args[0] == "DFS") {
        DepthFirstSearchOptionalParameters parameters;
        parameters.thread_id = thread_id;
        parameters.info = info;
        depth_first_search(branching_scheme, solution_pool, parameters);
    } else if (algorithm_args[0] == "IMBA*") {
        auto parameters = read_iterative_memory_bounded_a_star_args(algorithm_argv);
        parameters.thread_id = thread_id;
        parameters.info = info;
        iterative_memory_bounded_a_star(branching_scheme, solution_pool, parameters);
    } else if (algorithm_args[0] == "IBS") {
        auto parameters = read_iterative_beam_search_args(algorithm_argv);
        parameters.thread_id = thread_id;
        parameters.info = info;
        iterative_beam_search(branching_scheme, solution_pool, parameters);
    } else if (algorithm_args[0] == "DPA*") {
        auto parameters = read_dynamic_programming_a_star_args(algorithm_argv);
        parameters.thread_id = thread_id;
        parameters.info = info;
        dynamic_programming_a_star(branching_scheme, solution_pool, parameters);
    } else {
        VER(info, "WARNING: unknown algorithm \"" << algorithm_args[0] << "\"" << std::endl);
    }
    return 0;
}

int run_rectangleguillotine(
        Counter thread_id,
        SolutionPool<rectangleguillotine::Instance, rectangleguillotine::Solution>& solution_pool,
        const rectangleguillotine::Instance& instance,
        const std::vector<std::string> branching_scheme_args,
        std::string algorithm_str,
        Info info)
{
    if (branching_scheme_args[0] == "RG") {
        rectangleguillotine::BranchingScheme::Parameters parameters = read_rg_branching_scheme_parameters(branching_scheme_args);
        rectangleguillotine::BranchingScheme branching_scheme(instance, parameters);
        run(algorithm_str, branching_scheme, solution_pool, thread_id, info);
    } else {
        VER(info, "WARNING: unknown branching scheme \"" << branching_scheme_args[0] << "\"" << std::endl);
    }
    return 0;
}

int main(int argc, char *argv[])
{

    // Parse program options
    std::string items_path = "";
    std::string bins_path = "";
    std::string defects_path = "";
    std::string parameters_path = "";
    std::string certificate_path = "";
    std::string output_path = "";
    std::string log_path = "";
    int log_levelmax = 999;
    double time_limit = std::numeric_limits<double>::infinity();
    Seed seed = 0;

    ProblemType problem_type = ProblemType::RectangleGuillotine;
    Objective objective = Objective::Default;
    std::vector<std::string> branching_schemes = {
        "RG -p roadef2018 -c 0",
        "RG -p roadef2018 -c 1",
        "RG -p roadef2018 -c 0",
        "RG -p roadef2018 -c 1",
    };
    std::vector<std::string> algorithms = {
        "IMBA* -f 1.33",
        "IMBA* -f 1.33",
        "IMBA* -f 1.5",
        "IMBA* -f 1.5",
    };

    po::options_description desc("Allowed options");
    desc.add_options()
        (",h", "Produce help message")

        ("items,i",       po::value<std::string>(&items_path)->required(), "Items path")
        ("bins,b",        po::value<std::string>(&bins_path),              "Bins path")
        ("defects,d",     po::value<std::string>(&defects_path),           "Defects path")
        ("parameters",    po::value<std::string>(&parameters_path),        "Parameters path")
        ("bin-infinite-width", "")
        ("bin-infinite-height", "")
        ("bin-infinite-copies", "")
        ("bin-unweighted", "")
        ("item-infinite-copies", "")
        ("unweighted", "")

        ("output,o",      po::value<std::string>(&output_path),      "Output path")
        ("certificate,c", po::value<std::string>(&certificate_path), "Certificate path")
        ("log,l",         po::value<std::string>(&log_path),         "Log path")

        ("time-limit,t", po::value<double>(&time_limit), "Time limit in seconds")
        ("seed,s", po::value<Seed>(&seed), "Seed (not used)")

        ("problem-type,p", po::value<ProblemType>(&problem_type), "Problem type")
        ("objective,f", po::value<Objective>(&objective), "Objective")
        ("branching-scheme,q", po::value<std::vector<std::string>>(&branching_schemes)->multitoken(), "Branching schemes")
        ("algorithm,a", po::value<std::vector<std::string>>(&algorithms)->multitoken(), "Algorithms")

        ("only-write-at-the-end,e", "Only write output and certificate files at the end")
        ("verbose,v",               "Verbose")
        ("log2stderr,w",            "Write log in stderr")
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

    // Read parameters
    std::vector<std::string> parameters;
    if (parameters_path != "") {
        std::ifstream parameters_file(parameters_path);
        if (!parameters_file.good())
            std::cerr << "\033[31m" << "ERROR, unable to open file \"" << parameters_path << "\"" << "\033[0m" << std::endl;
        std::string parameters_line;
        std::getline(parameters_file, parameters_line);
        parameters = po::split_unix(parameters_line);
    }

    Info info = optimizationtools::Info()
        .set_verbose(vm.count("verbose"))
        .set_time_limit(time_limit)
        .set_certificate_path(certificate_path)
        .set_json_output_path(output_path)
        .set_only_write_at_the_end(vm.count("only-write-at-the-end"))
        .set_log_path(log_path)
        .set_log2stderr(vm.count("log2stderr"))
        .set_maximum_log_level(log_levelmax)
        .set_sigint_handler()
        ;

    switch (problem_type) {
    case ProblemType::RectangleGuillotine: {
        // Build instance.
        rectangleguillotine::Instance instance(objective, items_path, bins_path, defects_path);
        if (vm.count("bin-infinite-width"))
            instance.set_bin_infinite_width();
        if (vm.count("bin-infinite-height"))
            instance.set_bin_infinite_height();
        if (vm.count("bin-infinite-copies"))
            instance.set_bin_infinite_copies();
        if (vm.count("item-infinite-copies"))
            instance.set_item_infinite_copies();
        if (vm.count("unweighted"))
            instance.set_unweighted();
        if (vm.count("bin-unweighted"))
            instance.set_bin_unweighted();

        if (objective == Objective::VariableSizedBinPacking) {
            SolutionPool<rectangleguillotine::Instance, rectangleguillotine::Solution> solution_pool(instance, 1);
            solution_pool.best().algorithm_start(info);
            VariableSizeBinPackingPricingFunction<rectangleguillotine::Instance, rectangleguillotine::Solution> pricing_function = [&algorithms, &branching_schemes, &parameters](const rectangleguillotine::Instance& instance_kp)
            {
                Info info_tmp = Info()
                    //.set_verbose(true)
                    ;
                SolutionPool<rectangleguillotine::Instance, rectangleguillotine::Solution> solution_pool_kp(instance_kp, 10);
                solution_pool_kp.best().algorithm_start(info_tmp);
                std::vector<std::thread> threads;
                for (Counter i = 0; i < (Counter)algorithms.size(); ++i) {
                    // Parse branching scheme
                    std::vector<std::string> branching_scheme_args = po::split_unix(branching_schemes[i]);
                    // Insert parameters at the beginning of branching_scheme_args
                    branching_scheme_args.insert(branching_scheme_args.begin() + 1,
                            parameters.begin(), parameters.end());
                    threads.push_back(std::thread(run_rectangleguillotine,
                                i + 1,
                                std::ref(solution_pool_kp),
                                std::ref(instance_kp),
                                branching_scheme_args,
                                algorithms[i],
                                Info(info_tmp, true, "thread" + std::to_string(i))));
                }
                for (Counter i = 0; i < (Counter)algorithms.size(); ++i)
                    threads[i].join();
                solution_pool_kp.best().algorithm_end(info_tmp);
                return solution_pool_kp;
            };
            column_generation_heuristic_variable_sized_bin_packing<rectangleguillotine::Instance, rectangleguillotine::Solution>(
                    instance, solution_pool, pricing_function, info);
            solution_pool.best().algorithm_end(info);
        } else {
            std::vector<std::thread> threads;
            SolutionPool<rectangleguillotine::Instance, rectangleguillotine::Solution> solution_pool(instance, 1);
            solution_pool.best().algorithm_start(info);
            for (Counter i = 0; i < (Counter)algorithms.size(); ++i) {
                // Parse branching scheme
                std::vector<std::string> branching_scheme_args = po::split_unix(branching_schemes[i]);
                // Insert parameters at the beginning of branching_scheme_args
                branching_scheme_args.insert(branching_scheme_args.begin() + 1,
                        parameters.begin(), parameters.end());
                threads.push_back(std::thread(run_rectangleguillotine,
                            i + 1,
                            std::ref(solution_pool),
                            std::ref(instance),
                            branching_scheme_args,
                            algorithms[i],
                            Info(info, true, "thread" + std::to_string(i))));
            }
            for (Counter i = 0; i < (Counter)algorithms.size(); ++i)
                threads[i].join();
            solution_pool.best().algorithm_end(info);
        }
        break;
    } case ProblemType::Rectangle: {
        break;
    }
    }

    return 0;
}

