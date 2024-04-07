CPMAddPackage("gh:fmtlib/fmt#10.2.1")

add_executable(sandbox main.cpp)
target_compile_features(sandbox PRIVATE cxx_std_20)
target_compile_options(sandbox PRIVATE ${UVEXEC_WARNINGS})
target_link_libraries(sandbox PRIVATE uvexec_impl fmt::fmt)
