cmake_minimum_required(VERSION 3.16)
project(ArknightsAutoBot)

set(CMAKE_CXX_STANDARD 20)
set(PROJECT_ROOT_DIR "${CMAKE_SOURCE_DIR}")

# 先检测 CUDA，用于后续决定下载哪个版本的 ONNX Runtime
find_package(CUDAToolkit QUIET)

include(FetchContent)

# ============================================================
# ONNX Runtime — 本地优先，否则按平台自动下载
# ============================================================
set(ONNXRUNTIME_VERSION "1.17.1")

if(EXISTS "${CMAKE_SOURCE_DIR}/onnxruntime/lib" AND
   EXISTS "${CMAKE_SOURCE_DIR}/onnxruntime/include")
    message(STATUS "[OnnxRuntime] 使用本地目录: ${CMAKE_SOURCE_DIR}/onnxruntime")
    set(ONNXRUNTIME_DIR "${CMAKE_SOURCE_DIR}/onnxruntime")
else()
    if(CUDAToolkit_FOUND)
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
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_SUBDIR "DO_NOT_BUILD")
    FetchContent_MakeAvailable(onnxruntime)
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
        SOURCE_DIR "${CMAKE_SOURCE_DIR}/models"
        SOURCE_SUBDIR "DO_NOT_BUILD")
    FetchContent_MakeAvailable(ocr_models)
    message(STATUS "[Models] 模型已下载至: ${CMAKE_SOURCE_DIR}/models")
endif()

# ============================================================
# 生成配置头文件（输出到 build 目录，不污染源码树）
# ============================================================
configure_file(
    ${CMAKE_SOURCE_DIR}/include/Config.hpp.in
    ${CMAKE_BINARY_DIR}/include/Config.hpp @ONLY)

# ============================================================
# 第三方依赖
# ============================================================
find_package(OpenCV REQUIRED)

# jsoncpp: CMake Config → pkg-config 回退（仅非 vcpkg 环境如 linux-x64 会走后者）
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

# Boost: Asio + Beast（HTTP/WebSocket）
find_package(Boost CONFIG REQUIRED COMPONENTS system)

# ============================================================
# Windows 编译选项
# ============================================================
if(WIN32)
    add_compile_definitions(
        _WIN32_WINNT=0x0A00   # Windows 10
        NOMINMAX
    )
    add_compile_options(/utf-8)   # 源文件 UTF-8 编码
endif()

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
    ${CMAKE_BINARY_DIR}/include
    ${ONNXRUNTIME_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
)

if(WIN32)
    set(ONNXRUNTIME_LIB "${ONNXRUNTIME_DIR}/lib/onnxruntime.lib")
else()
    set(ONNXRUNTIME_LIB "${ONNXRUNTIME_DIR}/lib/libonnxruntime${CMAKE_SHARED_LIBRARY_SUFFIX}")
endif()

target_link_libraries(ArknightsAutoBot PRIVATE
    ${OpenCV_LIBS}
    JsonCpp::JsonCpp
    Boost::system
    ${ONNXRUNTIME_LIB}
)

if(NOT WIN32)
    set_target_properties(ArknightsAutoBot PROPERTIES
        BUILD_RPATH   "${ONNXRUNTIME_DIR}/lib"
        INSTALL_RPATH "${ONNXRUNTIME_DIR}/lib")
endif()

if(CUDAToolkit_FOUND)
    target_compile_definitions(ArknightsAutoBot PRIVATE USE_CUDA)
    target_link_libraries(ArknightsAutoBot PRIVATE CUDA::cudart)
    message(STATUS "CUDA found: ${CUDAToolkit_VERSION}, GPU 加速已启用")
else()
    message(STATUS "CUDA not found, building CPU-only version")
endif()