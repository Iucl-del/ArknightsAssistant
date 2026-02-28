# Arknights Assistant

明日方舟自动化助手，基于 OCR 和 ADB 实现游戏自动操作。

## 功能特性

- 🎮 **ADB 设备控制** - 支持点击、滑动、截图等操作
- 🔍 **OCR 文字识别** - 基于 PP-OCR + ONNX Runtime，识别游戏界面文字
- 🖼️ **模板匹配** - 基于 OpenCV 的图像模板匹配
- 📋 **节点式任务配置** - JSON 定义识别+动作节点，自动轮询截图
- 🔄 **异步任务队列** - 支持任务排队执行

## 项目结构

```
ArknightsAutoBot/
├── include/                    # 头文件
│   ├── adb/                    # ADB 模块
│   ├── vision/                 # 视觉模块 (OCR)
│   └── task/                   # 任务模块
├── src/                        # 源文件
├── resource/tasks/             # JSON 任务配置
├── models/onnx/                # OCR 模型文件
└── onnxruntime/                # ONNX Runtime 库
```

## 快速部署

### 前置条件

| 工具 | 版本要求 | 用途 |
|------|----------|------|
| CMake | >= 3.16 | 构建系统 |
| C++ 编译器 | 支持 C++20 | GCC 11+ / Clang 13+ / MSVC 2022+ |
| Ninja | 任意 | 构建加速（推荐） |
| ADB | 任意 | 连接安卓设备/模拟器 |

> OpenCV、jsoncpp、ONNX Runtime **无需手动安装**，构建时自动处理。

---

### 第一步：克隆项目

```bash
git clone https://github.com/Iucl-del/ArknightsAssistant.git
cd ArknightsAutoBot
```

---

### 第二步：编译

#### 方式 A — vcpkg（推荐，全平台通用）

```bash
# 安装 vcpkg（若未安装，仅首次）
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh          # Windows: bootstrap-vcpkg.bat
export VCPKG_ROOT=~/vcpkg           # Windows: set VCPKG_ROOT=C:\vcpkg

# 配置 + 编译（选择对应平台预设）
cmake --preset linux-x64-vcpkg      # Linux x64
cmake --preset windows-x64-vcpkg    # Windows x64
cmake --preset macos-vcpkg          # macOS

cmake --build --preset linux-x64-vcpkg   # preset 名与上一行对应
```

构建过程中 CMake 会自动：
1. 通过 vcpkg 下载编译 OpenCV、jsoncpp
2. 从 GitHub Releases 下载对应平台的 ONNX Runtime 预构建包

**全部预设一览：**

| 预设名 | 平台 | 依赖管理 |
|--------|------|----------|
| `linux-x64` | Linux x64 | 系统已安装的包 |
| `linux-x64-vcpkg` | Linux x64 | vcpkg |
| `linux-arm64-vcpkg` | Linux ARM64 | vcpkg |
| `windows-x64-vcpkg` | Windows x64 | vcpkg |
| `macos-vcpkg` | macOS | vcpkg |
| `linux-x64-cuda` | Linux x64 | vcpkg + CUDA GPU 加速 |

#### 方式 B — 系统包（Linux 快速部署）

```bash
# 安装系统依赖
sudo apt install libopencv-dev libjsoncpp-dev cmake ninja-build g++

# 编译（ONNX Runtime 自动下载）
cmake --preset linux-x64
cmake --build --preset linux-x64
```

---

### 第三步：连接设备

```bash
# 连接安卓模拟器（默认端口 5555）
adb connect 192.168.x.x:5555

# 确认连接成功
adb devices
```

---

### 第四步：运行

```bash
# 可执行文件位于 build/<preset-name>/ 目录下
./build/linux-x64-vcpkg/ArknightsAutoBot
```

> 确保以**项目根目录**为工作目录运行，程序依赖相对路径 `resource/tasks/` 和 `models/onnx/`。

---

## 模型管理

OCR 模型文件（~50MB）支持两种管理方式：

### 方式一：本地模型（默认）

模型已包含在仓库中，`git clone` 后直接可用，无需额外操作。

### 方式二：GitHub Releases 分发（适合 CI/CD）

模型已托管于 [GitHub Releases v1.0-models](https://github.com/Iucl-del/ArknightsAssistant/releases/tag/v1.0-models)，构建时通过 `MODELS_DOWNLOAD_URL` 自动下载：

```bash
cmake --preset linux-x64-vcpkg \
    -DMODELS_DOWNLOAD_URL=https://github.com/Iucl-del/ArknightsAssistant/releases/download/v1.0-models/models.tar.gz
cmake --build --preset linux-x64-vcpkg
```

## 使用

### 基本用法

```cpp
#include "SimpleController.hpp"
#include "task/TaskExecutor.hpp"
#include "Config.hpp"

int main() {
    SimpleController controller;
    controller.connect("/tmp/adb", "192.168.3.69:5555");

    TaskExecutor executor(controller);
    executor.start();

    std::string task_path = std::string(Config::PROJECT_ROOT_DIR)
                          + "/resource/tasks/start_arknights.json";
    executor.submit(task_path, [&](){});

    return 0;
}
```

### JSON 任务配置

任务采用**节点式设计**，每个节点 = 识别 + 动作。执行器自动轮询截图，直到识别通过再执行动作，**无需手动插入截图或等待步骤**。

#### 识别类型

| 类型 | 说明 | 必要参数 |
|------|------|----------|
| `DirectHit` | 直接执行，不做识别 | 无 |
| `OCR` | 文字识别，等待目标文字出现 | `expected` |
| `TemplateMatch` | 模板匹配，等待目标图片出现 | `template` |

#### 动作类型

| 类型 | 说明 | 参数 |
|------|------|------|
| `Click` | 点击（识别位置或指定坐标） | `target`: `[x, y]`（可选） |
| `Swipe` | 滑动 | `target`: `[x1, y1, x2, y2, duration]` |
| `StartApp` | 启动游戏（无需参数） | 无 |
| `StopApp` | 关闭游戏（无需参数） | 无 |

#### 节点可选参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `timeout` | 20000 | 识别超时时间（ms） |
| `interval` | 1000 | 识别轮询间隔（ms） |
| `pre_delay` | 0 | 动作前延迟（ms） |
| `post_delay` | 500 | 动作后延迟（ms） |

### 任务示例

```json
{
  "name": "启动明日方舟",
  "nodes": [
    {
      "recognition": "DirectHit",
      "action": "StartApp",
      "post_delay": 10000
    },
    {
      "recognition": "DirectHit",
      "action": "Click",
      "target": [960, 540],
      "post_delay": 500
    },
    {
      "recognition": "OCR",
      "expected": "开始唤醒",
      "action": "Click",
      "timeout": 20000,
      "interval": 2000
    }
  ]
}
```

## API 说明

### SimpleController

| 方法 | 说明 |
|------|------|
| `connect(adb_path, address, config_path)` | 连接设备 |
| `click(x, y)` | 点击 |
| `swipe(x1, y1, x2, y2, duration)` | 滑动 |
| `wait(ms)` | 等待 |
| `capture_screenshot(filename)` | 截图 |
| `auto_screenshot(hint)` | 自动生成文件名并截图 |
| `start_app()` | 启动游戏 |
| `stop_app()` | 关闭游戏 |
| `find_text(image, text, x, y)` | OCR 查找文本 |
| `find_template(image, template, x, y)` | 模板匹配 |
| `detect_text(image, out_text, roi)` | 区域 OCR |

### TaskExecutor

| 方法 | 说明 |
|------|------|
| `start()` | 启动工作线程 |
| `stop()` | 停止工作线程 |
| `submit(path, callback)` | 投递任务 (JSON 路径 + 回调) |
| `queue_size()` | 获取队列长度 |
| `is_running()` | 是否正在运行 |

## 更新日志

详见 [CHANGELOG.md](CHANGELOG.md)

## 许可证

MIT License
