cmake_minimum_required(VERSION 3.16)
project(video_processor)

set(CMAKE_CXX_STANDARD 17)

find_package(OpenCV REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(video_processor
    main.cpp
    MainWindow.cpp
    VideoProcessor.cpp)

target_link_libraries(video_processor
    Qt6::Widgets
    ${OpenCV_LIBS})