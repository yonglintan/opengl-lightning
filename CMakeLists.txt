cmake_minimum_required(VERSION 3.10)

project(GraphicsProj)

set(CMAKE_CXX_STANDARD 17)

find_package(GLFW3 REQUIRED)
find_package(glm REQUIRED)

add_library(glad src/glad.c)

file(GLOB PROJECT_SOURCES "src/*.cpp")

include_directories(include ${GLFW3_INCLUDE_DIRS} ${GLM_INCLUDE_DIRS} src/imgui.cpp 
    src/imgui_draw.cpp 
    src/imgui_widgets.cpp 
    src/imgui_impl_glfw.cpp 
    src/imgui_impl_opengl3.cpp)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

target_link_libraries(${PROJECT_NAME} glad glfw ${GLM_LIBRARIES})
