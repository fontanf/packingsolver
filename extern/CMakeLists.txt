# Enable FetchContent.
include(FetchContent)

# Fetch fontanf/columngenerationsolver.
FetchContent_Declare(
    columngenerationsolver
    GIT_REPOSITORY https://github.com/fontanf/columngenerationsolver.git
    GIT_TAG 32561f17c6fa7100b2dedb5d397dab1a03c9db2c)
    #SOURCE_DIR "${PROJECT_SOURCE_DIR}/../columngenerationsolver/")
FetchContent_MakeAvailable(columngenerationsolver)

# Fetch fontanf/knitrocpp.
if (PackingSolver_USE_KNITRO)
    FetchContent_Declare(
        knitrocpp
        GIT_REPOSITORY https://github.com/fontanf/knitrocpp.git
        GIT_TAG 6c63e3f3931329e2c1a2c22ce465fe8667516a36)
    #SOURCE_DIR "${PROJECT_SOURCE_DIR}/../knitrocpp/")
    FetchContent_MakeAvailable(knitrocpp)
endif()
