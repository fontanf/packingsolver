#include "packingsolver/rectangleguillotine/branching_scheme.hpp"

#include "packingsolver/algorithms/dfs.hpp"
#include "packingsolver/algorithms/astar.hpp"
#include "packingsolver/algorithms/mbastar.hpp"
#include "packingsolver/algorithms/dpastar.hpp"

#include <boost/program_options.hpp>

#include <iomanip>
#include <thread>

using namespace packingsolver;
namespace po = boost::program_options;

GuideId read_astar_args(std::vector<char*> argv)
{
    GuideId guide_id = 0;

    po::options_description desc("Allowed options");
    desc.add_options()
        ("guide,c", po::value<GuideId>(&guide_id), "")
        ;
    po::variables_map vm;
    po::store(po::parse_command_line((Counter)argv.size(), argv.data(), desc), vm);
    try {
        po::notify(vm);
    } catch (po::required_option e) {
        std::cout << desc << std::endl;;
        throw "";
    }

    return guide_id;
}

GuideId read_dfs_args(std::vector<char*> argv)
{
    GuideId guide_id = 0;

    po::options_description desc("Allowed options");
    desc.add_options()
        ("guide,c", po::value<GuideId>(&guide_id), "")
        ;
    po::variables_map vm;
    po::store(po::parse_command_line((Counter)argv.size(), argv.data(), desc), vm);
    try {
        po::notify(vm);
    } catch (po::required_option e) {
        std::cout << desc << std::endl;;
        throw "";
    }

    return guide_id;
}

std::pair<double, GuideId> read_mbastar_args(std::vector<char*> argv)
{
    double growth_factor = 1.5;
    GuideId guide_id = 0;

    po::options_description desc("Allowed options");
    desc.add_options()
        ("growth-factor,f", po::value<double>(&growth_factor), "")
        ("guide,c",         po::value<GuideId>(&guide_id),     "")
        ;
    po::variables_map vm;
    po::store(po::parse_command_line((Counter)argv.size(), argv.data(), desc), vm);
    try {
        po::notify(vm);
    } catch (po::required_option e) {
        std::cout << desc << std::endl;;
        throw "";
    }

    return {growth_factor, guide_id};
}

std::pair<Counter, GuideId> read_dpastar_args(std::vector<char*> argv)
{
    Counter s = -1;
    GuideId guide_id = 0;

    po::options_description desc("Allowed options");
    desc.add_options()
        (",s", po::value<Counter>(&s), "")
        ("guide,c", po::value<GuideId>(&guide_id), "")
        ;
    po::variables_map vm;
    po::store(po::parse_command_line((Counter)argv.size(), argv.data(), desc), vm);
    try {
        po::notify(vm);
    } catch (po::required_option e) {
        std::cout << desc << std::endl;;
        throw "";
    }

    return {s, guide_id};
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
        ("symmetry,s", po::value<Depth>(&p0.symmetry_depth), "")
        ("no-symmetry-2", "")
        ;
    po::variables_map vm;
    po::store(po::parse_command_line((Counter)argv.size(), argv.data(), desc), vm);
    try {
        po::notify(vm);
    } catch (po::required_option e) {
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
    if (vm.count("symmetry")) p.symmetry_depth = p0.symmetry_depth;
    if (vm.count("no-symmetry-2")) p.symmetry_2 = false;
    return p;
}

template <typename BranchingScheme>
int run_rectangleguillotine_2(
        Counter thread_id,
        rectangleguillotine::Solution& solution,
        BranchingScheme branching_scheme,
        std::string algorithm,
        Info info)
{
    std::vector<std::string> algorithm_args = po::split_unix(algorithm);
    std::vector<char*> algorithm_argv;
    for(Counter i = 0; i < (Counter)algorithm_args.size(); ++i)
        algorithm_argv.push_back(const_cast<char*>(algorithm_args[i].c_str()));

    if (algorithm_args[0] == "A*") {
        GuideId guide_id = read_astar_args(algorithm_argv);
        AStar<rectangleguillotine::Solution, BranchingScheme> solver(
                solution, branching_scheme, thread_id, guide_id, info);
        solver.run();
    } else if (algorithm_args[0] == "DFS") {
        GuideId guide_id = read_dfs_args(algorithm_argv);
        Dfs<rectangleguillotine::Solution, BranchingScheme> solver(
                solution, branching_scheme, thread_id, guide_id, info);
        solver.run();
    } else if (algorithm_args[0] == "MBA*") {
        auto p = read_mbastar_args(algorithm_argv);
        MbaStar<rectangleguillotine::Solution, BranchingScheme> solver(
                solution, branching_scheme, thread_id, p.first, p.second, info);
        solver.run();
    } else if (algorithm_args[0] == "DPA*") {
        auto p = read_dpastar_args(algorithm_argv);
        DpaStar<rectangleguillotine::Solution, BranchingScheme> solver(
                solution, branching_scheme, thread_id, p.first, p.second, info);
        solver.run();
    } else {
        VER(info, "WARNING: unknown algorithm \"" << algorithm_args[0] << "\"" << std::endl);
    }
    return 0;
}

int run_rectangleguillotine(
        Counter thread_id,
        rectangleguillotine::Solution& solution,
        const rectangleguillotine::Instance& instance,
        const std::vector<std::string> branching_scheme_args,
        std::string algorithm_str,
        Info info)
{
    if (branching_scheme_args[0] == "RG") {
        rectangleguillotine::BranchingScheme::Parameters parameters = read_rg_branching_scheme_parameters(branching_scheme_args);
        rectangleguillotine::BranchingScheme branching_scheme(instance, parameters);
        run_rectangleguillotine_2(thread_id, solution, branching_scheme, algorithm_str, info);
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
        "RG -p roadef2018 --symetry 2",
        "RG -p roadef2018 --symetry 2",
        "RG -p roadef2018 --symetry 2",
        "RG -p roadef2018 --symetry 2",
        "RG -p roadef2018 --symetry 4 --no-symmetry-2",
    };
    std::vector<std::string> algorithms = {
        "MBA* -f 1.33 -c 0",
        "MBA* -f 1.33 -c 1",
        "MBA* -f 1.5 -c 0",
        "MBA* -f 1.5 -c 1",
        "DPA* -s -1",
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
    } catch (po::required_option e) {
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

    Info info = Info()
        .set_verbose(vm.count("verbose"))
        .set_timelimit(time_limit)
        .set_certfile(certificate_path)
        .set_outputfile(output_path)
        .set_onlywriteattheend(vm.count("only-write-at-the-end"))
        .set_logfile(log_path)
        .set_log2stderr(vm.count("log2stderr"))
        .set_loglevelmax(log_levelmax)
        ;

    std::vector<std::thread> threads;
    switch (problem_type) {
    case ProblemType::RectangleGuillotine: {
        rectangleguillotine::Instance instance(objective, items_path, bins_path, defects_path);
        if (vm.count("bin-infinite-width"))
            instance.set_bin_infinite_width();
        if (vm.count("bin-infinite-height"))
            instance.set_bin_infinite_height();
        if (vm.count("bin-infinite-copies"))
            instance.set_bin_infinite_copies();
        if (vm.count("unweighted"))
            instance.set_unweighted();

        rectangleguillotine::Solution solution(instance);
        solution.algorithm_start(info);
        for (Counter i = 0; i < (Counter)algorithms.size(); ++i) {
            // Parse branching scheme
            std::vector<std::string> branching_scheme_args = po::split_unix(branching_schemes[i]);
            // Insert parameters at the beginning of branching_scheme_args
            branching_scheme_args.insert(branching_scheme_args.begin() + 1,
                    parameters.begin(), parameters.end());
            threads.push_back(std::thread(run_rectangleguillotine,
                        i + 1,
                        std::ref(solution),
                        std::ref(instance),
                        branching_scheme_args,
                        algorithms[i],
                        Info(info, true, "thread" + std::to_string(i))));
        }
        for (Counter i = 0; i < (Counter)algorithms.size(); ++i)
            threads[i].join();
        solution.algorithm_end(info);
        break;
    } case ProblemType::Rectangle: {
        break;
    }
    }

    return 0;
}

