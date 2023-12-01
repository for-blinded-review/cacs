cmake_minimum_required(VERSION 3.14.5)

project(PhotonCoroutineTests C CXX ASM)

if (EXISTS ${CMAKE_SOURCE_DIR}/ENV.CMake)
    message("Loading environment...")
    include(ENV.CMake)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED on)

add_link_options("-stdlib=libc++")
set(CMAKE_CXX_FLAGS "-stdlib=libc++")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_EXPORT_COMPILE_COMMANDS on)

set(CMAKE_EXE_LINKER_FLAGS_INIT "-fuse-ld=lld")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "-fuse-ld=lld")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-fuse-ld=lld")

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost 1.81.0 REQUIRED COMPONENTS context)

include_directories(
    ${CMAKE_SOURCE_DIR}
    ${Boost_INCLUDE_DIRS}
    /usr/include
)

link_directories(
    ${Boost_LIBRARY_DIR_RELEASE}
    /usr/local/lib64
    /usr/lib
    /usr/lib64
)

option(CACS "Use Context-aware context switch" OFF)
option(CACSTAIL "Use CACS with tail calling optimization" OFF)

add_subdirectory(common/photonlib)
add_subdirectory(common/fibsched)
add_subdirectory(common/co20executor)

add_subdirectory(sum-of-seq-test)
add_subdirectory(hanoi-test)
add_subdirectory(writefully-test)
add_subdirectory(yield-test)
add_subdirectory(http-test)
add_subdirectory(rpc-test)


