STDCPP = select({
            "@bazel_tools//src/conditions:windows": [''],
            "//conditions:default":                 ["-std=c++11"]})

CLP_COPTS = select({
            "//packingsolver:clp_build": ["-DCLP_FOUND"],
            "//conditions:default": []})
CLP_DEP = select({
            "//packingsolver:clp_windows": ["@clp_windows//:clp"],
            "//conditions:default": []
        }) + select({
            "//packingsolver:clp_linux": ["@clp_linux//:clp"],
            "//conditions:default": []})

CPLEX_COPTS = select({
            "//packingsolver:cplex_build": [
                    "-DCPLEX_FOUND",
                    "-m64",
                    "-DIL_STD"],
            "//conditions:default": []})
CPLEX_DEP = select({
            "//packingsolver:cplex_build": ["@cplex//:cplex"],
            "//conditions:default": []})

GUROBI_COPTS = select({
            "//packingsolver:gurobi_build": ["-DGUROBI_FOUND"],
            "//conditions:default": []})
GUROBI_DEP = select({
            "//packingsolver:gurobi_build": ["@gurobi//:gurobi"],
            "//conditions:default": []})

XPRESS_COPTS = select({
            "//packingsolver:xpress_build": ["-DXPRESS_FOUND"],
            "//conditions:default": []})
XPRESS_DEP = select({
            "//packingsolver:xpress_build": ["@xpress//:xpress"],
            "//conditions:default": []})

AMPL_COPTS = select({
            "//packingsolver:ampl_build": ["-DAMPL_FOUND"],
            "//conditions:default": []})
AMPL_DEP = select({
            "//packingsolver:ampl_build": ["@ampl//:ampl"],
            "//conditions:default": []})

