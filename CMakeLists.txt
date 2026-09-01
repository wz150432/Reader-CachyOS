cmake_minimum_required(VERSION 3.21)
project(ReaderCachyOS VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Test)
qt_standard_project_setup()

qt_add_executable(reader
    src/main.cpp
    src/app/MainWindow.cpp src/app/MainWindow.h
)
target_include_directories(reader PRIVATE src)
target_link_libraries(reader PRIVATE Qt6::Widgets)
