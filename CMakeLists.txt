
cmake_minimum_required(VERSION 3.20)

project(MinecraftCopy LANGUAGES CXX)

set(MINECRAFTCOPY_VERSION "0.0.1-alpha" CACHE STRING "MinecraftCopy release version")
option(MINECRAFTCOPY_DEVELOPMENT_BUILD "Use the game executable beside the launcher when developing" ON)
if(NOT MINECRAFTCOPY_VERSION MATCHES "^(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)(-[0-9A-Za-z-]+(\\.[0-9A-Za-z-]+)*)?$")
    message(FATAL_ERROR "MINECRAFTCOPY_VERSION must use semantic version syntax, for example 0.0.1-alpha.")
endif()
if(MINECRAFTCOPY_VERSION MATCHES "-")
    string(REGEX REPLACE "^[^-]*-" "" _version_prerelease "${MINECRAFTCOPY_VERSION}")
    string(REPLACE "." ";" _version_prerelease_identifiers "${_version_prerelease}")
    foreach(_identifier IN LISTS _version_prerelease_identifiers)
        if(_identifier MATCHES "^[0-9]+$" AND _identifier MATCHES "^0[0-9]+$")
            message(FATAL_ERROR "Numeric semantic-version prerelease identifiers cannot contain leading zeroes.")
        endif()
    endforeach()
endif()

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/BuildVersion.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/BuildVersion.h"
    @ONLY
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "MSVC runtime library")
endif()

include(FetchContent)

if(WIN32)
    set(WEBVIEW2_SDK_VERSION "1.0.4314.35" CACHE STRING "Microsoft WebView2 SDK version")
    set(WEBVIEW2_SDK_SHA256 "90778deb1b77e36292828b4b5833c9255c27533f40e0ddd55891ae718944edc4" CACHE STRING "SHA-256 of the Microsoft WebView2 NuGet package")
    set(_webview2_package "${CMAKE_BINARY_DIR}/_deps/webview2-sdk-${WEBVIEW2_SDK_VERSION}.nupkg")
    set(_webview2_root "${CMAKE_BINARY_DIR}/_deps/webview2-sdk-${WEBVIEW2_SDK_VERSION}")

    if(EXISTS "${_webview2_package}")
        file(SHA256 "${_webview2_package}" _webview2_existing_hash)
        if(NOT _webview2_existing_hash STREQUAL WEBVIEW2_SDK_SHA256)
            file(REMOVE "${_webview2_package}")
        endif()
    endif()

    if(NOT EXISTS "${_webview2_package}")
        file(DOWNLOAD
            "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/${WEBVIEW2_SDK_VERSION}"
            "${_webview2_package}"
            EXPECTED_HASH "SHA256=${WEBVIEW2_SDK_SHA256}"
            TLS_VERIFY ON
            STATUS _webview2_download_status
            SHOW_PROGRESS
        )
        list(GET _webview2_download_status 0 _webview2_download_code)
        if(NOT _webview2_download_code EQUAL 0)
            file(REMOVE "${_webview2_package}")
            message(FATAL_ERROR "Could not download Microsoft.Web.WebView2 ${WEBVIEW2_SDK_VERSION}: ${_webview2_download_status}")
        endif()
    endif()

    if(NOT EXISTS "${_webview2_root}/build/native/include/WebView2.h")
        file(MAKE_DIRECTORY "${_webview2_root}")
        file(ARCHIVE_EXTRACT INPUT "${_webview2_package}" DESTINATION "${_webview2_root}")
    endif()

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        if(CMAKE_GENERATOR_PLATFORM MATCHES "ARM64|arm64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64")
            set(_webview2_arch arm64)
        else()
            set(_webview2_arch x64)
        endif()
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(_webview2_arch x86)
    else()
        message(FATAL_ERROR "Unsupported target architecture for WebView2: ${CMAKE_GENERATOR_PLATFORM}")
    endif()

    set(WEBVIEW2_INCLUDE_DIR "${_webview2_root}/build/native/include")
    set(WEBVIEW2_LOADER_LIBRARY "${_webview2_root}/build/native/${_webview2_arch}/WebView2LoaderStatic.lib")
    if(NOT EXISTS "${WEBVIEW2_INCLUDE_DIR}/WebView2.h" OR NOT EXISTS "${WEBVIEW2_LOADER_LIBRARY}")
        message(FATAL_ERROR "The WebView2 ${WEBVIEW2_SDK_VERSION} NuGet package has no SDK files for ${_webview2_arch}.")
    endif()

    set(_json_version "3.12.0")
    set(_json_header "${CMAKE_BINARY_DIR}/_deps/nlohmann-${_json_version}/json.hpp")
    set(_json_sha256 "aaf127c04cb31c406e5b04a63f1ae89369fccde6d8fa7cdda1ed4f32dfc5de63")
    if(EXISTS "${_json_header}")
        file(SHA256 "${_json_header}" _json_existing_hash)
        if(NOT _json_existing_hash STREQUAL _json_sha256)
            file(REMOVE "${_json_header}")
        endif()
    endif()
    if(NOT EXISTS "${_json_header}")
        get_filename_component(_json_include_dir "${_json_header}" DIRECTORY)
        file(MAKE_DIRECTORY "${_json_include_dir}")
        file(DOWNLOAD
            "https://github.com/nlohmann/json/releases/download/v${_json_version}/json.hpp"
            "${_json_header}"
            EXPECTED_HASH "SHA256=${_json_sha256}"
            TLS_VERIFY ON
            STATUS _json_download_status
        )
        list(GET _json_download_status 0 _json_download_code)
        if(NOT _json_download_code EQUAL 0)
            file(REMOVE "${_json_header}")
            message(FATAL_ERROR "Could not download nlohmann/json ${_json_version}: ${_json_download_status}")
        endif()
    endif()
    set(NLOHMANN_JSON_INCLUDE_DIR "${CMAKE_BINARY_DIR}/_deps/nlohmann-${_json_version}")
endif()

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(glm)

# GLFW: window creation and keyboard/mouse input.
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG 2c980bb59875b0d32144a71867fbdebb2f77cd20
)

FetchContent_MakeAvailable(stb)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.5.1
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(glfw)

# GLAD 2: load modern OpenGL functions.

FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG v2.0.8
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR cmake
)

FetchContent_MakeAvailable(glad)

glad_add_library(glad_gl_core_33
    STATIC
    REPRODUCIBLE
    API gl:core=3.3
)

find_package(OpenGL REQUIRED)

add_executable(MinecraftCopy
    src/main.cpp
    src/Player.cpp
)

if(WIN32)
    add_executable(MinecraftCopyLauncher WIN32
        launcher/src/main.cpp
        launcher/src/LauncherApp.cpp
        launcher/src/GameLauncher.cpp
        launcher/src/Versioning.cpp
        launcher/src/ReleaseManifest.cpp
        launcher/src/UpdateService.cpp
    )
    target_include_directories(MinecraftCopyLauncher PRIVATE
        launcher/src
        "${CMAKE_CURRENT_BINARY_DIR}/generated"
        "${WEBVIEW2_INCLUDE_DIR}"
        "${NLOHMANN_JSON_INCLUDE_DIR}"
    )
    target_link_libraries(MinecraftCopyLauncher PRIVATE
        "${WEBVIEW2_LOADER_LIBRARY}"
        ole32
        shlwapi
        winhttp
        bcrypt
    )
    target_compile_definitions(MinecraftCopyLauncher PRIVATE UNICODE _UNICODE)
    if(MINECRAFTCOPY_DEVELOPMENT_BUILD)
        target_compile_definitions(MinecraftCopyLauncher PRIVATE MINECRAFTCOPY_DEVELOPMENT_BUILD=1)
    endif()

    set_target_properties(MinecraftCopy MinecraftCopyLauncher PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
endif()

target_link_libraries(MinecraftCopy
    PRIVATE
        glfw
        glad_gl_core_33
        OpenGL::GL
        glm::glm
)

target_include_directories(MinecraftCopy PRIVATE
    ${stb_SOURCE_DIR}
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
)

add_custom_command(TARGET MinecraftCopy POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:MinecraftCopy>/assets"
)

if(WIN32)
    add_custom_command(TARGET MinecraftCopyLauncher POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/launcher/ui"
            "$<TARGET_FILE_DIR:MinecraftCopyLauncher>/launcher/ui"
    )
endif()

if(MSVC)
    target_compile_options(MinecraftCopy PRIVATE /W4)
    if(WIN32)
        target_compile_options(MinecraftCopyLauncher PRIVATE /W4)
    endif()
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(WorldTests tests/WorldTests.cpp)
    target_include_directories(WorldTests PRIVATE src)
    target_link_libraries(WorldTests PRIVATE glm::glm)
    add_test(NAME WorldTests COMMAND WorldTests)

    add_executable(PlayerTests
        tests/PlayerTests.cpp
        src/Player.cpp
    )
    target_include_directories(PlayerTests PRIVATE src)
    target_link_libraries(PlayerTests PRIVATE
        glfw
        glad_gl_core_33
        OpenGL::GL
        glm::glm
    )
    add_test(NAME PlayerTests COMMAND PlayerTests)

    add_executable(RaycastTests tests/RaycastTests.cpp)
    target_include_directories(RaycastTests PRIVATE src)
    target_link_libraries(RaycastTests PRIVATE glm::glm)
    add_test(NAME RaycastTests COMMAND RaycastTests)

    add_executable(VersioningTests
        tests/VersioningTests.cpp
        launcher/src/Versioning.cpp
    )
    target_include_directories(VersioningTests PRIVATE launcher/src)
    add_test(NAME VersioningTests COMMAND VersioningTests)

    if(WIN32)
        add_executable(ReleaseManifestTests
            tests/ReleaseManifestTests.cpp
            launcher/src/ReleaseManifest.cpp
            launcher/src/Versioning.cpp
        )
        target_include_directories(ReleaseManifestTests PRIVATE
            launcher/src
            "${NLOHMANN_JSON_INCLUDE_DIR}"
        )
        add_test(NAME ReleaseManifestTests COMMAND ReleaseManifestTests)

        add_executable(GameVersionTests
            tests/GameVersionTests.cpp
            launcher/src/GameLauncher.cpp
        )
        target_include_directories(GameVersionTests PRIVATE
            launcher/src
            "${CMAKE_CURRENT_BINARY_DIR}/generated"
            "${NLOHMANN_JSON_INCLUDE_DIR}"
        )
        add_dependencies(GameVersionTests MinecraftCopy)
        add_test(NAME GameVersionTests COMMAND GameVersionTests "$<TARGET_FILE:MinecraftCopy>")
    endif()
endif()
