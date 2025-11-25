set(SOURCES
  MAIN.cpp
  VAO.cpp
  VBO.cpp
  EBO.cpp
  ShaderClass.cpp
)

add_executable(VoxelEngine ${SOURCES})

# glad
add_library(glad libs/glad/src/glad.c)
target_include_directories(glad PUBLIC libs/glad/include)

# include dirs
target_include_directories(VoxelEngine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    libs/glad/include
    libs/glm
)

# link libs
if (WIN32)
    target_link_libraries(VoxelEngine glad glfw3 opengl32)
else()
    target_link_libraries(VoxelEngine glad glfw GL)
endif()

