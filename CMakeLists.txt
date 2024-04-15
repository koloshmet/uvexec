cmake_minimum_required(VERSION 3.22.1)
project(uvexec VERSION 0.1)

include(cmake/CPM.cmake)

# TODO: Find out the minimal possible version of libuv
set(UVEXEC_UV_VERSION 1.47.0 CACHE STRING "Version of libuv to use")
set(UVEXEC_STDEXEC_VERSION f9d9459 CACHE STRING "Version of stdexec to use")

set(UVEXEC_WARNINGS "" CACHE STRING "Warnings for uvexec targets in format appropriate for compiler")
set(UVEXEC_DEFINITIONS "" CACHE STRING "Additional compile definitions for uvexec targets")

add_subdirectory(src)
add_library(uvexec::uvexec ALIAS uvexec_impl)

option(UVEXEC_ENABLE_TESTS "Enable test targets" OFF)
option(UVEXEC_ENABLE_SANDBOX "Enable sandbox target" OFF)
option(UVEXEC_ENABLE_EXAMPLES "Enable example targets" OFF)

if (UVEXEC_ENABLE_TESTS OR UVEXEC_ENABLE_EXAMPLES)
    set(UVEXEC_CATCH2_VERSION 2.13.6 CACHE STRING "Version of Catch2 to use in tests")
endif ()

if (UVEXEC_ENABLE_TESTS AND BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif ()
if (UVEXEC_ENABLE_EXAMPLES)
    add_subdirectory(examples)
endif ()
if (UVEXEC_ENABLE_SANDBOX)
    add_subdirectory(sandbox)
endif ()

option(UVEXEC_ENABLE_SANITIZERS "Enable sanitizers for all targets" OFF)
if (UVEXEC_ENABLE_SANITIZERS)
    set(UVEXEC_SANITIZE -fsanitize=undefined CACHE STRING "Sanitizer in format -fsanitize=undefined")
    set_property(CACHE UVEXEC_SANITIZE PROPERTY STRINGS
            -fsanitize=undefined
            -fsanitize=address
            -fsanitize=thread
            -fsanitize=memory)

    add_compile_options(${UVEXEC_SANITIZE})
    add_link_options(${UVEXEC_SANITIZE})
endif ()
