cc_library(
        name = "common",
        hdrs = ["common.hpp"],
        srcs = ["common.cpp"],
        deps = ["@optimizationtools//optimizationtools:info"],
        visibility = ["//visibility:public"],
)

cc_library(
        name = "algorithms",
        hdrs = [
                "a_star.hpp",
                "depth_first_search.hpp",
                "iterative_memory_bounded_a_star.hpp",
                "dynamic_programming_a_star.hpp",
        ],
        srcs = [
        ],
        deps = [":common"],
        visibility = ["//visibility:public"],
)

