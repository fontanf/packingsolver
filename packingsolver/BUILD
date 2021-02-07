config_setting(name = "coinor_build", values = {"define": "coinor=true"}, visibility = ["//visibility:public"])
config_setting(name = "cplex_build",  values = {"define": "cplex=true"},  visibility = ["//visibility:public"])
config_setting(name = "xpress_build", values = {"define": "xpress=true"}, visibility = ["//visibility:public"])
config_setting(name = "gurobi_build", values = {"define": "gurobi=true"}, visibility = ["//visibility:public"])

load("//packingsolver:variables.bzl", "STDCPP",
         "COINOR_COPTS", "COINOR_DEP",
         "CPLEX_COPTS", "CPLEX_DEP",
         "GUROBI_COPTS", "GUROBI_DEP",
         "XPRESS_COPTS", "XPRESS_DEP")

cc_binary(
        name = "main",
        srcs = ["main.cpp"],
        deps = [
                "//packingsolver/algorithms:algorithms",
                "//packingsolver/rectangleguillotine:rectangleguillotine",
                "@boost//:program_options",
        ],
        copts = STDCPP + COINOR_COPTS + CPLEX_COPTS + GUROBI_COPTS + XPRESS_COPTS,
        linkopts = select({
                "@bazel_tools//src/conditions:windows": [],
                "//conditions:default":                 ["-lpthread"],
        }),
        data = ["//data/rectangle:rectangle"],
)

