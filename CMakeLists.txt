cmake_minimum_required(VERSION 3.16)
project(ArknightsAutoBot)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(PROJECT_ROOT_DIR "${CMAKE_SOURCE_DIR}")

find_package(CUDAToolkit QUIET)

include(FetchContent)

# ============================================================
# ONNX Runtime
# ============================================================
option(ONNXRUNTIME_USE_GPU "Try to use ONNX Runtime GPU package when CUDA/cuDNN are available" ON)
set(ONNXRUNTIME_VERSION "" CACHE STRING "Override ONNX Runtime version. Leave empty to auto-select.")
set(ONNXRUNTIME_PACKAGE_SUFFIX "" CACHE STRING "Override ONNX Runtime package suffix, for example -gpu or -gpu_cuda13. Leave empty to auto-select.")

function(find_cudnn_runtime)
    find_file(CUDNN9_DLL cudnn64_9.dll
        HINTS
            ENV PATH
            "${CUDAToolkit_BIN_DIR}"
            "${CUDAToolkit_ROOT_DIR}/bin"
        NO_DEFAULT_PATH)

    find_file(CUDNN8_DLL cudnn64_8.dll
        HINTS
            ENV PATH
            "${CUDAToolkit_BIN_DIR}"
            "${CUDAToolkit_ROOT_DIR}/bin"
        NO_DEFAULT_PATH)

    set(CUDNN9_DLL "${CUDNN9_DLL}" PARENT_SCOPE)
    set(CUDNN8_DLL "${CUDNN8_DLL}" PARENT_SCOPE)
endfunction()

function(select_onnxruntime_package)
    set(selected_version "")
    set(selected_suffix "")
    set(use_cuda_package OFF)

    if(ONNXRUNTIME_USE_GPU AND CUDAToolkit_FOUND)
        find_cudnn_runtime()

        if(CUDAToolkit_VERSION_MAJOR EQUAL 13 AND CUDNN9_DLL)
            set(selected_version "1.24.4")
            set(selected_suffix "-gpu_cuda13")
            set(use_cuda_package ON)
            message(STATUS "[OnnxRuntime] CUDA 13 + cuDNN 9 detected")
        elseif(CUDAToolkit_VERSION_MAJOR EQUAL 12 AND CUDNN8_DLL)
            set(selected_version "1.17.1")
            set(selected_suffix "-gpu")
            set(use_cuda_package ON)
            message(STATUS "[OnnxRuntime] CUDA 12 + cuDNN 8 detected")
        else()
            message(STATUS "[OnnxRuntime] CUDA found, but no supported CUDA/cuDNN pair was detected; using CPU package")
        endif()
    endif()

    if(ONNXRUNTIME_VERSION)
        set(selected_version "${ONNXRUNTIME_VERSION}")
    elseif(NOT selected_version)
        set(selected_version "1.17.1")
    endif()

    if(NOT "${ONNXRUNTIME_PACKAGE_SUFFIX}" STREQUAL "")
        set(selected_suffix "${ONNXRUNTIME_PACKAGE_SUFFIX}")
        set(use_cuda_package ON)
    endif()

    set(ONNXRUNTIME_VERSION "${selected_version}" PARENT_SCOPE)
    set(ONNXRUNTIME_SELECTED_SUFFIX "${selected_suffix}" PARENT_SCOPE)
    set(ONNXRUNTIME_USE_CUDA_PACKAGE "${use_cuda_package}" PARENT_SCOPE)
    set(CUDNN9_DLL "${CUDNN9_DLL}" PARENT_SCOPE)
    set(CUDNN8_DLL "${CUDNN8_DLL}" PARENT_SCOPE)
endfunction()

function(resolve_onnxruntime_platform)
    if(WIN32)
        set(platform "win-x64")
        set(archive_ext "zip")
    elseif(APPLE)
        set(platform "osx-${CMAKE_SYSTEM_PROCESSOR}")
        set(archive_ext "tgz")
    else()
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
            set(platform "linux-aarch64")
        else()
            set(platform "linux-x64")
        endif()
        set(archive_ext "tgz")
    endif()

    set(ONNXRUNTIME_PLATFORM "${platform}" PARENT_SCOPE)
    set(ONNXRUNTIME_ARCHIVE_EXT "${archive_ext}" PARENT_SCOPE)
endfunction()

select_onnxruntime_package()
resolve_onnxruntime_platform()

if(EXISTS "${CMAKE_SOURCE_DIR}/onnxruntime/lib" AND
   EXISTS "${CMAKE_SOURCE_DIR}/onnxruntime/include")
    message(STATUS "[OnnxRuntime] Using local directory: ${CMAKE_SOURCE_DIR}/onnxruntime")
    set(ONNXRUNTIME_DIR "${CMAKE_SOURCE_DIR}/onnxruntime")
else()
    set(ONNXRUNTIME_PACKAGE "onnxruntime-${ONNXRUNTIME_PLATFORM}${ONNXRUNTIME_SELECTED_SUFFIX}-${ONNXRUNTIME_VERSION}")
    FetchContent_Declare(onnxruntime
        URL "https://github.com/microsoft/onnxruntime/releases/download/v${ONNXRUNTIME_VERSION}/${ONNXRUNTIME_PACKAGE}.${ONNXRUNTIME_ARCHIVE_EXT}"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_SUBDIR "DO_NOT_BUILD")
    FetchContent_MakeAvailable(onnxruntime)
    set(ONNXRUNTIME_DIR "${onnxruntime_SOURCE_DIR}")
    message(STATUS "[OnnxRuntime] Downloaded to: ${ONNXRUNTIME_DIR}")
endif()

message(STATUS "[OnnxRuntime] Version: ${ONNXRUNTIME_VERSION}")
message(STATUS "[OnnxRuntime] Package suffix: ${ONNXRUNTIME_SELECTED_SUFFIX}")

# ============================================================
# OCR models
# ============================================================
set(MODELS_DOWNLOAD_URL "" CACHE STRING "OCR models archive URL, for example models.tar.gz")

if(EXISTS "${CMAKE_SOURCE_DIR}/models/onnx/ch_ppocr_det.onnx")
    message(STATUS "[Models] Using local models")
elseif(NOT MODELS_DOWNLOAD_URL)
    message(FATAL_ERROR "[Models] Local models were not found. Set -DMODELS_DOWNLOAD_URL=<url> or put models under models/onnx.")
else()
    FetchContent_Declare(ocr_models
        URL "${MODELS_DOWNLOAD_URL}"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_DIR "${CMAKE_SOURCE_DIR}/models"
        SOURCE_SUBDIR "DO_NOT_BUILD")
    FetchContent_MakeAvailable(ocr_models)
    message(STATUS "[Models] Downloaded to: ${CMAKE_SOURCE_DIR}/models")
endif()

# ============================================================
# Generated config
# ============================================================
configure_file(
    ${CMAKE_SOURCE_DIR}/include/Config.hpp.in
    ${CMAKE_BINARY_DIR}/include/Config.hpp @ONLY)

# ============================================================
# Third-party dependencies
# ============================================================
find_package(OpenCV REQUIRED)

find_package(jsoncpp CONFIG QUIET)
if(jsoncpp_FOUND)
    if(NOT TARGET JsonCpp::JsonCpp AND TARGET jsoncpp_lib)
        add_library(JsonCpp::JsonCpp ALIAS jsoncpp_lib)
    endif()
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(JSONCPP REQUIRED jsoncpp)
    add_library(JsonCpp::JsonCpp INTERFACE IMPORTED)
    set_target_properties(JsonCpp::JsonCpp PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${JSONCPP_INCLUDE_DIRS}"
        INTERFACE_LINK_LIBRARIES "${JSONCPP_LIBRARIES}")
endif()

find_package(Boost CONFIG REQUIRED COMPONENTS system)

# ============================================================
# Compiler options
# ============================================================
if(WIN32)
    add_compile_definitions(
        _WIN32_WINNT=0x0A00
        NOMINMAX)
    add_compile_options(/utf-8)
endif()

# ============================================================
# Target
# ============================================================
add_executable(ArknightsAutoBot
    src/main.cpp
    src/adb/AdbStatus.cpp
    src/adb/ADBClient.cpp
    src/SimpleController.cpp
    src/task/TaskExecutor.cpp
    src/vision/OcrDet.cpp
    src/vision/OcrPack.cpp
    src/vision/OcrRec.cpp
    src/vision/ImagePreprocessor.cpp
    src/infrastructure/InfrastructureManager.cpp
    src/infrastructure/SkillParser.cpp
    src/infrastructure/EfficiencyCalculator.cpp
    src/infrastructure/ScheduleOptimizer.cpp)

target_include_directories(ArknightsAutoBot PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/adb
    ${PROJECT_SOURCE_DIR}/include/vision
    ${PROJECT_SOURCE_DIR}/include/task
    ${CMAKE_BINARY_DIR}/include
    ${ONNXRUNTIME_DIR}/include
    ${OpenCV_INCLUDE_DIRS})

if(WIN32)
    set(ONNXRUNTIME_LIB "${ONNXRUNTIME_DIR}/lib/onnxruntime.lib")
else()
    set(ONNXRUNTIME_LIB "${ONNXRUNTIME_DIR}/lib/libonnxruntime${CMAKE_SHARED_LIBRARY_SUFFIX}")
endif()

target_link_libraries(ArknightsAutoBot PRIVATE
    ${OpenCV_LIBS}
    JsonCpp::JsonCpp
    Boost::system
    ${ONNXRUNTIME_LIB})

if(ONNXRUNTIME_USE_CUDA_PACKAGE AND CUDAToolkit_FOUND)
    target_compile_definitions(ArknightsAutoBot PRIVATE USE_CUDA)
    target_link_libraries(ArknightsAutoBot PRIVATE CUDA::cudart)
    message(STATUS "CUDA found: ${CUDAToolkit_VERSION}, GPU acceleration enabled")
else()
    message(STATUS "Building CPU-only version")
endif()

if(WIN32)
    file(GLOB ONNXRUNTIME_DLLS "${ONNXRUNTIME_DIR}/lib/*.dll")
    set(RUNTIME_DLLS ${ONNXRUNTIME_DLLS})

    if(ONNXRUNTIME_USE_CUDA_PACKAGE AND CUDNN9_DLL)
        get_filename_component(CUDNN9_DIR "${CUDNN9_DLL}" DIRECTORY)
        file(GLOB CUDNN9_DLLS "${CUDNN9_DIR}/cudnn*_9.dll")
        list(APPEND RUNTIME_DLLS ${CUDNN9_DLLS})
    endif()

    if(ONNXRUNTIME_USE_CUDA_PACKAGE AND CUDNN8_DLL)
        list(APPEND RUNTIME_DLLS "${CUDNN8_DLL}")
    endif()

    if(RUNTIME_DLLS)
        add_custom_command(TARGET ArknightsAutoBot POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${RUNTIME_DLLS}
                $<TARGET_FILE_DIR:ArknightsAutoBot>
            COMMAND_EXPAND_LISTS
            COMMENT "Copying runtime DLLs next to ArknightsAutoBot.exe")
    endif()
else()
    set_target_properties(ArknightsAutoBot PROPERTIES
        BUILD_RPATH "${ONNXRUNTIME_DIR}/lib"
        INSTALL_RPATH "${ONNXRUNTIME_DIR}/lib")
endif()
