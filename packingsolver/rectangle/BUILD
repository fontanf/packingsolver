load("//packingsolver:variables.bzl", "STDCPP")
load("//packingsolver:variables.bzl", "CLP_COPTS")
load("//packingsolver:variables.bzl", "CPLEX_COPTS")
load("//packingsolver:variables.bzl", "XPRESS_COPTS")
load("//packingsolver:variables.bzl", "GUROBI_COPTS")

cc_library(
        name = "rectangle",
        hdrs = [
                "instance.hpp",
                "instance_builder.hpp",
                "solution.hpp",
                "algorithm_formatter.hpp",
                "optimize.hpp",
                "branching_scheme.hpp",
        ],
        srcs = [
                "instance.cpp",
                "instance_builder.cpp",
                "solution.cpp",
                "algorithm_formatter.cpp",
                "optimize.cpp",
                "branching_scheme.cpp",
        ],
        deps = [
                "//packingsolver/algorithms:common",
                "//packingsolver/algorithms:algorithms",
                "@treesearchsolver//treesearchsolver:treesearchsolver",
        ],
        copts = STDCPP + CLP_COPTS + CPLEX_COPTS + GUROBI_COPTS + XPRESS_COPTS,
        visibility = ["//visibility:public"],
)

cc_binary(
        name = "main",
        srcs = ["main.cpp"],
        deps = [
                ":rectangle",
                "@boost//:program_options",
        ],
        copts = STDCPP + CLP_COPTS + CPLEX_COPTS + GUROBI_COPTS + XPRESS_COPTS,
        linkopts = select({
                "@bazel_tools//src/conditions:windows": [],
                "//conditions:default":                 ["-lpthread"],
        }),
        data = ["//data/rectangle:rectangle"],
)

cc_test(
        name = "test",
        srcs = [
                "rectangle_test.cpp",
                "tests/branching_scheme_test.cpp",
                "tests/branching_scheme_defects_test.cpp",
                "tests/branching_scheme_unloading_test.cpp",
                "tests/branching_scheme_integration_test.cpp",
        ],
        deps = [
                ":rectangle",
                "//packingsolver/algorithms:algorithms",
                "@googletest//:gtest_main",
                "@boost//:filesystem",
        ],
        copts = STDCPP,
        data = ["//data/rectangle:rectangle"],
        timeout = "short",
)

