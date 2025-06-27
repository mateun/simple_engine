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
target_link_directories(app PUBLIC ${ASSIMP_LIB_DIR})
target_include_directories(app PUBLIC include ${ASSIMP_INCLUDE_DIR})
target_compile_definitions(app PRIVATE UNICODE GLEW_STATIC RENDERER_DX11)
target_link_libraries(app PUBLIC opengl32 d3d11 dxguid d3dcompiler assimp-vc143-mtd)

message(STATUS "assimp include path: " ${ASSIMP_INCLUDE_DIR})
message(STATUS "assimp library path: " ${ASSIMP_LIB_DIR})
message(STATUS "assimp dll path: " ${ASSIMP_DLL_DIR})


# Copy assets
file(GLOB ASSETS
        assets/*.spv
        assets/*.vert
        assets/*.frag
        assets/*.png
        assets/*.jpg
        assets/*.bmp
        assets/*.glb
        assets/*.fbx
        assets/*.obj
)
add_custom_command(TARGET app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:app>/assets
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ASSETS} $<TARGET_FILE_DIR:app>/assets
        COMMAND ${CMAKE_COMMAND} -E echo "Copied shaders and resources to output directory."
)

# Copy dlls
file(GLOB DLLS
    ${ASSIMP_DLL_DIR}/*.dll
)
add_custom_command(TARGET app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${DLLS} $<TARGET_FILE_DIR:app>
        COMMAND ${CMAKE_COMMAND} -E echo "Copied dlls to output directory."
)





