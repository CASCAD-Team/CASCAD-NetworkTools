cmake_minimum_required(VERSION 3.16)
project(CASCAD-NetworkTools)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)
find_package(CURL REQUIRED)

add_executable(CASCAD-NetworkTools
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
)

target_link_libraries(CASCAD-NetworkTools
    Qt6::Widgets
    Qt6::Network
    CURL::libcurl
)
