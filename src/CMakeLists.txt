CPMAddPackage(
        GITHUB_REPOSITORY nvidia/stdexec
        GIT_TAG ${UVEXEC_STDEXEC_VERSION}
        OPTIONS "STDEXEC_BUILD_EXAMPLES OFF" "STDEXEC_BUILD_TESTS OFF" "STDEXEC_ENABLE_IO_URING_TESTS OFF"
)
CPMAddPackage("gh:libuv/libuv@${UVEXEC_UV_VERSION}")

add_library(uvexec_safe_uv
        uv_util/safe_uv.c
)
target_compile_features(uvexec_safe_uv PRIVATE c_std_11)
target_compile_options(uvexec_safe_uv PRIVATE ${UVEXEC_WARNINGS})
target_link_libraries(uvexec_safe_uv PUBLIC uv)

add_library(uvexec_impl
        execution/loop.cpp
        sockets/addr.cpp
        sockets/tcp.cpp
        sockets/tcp_listener.cpp
        uv_util/errors.cpp
        uv_util/misc.cpp
        uv_util/reqs.cpp
)
target_compile_features(uvexec_impl PRIVATE cxx_std_20)
target_compile_options(uvexec_impl PRIVATE ${UVEXEC_WARNINGS})
target_include_directories(uvexec_impl PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(uvexec_impl PUBLIC STDEXEC::stdexec uvexec_safe_uv)
