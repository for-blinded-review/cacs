add_executable(sum-of-seq-test
    sum-of-seq-test-origin.cpp
    g.cpp
)

target_link_libraries(sum-of-seq-test pthread fibsched c++abi)

add_executable(sum-of-seq-test-bcontext
    sum-of-seq-test-bcontext.cpp
)

target_link_libraries(sum-of-seq-test-bcontext photon_co pthread fibsched)

add_executable(sum-of-seq-test-boost
    sum-of-seq-test-boost.cpp
)

target_link_libraries(sum-of-seq-test-boost photon_co pthread fibsched)


add_executable(sum-of-seq-test-photon
    sum-of-seq-test.cpp
    generator.cpp
    g.cpp
)

target_link_libraries(sum-of-seq-test-photon photon_co pthread fibsched)
