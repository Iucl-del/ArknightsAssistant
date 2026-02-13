#pragma once
#include <string>
#include <vector>
#include <optional>
#include <variant>

// OCR 区域配置
struct ROIConfig {
    int x = 0;
    int y = 0;
    int width = 100;
    int height = 50;
    int base_width = 1280;
    int base_height = 720;
    std::string preprocess = "auto";
    std::string filter_pattern;
    bool debug_save = false;
};

// 基础操作：点击、滑动、等待
struct BasicStep {
    std::string action;      // click, swipe, wait
    int x = 0;
    int y = 0;
    int x2 = 0;
    int y2 = 0;
    int duration = 0;
};

// 视觉操作：截图、OCR、模板匹配
struct VisionStep {
    std::string action;      // screenshot, ocr, ocr_click, ocr_region, template
    std::string image_name;
    std::string text;
    std::string template_path;
    std::optional<ROIConfig> roi;
    int retry = 1;
    int timeout = 5000;
};

// 系统操作：shell、启动应用
struct SystemStep {
    std::string action;      // shell, start_app
    std::string cmd;
    std::string package_name;
};

// 操作步骤变体
using TaskStep = std::variant<BasicStep, VisionStep, SystemStep>;

// 获取步骤 action 名称
inline std::string get_step_action(const TaskStep& step) {
    return std::visit([](auto&& arg) { return arg.action; }, step);
}

// 任务配置
struct TaskConfig {
    std::string name;
    std::string description;
    std::vector<TaskStep> steps;
    bool loop = false;
    int loop_count = 1;
    std::optional<std::string> on_success;
    std::optional<std::string> on_failure;
};
