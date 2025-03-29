//#include "packingsolver/box/instance_builder.hpp"
//#include "packingsolver/box/optimize.hpp"

//#include <gtest/gtest.h>
//#include <boost/filesystem.hpp>

//using namespace packingsolver::box;
//namespace fs = boost::filesystem;

//struct BoxOptimizeTestParams
//{
//    fs::path items_path;
//    fs::path bins_path;
//    fs::path defects_path;
//    fs::path parameters_path;
//    fs::path certificate_path;
//};

//inline std::ostream& operator<<(std::ostream& os, const BoxOptimizeTestParams& test_params)
//{
//    os << test_params.items_path;
//    return os;
//}

//class BoxOptimizeTest: public testing::TestWithParam<BoxOptimizeTestParams> { };

//TEST_P(BoxOptimizeTest, BoxOptimize)
//{
//    BoxOptimizeTestParams test_params = GetParam();
//    InstanceBuilder instance_builder;
//    instance_builder.read_item_types(test_params.items_path.string());
//    instance_builder.read_bin_types(test_params.bins_path.string());
//    instance_builder.read_parameters(test_params.parameters_path.string());
//    Instance instance = instance_builder.build();

//    OptimizeParameters optimize_parameters;
//    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
//    Output output = optimize(instance, optimize_parameters);

//    Solution solution(instance, test_params.certificate_path.string());
//    std::cout << std::endl
//        << "Reference solution" << std::endl
//        << "------------------" << std::endl;
//    solution.format(std::cout);

//    EXPECT_EQ(!(output.solution_pool.best() < solution), true);
//    EXPECT_EQ(!(solution < output.solution_pool.best()), true);
//}

//INSTANTIATE_TEST_SUITE_P(
//        Box,
//        BoxOptimizeTest,
//        testing::ValuesIn(std::vector<BoxOptimizeTestParams>{
//            {
//                fs::path("data") / "box" / "users" / "2024-11-24" / "items.csv",
//                fs::path("data") / "box" / "users" / "2024-11-24" / "bins.csv",
//                fs::path(""),
//                fs::path("data") / "box" / "users" / "2024-11-24" / "parameters.csv",
//                fs::path("data") / "box" / "users" / "2024-11-24" / "solution.csv",
//            }}));
