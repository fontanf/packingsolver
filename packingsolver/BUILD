cc_binary(
        name = "main",
        srcs = ["main.cpp"],
        deps = [
                "//packingsolver/algorithms:algorithms",
                "//packingsolver/rectangleguillotine:rectangleguillotine",
        ],
        linkopts = [
                "-pthread",
                "-lboost_program_options",
        ],
        data = ["//data/rectangle:rectangle"],
)

