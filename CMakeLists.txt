cmake_minimum_required(VERSION 3.20)
project(realtime-sdf-renderer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(glfw3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

add_executable(realtime-sdf-renderer
    src/main.cpp
    src/Shader.cpp
)

target_include_directories(realtime-sdf-renderer PRIVATE src)
target_link_libraries(realtime-sdf-renderer PRIVATE glfw glad::glad imgui::imgui)

add_custom_command(TARGET realtime-sdf-renderer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:realtime-sdf-renderer>/shaders
)
