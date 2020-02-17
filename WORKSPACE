load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository", "new_git_repository")

http_archive(
    name = "googletest",
    build_file_content = """
cc_library(
        name = "gtest",
        srcs = ["googletest-release-1.8.0/googletest/src/gtest-all.cc", "googletest-release-1.8.0/googlemock/src/gmock-all.cc",],
        hdrs = glob(["**/*.h", "googletest-release-1.8.0/googletest/src/*.cc", "googletest-release-1.8.0/googlemock/src/*.cc",]),
        includes = ["googletest-release-1.8.0/googlemock", "googletest-release-1.8.0/googletest", "googletest-release-1.8.0/googletest/include", "googletest-release-1.8.0/googlemock/include",],
        linkopts = ["-pthread"],
        visibility = ["//visibility:public"],
)

cc_library(
        name = "gtest_main",
        srcs = ["googletest-release-1.8.0/googlemock/src/gmock_main.cc"],
        linkopts = ["-pthread"],
        visibility = ["//visibility:public"],
        deps = [":gtest"],
)
""",
    url = "https://github.com/google/googletest/archive/release-1.8.0.zip",
    sha256 = "f3ed3b58511efd272eb074a3a6d6fb79d7c2e6a0e374323d1e6bcbcc1ef141bf",
)

http_archive(
    name = "json",
    build_file_content = """
cc_library(
        name = "json",
        hdrs = ["single_include/nlohmann/json.hpp"],
        visibility = ["//visibility:public"],
        strip_include_prefix = "single_include/"
)
""",
    urls = ["https://github.com/nlohmann/json/releases/download/v3.7.3/include.zip"],
    sha256 = "87b5884741427220d3a33df1363ae0e8b898099fbc59f1c451113f6732891014",
)

git_repository(
    name = "benchtools",
    remote = "https://github.com/fontanf/benchtools.git",
    commit = "fe56ed683d32f70cd248d77cd4107e57eee05758",
    shallow_since = "1576623294 +0100",
)

local_repository(
    name = "benchtools_",
    path = "/home/florian/Dev/benchtools/",
)

