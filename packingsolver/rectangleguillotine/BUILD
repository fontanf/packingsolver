cc_library(
        name = "rectangleguillotine",
        hdrs = [
                "instance.hpp",
                "solution.hpp",
                "branching_scheme.hpp",
        ],
        srcs = [
                "instance.cpp",
                "solution.cpp",
                "branching_scheme.cpp",
        ],
        deps = ["//packingsolver/algorithms:common"],
        copts = select({
                "@bazel_tools//src/conditions:windows": ['/std:c++latest'],
                "//conditions:default":                 ["-std=c++14"],
        }),
        visibility = ["//visibility:public"],
)

cc_test(
        name = "test",
        srcs = [
                "tests/waste_test.cpp",
                "tests/add_test.cpp",
                "tests/insertion_test.cpp",
                "tests/defect_test.cpp",
                "tests/integration_test.cpp",
        ],
        deps = [
                ":rectangleguillotine",
                "@googletest//:gtest_main",
                "//packingsolver/algorithms:algorithms",
        ],
        copts = select({
                "@bazel_tools//src/conditions:windows": ['/std:c++latest'],
                "//conditions:default":                 ["-std=c++14"],
        }),
        data = [
                "//data/rectangle:rectangle",
        ],
        timeout = "short",
)

