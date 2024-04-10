load("//packingsolver:variables.bzl", "STDCPP")
load("//packingsolver:variables.bzl", "CLP_COPTS")
load("//packingsolver:variables.bzl", "CPLEX_COPTS")
load("//packingsolver:variables.bzl", "XPRESS_COPTS")
load("//packingsolver:variables.bzl", "GUROBI_COPTS")

cc_library(
        name = "onedimensional",
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
                ":onedimensional",
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
                "onedimensional_test.cpp",
        ],
        deps = [
                ":onedimensional",
                "//packingsolver/algorithms:algorithms",
                "@googletest//:gtest_main",
                "@boost//:filesystem",
        ],
        copts = STDCPP,
        #data = ["//data/onedimensional:onedimensional"],
        timeout = "short",
)

