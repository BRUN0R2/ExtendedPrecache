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

include(cmake/sessionDependencies.cmake)
add_library(extendedPrecacheSession STATIC
    src/session/sessionTypes.cpp
    src/session/capabilities.cpp
    src/session/configuration.cpp
    src/session/configurationJson.cpp
    src/session/frameCodec.cpp
    src/session/negotiation.cpp
    src/session/negotiationPayload.cpp
    src/session/negotiationTranscript.cpp
)
target_include_directories(extendedPrecacheSession PUBLIC include)
target_compile_features(extendedPrecacheSession PUBLIC cxx_std_26)
target_link_libraries(extendedPrecacheSession PRIVATE nlohmann_json::nlohmann_json)
extendedPrecacheApplyOptions(extendedPrecacheSession)

if(WIN32)
    add_library(extendedPrecacheCrypto STATIC src/platform/windows/cngDigest.cpp)
    target_include_directories(extendedPrecacheCrypto PUBLIC include)
    target_compile_features(extendedPrecacheCrypto PUBLIC cxx_std_26)
    target_compile_definitions(extendedPrecacheCrypto PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00)
    target_link_libraries(extendedPrecacheCrypto PRIVATE bcrypt)
    extendedPrecacheApplyOptions(extendedPrecacheCrypto)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(resourceCatalogTests tests/resourceCatalogTests.cpp)
    target_link_libraries(resourceCatalogTests PRIVATE extendedPrecacheDomain)
    extendedPrecacheApplyOptions(resourceCatalogTests)
    add_test(NAME resourceCatalog COMMAND resourceCatalogTests)

    add_executable(configurationTests tests/configurationTests.cpp)
    target_link_libraries(configurationTests PRIVATE extendedPrecacheSession nlohmann_json::nlohmann_json)
    extendedPrecacheApplyOptions(configurationTests)
    add_test(NAME configuration COMMAND configurationTests "${CMAKE_CURRENT_SOURCE_DIR}/docs" "${CMAKE_CURRENT_BINARY_DIR}/configurationFixtures")
    add_executable(integrationContractTests tests/integrationContractTests.cpp)
    target_include_directories(integrationContractTests PRIVATE docs/contracts)
    target_link_libraries(integrationContractTests PRIVATE extendedPrecacheSession nlohmann_json::nlohmann_json)
    extendedPrecacheApplyOptions(integrationContractTests)
    add_test(NAME integrationContract COMMAND integrationContractTests "${CMAKE_CURRENT_SOURCE_DIR}/docs")
    if(WIN32)
        add_executable(frameCodecTests tests/frameCodecTests.cpp)
        target_link_libraries(frameCodecTests PRIVATE extendedPrecacheSession extendedPrecacheCrypto nlohmann_json::nlohmann_json)
        extendedPrecacheApplyOptions(frameCodecTests)
        add_test(NAME frameCodec COMMAND frameCodecTests "${CMAKE_CURRENT_SOURCE_DIR}/docs")
        add_executable(negotiationTests tests/negotiationTests.cpp)
        target_link_libraries(negotiationTests PRIVATE extendedPrecacheSession extendedPrecacheCrypto nlohmann_json::nlohmann_json)
        extendedPrecacheApplyOptions(negotiationTests)
        add_test(NAME negotiation COMMAND negotiationTests "${CMAKE_CURRENT_SOURCE_DIR}/docs")
    endif()
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
