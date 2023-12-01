add_library(photon_co STATIC
    co.cpp
    alog.cpp
    switch_context.s
    x.cpp
    )
target_link_libraries(photon_co PUBLIC pthread c++abi)
if(${CACS})
    target_compile_options(photon_co PUBLIC "-DCACSTH")
    if(${CACSTAIL})
        target_compile_options(photon_co PUBLIC "-DCACSTAIL")
    endif()
    if(${CACS_ONLY_YIELD})
        target_compile_options(photon_co PUBLIC "-DCACSYIELD")
    endif()
endif()
target_compile_options(photon_co PUBLIC "-DRANDOMIZE_SP")
target_include_directories(photon_co PUBLIC ../)

add_library(photon_co_net STATIC
    co.cpp
    alog.cpp
    switch_context.s
    x.cpp
    net/http/client.cpp
    net/http/cookie_jar.cpp
    net/http/headers.cpp
    net/http/message.cpp
    net/http/status.cpp
    net/http/url.cpp
    net/http/body.cpp
    common/estring.cpp
    common/iovector.cpp
    common/expirecontainer.cpp
    rpc/out-of-order-execution.cpp
    rpc/rpc.cpp
    )
target_link_libraries(photon_co_net PUBLIC pthread c++abi)

add_library(photon_co_net_cacs STATIC
    co.cpp
    alog.cpp
    switch_context.s
    x.cpp
    net/http/client.cpp
    net/http/cookie_jar.cpp
    net/http/headers.cpp
    net/http/message.cpp
    net/http/status.cpp
    net/http/url.cpp
    net/http/body.cpp
    common/estring.cpp
    common/iovector.cpp
    common/expirecontainer.cpp
    rpc/out-of-order-execution.cpp
    rpc/rpc.cpp
    )
target_link_libraries(photon_co_net_cacs PUBLIC pthread c++abi)
target_compile_options(photon_co_net_cacs PUBLIC "-DCACSTH")
target_compile_options(photon_co_net_cacs PUBLIC "-DCACSTAIL")


add_library(photon_co_net_cacs_yield STATIC
    co.cpp
    alog.cpp
    switch_context.s
    x.cpp
    net/http/client.cpp
    net/http/cookie_jar.cpp
    net/http/headers.cpp
    net/http/message.cpp
    net/http/status.cpp
    net/http/url.cpp
    net/http/body.cpp
    common/estring.cpp
    common/iovector.cpp
    common/expirecontainer.cpp
    rpc/out-of-order-execution.cpp
    rpc/rpc.cpp
    )
target_link_libraries(photon_co_net_cacs_yield PUBLIC pthread c++abi)
target_compile_options(photon_co_net_cacs_yield PUBLIC "-DCACSTH")
target_compile_options(photon_co_net_cacs_yield PUBLIC "-DCACSTAIL")
target_compile_options(photon_co_net_cacs_yield PUBLIC "-DCACSYIELD")