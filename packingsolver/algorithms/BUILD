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
                "astar.hpp",
                "dfs.hpp",
                "mbastar.hpp",
                "dpastar.hpp",
        ],
        srcs = [
        ],
        deps = [":common"],
        visibility = ["//visibility:public"],
)

