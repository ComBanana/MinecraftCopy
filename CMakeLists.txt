
cmake_minimum_required(VERSION 3.20)

project(MinecraftCopy LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

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
    GIT_TAG master
    GIT_SHALLOW TRUE
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

target_link_libraries(MinecraftCopy
    PRIVATE
        glfw
        glad_gl_core_33
        OpenGL::GL
        glm::glm
)

target_include_directories(MinecraftCopy PRIVATE
    ${stb_SOURCE_DIR}
)

add_custom_command(TARGET MinecraftCopy POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:MinecraftCopy>/assets"
)

if(MSVC)
    target_compile_options(MinecraftCopy PRIVATE /W4)
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
endif()
