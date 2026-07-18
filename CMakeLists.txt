cmake_minimum_required(VERSION 3.10)
project(Voxel-Engine-OpenGL)

set(CMAKE_CXX_STANDARD 17)

add_executable(Voxel-Engine-OpenGL
    Source/Engine/engine.cpp
    Source/World/world.cpp
    Source/Render/render.cpp
    Source/Physics/physics.cpp
    Source/Game/game.cpp
    External/glad/src/glad.c
    Source/main.cpp
)

include_directories(
    External/glad/include
    External/glfw/include
    External/glm
    External
    Source
)

add_subdirectory(External/glfw)

target_link_libraries(Voxel-Engine-OpenGL
    glfw
    opengl32
)