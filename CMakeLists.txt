cmake_minimum_required(VERSION 3.31)
project(sw_renderer25)

set(CMAKE_CXX_STANDARD 23)

file(GLOB ENGINE_SRC
        src/*.cpp
        src/*.h
)

file(GLOB EXT_SRC
    src/external/*.c
    src/external/*.cpp

)



add_executable(app WIN32 ${ENGINE_SRC} ${EXT_SRC})
target_include_directories(app PUBLIC include)
target_compile_definitions(app PRIVATE UNICODE GLEW_STATIC)
target_link_libraries(app PUBLIC opengl32)
