load("//packingsolver:variables.bzl", "STDCPP")
load("//packingsolver:variables.bzl", "CLP_COPTS")
load("//packingsolver:variables.bzl", "CPLEX_COPTS")
load("//packingsolver:variables.bzl", "XPRESS_COPTS")
load("//packingsolver:variables.bzl", "GUROBI_COPTS")

cc_library(
        name = "rectangleguillotine",
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
        ],
        copts = STDCPP + CLP_COPTS + CPLEX_COPTS + GUROBI_COPTS + XPRESS_COPTS,
        visibility = ["//visibility:public"],
)

cc_binary(
        name = "main",
        srcs = ["main.cpp"],
        deps = [
                ":rectangleguillotine",
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
                "rectangleguillotine_test.cpp",
                "tests/waste_test.cpp",
                "tests/add_test.cpp",
                "tests/insertion_test.cpp",
                "tests/defect_test.cpp",
                "tests/cut_through_defect_test.cpp",
                "tests/trim_test.cpp",
                "tests/cut_thickness_test.cpp",
                "tests/integration_test.cpp",
        ],
        deps = [
                ":rectangleguillotine",
                "@googletest//:gtest_main",
                "//packingsolver/algorithms:algorithms",
        ],
        copts = STDCPP,
        data = [
                "//data/rectangle:rectangle",
                "//data/rectangle_solutions:rectangle_solutions",
        ],
        timeout = "short",
)

