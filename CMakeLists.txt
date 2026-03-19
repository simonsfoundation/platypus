cmake_minimum_required(VERSION 3.16)

project(Platypus
VERSION 0.0.1
LANGUAGES CXX)

include(CTest)
include(GNUInstallDirs)

# Enable automatic processing of Qt's MOC, UIC, and RCC tools
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_VERBOSE_MAKEFILE YES)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
# Specify C++ standard
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Configuration Options
option(BUILD_PHOTOSHOP_PLUGIN "Build the Photoshop plugin" OFF)
option(BUILD_APPLE_APP "Build the Apple App instead of an executable" OFF)
set(PLATYPUS_OPENCV_COMPONENTS core imgproc imgcodecs flann)
option(OpenCV_DIR "Path to OpenCV if not found automatically" "")
set(PLATYPUS_PACKAGE_VERSION_OVERRIDE "" CACHE STRING
    "Override the version string used for packaged release artifacts")

if (PLATYPUS_PACKAGE_VERSION_OVERRIDE)
  set(PLATYPUS_PACKAGE_VERSION "${PLATYPUS_PACKAGE_VERSION_OVERRIDE}")
else()
  set(PLATYPUS_PACKAGE_VERSION "${PROJECT_VERSION}")
endif()

if (NOT WIN32)
  set(OpenCV_STATIC ON)
endif()

# Find the required Qt packages
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Concurrent)
if(NOT Qt6_FOUND)
    message(FATAL_ERROR
        "Qt6 not found!\n"
        "  Run ./configure.sh (macOS/Linux) or configure.bat (Windows) to set up dependencies via vcpkg.\n")
endif()

# Find OpenCV
if(OpenCV_DIR)
    find_package(OpenCV REQUIRED COMPONENTS ${PLATYPUS_OPENCV_COMPONENTS}
                 PATHS ${OpenCV_DIR} NO_DEFAULT_PATH)
else()
    find_package(OpenCV REQUIRED COMPONENTS ${PLATYPUS_OPENCV_COMPONENTS})
endif()
if(NOT OpenCV_FOUND)
    message(FATAL_ERROR
        "OpenCV not found!\n"
        "  Run ./configure.sh (macOS/Linux) or configure.bat (Windows) to set up dependencies automatically.\n"
        "  Or install manually:\n"
        "    macOS:  brew install opencv\n"
        "    Linux:  sudo apt-get install libopencv-dev\n"
        "    Windows: use vcpkg (see readme.md)\n")
endif()

set(PLATYPUS_USER_GUIDE_SOURCE
    "${CMAKE_CURRENT_SOURCE_DIR}/installer/payload/files/user_guide.pdf")
set(PLATYPUS_APP_ICON_SOURCE
    "${CMAKE_CURRENT_SOURCE_DIR}/mac/AppIcon.icns")

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

file(GLOB GUI_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)
list(REMOVE_ITEM GUI_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

add_library(platypus_gui STATIC ${GUI_SOURCES} src/platypus.qrc)
target_include_directories(platypus_gui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(platypus_gui PUBLIC
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent ${OpenCV_LIBS} platypus)

# Add the executable
if (BUILD_APPLE_APP)
  set(MACOSX_BUNDLE TRUE)
  add_executable(PlatypusGui MACOSX_BUNDLE src/main.cpp)
  target_sources(PlatypusGui PRIVATE
      "${PLATYPUS_USER_GUIDE_SOURCE}"
      "${PLATYPUS_APP_ICON_SOURCE}")
  set_source_files_properties(
      "${PLATYPUS_USER_GUIDE_SOURCE}"
      "${PLATYPUS_APP_ICON_SOURCE}"
      PROPERTIES
      MACOSX_PACKAGE_LOCATION "Resources")
  set_target_properties(PlatypusGui PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "platypus"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.simonsfoundation.platypus"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PLATYPUS_PACKAGE_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PLATYPUS_PACKAGE_VERSION}"
    MACOSX_RPATH TRUE
    MACOSX_FRAMEWORK_IDENTIFIER "com.simonsfoundation.platypus"
    XCODE_ATTRIBUTE_LD_RUNPATH_SEARCH_PATHS "@loader_path/Libraries"
    XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME YES
    MACOSX_BUNDLE_ICON_FILE "AppIcon.icns"
  )
elseif(WIN32)
  add_executable(PlatypusGui WIN32 src/main.cpp)
  target_sources(PlatypusGui PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/platypus.rc")
else()
  add_executable(PlatypusGui src/main.cpp)
endif()

include(CheckIPOSupported)
check_ipo_supported(RESULT supported OUTPUT error)

if( supported )
    message(STATUS "IPO / LTO enabled")
    set_property(TARGET PlatypusGui PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
    message(STATUS "IPO / LTO not supported: <${error}>")
endif()

target_link_libraries(PlatypusGui PRIVATE platypus_gui)
target_compile_definitions(PlatypusGui PRIVATE
    PLATYPUS_APP_VERSION="${PLATYPUS_PACKAGE_VERSION}")

install(TARGETS PlatypusGui
    BUNDLE DESTINATION .
    RUNTIME DESTINATION .)

if (NOT BUILD_APPLE_APP)
  install(FILES
      "${CMAKE_CURRENT_BINARY_DIR}/user_guide.pdf"
      DESTINATION .)
endif()

# Only find and link xcb on Linux
if(UNIX AND NOT APPLE)
  find_package(xcb REQUIRED)
  target_link_libraries(PlatypusGui PRIVATE xcb)
endif()

if (NOT BUILD_APPLE_APP)
  configure_file("${PLATYPUS_USER_GUIDE_SOURCE}"
                 "${CMAKE_CURRENT_BINARY_DIR}/user_guide.pdf" COPYONLY)
endif()

if (APPLE AND BUILD_APPLE_APP)
  if (NOT MACDEPLOYQT_EXECUTABLE)
    find_program(MACDEPLOYQT_EXECUTABLE
        NAMES macdeployqt
        HINTS
          "${Qt6_DIR}/../../../bin"
          "${Qt6_DIR}/../../bin"
          "${CMAKE_PREFIX_PATH}/bin")
  endif()
  if (NOT MACDEPLOYQT_EXECUTABLE)
    message(FATAL_ERROR
        "macdeployqt not found.\n"
        "  Ensure Qt is installed and macdeployqt is on PATH, or pass -DMACDEPLOYQT_EXECUTABLE=/path/to/macdeployqt.")
  endif()

  set(PLATYPUS_BUNDLE_FIXUP_DIRS "")
  foreach(prefix IN LISTS CMAKE_PREFIX_PATH)
    if (EXISTS "${prefix}/lib")
      list(APPEND PLATYPUS_BUNDLE_FIXUP_DIRS "${prefix}/lib")
    endif()
  endforeach()

  if (OpenCV_DIR)
    get_filename_component(_platypus_opencv_prefix "${OpenCV_DIR}/../../.." ABSOLUTE)
    if (EXISTS "${_platypus_opencv_prefix}/lib")
      list(APPEND PLATYPUS_BUNDLE_FIXUP_DIRS "${_platypus_opencv_prefix}/lib")
    endif()
  endif()

  list(REMOVE_DUPLICATES PLATYPUS_BUNDLE_FIXUP_DIRS)
  get_filename_component(_platypus_qt_host_prefix
                         "${MACDEPLOYQT_EXECUTABLE}/../../../.." ABSOLUTE)
  if (EXISTS "${_platypus_qt_host_prefix}/lib")
    list(APPEND PLATYPUS_BUNDLE_FIXUP_DIRS "${_platypus_qt_host_prefix}/lib")
  endif()
  if (EXISTS "${_platypus_qt_host_prefix}/opt")
    file(GLOB _platypus_homebrew_opt_lib_dirs LIST_DIRECTORIES TRUE
         "${_platypus_qt_host_prefix}/opt/*/lib")
    list(APPEND PLATYPUS_BUNDLE_FIXUP_DIRS ${_platypus_homebrew_opt_lib_dirs})
    file(GLOB _platypus_homebrew_gcc_lib_dirs LIST_DIRECTORIES TRUE
         "${_platypus_qt_host_prefix}/opt/gcc/lib/gcc/*")
    list(APPEND PLATYPUS_BUNDLE_FIXUP_DIRS ${_platypus_homebrew_gcc_lib_dirs})
  endif()
  list(REMOVE_DUPLICATES PLATYPUS_BUNDLE_FIXUP_DIRS)
  list(JOIN PLATYPUS_BUNDLE_FIXUP_DIRS ";" PLATYPUS_BUNDLE_FIXUP_DIRS_JOINED)

  set(PLATYPUS_QT_PLUGIN_DIR "${_platypus_qt_host_prefix}/share/qt/plugins")
  set(PLATYPUS_QT_PLUGIN_FILES
      "platforms/libqcocoa.dylib"
      "imageformats/libqgif.dylib"
      "imageformats/libqico.dylib"
      "imageformats/libqjpeg.dylib"
      "imageformats/libqtiff.dylib"
      "imageformats/libqwebp.dylib")
  list(JOIN PLATYPUS_QT_PLUGIN_FILES ";" PLATYPUS_QT_PLUGIN_FILES_JOINED)

  configure_file(
      "${CMAKE_CURRENT_SOURCE_DIR}/cmake/PlatypusDeployBundle.cmake.in"
      "${CMAKE_CURRENT_BINARY_DIR}/PlatypusDeployBundle.cmake"
      @ONLY)
  install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/PlatypusDeployBundle.cmake")

  configure_file(
      "${CMAKE_CURRENT_SOURCE_DIR}/cmake/PlatypusFixBundle.cmake.in"
      "${CMAKE_CURRENT_BINARY_DIR}/PlatypusFixBundle.cmake"
      @ONLY)
  install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/PlatypusFixBundle.cmake")
endif()

if (WIN32 AND NOT BUILD_PHOTOSHOP_PLUGIN)
  set(CPACK_GENERATOR "NSIS")
  set(CPACK_MONOLITHIC_INSTALL ON)
  set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
  set(CPACK_PACKAGE_NAME "Platypus")
  set(CPACK_PACKAGE_VENDOR "Simons Foundation")
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
      "Platypus cradle removal tool for X-ray images of paintings on panel")
  set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/simonsfoundation/platypus")
  set(CPACK_PACKAGE_INSTALL_DIRECTORY "Platypus")
  set(CPACK_PACKAGE_EXECUTABLES "PlatypusGui" "Platypus")
  set(CPACK_PACKAGE_FILE_NAME
      "Platypus-${PLATYPUS_PACKAGE_VERSION}-windows-x64-setup")
  set(CPACK_PACKAGE_ICON
      "${CMAKE_CURRENT_SOURCE_DIR}/installer/payload/resources/icon.bmp")
  set(CPACK_RESOURCE_FILE_LICENSE
      "${CMAKE_CURRENT_SOURCE_DIR}/installer/payload/resources/license_agreement.txt")
  set(CPACK_NSIS_INSTALL_ROOT "$LOCALAPPDATA\\Programs")
  set(CPACK_NSIS_PACKAGE_NAME "Platypus")
  set(CPACK_NSIS_DISPLAY_NAME "Platypus")
  set(CPACK_NSIS_INSTALLED_ICON_NAME "PlatypusGui.exe")
  set(CPACK_NSIS_EXECUTABLES_DIRECTORY ".")
  set(CPACK_NSIS_MUI_ICON
      "${CMAKE_CURRENT_SOURCE_DIR}/installer/payload/resources/icon.ico")
  set(CPACK_NSIS_MUI_UNIICON
      "${CMAKE_CURRENT_SOURCE_DIR}/installer/payload/resources/icon.ico")
  set(CPACK_NSIS_MUI_FINISHPAGE_RUN "PlatypusGui.exe")
  set(CPACK_NSIS_MENU_LINKS "user_guide.pdf" "Platypus User Guide")
  set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
  set(CPACK_NSIS_MODIFY_PATH OFF)
  set(CPACK_NSIS_HELP_LINK "https://github.com/simonsfoundation/platypus")
  set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/simonsfoundation/platypus")
  set(CPACK_NSIS_UNINSTALL_NAME "Uninstall Platypus")
  set(CPACK_NSIS_BRANDING_TEXT "Platypus")
  include(CPack)
endif()

if (BUILD_TESTING)
  include(FetchContent)
  FetchContent_Declare(
      googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
  include(GoogleTest)

  add_executable(platypus_backend_tests
      tests/backend_tests.cpp
      tests/test_helpers.cpp
  )
  target_include_directories(platypus_backend_tests PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/tests
  )
  target_compile_definitions(platypus_backend_tests PRIVATE
      PLATYPUS_TEST_IMAGE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/images"
  )
  target_link_libraries(platypus_backend_tests PRIVATE
      GTest::gtest_main
      Qt6::Core
      ${OpenCV_LIBS}
      platypus
  )
  gtest_discover_tests(platypus_backend_tests)

  add_executable(platypus_qt_tests
      tests/qt_test_main.cpp
      tests/qt_tests.cpp
      tests/test_helpers.cpp
  )
  target_include_directories(platypus_qt_tests PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/tests
  )
  target_compile_definitions(platypus_qt_tests PRIVATE
      PLATYPUS_TEST_IMAGE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/images"
  )
  target_link_libraries(platypus_qt_tests PRIVATE
      GTest::gtest
      Qt6::Core Qt6::Gui Qt6::Widgets
      ${OpenCV_LIBS}
      platypus_gui
  )
  gtest_discover_tests(platypus_qt_tests
      PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
  )
endif()

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
