cmake_minimum_required(VERSION 3.16)
project(ArknightsAutoBot)

set(CMAKE_CXX_STANDARD 20)
set(PROJECT_ROOT_DIR "${CMAKE_SOURCE_DIR}")

# ============================================================
# 选项
# ============================================================
option(ORT_WITH_CUDA "使用带 CUDA 支持的 ONNX Runtime 预构建版本" OFF)

# ============================================================
# ONNX Runtime
# 优先使用本地 onnxruntime/ 目录；若不存在则自动下载对应平台预构建版本
# ============================================================
set(ONNXRUNTIME_VERSION "1.17.1")

if(EXISTS "${CMAKE_SOURCE_DIR}/onnxruntime/lib" AND
   EXISTS "${CMAKE_SOURCE_DIR}/onnxruntime/include")
    message(STATUS "[OnnxRuntime] 使用本地目录: ${CMAKE_SOURCE_DIR}/onnxruntime")
    set(ONNXRUNTIME_DIR "${CMAKE_SOURCE_DIR}/onnxruntime")
else()
    message(STATUS "[OnnxRuntime] 本地目录不存在，自动下载 v${ONNXRUNTIME_VERSION}...")
    include(FetchContent)

    # 兼容 CMake 3.30+ 弃用 FetchContent_Populate 的策略
    if(POLICY CMP0169)
        cmake_policy(SET CMP0169 OLD)
    endif()

    if(ORT_WITH_CUDA)
        set(_ORT_SUFFIX "-gpu")
    else()
        set(_ORT_SUFFIX "")
    endif()

    if(WIN32)
        set(_ORT_PLATFORM "win-x64")
        set(_ORT_EXT "zip")
    elseif(APPLE)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
            set(_ORT_PLATFORM "osx-arm64")
        else()
            set(_ORT_PLATFORM "osx-x86_64")
        endif()
        set(_ORT_EXT "tgz")
    else()  # Linux
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
    )
    FetchContent_GetProperties(onnxruntime)
    if(NOT onnxruntime_POPULATED)
        FetchContent_Populate(onnxruntime)
    endif()
    set(ONNXRUNTIME_DIR "${onnxruntime_SOURCE_DIR}")
    message(STATUS "[OnnxRuntime] 已下载至: ${ONNXRUNTIME_DIR}")
endif()

include_directories(${ONNXRUNTIME_DIR}/include)
link_directories(${ONNXRUNTIME_DIR}/lib)

# ============================================================
# OCR 模型
# 优先使用本地 models/ 目录；若不存在则从 MODELS_DOWNLOAD_URL 下载
# 打包命令: bash scripts/pack_models.sh
# ============================================================
set(MODELS_DOWNLOAD_URL "" CACHE STRING "OCR 模型下载 URL（models.tar.gz），本地已有模型时忽略此项")

if(EXISTS "${CMAKE_SOURCE_DIR}/models/onnx/ch_ppocr_det.onnx")
    message(STATUS "[Models] 使用本地模型: ${CMAKE_SOURCE_DIR}/models")
else()
    if(NOT MODELS_DOWNLOAD_URL)
        message(FATAL_ERROR
            "[Models] 未找到本地模型文件，且未设置 MODELS_DOWNLOAD_URL\n"
            "  方式一: 提供下载链接\n"
            "    cmake ... -DMODELS_DOWNLOAD_URL=https://github.com/<owner>/<repo>/releases/download/<tag>/models.tar.gz\n"
            "  方式二: 手动将模型放入 models/ 目录\n"
            "    models/onnx/ch_ppocr_det.onnx\n"
            "    models/onnx/ch_ppocr_cls.onnx\n"
            "    models/onnx/ch_ppocr_rec.onnx\n"
            "    models/ppocr_keys_v1.txt"
        )
    endif()
    message(STATUS "[Models] 下载模型: ${MODELS_DOWNLOAD_URL}")
    if(POLICY CMP0169)
        cmake_policy(SET CMP0169 OLD)
    endif()
    FetchContent_Declare(ocr_models
        URL "${MODELS_DOWNLOAD_URL}"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_DIR "${CMAKE_SOURCE_DIR}/models"
    )
    FetchContent_GetProperties(ocr_models)
    if(NOT ocr_models_POPULATED)
        FetchContent_Populate(ocr_models)
    endif()
    message(STATUS "[Models] 模型下载完成: ${CMAKE_SOURCE_DIR}/models")
endif()

# ============================================================
# 生成配置头文件
# ============================================================
configure_file(
    ${CMAKE_SOURCE_DIR}/include/Config.hpp.in
    ${CMAKE_SOURCE_DIR}/include/Config.hpp
    @ONLY
)

# ============================================================
# OpenCV
# ============================================================
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

# ============================================================
# jsoncpp
# 优先使用 CMake Config 模式（vcpkg / 现代系统包），回退到 pkg-config
# 统一暴露 JsonCpp::JsonCpp 目标
# ============================================================
find_package(jsoncpp CONFIG QUIET)
if(jsoncpp_FOUND)
    # 兼容 vcpkg (JsonCpp::JsonCpp) 与部分系统包 (jsoncpp_lib)
    if(NOT TARGET JsonCpp::JsonCpp AND TARGET jsoncpp_lib)
        add_library(JsonCpp::JsonCpp ALIAS jsoncpp_lib)
    endif()
    message(STATUS "[jsoncpp] 通过 CMake config 找到")
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(JSONCPP REQUIRED jsoncpp)
    add_library(JsonCpp::JsonCpp INTERFACE IMPORTED)
    set_target_properties(JsonCpp::JsonCpp PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${JSONCPP_INCLUDE_DIRS}"
        INTERFACE_LINK_LIBRARIES      "${JSONCPP_LIBRARIES}"
    )
    message(STATUS "[jsoncpp] 通过 pkg-config 找到")
endif()

# ============================================================
# CUDA（可选）
# ============================================================
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
    ${CMAKE_SOURCE_DIR}/include/adb
    ${CMAKE_SOURCE_DIR}/include/vision
    ${CMAKE_SOURCE_DIR}/include/task
)

target_link_libraries(ArknightsAutoBot
    ${OpenCV_LIBS}
    onnxruntime
    JsonCpp::JsonCpp
)

set_target_properties(ArknightsAutoBot PROPERTIES
    INSTALL_RPATH "${ONNXRUNTIME_DIR}/lib"
    BUILD_WITH_INSTALL_RPATH TRUE
)

if(CUDAToolkit_FOUND)
    target_link_libraries(ArknightsAutoBot CUDA::cudart)
    message(STATUS "CUDA found: ${CUDAToolkit_VERSION}")
else()
    message(STATUS "CUDA not found, building CPU-only version")
endif()
