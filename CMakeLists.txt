cmake_minimum_required(VERSION 3.23)

project(QtCameraSimple VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets)

qt_add_executable(QtCameraSimple
    main.cpp
    irsensor.cpp
    irsensor.h
    gui.cpp
    gui.h
    data_processing.cpp
    data_processing.h
)

target_link_libraries(QtCameraSimple PRIVATE Qt6::Widgets)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(QtCameraSimple PRIVATE -Wall -Wextra -Wpedantic)
endif()
