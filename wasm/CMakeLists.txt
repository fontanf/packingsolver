add_executable(PackingSolver_wasm)
target_sources(PackingSolver_wasm PRIVATE
    src/packingsolver_wasm.cpp)
target_link_libraries(PackingSolver_wasm PRIVATE
    PackingSolver_rectangleguillotine
    PackingSolver_rectangle
    PackingSolver_box
    PackingSolver_boxstacks
    PackingSolver_onedimensional
    PackingSolver_irregular)
target_link_options(PackingSolver_wasm PRIVATE
    -lembind
    # Load with 'await PackingSolver()', in the browser, a web worker or Node.
    -sMODULARIZE=1
    -sEXPORT_NAME=PackingSolver
    -sENVIRONMENT=web,worker,node
    -sALLOW_MEMORY_GROWTH=1
    -sMAXIMUM_MEMORY=4GB
    # The workers of the threads are started when the module is loaded:
    # 'solve' blocks the calling thread, which then can't start new ones. If
    # the algorithms need more threads than the pool has, the thread creation
    # fails ('solve' returns an error) instead of blocking forever.
    -sPTHREAD_POOL_SIZE=16
    -sPTHREAD_POOL_SIZE_STRICT=2
    # The default allocator (dlmalloc) has a single lock: with several
    # threads, the algorithms were up to 7 times slower than natively.
    -sMALLOC=mimalloc)
# embind requires C++17: only the bindings are compiled with it, the
# libraries stay C++14.
set_target_properties(PackingSolver_wasm PROPERTIES
    CXX_STANDARD 17
    OUTPUT_NAME "packingsolver"
    SUFFIX ".js")
