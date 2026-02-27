cmake_minimum_required(VERSION 3.16)
project(ArknightsAutoBot)

set(CMAKE_CXX_STANDARD 20)
set(PROJECT_ROOT_DIR "${CMAKE_SOURCE_DIR}")

option(ORT_WITH_CUDA "使用带 CUDA 支持的 ONNX Runtime 预构建版本" OFF)

include(FetchContent)
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

# ============================================================
# ONNX Runtime — 本地优先，否则按平台自动下载
# ============================================================
set(ONNXRUNTIME_VERSION "1.17.1")

if(EXISTS "${CMAKE_SOURCE_DIR}/onnxruntime/lib" AND
   EXISTS "${CMAKE_SOURCE_DIR}/onnxruntime/include")
    message(STATUS "[OnnxRuntime] 使用本地目录: ${CMAKE_SOURCE_DIR}/onnxruntime")
    set(ONNXRUNTIME_DIR "${CMAKE_SOURCE_DIR}/onnxruntime")
else()
    if(ORT_WITH_CUDA)
        set(_ORT_SUFFIX "-gpu")
    else()
        set(_ORT_SUFFIX "")
    endif()
    if(WIN32)
        set(_ORT_PLATFORM "win-x64")
        set(_ORT_EXT "zip")
    elseif(APPLE)
        set(_ORT_PLATFORM "osx-${CMAKE_SYSTEM_PROCESSOR}")
        set(_ORT_EXT "tgz")
    else()
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
            set(_ORT_PLATFORM "linux-aarch64")
        else()
            set(_ORT_PLATFORM "linux-x64")
        endif()
        set(_ORT_EXT "tgz")
    endif()
    set(_ORT_PKG "onnxruntime-${_ORT_PLATFORM}${_ORT_SUFFIX}-${ONNXRUNTIME_VERSION}")
    FetchContent_Declare(onnxruntime
        URL "https://github.com/microsoft/onnxruntime/releases/download/v${ONNXRUNTIME_VERSION}/${_ORT_PKG}.${_ORT_EXT}"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_GetProperties(onnxruntime)
    if(NOT onnxruntime_POPULATED)
        FetchContent_Populate(onnxruntime)
    endif()
    set(ONNXRUNTIME_DIR "${onnxruntime_SOURCE_DIR}")
    message(STATUS "[OnnxRuntime] 已下载至: ${ONNXRUNTIME_DIR}")
endif()

# ============================================================
# OCR 模型 — 本地优先，否则从 MODELS_DOWNLOAD_URL 下载
# ============================================================
set(MODELS_DOWNLOAD_URL "" CACHE STRING "OCR 模型下载 URL（models.tar.gz），本地已有模型时忽略")

if(EXISTS "${CMAKE_SOURCE_DIR}/models/onnx/ch_ppocr_det.onnx")
    message(STATUS "[Models] 使用本地模型")
elseif(NOT MODELS_DOWNLOAD_URL)
    message(FATAL_ERROR "[Models] 未找到本地模型，请设置 -DMODELS_DOWNLOAD_URL=<url> 或手动放入 models/ 目录")
else()
    FetchContent_Declare(ocr_models
        URL "${MODELS_DOWNLOAD_URL}"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_DIR "${CMAKE_SOURCE_DIR}/models")
    FetchContent_GetProperties(ocr_models)
    if(NOT ocr_models_POPULATED)
        FetchContent_Populate(ocr_models)
    endif()
    message(STATUS "[Models] 模型已下载至: ${CMAKE_SOURCE_DIR}/models")
endif()

# ============================================================
# 生成配置头文件
# ============================================================
configure_file(
    ${CMAKE_SOURCE_DIR}/include/Config.hpp.in
    ${CMAKE_SOURCE_DIR}/include/Config.hpp @ONLY)

# ============================================================
# 第三方依赖
# ============================================================
find_package(OpenCV REQUIRED)

# jsoncpp: CMake Config → pkg-config 回退，统一暴露 JsonCpp::JsonCpp
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
        INTERFACE_LINK_LIBRARIES      "${JSONCPP_LIBRARIES}")
endif()

# Asio: CMake Config → FetchContent 回退
find_package(asio CONFIG QUIET)
if(NOT asio_FOUND)
    FetchContent_Declare(asio
        URL "https://github.com/chriskohlhoff/asio/archive/refs/tags/asio-1-30-2.tar.gz"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_GetProperties(asio)
    if(NOT asio_POPULATED)
        FetchContent_Populate(asio)
    endif()
    add_library(asio_imported INTERFACE)
    target_include_directories(asio_imported INTERFACE "${asio_SOURCE_DIR}/asio/include")
    target_compile_definitions(asio_imported INTERFACE ASIO_STANDALONE)
    add_library(asio::asio ALIAS asio_imported)
endif()

find_package(CUDAToolkit QUIET)

# ============================================================
# 可执行文件
# ============================================================
add_executable(ArknightsAutoBot
    src/main.cpp
    src/adb/AdbStatus.cpp
    src/adb/ADBClient.cpp
    src/SimpleController.cpp
    src/task/TaskExecutor.cpp
    src/vision/ocr_det.cpp
    src/vision/ocr_pack.cpp
    src/vision/ocr_rec.cpp
    src/vision/image_preprocessor.cpp
)

target_include_directories(ArknightsAutoBot PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/adb
    ${PROJECT_SOURCE_DIR}/include/vision
    ${PROJECT_SOURCE_DIR}/include/task
    ${ONNXRUNTIME_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(ArknightsAutoBot PRIVATE
    ${OpenCV_LIBS}
    JsonCpp::JsonCpp
    asio::asio
    ${ONNXRUNTIME_DIR}/lib/libonnxruntime${CMAKE_SHARED_LIBRARY_SUFFIX}
)

set_target_properties(ArknightsAutoBot PROPERTIES
    BUILD_RPATH   "${ONNXRUNTIME_DIR}/lib"
    INSTALL_RPATH "${ONNXRUNTIME_DIR}/lib")

if(CUDAToolkit_FOUND)
    target_link_libraries(ArknightsAutoBot PRIVATE CUDA::cudart)
    message(STATUS "CUDA found: ${CUDAToolkit_VERSION}")
else()
    message(STATUS "CUDA not found, building CPU-only version")
endif()
