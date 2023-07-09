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
                "optimize.hpp",
                "branching_scheme.hpp",
        ],
        srcs = [
                "instance.cpp",
                "instance_builder.cpp",
                "solution.cpp",
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
        ],
        deps = [
                ":onedimensional",
                "@googletest//:gtest_main",
                "//packingsolver/algorithms:algorithms",
        ],
        copts = STDCPP,
        data = ["//data/box:box"],
        timeout = "short",
)

