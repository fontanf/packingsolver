load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

git_repository(
    name = "googletest",
    remote = "https://github.com/google/googletest.git",
    commit = "58d77fa8070e8cec2dc1ed015d66b454c8d78850",
    shallow_since = "1656350095 -0400",
)

git_repository(
    name = "com_github_nelhage_rules_boost",
    remote = "https://github.com/nelhage/rules_boost",
    commit = "e83dfef18d91a3e35c8eac9b9aeb1444473c0efd",
    shallow_since = "1671181466 +0000",
)
load("@com_github_nelhage_rules_boost//:boost/boost.bzl", "boost_deps")
boost_deps()

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
    name = "optimizationtools",
    remote = "https://github.com/fontanf/optimizationtools.git",
    commit = "9c47813badb03c9b46c5f7c3e60a8b5f8647ac5b",
)

local_repository(
    name = "optimizationtools_",
    path = "../optimizationtools/",
)

git_repository(
    name = "treesearchsolver",
    remote = "https://github.com/fontanf/treesearchsolver.git",
    commit = "2aee51c507fb28d945462f6d88ec064f35baba5d",
)

local_repository(
    name = "treesearchsolver_",
    path = "../treesearchsolver/",
)

git_repository(
    name = "columngenerationsolver",
    remote = "https://github.com/fontanf/columngenerationsolver.git",
    commit = "06f706eae6181347509de28f4166adec47d3feb1",
)

local_repository(
    name = "columngenerationsolver_",
    path = "../../Dev/columngenerationsolver/",
)

git_repository(
    name = "knapsacksolver",
    remote = "https://github.com/fontanf/knapsacksolver.git",
    commit = "8cf1ef9a9c2cf2622e514cc8c1fce36eb924b3c1",
)

local_repository(
    name = "knapsacksolver_",
    path = "../../Dev/knapsacksolver/",
)

http_archive(
    name = "osi_linux",
    urls = ["https://github.com/coin-or/Osi/releases/download/releases%2F0.108.8/Osi-releases.0.108.8-x86_64-ubuntu20-gcc940-static.tar.gz"],
    sha256 = "bd5a5bf1e6b6a28d13d41ab1554becd9f3992afe775785e51a88c9405cf2853e",
    build_file_content = """
cc_library(
    name = "osi",
    hdrs = glob(["include/coin/Osi*.h*"], exclude_directories = 0),
    strip_include_prefix = "include/coin/",
    srcs = ["lib/libOsi.a", "lib/libOsiCommonTests.a"],
    visibility = ["//visibility:public"],
)
""",
)

http_archive(
    name = "osi_windows",
    urls = ["https://github.com/coin-or/Osi/releases/download/releases%2F0.108.8/Osi-releases.0.108.8-w64-msvc16-md.zip"],
    sha256 = "a61fc462cb598139d205cd2323522581a01900575d0d6bccf660b6c7e1b0b71c",
    build_file_content = """
cc_library(
    name = "osi",
    hdrs = glob(["include/coin/Osi*.h*"], exclude_directories = 0),
    strip_include_prefix = "include/coin/",
    visibility = ["//visibility:public"],
    srcs = ["lib/libOsi.lib", "lib/libOsiCommonTests.lib"],
)
""",
)

http_archive(
    name = "coinutils_linux",
    urls = ["https://github.com/coin-or/CoinUtils/releases/download/releases%2F2.11.9/CoinUtils-releases.2.11.9-x86_64-ubuntu20-gcc940-static.tar.gz"],
    sha256 = "14d07de1b7961f68e037da6f0c57844fd67d4cc1a4b125642f42cd134b228094",
    build_file_content = """
cc_library(
    name = "coinutils",
    hdrs = glob(["include/coin/Coin*.h*"], exclude_directories = 0),
    strip_include_prefix = "include/coin/",
    srcs = ["lib/libCoinUtils.a"],
    linkopts = ["-llapack", "-lblas", "-lbz2", "-lz"],
    visibility = ["//visibility:public"],
)
""",
)

http_archive(
    name = "coinutils_windows",
    urls = ["https://github.com/coin-or/CoinUtils/releases/download/releases%2F2.11.9/CoinUtils-releases.2.11.9-w64-msvc16-md.zip"],
    sha256 = "2bc64f0afd80113571697e949b2663e9047272decf90d5f62e452c2628d33ca6",
    build_file_content = """
cc_library(
    name = "coinutils",
    hdrs = glob(["include/coin/Coin*.h*"], exclude_directories = 0),
    strip_include_prefix = "include/coin/",
    srcs = ["lib/libCoinUtils.lib"],
    visibility = ["//visibility:public"],
)
""",
)

http_archive(
    name = "clp_linux",
    urls = ["https://github.com/coin-or/Clp/releases/download/releases%2F1.17.8/Clp-releases.1.17.8-x86_64-ubuntu20-gcc940-static.tar.gz"],
    sha256 = "d569b04d19c25876e55d2557a1d9739df8eb50ec8ca11a98ad387fd8b90212c9",
    build_file_content = """
cc_library(
    name = "clp",
    hdrs = glob(["include/coin/*Clp*.h*"], exclude_directories = 0),
    strip_include_prefix = "include/coin",
    srcs = ["lib/libClp.a", "lib/libOsiClp.a"],
    deps = ["@osi_linux//:osi", "@coinutils_linux//:coinutils"],
    visibility = ["//visibility:public"],
)
""",
)

http_archive(
    name = "clp_windows",
    urls = ["https://github.com/coin-or/Clp/releases/download/releases%2F1.17.8/Clp-releases.1.17.8-w64-msvc16-md.zip"],
    sha256 = "e37c834aea5c31dfd8620b7d2432cb31fc16ecb0c6ffb398e8f07c9c82dd5028",
    build_file_content = """
cc_library(
    name = "clp",
    hdrs = glob(["include/coin/*Clp*.h*"], exclude_directories = 0),
    strip_include_prefix = "include/coin",
    srcs = ["lib/libClp.lib", "lib/libOsiClp.lib"],
    deps = ["@osi_windows//:osi", "@coinutils_windows//:coinutils"],
    visibility = ["//visibility:public"],
)
""",
)

new_local_repository(
    name = "cplex",
    path = "/opt/ibm/ILOG/CPLEX_Studio129/",
    build_file_content = """
cc_library(
    name = "concert",
    hdrs = glob(["concert/include/ilconcert/**/*.h"], exclude_directories = 0),
    strip_include_prefix = "concert/include/",
    srcs = ["concert/lib/x86-64_linux/static_pic/libconcert.a"],
    linkopts = [
            "-lm",
            "-lpthread",
            "-ldl",
    ],
    visibility = ["//visibility:public"],
)
cc_library(
    name = "cplex",
    hdrs = glob(["cplex/include/ilcplex/*.h"]),
    strip_include_prefix = "cplex/include/",
    srcs = [
            "cplex/lib/x86-64_linux/static_pic/libilocplex.a",
            "cplex/lib/x86-64_linux/static_pic/libcplex.a",
    ],
    deps = [":concert"],
    visibility = ["//visibility:public"],
)
cc_library(
    name = "cpoptimizer",
    hdrs = glob(["cpoptimizer/include/ilcp/*.h"]),
    strip_include_prefix = "cpoptimizer/include/",
    srcs = ["cpoptimizer/lib/x86-64_linux/static_pic/libcp.a"],
    deps = [":cplex"],
    visibility = ["//visibility:public"],
)
""",
)

new_local_repository(
    name = "gurobi",
    path = "/home/florian/Programmes/gurobi811/linux64/",
    build_file_content = """
cc_library(
    name = "gurobi",
    hdrs = [
            "include/gurobi_c.h",
            "include/gurobi_c++.h",
    ],
    strip_include_prefix = "include/",
    srcs = [
            "lib/libgurobi_c++.a",
            "lib/libgurobi81.so",
    ],
    visibility = ["//visibility:public"],
)
""",
)

new_local_repository(
    name = "xpress",
    path = "/opt/xpressmp/",
    build_file_content = """
cc_library(
    name = "xpress",
    hdrs = glob(["include/*.h"], exclude_directories = 0),
    strip_include_prefix = "include/",
    srcs = ["lib/libxprs.so"],
    visibility = ["//visibility:public"],
)
""",
)

# Knitro

load("//bazel:knitro.bzl", "knitro")
knitro(name = "knitro")

git_repository(
    name = "knitrocpp",
    remote = "https://github.com/fontanf/knitrocpp.git",
    commit = "125c6c359fdfc74fbeec5c0497de4e6b4820e909",
)

# CGAL

http_archive(
    name = "cgal",
    build_file_content = """
cc_library(
        name = "cgal",
        hdrs = glob(["CGAL-5.6/include/**/*.h*"], exclude_directories = 0),
        visibility = ["//visibility:public"],
        strip_include_prefix = "CGAL-5.6/include/",
        deps = [
                "@boost//:iterator",
        ],
)
""",
    urls = ["https://github.com/CGAL/cgal/releases/download/v5.6/CGAL-5.6.zip"],
    sha256 = "b80e4825c4dd24c32b7326842cabf997129cd4ec6f06d73be144e979e592c74a",
)

