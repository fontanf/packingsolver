load("//packingsolver:variables.bzl", "STDCPP")
load("//packingsolver:variables.bzl", "CLP_COPTS")
load("//packingsolver:variables.bzl", "CPLEX_COPTS")
load("//packingsolver:variables.bzl", "XPRESS_COPTS")
load("//packingsolver:variables.bzl", "GUROBI_COPTS")

cc_library(
        name = "boxstacks",
        hdrs = [
                "instance.hpp",
                "instance_builder.hpp",
                "solution.hpp",
                "algorithm_formatter.hpp",
                "optimize.hpp",
                "sequential_onedimensional_rectangle.hpp",
                "branching_scheme.hpp",
        ],
        srcs = [
                "instance.cpp",
                "instance_builder.cpp",
                "solution.cpp",
                "algorithm_formatter.cpp",
                "optimize.cpp",
                "sequential_onedimensional_rectangle.cpp",
                "branching_scheme.cpp",
        ],
        deps = [
                "//packingsolver/algorithms:common",
                "//packingsolver/algorithms:algorithms",
                "//packingsolver/onedimensional:onedimensional",
                "//packingsolver/rectangle:rectangle",
                "@treesearchsolver//treesearchsolver:treesearchsolver",
                "@knapsacksolver//knapsacksolver/multiple_choice_subset_sum/algorithms:dynamic_programming_bellman",
        ],
        copts = STDCPP + CLP_COPTS + CPLEX_COPTS + GUROBI_COPTS + XPRESS_COPTS,
        visibility = ["//visibility:public"],
)

cc_binary(
        name = "main",
        srcs = ["main.cpp"],
        deps = [
                ":boxstacks",
                "@boost//:program_options",
        ],
        copts = STDCPP + CLP_COPTS + CPLEX_COPTS + GUROBI_COPTS + XPRESS_COPTS,
        linkopts = select({
                "@bazel_tools//src/conditions:windows": [],
                "//conditions:default":                 ["-lpthread"],
        }),
        data = ["//data/box:box"],
)

cc_test(
        name = "test",
        srcs = [
                "boxstacks_test.cpp",
                "tests/branching_scheme_test.cpp",
        ],
        deps = [
                ":boxstacks",
                "@googletest//:gtest_main",
                "//packingsolver/algorithms:algorithms",
        ],
        copts = STDCPP,
        data = ["//data/box:box"],
        timeout = "short",
)

