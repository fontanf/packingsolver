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
                "@optimizationtools//optimizationtools/utils:info",
                "@optimizationtools//optimizationtools/utils:utils",
        ],
        visibility = ["//visibility:public"],
)

cc_library(
        name = "algorithms",
        hdrs = [
                "a_star.hpp",
                "depth_first_search.hpp",
                "iterative_memory_bounded_a_star.hpp",
                "iterative_beam_search.hpp",
                "dynamic_programming_a_star.hpp",
                "vbpp2bpp.hpp",
                "column_generation.hpp",
                "dichotomic_search.hpp",
        ],
        srcs = [
        ],
        deps = [
                ":common",
                "@columngenerationsolver//columngenerationsolver:columngenerationsolver",
                "@knapsacksolver//knapsacksolver/algorithms:minknap",
        ] + COINOR_DEP + CPLEX_DEP + GUROBI_DEP + XPRESS_DEP,
        copts = COINOR_COPTS + CPLEX_COPTS + GUROBI_COPTS + XPRESS_COPTS,
        visibility = ["//visibility:public"],
)

