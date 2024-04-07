include(CTest)

CPMAddPackage("gh:catchorg/Catch2@${UVEXEC_CATCH2_VERSION}")

add_library(uvexec_test_common INTERFACE)
target_compile_features(uvexec_test_common INTERFACE cxx_std_20)
target_link_libraries(uvexec_test_common INTERFACE uvexec_impl Catch2::Catch2)


add_executable(async_value_test async_value.cpp)
target_link_libraries(async_value_test PRIVATE uvexec_test_common)

add_executable(execution_test execution.cpp)
target_link_libraries(execution_test PRIVATE uvexec_test_common)

add_executable(time_test time.cpp)
target_link_libraries(time_test PRIVATE uvexec_test_common)

add_executable(signal_test signal.cpp)
target_link_libraries(signal_test PRIVATE uvexec_test_common)

add_executable(tcp_test tcp.cpp)
target_link_libraries(tcp_test PRIVATE uvexec_test_common)


add_test(AsyncValueTest async_value_test)
add_test(ExecutionTest execution_test)
add_test(TimeTest time_test)
add_test(SignalTest signal_test)
add_test(TcpTest tcp_test)
