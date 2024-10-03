cmake_minimum_required(VERSION 3.16)

project(Platypus LANGUAGES CXX)

# Enable automatic processing of Qt's MOC, UIC, and RCC tools
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_VERBOSE_MAKEFILE YES)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
# Specify C++ standard
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find the required Qt packages
find_package(Qt5 5 REQUIRED COMPONENTS Core Gui Widgets Xml)
find_package(xcb)
# Configuration Options
option(OpenCV_DIR "Path to OpenCV if not found automatically" "")

# Find OpenCV
find_package(OpenCV)

# If still not found, provide an error message
if(NOT OpenCV_FOUND AND NOT OpenCV_DIR)
    message(FATAL_ERROR "OpenCV not found! Please set OpenCV_DIR to the path where OpenCV is installed.")
endif()

# If not found, try using the user-specified path
find_package(OpenCV REQUIRED PATHS ${OpenCV_DIR} NO_DEFAULT_PATH)

include(FetchContent)

FetchContent_Declare(
  platypus
  GIT_REPOSITORY https://github.com/SteveBronder/platypus
  GIT_TAG main)
FetchContent_MakeAvailable(platypus)

# Include directories for OpenCV and other dependencies if necessary
include_directories(${OpenCV_INCLUDE_DIRS})

# Collect all source and header files
file(GLOB_RECURSE SOURCES
    src/*.cpp
    src/*.h
)

# Add the executable
add_executable(PlatypusGui ${SOURCES} src/platypus.qrc)

# Link against Qt and OpenCV libraries
target_link_libraries(PlatypusGui PRIVATE
  Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Xml ${OpenCV_LIBS} platypus xcb)

# Add include directories if needed
target_include_directories(PlatypusGui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR} # For generated files
)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/installer/payload/files/user_guide.pdf ${CMAKE_CURRENT_BINARY_DIR}/user_guide.pdf COPYONLY)

# Handle other dependencies like libjpeg, libtiff, and zlib if necessary
# find_package(JPEG REQUIRED)
# find_package(TIFF REQUIRED)
# find_package(ZLIB REQUIRED)
# target_link_libraries(Platypus PRIVATE ${JPEG_LIBRARIES} ${TIFF_LIBRARIES} ${ZLIB_LIBRARIES})
# target_include_directories(Platypus PRIVATE ${JPEG_INCLUDE_DIRS} ${TIFF_INCLUDE_DIRS} ${ZLIB_INCLUDE_DIRS})
