cmake_minimum_required(VERSION 3.16)

project(Platypus
VERSION 0.0.1
LANGUAGES CXX)

# Enable automatic processing of Qt's MOC, UIC, and RCC tools
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_VERBOSE_MAKEFILE YES)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
# Specify C++ standard
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (WIN32)
else()
  set(OpenCV_STATIC ON)
  set(QT_STATIC ON)
  add_definitions(-DQT_STATIC)
endif()
set(QT_PLUGINS "")
# Find the required Qt packages
find_package(Qt5 5 REQUIRED COMPONENTS Core Gui Widgets Xml)
# Configuration Options
option(BUILD_PHOTOSHOP_PLUGIN "Build the Photoshop plugin" OFF)
option(BUILD_APPLE_APP "Build the Apple App instead of an executable" OFF)
# Find OpenCV
find_package(OpenCV)
# If still not found, provide an error message
if(NOT OpenCV_FOUND AND NOT OpenCV_DIR)
    message(FATAL_ERROR "OpenCV not found! Please set OpenCV_DIR to the path where OpenCV is installed.")
endif()
option(OpenCV_DIR "Path to OpenCV if not found automatically" "")

# If not found, try using the user-specified path
find_package(OpenCV REQUIRED PATHS ${OpenCV_DIR} NO_DEFAULT_PATH)

include(FetchContent)

add_library(platypus STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platypus/CradleFunctions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platypus/DWT.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platypus/FDCT.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platypus/FFST.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platypus/HaarDWT.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platypus/MCA.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platypus/Shearlet.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platypus/TextureRemoval.cpp)

target_include_directories(platypus PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>  # This tells CMake where to find the headers when installed
    PRIVATE
    ${OpenCV_INCLUDE_DIRS})
target_link_libraries(platypus PRIVATE ${OpenCV_LIBS})

# Export the platypus target for use by other projects
export(TARGETS platypus FILE platypusTargets.cmake)

# Install the library and headers
install(TARGETS platypus
    EXPORT platypusTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION include)

# Install the export targets
install(EXPORT platypusTargets
    FILE platypusTargets.cmake
    NAMESPACE platypus::
    DESTINATION lib/cmake/platypus)

# Create a ConfigVersion.cmake file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/platypusConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion)

# Configure the Config.cmake file
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/platypusConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/platypusConfig.cmake"
    INSTALL_DESTINATION lib/cmake/platypus)

# Install the Config and ConfigVersion files
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/platypusConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/platypusConfigVersion.cmake"
    DESTINATION lib/cmake/platypus)

# Include directories for OpenCV and other dependencies if necessary
include_directories(${OpenCV_INCLUDE_DIRS})

# Collect all source and header files
file(GLOB_RECURSE SOURCES
    src/*.cpp
    src/*.h
)

# Add the executable
if (BUILD_APPLE_APP)
  set(MACOSX_BUNDLE TRUE)
  add_executable(PlatypusGui MACOSX_BUNDLE ${SOURCES} src/platypus.qrc)
  set_target_properties(PlatypusGui PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "platypus"
    MACOSX_RPATH TRUE
    MACOSX_FRAMEWORK_IDENTIFIER com.host.target
    XCODE_ATTRIBUTE_LD_RUNPATH_SEARCH_PATHS "@loader_path/Libraries"
    XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME YES
    MACOSX_BUNDLE_ICON_FILE "${CMAKE_CURRENT_SOURCE_DIR}/mac/AppIcon.icns"
  )
elseif(WIN32)
  add_executable(PlatypusGui WIN32 ${SOURCES} src/platypus.qrc)
else()
  add_executable(PlatypusGui ${SOURCES} src/platypus.qrc)
endif()

include(CheckIPOSupported)
check_ipo_supported(RESULT supported OUTPUT error)

if( supported )
    message(STATUS "IPO / LTO enabled")
    set_property(TARGET PlatypusGui PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
    message(STATUS "IPO / LTO not supported: <${error}>")
endif()

# Link against Qt and OpenCV libraries
target_link_libraries(PlatypusGui PRIVATE
  Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Xml ${OpenCV_LIBS} platypus)

# Only find and link xcb on Linux
if(UNIX AND NOT APPLE)
  find_package(xcb REQUIRED)
  target_link_libraries(PlatypusGui PRIVATE xcb)
endif()

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


if (BUILD_PHOTOSHOP_PLUGIN)
# Set up Photoshop SDK
set(PHOTOSHOP_SDK_DIR "/path/to/photoshop/sdk" CACHE PATH "Path to the Adobe Photoshop SDK")
if (NOT EXISTS "${PHOTOSHOP_SDK_DIR}")
    message(FATAL_ERROR "Please set PHOTOSHOP_SDK_DIR to the path of the Adobe Photoshop SDK.")
endif()

# Add Photoshop plugin source files
set(PLUGIN_SOURCES
    photoshop/driver.cpp
    photoshop/tiff.cpp
    photoshop/tiff.h
    # Add other necessary files
)

# Define the plugin target
add_library(PlatypusPlugin MODULE ${PLUGIN_SOURCES})

# Set the output name and properties
set_target_properties(PlatypusPlugin PROPERTIES
    OUTPUT_NAME "Platypus"
    PREFIX "" # Remove 'lib' prefix
)

# For Windows, set the extension to .8bf
if (WIN32)
    set_target_properties(PlatypusPlugin PROPERTIES SUFFIX ".8bf")
endif()

# Include directories for the Photoshop SDK
target_include_directories(PlatypusPlugin PRIVATE
    ${PHOTOSHOP_SDK_DIR}/interfaces
    ${PHOTOSHOP_SDK_DIR}/samples/common
    ${PHOTOSHOP_SDK_DIR}/samples/win
    ${PHOTOSHOP_SDK_DIR}/samples/mac
    # Add other necessary include directories
)

if (APPLE)
    # Paths to resource files
    set(RESOURCE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/photoshop/platypus.r")
    set(RESOURCE_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/platypus.rsrc")

    # Compile the resource file
    add_custom_command(
        OUTPUT "${RESOURCE_OUTPUT}"
        COMMAND Rez -d SystemSevenOrLater=1 -useDF "${RESOURCE_FILE}" -o "${RESOURCE_OUTPUT}"
        DEPENDS "${RESOURCE_FILE}"
        COMMENT "Compiling resource file ${RESOURCE_FILE}"
    )

    # Custom target for resource compilation
    add_custom_target(PlatypusResource ALL DEPENDS "${RESOURCE_OUTPUT}")

    # Ensure the plugin depends on the resource
    add_dependencies(PlatypusPlugin PlatypusResource)

    # Link the resource into the plugin
    set_target_properties(PlatypusPlugin PROPERTIES
        LINK_FLAGS "-sectcreate __TEXT __rsrc \"${RESOURCE_OUTPUT}\""
    )

    # Optionally, install the resource file into the plugin bundle
    install(FILES "${RESOURCE_OUTPUT}"
        DESTINATION "path/to/photoshop/plugins/Platypus.plugin/Contents/Resources"
    )
endif()

# Platform-specific include directories
if (WIN32)
    target_include_directories(PlatypusPlugin PRIVATE
        ${PHOTOSHOP_SDK_DIR}/samples/win
    )
else()
    target_include_directories(PlatypusPlugin PRIVATE
        ${PHOTOSHOP_SDK_DIR}/samples/mac
    )
endif()

# Define necessary preprocessor macros
target_compile_definitions(PlatypusPlugin PRIVATE
    # Add necessary definitions
)

# Link platform-specific libraries
if (WIN32)
    target_link_libraries(PlatypusPlugin PRIVATE
        # Add Windows-specific libraries
    )
else()
    target_link_libraries(PlatypusPlugin PRIVATE
        "-framework CoreFoundation"
        # Add other macOS-specific libraries
    )
endif()

# Set compiler flags
if (MSVC)
    target_compile_options(PlatypusPlugin PRIVATE
        /W3 /EHsc
    )
else()
    target_compile_options(PlatypusPlugin PRIVATE
        -Wall -Wextra -fPIC
    )
endif()

# Install the plugin
install(TARGETS PlatypusPlugin
    LIBRARY DESTINATION "path/to/photoshop/plugins" # Replace with actual path
    RUNTIME DESTINATION "path/to/photoshop/plugins" # For Windows
)
endif()
