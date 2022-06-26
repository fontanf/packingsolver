load("//packingsolver:variables.bzl", "STDCPP")

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
        copts = STDCPP,
        visibility = ["//visibility:public"],
)

cc_test(
        name = "test",
        srcs = [
                "tests/waste_test.cpp",
                "tests/add_test.cpp",
                "tests/insertion_test.cpp",
                "tests/defect_test.cpp",
                "tests/trim_test.cpp",
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

