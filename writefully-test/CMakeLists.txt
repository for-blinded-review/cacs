add_executable(socket-test socket-test.cpp)
target_link_libraries(socket-test PUBLIC photon_co fibsched co20executor pthread c++abi)
target_include_directories(socket-test PUBLIC ../)

add_custom_target(
    cs-writefully ALL
    COMMAND
        sh build.sh && cp
        ${CMAKE_CURRENT_SOURCE_DIR}/csharp-test/bin/Release/net7.0/linux-x64/publish/cs-writefully
        ${CMAKE_CURRENT_BINARY_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/csharp-test)

add_custom_target(
    rust-writefully ALL
    COMMAND
        sh build.sh && cp
        ${CMAKE_CURRENT_SOURCE_DIR}/rust-test/target/release/rust-writefully
        ${CMAKE_CURRENT_BINARY_DIR}/
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/rust-test)
