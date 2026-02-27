# Changelog

本文件记录 ArknightsAutoBot 项目的所有重要更改。

格式基于 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.0.0/)，
版本号遵循 [语义化版本](https://semver.org/lang/zh-CN/)。

## [0.4.0] - 2026-02-27

### 新增
- SDK 打包支持：项目功能模块编译为共享库（`libArknightsAutoBot.so`）+ 静态库（`libArknightsAutoBot.a`），供其他程序集成调用
- CMake 包配置导出：下游项目可通过 `find_package(ArknightsAutoBot)` 直接使用，提供 `ArknightsAutoBot::ArknightsAutoBot_shared` / `ArknightsAutoBot::ArknightsAutoBot_static` 两个目标
- 新增 `cmake/ArknightsAutoBotConfig.cmake.in` 模板，自动处理第三方依赖查找（OpenCV、jsoncpp）
- SDK 安装规则打包第三方库（ONNX Runtime、OpenCV、jsoncpp 的 `.so` 文件），下游无需自行安装
- ONNX Runtime 头文件随 SDK 一并安装至 `include/onnxruntime/`
- 新增 `scripts/pack_sdk.sh`：一键构建 + CPack 打包 SDK 发布包
- CPack 配置生成 `.tar.gz`（跨平台）和 `.deb`（Linux）格式的 SDK 包

### 变更
- 可执行文件 `ArknightsAutoBot` 改为链接 SDK 共享库，不再直接编译所有源文件
- 共享库与静态库的重复配置（include dirs、link libs、CUDA）合并为 `foreach()` 循环，消除冗余
- 头文件安装从 4 条 `install()` 合并为 1 条 `install(DIRECTORY include/ ...)`
- 第三方库 `.so` 收集逻辑抽取为 `install_imported_libs()` 辅助函数
- 版本号统一使用 `project(VERSION)`，消除硬编码散落
- FetchContent 调用统一为 `FetchContent_MakeAvailable()` 简化写法
- `project()` 声明中新增 `VERSION 1.0.0`

### SDK 安装产物

| 路径 | 内容 |
|------|------|
| `lib/` | `libArknightsAutoBot.so` / `.a` + ONNX Runtime / OpenCV / jsoncpp 动态库 |
| `include/` | 项目公共头文件 + ONNX Runtime 头文件 |
| `lib/cmake/ArknightsAutoBot/` | CMake Config / Targets / Version 文件 |
| `share/ArknightsAutoBot/models/` | OCR 模型（`.onnx` + 字典） |
| `share/ArknightsAutoBot/resource/` | 任务 JSON + 模板图片 |

---

## [0.3.0] - 2026-02-27

### 新增
- 跨平台构建支持
  - 新增 `CMakePresets.json`：标准化多平台构建预设（Linux x64/ARM64、Windows x64、macOS）
  - 新增 `vcpkg.json`：声明 OpenCV4、jsoncpp、Asio 依赖，vcpkg 一键安装
  - 新增 `scripts/pack_models.sh`：OCR 模型打包脚本，用于上传至 GitHub Releases

### 变更
- **ONNX Runtime**：自动检测本地 `onnxruntime/` 目录，不存在时通过 CMake FetchContent 从 GitHub Releases 下载对应平台预构建版本（支持 win-x64 / osx-arm64 / osx-x86_64 / linux-x64 / linux-aarch64）
- **Asio**：用独立 Asio 1.30.2 替换 Boost.Asio，移除对 Boost 的依赖
- **jsoncpp**：支持 vcpkg CONFIG 模式与 pkg-config 双重回退，统一暴露 `JsonCpp::JsonCpp` target
- **OCR 模型**：从 git 仓库迁移至 [GitHub Releases v1.0-models](https://github.com/Iucl-del/ArknightsAssistant/releases/tag/v1.0-models)，构建时通过 `MODELS_DOWNLOAD_URL` 自动下载
- 新增 `ORT_WITH_CUDA` CMake 选项，控制是否使用 ONNX Runtime GPU 构建
- 修复 `include(FetchContent)` 仅在部分分支引入导致模型下载段崩溃的 Bug

### 移除
- 移除对 Boost 的依赖

### 依赖变更

| 依赖 | 旧方式 | 新方式 |
|------|-------|-------|
| OpenCV | 系统安装 | vcpkg / 系统安装 |
| jsoncpp | pkg-config | vcpkg / pkg-config |
| ONNX Runtime | 本地目录 | 本地目录 / FetchContent 自动下载 |
| Boost.Asio | 系统安装 | 独立 Asio（vcpkg / FetchContent） |
| OCR 模型 | git 仓库 | GitHub Releases |

---

## [0.2.1] - 2026-02-27

### 新增
- `TaskCallback`：使用 `using TaskCallback = std::function<void()>` 定义任务完成回调类型
- `submit(path, callback)` 支持在投递任务时附带回调函数，任务执行完毕后自动触发

---

## [0.2.0] - 2026-02-13

### 新增
- TaskExecutor 异步线程+队列模式
  - `start()` 启动工作线程
  - `stop()` 停止工作线程
  - `submit(path)` 投递 JSON 任务路径
  - `queue_size()` 获取队列长度
  - `is_running()` 查询运行状态
- `ocr_region` 任务动作：指定 ROI 区域进行 OCR 识别
- 新增预置任务 `infrastructure_harvest.json`：基建收获

### 变更
- TaskConfig 使用 `std::variant` 重构步骤类型
  - `BasicStep`：点击、滑动、等待
  - `VisionStep`：截图、OCR、模板匹配
  - `SystemStep`：Shell、启动应用
- TaskExecutor 使用静态多态（函数重载）替代运行时分支
- TaskExecutor 分离头文件和实现文件
- 完善日志输出，添加步骤编号、耗时统计、emoji 标识
- 删除冗余文件：`task_config.h/cpp`、`region_ocrer.h/cpp`

### 文档
- 新增 `dosc/项目结构图.md`：项目整体架构图

---

## [0.1.1] - 2026-02-13

### 新增
- SimpleController 构造函数初始化 OCR 模块
- `find_text` 方法：通过 OCR 定位指定文本位置
- `ocr_click` 任务动作：识别文本并点击对应位置
- TaskLoader 添加文件打开和 JSON 解析错误检查

### 变更
- 预制任务
  - 更改 start_arknights 任务启动流程
- TaskExecutor 任务失败时立即终止并返回失败
- `save_path` 重命名为 `save_name`（截图文件名）
- 图片路径拼接使用 `work_dir_` 和 `Config::PROJECT_ROOT_DIR`

### 修复
- 修复任务失败时仍返回成功的问题


## [0.1.0] - 2026-02-12

### 新增
- ADB 连接与设备管理功能
  - ADBClient 客户端实现
  - AdbStatus 状态管理
- SimpleController 简单控制器
- OCR 文字识别模块
  - 基于 ONNX Runtime 的 PP-OCR 推理
  - 图像预处理 (image_preprocessor)
  - 文本检测 (ocr_det)
  - 文本识别 (ocr_rec)
  - OCR 打包封装 (ocr_pack)
  - 区域 OCR (region_ocrer)
- 任务配置与加载系统 (include/task/)
  - JSON 格式任务配置文件支持
  - TaskLoader 任务加载器
  - TaskExecutor 任务执行器
  - TaskConfig 任务配置
- 预置任务
  - start_arknights: 启动明日方舟游戏

### 变更
- 重构项目结构，将任务相关头文件移至 `include/task/` 目录

### 移除
- 删除老版本 ADB 类

### 支持的任务动作
- `shell`: 执行 ADB shell 命令
- `wait`: 等待指定时间
- `screenshot`: 截屏并保存

### 依赖
- OpenCV 4.6
- ONNX Runtime 1.17.1
- jsoncpp
- Boost.Asio
- CUDA Runtime (可选)

---

## 版本历史模板

### [x.y.z] - YYYY-MM-DD

#### 新增
- 新功能描述

#### 变更
- 变更内容描述

#### 修复
- Bug 修复描述

#### 移除
- 移除功能描述
