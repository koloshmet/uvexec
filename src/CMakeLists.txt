CPMAddPackage(
        GITHUB_REPOSITORY nvidia/stdexec
        GIT_TAG ${UVEXEC_STDEXEC_VERSION}
        OPTIONS "STDEXEC_BUILD_EXAMPLES OFF" "STDEXEC_BUILD_TESTS OFF" "BUILD_TESTING OFF" "STDEXEC_ENABLE_IO_URING_TESTS OFF"
)
CPMAddPackage("gh:libuv/libuv@${UVEXEC_UV_VERSION}")

if(BUILD_SHARED_LIBS)
    set(UV_LIB_GENERIC uv)
else()
    set(UV_LIB_GENERIC uv_a)
endif()
add_library(uvexec_uv ALIAS ${UV_LIB_GENERIC})

add_library(uvexec_safe_uv
        uv_util/safe_uv.c
)
target_compile_features(uvexec_safe_uv PRIVATE c_std_11)
target_compile_options(uvexec_safe_uv PRIVATE ${UVEXEC_WARNINGS})
target_link_libraries(uvexec_safe_uv PUBLIC uvexec_uv)

add_library(uvexec_impl
        execution/loop.cpp
        sockets/addr.cpp
        sockets/tcp.cpp
        sockets/tcp_listener.cpp
        sockets/udp.cpp
        uv_util/errors.cpp
        uv_util/misc.cpp
        uv_util/reqs.cpp
)
target_compile_features(uvexec_impl PRIVATE cxx_std_20)
target_compile_options(uvexec_impl PRIVATE ${UVEXEC_WARNINGS})
target_compile_definitions(uvexec_impl PUBLIC ${UVEXEC_DEFINITIONS})
target_include_directories(uvexec_impl PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(uvexec_impl PUBLIC STDEXEC::stdexec uvexec_safe_uv)
