#pragma once
#include <string>
#include <vector>
#include <optional>
#include "vision/vision_types.h"

// ============================================================
// 颜色范围（用于 HSVCount / RGBCount 方法）
// JSON: [h_min, s_min, v_min, h_max, s_max, v_max]
// ============================================================
struct ColorRange {
    int lower[3] = {0, 0, 0};
    int upper[3] = {255, 255, 255};
};

// ============================================================
// 任务节点
// 每个节点 = 识别(method) + 动作，nodes 按顺序执行
//
// method 决定识别算法（仅 TemplateMatch 生效）:
//   Ccoeff        — 标准彩色模板匹配 (TM_CCOEFF_NORMED)
//   Grayscale     — 灰度模板匹配，忽略颜色只看形状/亮度
//   HSVCount      — HSV 颜色范围计数
//   RGBCount      — RGB 颜色范围计数
//
// JSON 示例:
//   { "name": "start_btn", "recognition": "TemplateMatch",
//     "method": "Ccoeff", "template": "START.png", "action": "Click" }
//   { "name": "close_blue", "recognition": "TemplateMatch",
//     "method": "HSVCount", "template": "close.png",
//     "color_scales": [100, 50, 50, 130, 255, 255], "action": "Click" }
// ============================================================
struct TaskNode {
    // ---- 识别 ----
    std::string recognition = "DirectHit";  // DirectHit | OCR | TemplateMatch
    std::string method = "Ccoeff";          // Ccoeff | HSVCount | RGBCount（仅 TemplateMatch 生效）
    std::string expected;                   // OCR 期望匹配文本
    std::vector<std::string> template_paths;// TemplateMatch 模板图路径（支持多个，轮询匹配）
    std::optional<ROI> roi;                 // 识别区域（可选）
    std::vector<ColorRange> color_scales;   // 颜色范围列表（HSVCount/RGBCount）
    double threshold = 0.8;                 // 匹配阈值（Ccoeff 默认 0.8；颜色计数为像素占比 0.0~1.0）
    int timeout = 20000;                    // 识别超时(ms)
    int interval = 100;                     // 识别轮询间隔(ms)


    // ---- 动作 ----
    std::string action = "Click";           // Click | Swipe | Shell | StartApp | StopApp
    std::vector<int> target;                // Click 坐标 [x,y]；Swipe 为 [x1,y1,x2,y2,duration]
    std::string shell_cmd;                  // Shell 命令

    // ---- 延迟 ----
    int pre_delay = 0;                      // 动作前延迟(ms)
    int post_delay = 500;                   // 动作后延迟(ms)

    // ---- 失败策略 ----
    bool repeat_until_failed = false;       // true: 反复执行(识别→动作)直到识别失败，适用于关闭多个弹窗
};

// 任务配置
struct TaskConfig {
    std::string name;
    std::vector<TaskNode> nodes;
    bool loop = false;
    int loop_count = 1;
};
