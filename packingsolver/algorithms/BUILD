load("//packingsolver:variables.bzl",
         "COINOR_COPTS", "COINOR_DEP",
         "CPLEX_COPTS", "CPLEX_DEP",
         "GUROBI_COPTS", "GUROBI_DEP",
         "XPRESS_COPTS", "XPRESS_DEP")

cc_library(
        name = "common",
        hdrs = ["common.hpp"],
        srcs = ["common.cpp"],
        deps = [
                "@optimizationtools//optimizationtools:info",
                "@optimizationtools//optimizationtools:utils",
        ],
        visibility = ["//visibility:public"],
)

cc_library(
        name = "algorithms",
        hdrs = [
                "a_star.hpp",
                "depth_first_search.hpp",
                "iterative_memory_bounded_a_star.hpp",
                "dynamic_programming_a_star.hpp",
                "column_generation.hpp",
        ],
        srcs = [
        ],
        deps = [
                ":common",
                "@columngenerationsolver//columngenerationsolver:columngenerationsolver",
        ] + COINOR_DEP + CPLEX_DEP + GUROBI_DEP + XPRESS_DEP,
        copts = COINOR_COPTS + CPLEX_COPTS + GUROBI_COPTS + XPRESS_COPTS,
        visibility = ["//visibility:public"],
)

