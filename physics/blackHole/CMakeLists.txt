cmake_minimum_required(VERSION 3.20)

project(blackHole LANGUAGES CXX)

# O executavel e gerado na pasta do projeto (mesmo caminho usado na documentacao)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
foreach(_cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_cfg} "${CMAKE_CURRENT_SOURCE_DIR}")
endforeach()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Em geradores single-config (Ninja/MinGW Makefiles) compila em Release (-O3) por padrao
get_property(_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT _multi_config AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Tipo de build (Debug/Release)" FORCE)
endif()

add_executable(blackhole
    main.cpp
    window.cpp
    camera.cpp
    render.cpp
    window.h
    camera.h
    render.h
)

target_link_libraries(blackhole PRIVATE opengl32 gdi32 glu32)
