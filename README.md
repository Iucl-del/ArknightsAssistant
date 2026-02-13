# Arknights Assistant

明日方舟自动化助手，基于 OCR 和 ADB 实现游戏自动操作。

## 功能特性

- 🎮 **ADB 设备控制** - 支持点击、滑动、截图等操作
- 🔍 **OCR 文字识别** - 基于 PP-OCR + ONNX Runtime，识别游戏界面文字
- 🖼️ **模板匹配** - 基于 OpenCV 的图像模板匹配
- 📋 **JSON 任务配置** - 灵活的 JSON 格式任务定义
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

## 依赖

- **OpenCV** >= 4.6
- **ONNX Runtime** >= 1.17
- **jsoncpp**
- **Boost**
- **CMake** >= 3.16
- **C++17**

## 编译

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## 使用

### 基本用法

```cpp
#include "SimpleController.hpp"
#include "task/TaskExecutor.hpp"

int main() {
    SimpleController controller;
    controller.connect("adb", "192.168.3.69:5555");

    TaskExecutor executor(controller);
    executor.start();  // 启动工作线程

    // 投递任务
    executor.submit("resource/tasks/start_arknights.json");

    // 等待任务完成...
    
    executor.stop();   // 停止工作线程
    return 0;
}
```

### JSON 任务配置

任务配置文件位于 `resource/tasks/` 目录，支持以下操作：

#### 基础操作 (BasicStep)

| 操作 | 说明 | 参数 |
|------|------|------|
| `click` | 点击 | `x`, `y` |
| `swipe` | 滑动 | `x`, `y`, `x2`, `y2`, `duration` |
| `wait` | 等待 | `duration` (毫秒) |

#### 视觉操作 (VisionStep)

| 操作 | 说明 | 参数 |
|------|------|------|
| `screenshot` | 截图 | `save_name` |
| `ocr_click` | OCR 识别并点击 | `save_name`, `text` |
| `ocr_region` | 区域 OCR | `save_name`, `roi`, `text` |
| `template` | 模板匹配并点击 | `save_name`, `template_path` |

#### 系统操作 (SystemStep)

| 操作 | 说明 | 参数 |
|------|------|------|
| `shell` | 执行 Shell 命令 | `shell_cmd` |
| `start_app` | 启动应用 | `package_name` |

### 任务示例

```json
{
  "name": "start_arknights",
  "description": "启动明日方舟游戏",
  "loop": false,
  "steps": [
    {
      "action": "shell",
      "shell_cmd": "am start -n com.hypergryph.arknights/com.u8.sdk.U8UnityContext"
    },
    {
      "action": "wait",
      "duration": 10000
    },
    {
      "action": "screenshot",
      "save_name": "start_screen.png"
    },
    {
      "action": "ocr_click",
      "save_name": "start_screen.png",
      "text": "开始唤醒"
    }
  ]
}
```

## API 说明

### TaskExecutor

| 方法 | 说明 |
|------|------|
| `start()` | 启动工作线程 |
| `stop()` | 停止工作线程 |
| `submit(path)` | 投递任务 (JSON 路径) |
| `queue_size()` | 获取队列长度 |
| `is_running()` | 是否正在运行 |

### SimpleController

| 方法 | 说明 |
|------|------|
| `connect(adb_path, address)` | 连接设备 |
| `click(x, y)` | 点击 |
| `swipe(x1, y1, x2, y2, duration)` | 滑动 |
| `capture_screenshot(filename)` | 截图 |
| `find_text(image, text, x, y)` | OCR 查找文本 |
| `find_template(image, template, x, y)` | 模板匹配 |

## 日志输出示例

```
============================================================
[TaskExecutor] 🚀 开始执行任务: start_arknights
[TaskExecutor] 📋 启动明日方舟游戏
[TaskExecutor] 📝 步骤总数: 5
============================================================

[Step 1/5] 💻 Shell: am start -n com.hypergryph.arknights/...
[Step 1] ✅ 完成 (120ms)

[Step 2/5] ⏳ 等待 10000ms
[Step 2] ✅ 完成 (10001ms)

[Step 3/5] 📷 截图 -> start_screen.png
[Step 3] ✅ 完成 (350ms)

[Step 4/5] 🔍🖱️  OCR点击: "开始唤醒"
  ✅ 位置: (640, 500)
[Step 4] ✅ 完成 (1200ms)

============================================================
[TaskExecutor] ✅ 任务完成: start_arknights
============================================================
```

## 许可证

MIT License
