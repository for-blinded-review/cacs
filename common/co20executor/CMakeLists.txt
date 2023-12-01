add_library(
    co20executor STATIC
    executor.cpp
)
target_link_libraries(co20executor PUBLIC pthread c++abi)
target_include_directories(co20executor PUBLIC ../)