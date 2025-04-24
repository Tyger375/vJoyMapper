cmake_minimum_required(VERSION 3.10)
project(ControlMapper CXX)

set(CMAKE_CXX_STANDARD 17)

# vJoy
include_directories(${PROJECT_SOURCE_DIR}/include)
link_directories(${PROJECT_SOURCE_DIR}/lib)

# ImGui & GLFW
set(IMGUI_DIR C:/imgui)
set(GLFW_ROOT C:/glfw-3.4)

set(IMGUI_SOURCES
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
        ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
include_directories(${IMGUI_DIR} ${IMGUI_DIR}/backends ${GLFW_ROOT}/include)
set(GLFW_LIBRARY ${GLFW_ROOT}/build/src/Release/glfw3dll.lib)
find_package(OpenGL REQUIRED)

add_executable(ControlMapper
    ${IMGUI_SOURCES}
    main.cpp
)

target_link_libraries(ControlMapper
    vJoyInterface
    ${GLFW_LIBRARY}
    OpenGL::GL
)