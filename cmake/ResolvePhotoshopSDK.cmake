include(FetchContent)

set(PHOTOSHOP_SDK_DIR "" CACHE PATH
    "Path to a local checkout of AdobeDocs/photoshop-cpp-sdk")
set(PLATYPUS_FETCH_PHOTOSHOP_SDK ON CACHE BOOL
    "Automatically fetch the Adobe Photoshop C++ SDK when building the legacy plugin")
set(PHOTOSHOP_SDK_GIT_REPOSITORY
    "https://github.com/AdobeDocs/photoshop-cpp-sdk.git" CACHE STRING
    "Git repository for the Adobe Photoshop C++ SDK")
set(PHOTOSHOP_SDK_GIT_TAG
    "7e9aca4353a3d4615e9522f47521bf840fb4388c" CACHE STRING
    "Pinned git revision for the Adobe Photoshop C++ SDK")

function(resolve_photoshop_sdk out_var)
  if(PHOTOSHOP_SDK_DIR)
    get_filename_component(_sdk_root "${PHOTOSHOP_SDK_DIR}" ABSOLUTE
                           BASE_DIR "${CMAKE_SOURCE_DIR}")
  elseif(PLATYPUS_FETCH_PHOTOSHOP_SDK)
    FetchContent_Declare(
      photoshop_cpp_sdk
      GIT_REPOSITORY "${PHOTOSHOP_SDK_GIT_REPOSITORY}"
      GIT_TAG "${PHOTOSHOP_SDK_GIT_TAG}"
      GIT_SHALLOW TRUE
    )
    FetchContent_GetProperties(photoshop_cpp_sdk)
    if(NOT photoshop_cpp_sdk_POPULATED)
      if(POLICY CMP0169)
        cmake_policy(PUSH)
        cmake_policy(SET CMP0169 OLD)
      endif()
      FetchContent_Populate(photoshop_cpp_sdk)
      if(POLICY CMP0169)
        cmake_policy(POP)
      endif()
    endif()
    set(_sdk_root "${photoshop_cpp_sdk_SOURCE_DIR}")
  else()
    message(FATAL_ERROR
      "BUILD_PHOTOSHOP_PLUGIN is ON, but no Photoshop SDK is available.\n"
      "  Set PHOTOSHOP_SDK_DIR to a local checkout of AdobeDocs/photoshop-cpp-sdk,\n"
      "  or leave PLATYPUS_FETCH_PHOTOSHOP_SDK=ON to fetch it automatically.")
  endif()

  set(_required_paths
    "pluginsdk/photoshopapi/photoshop/PIAbout.h"
    "pluginsdk/photoshopapi/pica_sp/SPSuites.h"
    "pluginsdk/samplecode/common/includes/PICA2PSErrorMap.h"
    "pluginsdk/samplecode/common/sources/PIDLLInstance.cpp"
  )

  foreach(_required_path IN LISTS _required_paths)
    if(NOT EXISTS "${_sdk_root}/${_required_path}")
      message(FATAL_ERROR
        "Resolved Photoshop SDK is missing '${_required_path}'.\n"
        "  Resolved root: ${_sdk_root}\n"
        "  Expected an AdobeDocs/photoshop-cpp-sdk checkout.")
    endif()
  endforeach()

  set(${out_var} "${_sdk_root}" PARENT_SCOPE)
endfunction()
