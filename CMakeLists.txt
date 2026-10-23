cmake_minimum_required(VERSION 3.16)
project(PrimalityTesting CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE) # timings are meaningless unoptimised
endif()

include(FetchContent)

# --- Algorithms: one module per primary author --------------------------
# Built as a library so the GUI, the tests and (later) the benchmark
# harness all run exactly the same code.
add_library(primality STATIC
    MillerRabinFixed/miller_rabin_fixed.cpp    # Atnm
    MillerRabinRandom/miller_rabin_random.cpp  # Dino
    common/aks_adapter.cpp                     # Finley's AKS/aks.cpp, via the shared interface
    common/trial_division.cpp                  # reference oracle
)
target_include_directories(primality PUBLIC common)

# Finley's standalone AKS programs, unchanged (each has its own main()).
add_executable(aks AKS/aks.cpp)
add_executable(aks_testing AKS/aks_testing.cpp)

# --- Tests ----------------------------------------------------------------
FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(tests
    tests/test_algorithms.cpp
    tests/test_number_input.cpp
    gui/number_input.cpp
)
target_include_directories(tests PRIVATE gui)
target_link_libraries(tests PRIVATE primality gtest_main)

# --- GUI ------------------------------------------------------------------
option(ISPRIME_BUILD_GUI "Build the ImGui front end" ON)
if(ISPRIME_BUILD_GUI)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE) # X11 backend only on Linux
    FetchContent_Declare(glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.4)
    FetchContent_MakeAvailable(glfw)

    # ImGui and tinyfiledialogs ship no usable CMakeLists; compile their sources directly.
    FetchContent_Declare(imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG        v1.90.9)
    FetchContent_MakeAvailable(imgui)

    FetchContent_Declare(json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3)
    FetchContent_MakeAvailable(json)

    FetchContent_Declare(tinyfiledialogs
        GIT_REPOSITORY https://github.com/native-toolkit/tinyfiledialogs.git
        GIT_TAG        master)
    FetchContent_MakeAvailable(tinyfiledialogs)

    find_package(OpenGL REQUIRED)
    find_package(Threads REQUIRED)

    add_executable(isPrime
        gui/main.cpp
        gui/app.cpp
        gui/runner.cpp
        gui/number_input.cpp
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
        ${tinyfiledialogs_SOURCE_DIR}/tinyfiledialogs.c
    )
    target_include_directories(isPrime PRIVATE
        gui
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
        ${imgui_SOURCE_DIR}/misc/cpp
        ${tinyfiledialogs_SOURCE_DIR})
    target_link_libraries(isPrime PRIVATE primality glfw OpenGL::GL nlohmann_json::nlohmann_json Threads::Threads)

    # Windows (MinGW): bundle the C++ runtime into the .exe so it runs on a
    # machine without MinGW, and use the GUI subsystem (no console window)
    # outside Debug builds.
    if(WIN32 AND MINGW)
        target_link_options(isPrime PRIVATE -static $<$<NOT:$<CONFIG:Debug>>:-mwindows>)
    endif()
endif()
