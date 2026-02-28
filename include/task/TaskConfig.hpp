#pragma once
#include <string>
#include <vector>
#include <optional>
#include "vision/vision_types.h"

// ============================================================
// 任务节点
// 每个节点 = 识别 + 动作，执行器自动轮询截图直到识别通过再执行动作
// JSON 示例:
//   { "recognition": "OCR", "expected": "开始唤醒", "action": "Click" }
//   { "recognition": "DirectHit", "action": "Click", "target": [960, 540] }
// ============================================================
struct TaskNode {

    // ---- 识别 ----
    std::string recognition = "DirectHit";  // DirectHit | OCR | TemplateMatch
    std::string expected;                   // OCR 期望匹配文本
    std::string template_path;              // TemplateMatch 模板图路径
    std::optional<ROI> roi;                 // 识别区域（可选）
    int timeout = 20000;                    // 识别超时(ms)
    int interval = 1000;                    // 识别轮询间隔(ms)

    // ---- 动作 ----
    std::string action = "Click";           // Click | Swipe | Shell
    std::vector<int> target;                // Click 坐标 [x,y]；Swipe 为 [x1,y1,x2,y2,duration]
    std::string shell_cmd;                  // Shell 命令（内部使用，JSON 不暴露）

    // ---- 延迟 ----
    int pre_delay = 0;                      // 动作前延迟(ms)
    int post_delay = 500;                   // 动作后延迟(ms)
};

// 任务配置
struct TaskConfig {
    std::string name;
    std::vector<TaskNode> nodes;
    bool loop = false;
    int loop_count = 1;
};
