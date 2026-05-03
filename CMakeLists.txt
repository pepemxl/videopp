cmake_minimum_required(VERSION 3.16)
project(video_processor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# --- Qt (prefer Qt6, fall back to Qt5) ---
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

# --- OpenCV ---
# On Windows, set OpenCV_DIR via -DOpenCV_DIR=... or the OpenCV_DIR env var
# (e.g. C:/opencv/build, or the vcpkg toolchain handles it automatically).
if(DEFINED ENV{OpenCV_DIR} AND NOT DEFINED OpenCV_DIR)
    set(OpenCV_DIR "$ENV{OpenCV_DIR}" CACHE PATH "OpenCV installation directory")
endif()
find_package(OpenCV REQUIRED)

set(SOURCES
    main.cpp
    MainWindow.cpp
    MainWindow.h
    VideoProcessor.cpp
    VideoProcessor.h
)

# WIN32 makes the executable a GUI app on Windows (no console window);
# the flag is silently ignored on other platforms.
add_executable(video_processor WIN32 ${SOURCES})

target_include_directories(video_processor PRIVATE ${OpenCV_INCLUDE_DIRS})

target_link_libraries(video_processor PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    ${OpenCV_LIBS}
)

if(MSVC)
    target_compile_options(video_processor PRIVATE /W4 /permissive-)
else()
    target_compile_options(video_processor PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Copy required Qt runtime DLLs alongside the binary on Windows.
if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt
        HINTS "${Qt${QT_VERSION_MAJOR}_DIR}/../../../bin"
              "${_qt${QT_VERSION_MAJOR}_install_prefix}/bin"
    )
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET video_processor POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                    --no-translations
                    --no-system-d3d-compiler
                    --no-opengl-sw
                    "$<TARGET_FILE:video_processor>"
            COMMENT "Deploying Qt runtime DLLs"
        )
    endif()
endif()

# --- Standalone disc-trajectory subservice (no Qt dependency) ---
option(BUILD_DISC_TRACKER "Build the standalone disc-tracking subservice" ON)
if(BUILD_DISC_TRACKER)
    add_subdirectory(disc_tracker)
endif()
