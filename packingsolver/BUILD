cc_binary(
        name = "main",
        srcs = ["main.cpp"],
        deps = [
                "//packingsolver/algorithms:algorithms",
                "//packingsolver/rectangleguillotine:rectangleguillotine",
                "@boost//:program_options",
        ],
        copts = select({
                "@bazel_tools//src/conditions:windows": ['/std:c++latest'],
                "//conditions:default":                 ["-std=c++14"],
        }),
        linkopts = select({
                "@bazel_tools//src/conditions:windows": [],
                "//conditions:default":                 ["-lpthread"],
        }),
        data = ["//data/rectangle:rectangle"],
)

