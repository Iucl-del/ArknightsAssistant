# Changelog

本文件记录 ArknightsAutoBot 项目的所有重要更改。

格式基于 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.0.0/)，
版本号遵循 [语义化版本](https://semver.org/lang/zh-CN/)。

## [0.1.0] - 2026-02-13

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
- fastdeploy
- Boost
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
