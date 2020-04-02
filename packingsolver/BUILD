load("//packingsolver:variables.bzl", "STDCPP")

cc_binary(
        name = "main",
        srcs = ["main.cpp"],
        deps = [
                "//packingsolver/algorithms:algorithms",
                "//packingsolver/rectangleguillotine:rectangleguillotine",
                "@boost//:program_options",
        ],
        copts = STDCPP,
        linkopts = select({
                "@bazel_tools//src/conditions:windows": [],
                "//conditions:default":                 ["-lpthread"],
        }),
        data = ["//data/rectangle:rectangle"],
)

