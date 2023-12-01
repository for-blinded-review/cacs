
add_library(
    fibsched STATIC
    boost-fibersched.cpp
)
target_include_directories(fibsched PUBLIC ${Boost_INCLUDE_DIRS})
target_link_directories(fibsched PUBLIC ${Boost_LIBRARY_DIR_RELEASE})
target_link_libraries(fibsched PUBLIC pthread c++abi boost_context)
target_include_directories(fibsched PUBLIC ../)