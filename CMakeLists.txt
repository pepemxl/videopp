cmake_minimum_required(VERSION 3.16)
project(video_processor)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(OpenCV REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(video_processor
    main.cpp
    MainWindow.cpp
    MainWindow.h
    VideoProcessor.cpp
    VideoProcessor.h)

target_link_libraries(video_processor
    Qt6::Widgets
    ${OpenCV_LIBS})