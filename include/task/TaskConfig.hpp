#pragma once
#include <string>
#include <vector>
#include <optional>
#include "vision/vision_types.h"

// ============================================================
// 任务节点（平铺结构）
// 每个节点 = 识别 + 动作，按顺序执行
// ============================================================
struct TaskNode {
    // ---- 识别 ----
    std::string recognition = "DirectHit";  // DirectHit | OCR | TemplateMatch
    std::vector<std::string> expected;      // OCR 期望匹配文本（支持多个，任一匹配即成功）
    std::vector<std::string> template_paths;// TemplateMatch 模板图路径（支持多个，轮询匹配）
    std::optional<ROI> roi;                 // 识别区域（可选）
    double threshold = 0.8;                 // 匹配阈值
    int timeout = 10000;                    // 识别超时(ms)
    int interval = 100;                     // 识别轮询间隔(ms)

    // ---- 动作 ----
    std::string action = "Click";           // Click | Swipe | Shell | StartApp | StopApp
    std::vector<int> target;                // Click 坐标 [x,y]；Swipe 为 [x1,y1,x2,y2,duration]
    std::string shell_cmd;                  // Shell 命令

    // ---- 延迟 ----
    int pre_delay = 0;                      // 动作前延迟(ms)
    int post_delay = 500;                   // 动作后延迟(ms)

    // ---- 失败策略 ----
    bool repeat_until_failed = false;       // true: 反复执行直到识别失败
    bool optional = false;                  // true: 识别失败时跳过该节点
    int on_fail_jump = -1;                  // 识别失败时跳转到的节点索引，-1 表示不跳转
};

// 任务配置
struct TaskConfig {
    std::string name;
    std::vector<TaskNode> nodes;
    bool loop = false;
    int loop_count = 1;
};