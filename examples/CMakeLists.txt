add_library(echo_uv echo_uv.c)
target_compile_features(echo_uv PUBLIC c_std_11)
target_compile_options(echo_uv PRIVATE ${UVEXEC_WARNINGS})
target_link_libraries(echo_uv PRIVATE uvexec_uv)

CPMAddPackage("gh:fmtlib/fmt#10.2.1")

add_library(echo_uvexec echo_uvexec.cpp)
target_compile_features(echo_uvexec PRIVATE cxx_std_20)
target_compile_options(echo_uvexec PRIVATE ${UVEXEC_WARNINGS})
target_link_libraries(echo_uvexec PUBLIC uvexec::uvexec fmt::fmt)


include(CTest)

CPMAddPackage("gh:catchorg/Catch2@${UVEXEC_CATCH2_VERSION}")

add_library(uvexec_bench_common INTERFACE)
target_compile_features(uvexec_bench_common INTERFACE cxx_std_20)
target_link_libraries(uvexec_bench_common INTERFACE Catch2::Catch2)

add_executable(tcp_bench tcp_bench.cpp)
target_link_libraries(tcp_bench PRIVATE echo_uvexec echo_uv uvexec_bench_common)

add_executable(schedule_bench schedule_bench.cpp)
target_link_libraries(schedule_bench PRIVATE uvexec::uvexec uvexec_bench_common)


add_test(TcpBenchmark tcp_bench)
add_test(ScheduleBenchmark schedule_bench)


add_library(uvexec_example_common INTERFACE)
target_compile_features(uvexec_example_common INTERFACE cxx_std_20)
target_compile_options(uvexec_example_common INTERFACE ${UVEXEC_WARNINGS})
target_link_libraries(uvexec_example_common INTERFACE uvexec::uvexec fmt::fmt)

add_executable(echo_with_async_value echo_with_async_value.cpp)
target_link_libraries(echo_with_async_value PRIVATE uvexec_example_common)

add_executable(echo_uvexec_udp echo_uvexec_udp.cpp)
target_link_libraries(echo_uvexec_udp PRIVATE uvexec_example_common)
