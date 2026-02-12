#pragma once
#include <string>
#include <vector>
#include <optional>

// 单个操作步骤
struct TaskStep {
    std::string action;      // 操作类型: click, wait, screenshot, ocr, swipe, shell, start_app
    int x = 0;               // 点击/滑动 x 坐标
    int y = 0;               // 点击/滑动 y 坐标
    int x2 = 0;              // 滑动终点 x
    int y2 = 0;              // 滑动终点 y
    int duration = 0;        // 等待时间(ms) 或 滑动时长
    std::string template_path;  // 模板匹配图片路径
    std::string save_path;      // 截图保存路径
    std::string text;           // OCR 匹配文本
    std::string shell_cmd;      // shell 命令
    std::string package_name;   // 应用包名 (如 com.hypergryph.arknights/com.u8.sdk.U8UnityContext)
    int retry = 1;              // 重试次数
    int timeout = 5000;         // 超时时间(ms)
};

// 任务描述
struct TaskConfig {
    std::string name;              // 任务名称
    std::string description;       // 任务描述
    std::vector<TaskStep> steps;   // 操作步骤列表
    bool loop = false;             // 是否循环执行
    int loop_count = 1;            // 循环次数
    std::optional<std::string> on_success;  // 成功后执行的任务
    std::optional<std::string> on_failure;  // 失败后执行的任务
};
