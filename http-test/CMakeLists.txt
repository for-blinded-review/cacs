add_executable(http-test
    http-test.cpp
)

target_link_libraries(http-test pthread photon_co_net c++abi)

add_executable(http-test-cacs
    http-test.cpp
)

target_link_libraries(http-test-cacs pthread photon_co_net_cacs c++abi)
target_compile_definitions(http-test-cacs PUBLIC "-DCACSTH")
target_compile_definitions(http-test-cacs PUBLIC "-DCACSTAIL")

add_executable(http-test-cacs-yield-only
    http-test.cpp
)

target_link_libraries(http-test-cacs-yield-only pthread photon_co_net_cacs_yield c++abi)
target_compile_definitions(http-test-cacs-yield-only PUBLIC "-DCACSTH")
target_compile_definitions(http-test-cacs-yield-only PUBLIC "-DCACSTAIL")
target_compile_definitions(http-test-cacs-yield-only PUBLIC "-DCACSYIELD")
