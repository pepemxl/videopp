cmake_minimum_required(VERSION 3.16)

if(NOT PROJECT_NAME OR NOT "${PROJECT_NAME}" STREQUAL "video_processor")
    project(player_highlighter LANGUAGES CXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT OpenCV_FOUND)
    find_package(OpenCV REQUIRED)
endif()

# DepthEstimator depends on opencv_dnn. The class is always compiled — at
# runtime, depth is gated by the config flag, so a non-dnn build would
# only fail when the user actually enables depth. We do still require dnn
# at link time because DepthEstimator's translation unit references it.
set(_HAVE_DNN FALSE)
foreach(_lib IN LISTS OpenCV_LIBS)
    if("${_lib}" MATCHES "opencv_dnn")
        set(_HAVE_DNN TRUE)
        break()
    endif()
endforeach()
if(NOT _HAVE_DNN AND TARGET opencv_dnn)
    set(_HAVE_DNN TRUE)
endif()
if(NOT _HAVE_DNN)
    message(FATAL_ERROR
        "player_highlighter: opencv_dnn is required (the optional depth path "
        "uses cv::dnn). Add 'dnn' to the OpenCV features in vcpkg.json and "
        "re-run vcpkg install for your triplet.")
endif()

set(_PH_SOURCES
    main.cpp
    Config.h
    Config.cpp
    Skeleton.h
    PlayerCsvLoader.h
    PlayerCsvLoader.cpp
    Segmenter.h
    Segmenter.cpp
    DepthEstimator.h
    DepthEstimator.cpp
    ZoomTracker.h
    ZoomTracker.cpp
    Highlighter.h
    Highlighter.cpp
)

add_executable(player_highlighter ${_PH_SOURCES})

target_include_directories(player_highlighter PRIVATE
    ${OpenCV_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(player_highlighter PRIVATE ${OpenCV_LIBS})

if(MSVC)
    target_compile_options(player_highlighter PRIVATE /W4 /permissive-)
else()
    target_compile_options(player_highlighter PRIVATE -Wall -Wextra -Wpedantic)
endif()
