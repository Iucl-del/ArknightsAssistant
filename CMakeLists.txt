cmake_minimum_required(VERSION 3.31)
project(ArknightsAutoBot)

set(CMAKE_CXX_STANDARD 20)

# 项目路径配置
set(PROJECT_ROOT_DIR "${CMAKE_SOURCE_DIR}")

# ONNX Runtime
set(ONNXRUNTIME_DIR "${CMAKE_SOURCE_DIR}/onnxruntime")
include_directories(${ONNXRUNTIME_DIR}/include)
link_directories(${ONNXRUNTIME_DIR}/lib)

# 生成配置头文件
configure_file(
    ${CMAKE_SOURCE_DIR}/include/Config.hpp.in
    ${CMAKE_SOURCE_DIR}/include/Config.hpp
    @ONLY
)

# OpenCV
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

# jsoncpp
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)
include_directories(${JSONCPP_INCLUDE_DIRS})

# CUDA (使用现代CMake方式)
find_package(CUDAToolkit)

# 添加可执行文件（包含所有源文件）
add_executable(ArknightsAutoBot
        src/adb/AdbStatus.cpp
    src/adb/ADB.cpp
    src/adb/ADBClient.cpp
    src/SimpleController.cpp
    src/vision/ocr_det.cpp
    src/vision/ocr_pack.cpp
    src/vision/ocr_rec.cpp
    src/vision/image_preprocessor.cpp
    src/vision/region_ocrer.cpp
    src/vision/task_config.cpp
)

# 添加头文件目录（仅对 ArknightsAutoBot 目标有效）
target_include_directories(ArknightsAutoBot PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/adb
    ${CMAKE_SOURCE_DIR}/include/vision
)

target_link_libraries(ArknightsAutoBot
        ${OpenCV_LIBS}
        onnxruntime
        ${JSONCPP_LIBRARIES}
)

set_target_properties(ArknightsAutoBot PROPERTIES
        INSTALL_RPATH "${ONNXRUNTIME_DIR}/lib"
        BUILD_WITH_INSTALL_RPATH TRUE
)

# 如果找到CUDA，链接CUDA库
if(CUDAToolkit_FOUND)
    target_link_libraries(ArknightsAutoBot CUDA::cudart)
    message(STATUS "CUDA found: ${CUDAToolkit_VERSION}")
else()
    message(STATUS "CUDA not found, building CPU-only version")
endif()
