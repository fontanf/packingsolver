load("@bazel_skylib//lib:selects.bzl", "selects")

config_setting(
    name = "clp_build",
    values = {"define": "clp=true"},
    visibility = ["//visibility:public"],
)

selects.config_setting_group(
    name = "clp_linux",
    match_all = [":clp_build", "@bazel_tools//src/conditions:linux"],
)

selects.config_setting_group(
    name = "clp_windows",
    match_all = [":clp_build", "@bazel_tools//src/conditions:windows"],
)

selects.config_setting_group(
    name = "clp_darwin",
    match_all = [":clp_build", "@bazel_tools//src/conditions:darwin"],
)

config_setting(
    name = "cplex_build",
    values = {"define": "cplex=true"},
    visibility = ["//visibility:public"],
)

config_setting(
    name = "xpress_build",
    values = {"define": "xpress=true"},
    visibility = ["//visibility:public"],
)

config_setting(
    name = "gurobi_build",
    values = {"define": "gurobi=true"},
    visibility = ["//visibility:public"],
)

config_setting(
    name = "knitro_build",
    values = {"define": "knitro=true"},
    visibility = ["//visibility:public"],
)

load("//packingsolver:variables.bzl", "STDCPP",
         "CLP_COPTS", "CLP_DEP",
         "CPLEX_COPTS", "CPLEX_DEP",
         "GUROBI_COPTS", "GUROBI_DEP",
         "XPRESS_COPTS", "XPRESS_DEP")
