STDCPP = select({
            "@bazel_tools//src/conditions:windows": ['/std:c++latest'],
            "//conditions:default":                 ["-std=c++11"]})

COINOR_COPTS = select({
            "//packingsolver:coinor_build": ["-DCOINOR_FOUND"],
            "//conditions:default": []})
CPLEX_COPTS = select({
            "//packingsolver:cplex_build": [
                    "-DCPLEX_FOUND",
                    "-m64",
                    "-DIL_STD"],
            "//conditions:default": []})
GUROBI_COPTS = select({
            "//packingsolver:gurobi_build": ["-DGUROBI_FOUND"],
            "//conditions:default": []})
XPRESS_COPTS = select({
            "//packingsolver:xpress_build": ["-DXPRESS_FOUND"],
            "//conditions:default": []})

COINOR_DEP = select({
            "//packingsolver:coinor_build": ["@coinor//:coinor"],
            "//conditions:default": []})
CPLEX_DEP = select({
            "//packingsolver:cplex_build": ["@cplex//:cplex"],
            "//conditions:default": []})
GUROBI_DEP = select({
            "//packingsolver:gurobi_build": ["@gurobi//:gurobi"],
            "//conditions:default": []})
XPRESS_DEP = select({
            "//packingsolver:xpress_build": ["@xpress//:xpress"],
            "//conditions:default": []})

