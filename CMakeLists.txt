

cmake_minimum_required(VERSION 3.29)
project(servidor_sesiones)

set(CMAKE_CXX_STANDARD 20)
# Carpeta donde pusiste httplib.h y json.hpp
include_directories(${CMAKE_SOURCE_DIR}/external)
add_executable(servidor_sesiones
        main.cpp
        linearhash.h
)
# En Windows (MinGW / MSVC) hace falta winsock
if (WIN32)
    target_link_libraries(servidor_sesiones ws2_32)
endif()