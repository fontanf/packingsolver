#include "packingsolver/irregular/instance_builder.hpp"
#include "packingsolver/irregular/solution.hpp"

#include <boost/program_options.hpp>

using namespace packingsolver;
using namespace packingsolver::irregular;
namespace po = boost::program_options;

int main(int argc, char *argv[])
{
    po::options_description desc("Allowed options");
    desc.add_options()
        (",h", "Produce help message")
        ("instance,i", po::value<std::string>()->required(), "Instance path")
        ("solution,s", po::value<std::string>()->required(), "Solution path")
        ;
    po::positional_options_description pod;
    pod.add("instance", 1);
    pod.add("solution", 1);

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).options(desc).positional(pod).run(), vm);
    if (vm.count("help")) {
        std::cout << desc << std::endl;
        return 1;
    }
    try {
        po::notify(vm);
    } catch (const po::required_option& e) {
        std::cout << desc << std::endl;
        return 1;
    }

    InstanceBuilder instance_builder;
    instance_builder.read(vm["instance"].as<std::string>());
    Instance instance = instance_builder.build();

    const std::string solution_path = vm["solution"].as<std::string>();
    Solution solution(instance, solution_path);
    solution.write(solution_path);

    return 0;
}
