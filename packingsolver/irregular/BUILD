load("//packingsolver:variables.bzl", "STDCPP17")
load("//packingsolver:variables.bzl", "CLP_COPTS")
load("//packingsolver:variables.bzl", "CPLEX_COPTS")
load("//packingsolver:variables.bzl", "XPRESS_COPTS")
load("//packingsolver:variables.bzl", "GUROBI_COPTS")
load("//packingsolver:variables.bzl", "KNITRO_COPTS", "KNITRO_DEP")

cc_library(
        name = "irregular",
        hdrs = [
                "instance.hpp",
                "instance_builder.hpp",
                "solution.hpp",
                "optimize.hpp",
                "irregular_to_rectangle.hpp",
                "nlp_circle.hpp",
                "nlp.hpp",
        ],
        srcs = [
                "instance.cpp",
                "instance_builder.cpp",
                "solution.cpp",
                "optimize.cpp",
                "irregular_to_rectangle.cpp",
                "nlp_circle.cpp",
                "nlp.cpp",
        ],
        deps = [
                "//packingsolver/algorithms:common",
                "//packingsolver/algorithms:algorithms",
                "//packingsolver/rectangle:rectangle",
                "@treesearchsolver//treesearchsolver:treesearchsolver",
                "@cgal//:cgal",
        ] + KNITRO_DEP,
        copts = STDCPP17 + CLP_COPTS + CPLEX_COPTS + GUROBI_COPTS + XPRESS_COPTS + KNITRO_COPTS,
        visibility = ["//visibility:public"],
)

cc_binary(
        name = "main",
        srcs = ["main.cpp"],
        deps = [
                ":irregular",
                "@boost//:program_options",
        ],
        copts = STDCPP17 + CLP_COPTS + CPLEX_COPTS + GUROBI_COPTS + XPRESS_COPTS,
        linkopts = select({
                "@bazel_tools//src/conditions:windows": [],
                "//conditions:default":                 ["-lpthread"],
        }),
)
