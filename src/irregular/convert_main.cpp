#include "convert.hpp"
#include "packingsolver/irregular/instance_builder.hpp"

#include <boost/program_options.hpp>

using namespace packingsolver;
using namespace packingsolver::irregular;
namespace po = boost::program_options;

int main(int argc, char *argv[])
{
    po::options_description desc("Allowed options");
    desc.add_options()
        (",h", "Produce help message")
        ("input,i", po::value<std::string>()->required(), "Input instance path")
        ("output,o", po::value<std::string>()->required(), "Output instance path")
        ("conversion,c", po::value<std::string>()->required(), "Conversion rule (e.g. song2014)")
        ;

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
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
    instance_builder.read(vm["input"].as<std::string>());
    Instance instance = instance_builder.build();

    const std::string conversion = vm["conversion"].as<std::string>();
    Instance converted_instance = [&]() -> Instance {
        if (conversion == "song2014") {
            return convert_song2014(instance);
        } else if (conversion == "martinez2017_sb") {
            return convert_martinez2017(instance, 1.1);
        } else if (conversion == "martinez2017_mb") {
            return convert_martinez2017(instance, 1.5);
        } else if (conversion == "martinez2017_lb") {
            return convert_martinez2017(instance, 2.0);
        } else {
            throw std::invalid_argument(
                    "Unknown conversion rule: \"" + conversion + "\".");
        }
    }();

    converted_instance.write(vm["output"].as<std::string>());
    return 0;
}
