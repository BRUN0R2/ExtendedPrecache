cmake_minimum_required(VERSION 3.31)
project(ExtendedPrecache VERSION 0.1.0 LANGUAGES CXX)

option(extendedPrecacheBuildDll "Build the Windows x86 diagnostic DLL" ON)
option(extendedPrecacheEnableAnalysis "Run clang-tidy while compiling" OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
configure_file(cmake/projectVersion.hpp.in
    generated/extendedPrecache/projectVersion.hpp @ONLY)

include(cmake/projectOptions.cmake)
include(CheckCXXSourceCompiles)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
check_cxx_source_compiles("
    #include <expected>
    #include <version>
    #if !defined(__cpp_lib_expected) || __cpp_lib_expected < 202202L
    #error std::expected is required
    #endif
    int main() { std::expected<int, int> value{1}; return value.value() - 1; }
" extendedPrecacheHasExpected)
if(NOT extendedPrecacheHasExpected)
    message(FATAL_ERROR "A C++26 compiler and a standard library with std::expected are required")
endif()

add_library(extendedPrecacheDomain STATIC src/domain/resourcePath.cpp)
target_include_directories(extendedPrecacheDomain PUBLIC include)
target_compile_features(extendedPrecacheDomain PUBLIC cxx_std_26)
extendedPrecacheApplyOptions(extendedPrecacheDomain)

include(CTest)
if(BUILD_TESTING)
    add_executable(resourceCatalogTests tests/resourceCatalogTests.cpp)
    target_link_libraries(resourceCatalogTests PRIVATE extendedPrecacheDomain)
    extendedPrecacheApplyOptions(resourceCatalogTests)
    add_test(NAME resourceCatalog COMMAND resourceCatalogTests)
endif()

if(extendedPrecacheBuildDll)
    if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
        message(FATAL_ERROR "The engine DLL requires a Windows x86 toolchain; use extendedPrecacheBuildDll=OFF for core tests")
    endif()
    add_library(extendedPrecache SHARED
        src/platform/windows/extensionApi.cpp
        src/platform/windows/extendedPrecache.def
    )
    target_include_directories(extendedPrecache PUBLIC include
        PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
    extendedPrecacheApplyOptions(extendedPrecache)
    set_target_properties(extendedPrecache PROPERTIES PREFIX "")

    if(BUILD_TESTING)
        add_executable(extensionApiTests tests/extensionApiTests.cpp)
        target_include_directories(extensionApiTests PRIVATE include)
        target_compile_definitions(extensionApiTests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
        extendedPrecacheApplyOptions(extensionApiTests)
        add_dependencies(extensionApiTests extendedPrecache)
        add_test(NAME extensionApi COMMAND extensionApiTests $<TARGET_FILE:extendedPrecache>)
    endif()
endif()
