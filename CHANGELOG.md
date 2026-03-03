# Arknights Assistant

本文件记录 ArknightsAutoBot 项目的所有重要更改。

格式基于 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.0.0/)，
版本号遵循 [语义化版本](https://semver.org/lang/zh-CN/)。

## [0.5.0] - 2026-03-03

### 新增
- **ADB 工作目录生命周期管理**
  - 构造 `ADBClient` 时自动检测 `work_dir`，不存在则创建并标记为自建目录
  - 析构时自动清理：自建目录整体删除，已有目录仅删除运行期间产生的截图文件
  - 新增 `work_dir_created_` 标记与 `screenshot_paths_` 记录，确保资源不泄漏
- 任务配置 `start_arknights.json` 新增"关闭公告"节点（模板匹配 `close_announcement.png`）
- 任务配置"开始唤醒"节点改用 `TemplateMatch` 识别 `START.png`，替代 `DirectHit` 固定坐标点击

### 变更
- **Asio → Boost.Asio 迁移**
  - 将独立 Asio 替换为 Boost.Asio，`asio::` 命名空间统一改为 `net`（`boost::asio`）
  - vcpkg 依赖从 `asio` 改为 `boost-asio` + `boost-beast`
  - CMake `find_package(Boost REQUIRED COMPONENTS system)` 替代 FetchContent 回退
- **SimpleController 接口统一使用 `cv::Point`**
  - `click(int x, int y)` → `click(const cv::Point& pos)`
  - `swipe(int x1, int y1, int x2, int y2, int duration_ms)` → `swipe(const cv::Point& from, const cv::Point& to, int duration_ms)`
  - `find_template` / `find_text` 输出参数从 `int& out_x, int& out_y` 改为 `cv::Point& out_pos`
- **CMake 构建改进**
  - 最低版本从 3.16 升至 3.30
  - FetchContent 从 `Populate` 改为 `MakeAvailable`，使用 `SOURCE_SUBDIR "DO_NOT_BUILD"` 跳过子项目构建
  - 移除 `CMP0169` 策略兼容代码
- README 重写：补充架构图、技术栈表格，重新组织快速开始章节

### 移除
- 任务动作类型中移除 `StartApp` / `StopApp` 文档说明
- README 中移除提前展示的预设一览表（调整至方式 B 之后）

### 依赖变更

| 依赖 | 旧方式 | 新方式 |
|------|-------|-------|
| Asio | 独立 Asio（vcpkg / FetchContent） | Boost.Asio（vcpkg `boost-asio`） |
| Boost | 无 | `boost-asio` + `boost-beast` |

---

## [0.4.0] - 2026-02-28

### 新增
- **节点式任务系统**（借鉴 MAA pipeline 设计）
  - 每个节点 = 识别 + 动作，执行器自动轮询截图直到识别通过再执行动作
  - 识别类型：`DirectHit`（直接执行）、`OCR`（文字识别）、`TemplateMatch`（模板匹配）
  - 动作类型：`Click`、`Swipe`、`StartApp`、`StopApp`
  - 节点支持 `pre_delay` / `post_delay` 延迟控制
  - 节点支持 `timeout` / `interval` 识别轮询控制
- `SimpleController::auto_screenshot()`：自动生成唯一文件名并截图，视觉操作无需手动指定截图文件名
- `SimpleController::start_app()` / `stop_app()`：启动/关闭游戏，包名通过配置文件或默认值内部管理
- `SimpleController::shell()`：执行 ADB shell 命令
- `SimpleController` 支持通过 `config_path` 加载 JSON 配置文件（读取 `game_package` 等参数）

### 变更
- **任务配置重构**：`TaskConfig` 从 `steps`（`BasicStep` / `VisionStep` / `SystemStep` 变体列表）重构为 `nodes`（`TaskNode` 顺序列表）
- **ROI 统一**：删除 `ROIConfig`，直接复用 `vision_types.h` 中的 `ROI` 结构体
- JSON 任务文件大幅简化
  - 不再需要手动插入 `screenshot` / `wait` / `wait_until` 步骤
  - 不再需要指定 `save_name` 截图文件名
  - 不再需要 `next` 跳转和 `entry` 入口，节点按顺序执行
  - 不再需要 `description` 字段，`name` 即可标识任务
- 启动游戏参数（包名）隐藏在 `SimpleController` 内部，JSON 中 `StartApp` 无需任何参数

### 移除
- 删除 `BasicStep` / `VisionStep` / `SystemStep` 类型及 `std::variant` 分发
- 删除 `ROIConfig` 结构体
- 删除 `TaskConfig::description` / `TaskConfig::entry` / `TaskNode::name` / `TaskNode::next` 字段
- 删除 `SimpleController::build_cmd()` 方法

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
