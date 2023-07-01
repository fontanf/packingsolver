load("//packingsolver:variables.bzl",
         "CLP_COPTS", "CLP_DEP",
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
                "iterative_beam_search.hpp",
                "vbpp2bpp.hpp",
                "column_generation.hpp",
                "dichotomic_search.hpp",
        ],
        srcs = [
        ],
        deps = [
                ":common",
                "@columngenerationsolver//columngenerationsolver:columngenerationsolver",
                "@knapsacksolver//knapsacksolver/algorithms:dynamic_programming_primal_dual",
        ] + CLP_DEP + CPLEX_DEP + GUROBI_DEP + XPRESS_DEP,
        copts = CLP_COPTS + CPLEX_COPTS + GUROBI_COPTS + XPRESS_COPTS,
        visibility = ["//visibility:public"],
)

