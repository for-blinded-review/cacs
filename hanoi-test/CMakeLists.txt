add_executable(hanoi-test hanoi.cpp)

target_link_libraries(hanoi-test pthread photon_co fibsched)

# add_custom_target(
#     cs-hanoi ALL
#     COMMAND
#         sh build.sh && cp
#         ${CMAKE_CURRENT_SOURCE_DIR}/csharp-test/bin/Release/net7.0/linux-x64/publish/cs-hanoi
#         ${CMAKE_CURRENT_BINARY_DIR}
#     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/csharp-test)
