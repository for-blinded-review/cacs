add_executable(
    yield-test
    yield-test.cpp
)
target_link_libraries(yield-test PUBLIC photon_co pthread c++abi)
target_include_directories(yield-test PUBLIC ../)