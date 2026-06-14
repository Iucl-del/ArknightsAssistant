# Arknights Assistant

本文件记录 Arknights Assistant 项目的重要工程变更。

格式基于 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.0.0/)，版本号遵循 [语义化版本](https://semver.org/lang/zh-CN/)。

## [1.2.0] - 2026-06-14

### 新增
- 添加基建布局识别功能。
- `InfrastructureManager` 增加布局识别相关能力。
- 新增基建导航与公告关闭相关模板资源：`BackArrow.png`、`AnnouncementClose_circle.png`。

---

## [1.1.0] - 2026-06-02

### 新增
- 统一截图分辨率为 `1280x720`，提升模板与 ROI 坐标的一致性。
- 模板匹配支持 ROI 区域，任务节点可以限制匹配范围。
- 新增统一日志组件 `Logger`。

### 变更
- 项目升级到 C++23。
- CMake 增加 CUDA / cuDNN 版本检测，并据此自动匹配 ONNX Runtime 配置。
- 视觉、主流程和基建排班模块逐步切换到统一日志输出。
- 替换流式日志打印方式，减少日志调用分散问题。

### 修复
- 修复启动任务 JSON 最后一个节点无法正常识别的问题。
- 更新启动流程相关模板：`START.png`、`AnnouncementClose.png`、`AnnouncementClose_circle_tight.png`。

---

## [1.0.0] - 2026-04-29

### 新增
- 完成基建排班优化能力的主要迭代。
- 新增干员数据获取脚本与配置：`scripts/get_operators.py`、`scripts/config.env`。

### 变更
- 优化基建排班算法，调整效率计算、技能解析和排班搜索流程。
- 优化基建排版算法相关数据结构与流程。
- 更新 `InfrastructureManager`、`ScheduleOptimizer`、`EfficiencyCalculator`、`SkillParser` 等核心模块。
- 调整启动任务配置以配合基建流程。

### 文档
- 更新基建排版算法实现计划书。

---

## [0.9.0] - 2026-04-10

### 新增
- 实现基建排班优化模块。
- 新增贪心初始化与模拟退火的排班求解流程。
- 新增基建领域模型与服务：
  - `InfrastructureManager`
  - `InfrastructureState`
  - `ScheduleOptimizer`
  - `EfficiencyCalculator`
  - `SkillParser`
  - `SkillEffect`
  - `GroupMapping`
- 新增基建数据资源：`building_data.json`、`character_table.json`、`operator_skills.json`。
- 新增技能解析脚本 `scripts/parse_building_skills.py`。
- 新增模板资源 `PopupConfirm_Tick.png`。

### 变更
- 头文件后缀统一从 `.h` 调整为 `.hpp`。
- 视觉模块文件命名统一为大驼峰风格，例如 `OcrPack.cpp`、`ImagePreprocessor.cpp`。
- 更新 CMake 与源码引用以适配新命名。

### 文档
- 新增基建排版算法实现计划书。

---

## [0.8.0] - 2026-04-09

### 新增
- 任务执行节点支持通过回调函数映射执行逻辑。

### 变更
- `TaskExecutor` 改为作为 `SimpleController` 的成员使用，降低外部调用复杂度。
- 重构任务执行器、任务配置和控制器之间的协作关系。
- 调整 OCR 头文件和打包接口以适配新的任务调用方式。

### 性能
- 优化文字检测、文字识别的预处理流程，减少图像数据内存拷贝。

---

## [0.7.0] - 2026-04-04

### 变更
- `Config.hpp` 改为输出到构建目录，避免生成文件污染源码目录。
- 新增 Debug 构建预设。
- 修复 Windows x64 vcpkg triplet 配置。
- 简化 CMakePresets，不再按不同平台额外拆分 Debug / Release 预设，默认配置两个版本。
- 更新 ONNX Runtime 与 OCR 相关构建配置。

### 移除
- 从仓库跟踪中移除生成文件 `Config.hpp`。
- 从仓库跟踪中移除 `onnxruntime/` 与旧生成/遗留文件。

---

## [0.6.1] - 2026-03-04

### 新增
- 新增基建回收任务 `infrastructure_harvest.json`。
- 基建回收流程支持资源收集、干员信赖收集和基建换班任务。
- 新增公告关闭模板 `AnnouncementClose.png`。

### 变更
- 更新 `start_arknights.json` 与弹窗处理模板，适配基建任务入口。
- 扩展任务配置、加载器和执行器以支持新的基建回收流程。

---

## [0.6.0] - 2026-03-03

### 新增
- `OcrPack::findTemplate` 模板匹配方法。
  - 接受 `ImagePreprocessor::Strategy` 参数，对场景图和模板图做相同预处理后执行 `TM_CCOEFF_NORMED` 匹配。
  - 自动处理预处理后通道数不一致的情况。
- `SimpleController::find_template_with_preprocess`。
  - 支持多模板轮询匹配，任一模板匹配成功即返回。
  - 预处理策略通过 `ImagePreprocessor::Strategy` 枚举指定。
- `TaskNode` 新增字段。
  - `method`：识别算法选择（`Ccoeff` / `Grayscale` / `HSVCount` / `RGBCount`）。
  - `template_paths`：支持多个模板图路径，替代原 `template_path` 单路径。
  - `threshold`：可配置匹配阈值，默认 `0.8`。
  - `color_scales`：颜色范围配置（`ColorRange` 结构体）。
  - `repeat_until_failed`：反复执行直到识别失败，适用于关闭多个弹窗。
- `TaskLoader` 解析增强。
  - `template` 字段支持字符串或数组两种格式。
  - `roi` 字段支持 `[x, y, w, h]` 或 `[x, y, w, h, base_w, base_h]` 数组格式。
  - 新增 `color_scales`、`threshold`、`method`、`repeat_until_failed` 字段解析。
- 新增模板图片资源：`START.png`、`OfflineCancel.png`、`OfflineConfirm.png`、`PopupCancel.png`、`PopupConfirm.png`、`bell_infrastructure.png`。

### 变更
- `TaskExecutor` 统一模板匹配调用。
  - 删除原 `find_template_ccoeff` / `find_template_grayscale` / `find_template_hsv` / `find_template_rgb` 分散接口。
  - `recognize` 和 `perform_action` 统一通过 `find_template_with_preprocess` 与 `methodToStrategy` 分发。
- 重构 `execute_node`。
  - 截图逻辑从 `recognize` 移至 `execute_node`，截图失败立即返回错误。
  - 新增 `repeat_until_failed` 循环执行逻辑。
- 删除旧的 `SimpleController::find_template` 单模板匹配方法。
- 模板图片路径使用 `Config::PROJECT_ROOT_DIR` 拼接，修复 `work_dir_` 路径错误。
- `start_arknights.json` 更新：新增 `method` 字段、模板改用数组格式、关闭弹窗节点启用 `repeat_until_failed`。

### 修复
- 修复模板图片路径拼接问题，从 ADB 工作目录改为项目工作目录。

---

## [0.5.0] - 2026-03-03

### 新增
- ADB 工作目录生命周期管理。
  - 构造 `ADBClient` 时自动检测 `work_dir`，不存在则创建并标记为自建目录。
  - 析构时自动清理：自建目录整体删除，已有目录仅删除运行期间产生的截图文件。
  - 新增 `work_dir_created_` 标记与 `screenshot_paths_` 记录。
- 任务配置 `start_arknights.json` 新增“关闭公告”节点。
- “开始唤醒”节点改用 `TemplateMatch` 识别 `START.png`，替代 `DirectHit` 固定坐标点击。

### 变更
- Asio 迁移到 Boost.Asio。
  - 将独立 Asio 替换为 Boost.Asio，`asio::` 命名空间统一改为 `net`（`boost::asio`）。
  - vcpkg 依赖从 `asio` 改为 `boost-asio` 和 `boost-beast`。
  - CMake 使用 `find_package(Boost REQUIRED COMPONENTS system)`。
- `SimpleController` 接口统一使用 `cv::Point`。
  - `click(int x, int y)` 改为 `click(const cv::Point& pos)`。
  - `swipe(int x1, int y1, int x2, int y2, int duration_ms)` 改为 `swipe(const cv::Point& from, const cv::Point& to, int duration_ms)`。
  - `find_template` / `find_text` 输出参数从 `int& out_x, int& out_y` 改为 `cv::Point& out_pos`。
- CMake 构建改进。
  - 最低版本从 3.16 升至 3.30。
  - FetchContent 从 `Populate` 改为 `MakeAvailable`。
  - 使用 `SOURCE_SUBDIR "DO_NOT_BUILD"` 跳过子项目构建。
  - 移除 `CMP0169` 策略兼容代码。
- README 重写：补充架构图、技术栈表格，重新组织快速开始章节。

### 移除
- 任务动作类型中移除 `StartApp` / `StopApp` 文档说明。
- README 中移除提前展示的预设一览表，调整到方式 B 之后。

### 依赖变更

| 依赖 | 旧方式 | 新方式 |
|------|-------|-------|
| Asio | 独立 Asio（vcpkg / FetchContent） | Boost.Asio（vcpkg `boost-asio`） |
| Boost | 无 | `boost-asio` + `boost-beast` |

---

## [0.4.0] - 2026-02-28

### 新增
- 节点式任务系统，借鉴 MAA pipeline 设计。
  - 每个节点由识别与动作组成，执行器自动轮询截图直到识别通过再执行动作。
  - 识别类型：`DirectHit`、`OCR`、`TemplateMatch`。
  - 动作类型：`Click`、`Swipe`、`StartApp`、`StopApp`。
  - 节点支持 `pre_delay` / `post_delay` 延迟控制。
  - 节点支持 `timeout` / `interval` 识别轮询控制。
- `SimpleController::auto_screenshot()`：自动生成唯一文件名并截图。
- `SimpleController::start_app()` / `stop_app()`：启动/关闭游戏。
- `SimpleController::shell()`：执行 ADB shell 命令。
- `SimpleController` 支持通过 `config_path` 加载 JSON 配置文件。

### 变更
- `TaskConfig` 从 `steps`（`BasicStep` / `VisionStep` / `SystemStep` 变体列表）重构为 `nodes`（`TaskNode` 顺序列表）。
- ROI 配置统一，删除 `ROIConfig`，直接复用 `vision_types.h` 中的 `ROI` 结构体。
- JSON 任务文件大幅简化。
- ADB 连接参数拆分为 IP 与端口。

### 移除
- 删除无用成员变量 `adb_path_`、`config_path_`。

### 文档
- README 引入更新日志链接。
- 更新 `CHANGELOG.md` 标题为 `Arknights Assistant`。

---

## [0.3.0] - 2026-02-27

### 新增
- 跨平台构建支持。
- 新增 `CMakePresets.json`。
- 新增 `vcpkg.json` 依赖清单。
- 新增模型打包脚本 `scripts/pack_models.sh`。
- 新增 sanity 检查任务 `check_sanity.json`。
- 新增模板资源 `btn_infrastructure.png`。

### 变更
- OCR 模型迁移到 GitHub Releases 分发，减少仓库体积。
- 精简 CMakeLists，移除过度设计的 SDK、安装和 CPack 配置。
- 用独立 Asio 替换 Boost.Asio。
- 修复 FetchContent 作用域问题。
- 统一 OCR 区域识别接口。
- 截图动作移至 `SystemStep`。
- 更新 README 中的构建、依赖和运行说明。

### 移除
- 从仓库移除 ONNX 模型、Paddle 模型和 OCR 字典等大体积模型文件。

### 依赖变更

| 依赖 | 旧方式 | 新方式 |
|------|-------|-------|
| ONNX / OCR 模型 | 直接提交到仓库 | GitHub Releases 下载 |
| Asio | Boost.Asio | 独立 Asio |

---

## [0.2.1] - 2026-02-27

### 新增
- 任务投递接口支持回调函数形参。
- 任务执行完成后自动触发回调。

---

## [0.2.0] - 2026-02-13

### 新增
- `TaskExecutor` 异步线程与队列模式。
  - `start()` 启动工作线程。
  - `stop()` 停止工作线程。
  - `submit(path)` 投递 JSON 任务路径。
  - `queue_size()` 获取队列长度。
  - `is_running()` 查询运行状态。
- `ocr_region` 任务动作：指定 ROI 区域进行 OCR 识别。
- 新增预置任务 `infrastructure_harvest.json`：基建收获。

### 变更
- `TaskConfig` 使用 `std::variant` 重构步骤类型。
  - `BasicStep`：点击、滑动、等待。
  - `VisionStep`：截图、OCR、模板匹配。
  - `SystemStep`：Shell、启动应用。
- `TaskExecutor` 使用静态多态（函数重载）替代运行时分支。
- `TaskExecutor` 分离头文件和实现文件。
- 完善日志输出，添加步骤编号和耗时统计。
- 删除冗余文件：`task_config.h/cpp`、`region_ocrer.h/cpp`。

### 文档
- 新增 `dosc/项目结构图.md`：项目整体架构图。
- 更新 README 项目文档。

---

## [0.1.1] - 2026-02-13

### 新增
- `SimpleController` 构造函数初始化 OCR 模块。
- `find_text` 方法：通过 OCR 定位指定文本位置。
- `ocr_click` 任务动作：识别文本并点击对应位置。
- `TaskLoader` 添加文件打开和 JSON 解析错误检查。

### 变更
- 更新 `start_arknights` 任务启动流程。
- `TaskExecutor` 任务失败时立即终止并返回失败。
- `save_path` 重命名为 `save_name`（截图文件名）。
- 图片路径拼接使用 `work_dir_` 和 `Config::PROJECT_ROOT_DIR`。

### 修复
- 修复任务失败时仍返回成功的问题。

---

## [0.1.0] - 2026-02-12

### 新增
- ADB 连接与设备管理功能。
  - ADB 客户端实现。
  - `AdbStatus` 状态管理。
- `SimpleController` 简单控制器。
- OCR 文字识别模块。
  - 基于 ONNX Runtime 的 PP-OCR 推理。
  - 图像预处理 `image_preprocessor`。
  - 文本检测 `ocr_det`。
  - 文本识别 `ocr_rec`。
  - OCR 打包封装 `ocr_pack`。
  - 区域 OCR `region_ocrer`。
- 任务配置与加载系统。
  - JSON 格式任务配置文件支持。
  - `TaskLoader` 任务加载器。
  - `TaskExecutor` 任务执行器。
  - `TaskConfig` 任务配置。
- 预置任务 `start_arknights`：启动明日方舟游戏。

### 变更
- 重构项目结构，将任务相关头文件移至 `include/task/` 目录。

### 移除
- 删除老版本 ADB 类。

### 支持的任务动作
- `shell`：执行 ADB shell 命令。
- `wait`：等待指定时间。
- `screenshot`：截屏并保存。

### 依赖
- OpenCV 4.6。
- ONNX Runtime 1.17.1。
- jsoncpp。
- Boost.Asio。
- CUDA Runtime（可选）。

---

## [0.0.0] - 2026-02-03

### 新增
- 初始化项目仓库。
- 新增 `.gitignore`、`LICENSE`、`README.md`。
- 新增早期 CMake 工程骨架。
- 新增早期 ADB 与 `SimpleController` 原型代码。
