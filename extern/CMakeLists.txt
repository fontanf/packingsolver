# Enable FetchContent.
include(FetchContent)

# Fetch fontanf/optimizationtools.
FetchContent_Declare(
    optimizationtools
    GIT_REPOSITORY https://github.com/fontanf/optimizationtools.git
    GIT_TAG a00cdd490eda141b869a6dad6f661e55d33fab38
    #SOURCE_DIR "${PROJECT_SOURCE_DIR}/../optimizationtools/"
    EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(optimizationtools)

# Fetch fontanf/mathoptsolverscmake.
set(MATHOPTSOLVERSCMAKE_USE_CLP ON)
FetchContent_Declare(
    mathoptsolverscmake
    GIT_REPOSITORY https://github.com/fontanf/mathoptsolverscmake.git
    GIT_TAG 80cd13a843e2c10684abbfe76cf9aae0d71ff79d
    #SOURCE_DIR "${PROJECT_SOURCE_DIR}/../mathoptsolverscmake/"
    EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(mathoptsolverscmake)

# Fetch boost.
set(BOOST_INCLUDE_LIBRARIES thread filesystem system program_options dynamic_bitset)
set(BOOST_ENABLE_CMAKE ON)
include(FetchContent)
FetchContent_Declare(
    Boost
    URL https://github.com/boostorg/boost/releases/download/boost-1.84.0/boost-1.84.0.tar.xz
    EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(Boost)

# Fetch fontanf/treesearchsolver.
FetchContent_Declare(
    treesearchsolver
    GIT_REPOSITORY https://github.com/fontanf/treesearchsolver.git
    GIT_TAG 8e2d5190d4005a753a815641b1e3f8753a9d7095
    #SOURCE_DIR "${PROJECT_SOURCE_DIR}/../treesearchsolver/"
    EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(treesearchsolver)

# Fetch fontanf/columngenerationsolver.
set(COLUMNGENERATIONSOLVER_USE_CLP ON)
FetchContent_Declare(
    columngenerationsolver
    GIT_REPOSITORY https://github.com/fontanf/columngenerationsolver.git
    GIT_TAG 8f9ac25f724ffe13d239728974752567afd2a586
    #SOURCE_DIR "${PROJECT_SOURCE_DIR}/../columngenerationsolver/"
    EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(columngenerationsolver)

# Fetch fontanf/knitrocpp.
if (PACKINGSOLVER_USE_KNITRO)
    FetchContent_Declare(
        knitrocpp
        GIT_REPOSITORY https://github.com/fontanf/knitrocpp.git
        GIT_TAG 6c63e3f3931329e2c1a2c22ce465fe8667516a36
        #SOURCE_DIR "${PROJECT_SOURCE_DIR}/../knitrocpp/"
        EXCLUDE_FROM_ALL)
    FetchContent_MakeAvailable(knitrocpp)
endif()
