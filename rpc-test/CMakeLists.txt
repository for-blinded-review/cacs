add_executable(rpc-test
    rpc-test.cpp
)

target_link_libraries(rpc-test pthread photon_co_net c++abi)

add_executable(rpc-test-cacs
    rpc-test.cpp
)

target_link_libraries(rpc-test-cacs pthread photon_co_net_cacs c++abi)
target_compile_definitions(rpc-test-cacs PUBLIC "-DCACSTH" "-DCACSTAIL")

add_executable(rpc-test-cacs-yield-only
    rpc-test.cpp
)

target_link_libraries(rpc-test-cacs-yield-only pthread photon_co_net_cacs_yield c++abi)
target_compile_definitions(rpc-test-cacs-yield-only PUBLIC "-DCACSTH" "-DCACSTAIL" "-DCACSYIELD")
