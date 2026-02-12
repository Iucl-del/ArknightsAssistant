#include "task_config.h"
#include <iostream>
#include <iomanip>

TaskConfigManager& TaskConfigManager::getInstance() {
    static TaskConfigManager instance;
    return instance;
}

void TaskConfigManager::registerTask(const std::string& task_name, const ROIConfig& config) {
    task_configs_[task_name] = config;
    std::cout << "✅ 注册任务: " << task_name;
    if (!config.description.empty()) {
        std::cout << " - " << config.description;
    }
    std::cout << std::endl;
}

ROIConfig TaskConfigManager::getTaskConfig(const std::string& task_name) const {
    auto it = task_configs_.find(task_name);
    if (it != task_configs_.end()) {
        return it->second;
    }
    std::cerr << "⚠️  任务未找到: " << task_name << std::endl;
    return ROIConfig();
}

bool TaskConfigManager::hasTask(const std::string& task_name) const {
    return task_configs_.find(task_name) != task_configs_.end();
}

std::vector<std::string> TaskConfigManager::listTasks() const {
    std::vector<std::string> tasks;
    for (const auto& pair : task_configs_) {
        tasks.push_back(pair.first);
    }
    return tasks;
}

void TaskConfigManager::printAllTasks() const {
    std::cout << "\n========================================\n";
    std::cout << "已注册的任务列表:\n";
    std::cout << "========================================\n";

    if (task_configs_.empty()) {
        std::cout << "  (无任务)\n";
        return;
    }

    int index = 1;
    for (const auto& pair : task_configs_) {
        const std::string& name = pair.first;
        const ROIConfig& config = pair.second;

        std::cout << "\n[" << index++ << "] " << name << "\n";
        if (!config.description.empty()) {
            std::cout << "    描述: " << config.description << "\n";
        }
        std::cout << "    ROI: x=" << config.roi.x
                  << ", y=" << config.roi.y
                  << ", w=" << config.roi.width
                  << ", h=" << config.roi.height << "\n";
        std::cout << "    预处理策略: ";
        switch (config.preprocess_strategy) {
            case ImagePreprocessor::Strategy::NONE: std::cout << "无"; break;
            case ImagePreprocessor::Strategy::GRAYSCALE: std::cout << "灰度化"; break;
            case ImagePreprocessor::Strategy::BINARY: std::cout << "二值化"; break;
            case ImagePreprocessor::Strategy::ADAPTIVE_BINARY: std::cout << "自适应二值化"; break;
            case ImagePreprocessor::Strategy::DENOISE: std::cout << "去噪"; break;
            case ImagePreprocessor::Strategy::ENHANCE_CONTRAST: std::cout << "增强对比度"; break;
            case ImagePreprocessor::Strategy::AUTO: std::cout << "自动"; break;
        }
        std::cout << "\n";

        if (!config.replace_rules.empty()) {
            std::cout << "    替换规则: " << config.replace_rules.size() << " 条\n";
        }
        if (!config.filter_pattern.empty()) {
            std::cout << "    过滤模式: " << config.filter_pattern << "\n";
        }
        std::cout << "    调试保存: " << (config.debug_save ? "是" : "否") << "\n";
    }
    std::cout << "========================================\n\n";
}

void TaskConfigManager::initDefaultTasks() {
    // ========================================
    // 所有 ROI 坐标基于 1280x720 基准分辨率定义
    // 系统会根据实际输入图片分辨率自动缩放
    // ========================================

    // 任务1: 当前理智值识别（游戏主页面）
    // 基准分辨率: 1280x720
    // 全页面扫描结果: "+98" (当前理智) 位于 x=1675-2021, y=232-375 (2800x1260)
    // 换算到 1280x720: x≈765-923, y≈133-214
    // 当前理智值显示在最大理智值上方，格式为 "+数字"
    {
        ROIConfig config;
        config.base_width = 1280;
        config.base_height = 720;
        config.roi = cv::Rect(800, 145, 100, 50);  // 缩小区域，精确覆盖当前理智值
        config.preprocess_strategy = ImagePreprocessor::Strategy::BINARY;  // 二值化预处理
        config.replace_rules = {
            {"[oO]", "0"},    // o/O -> 0
            {"[lI|]", "1"},   // l/I/| -> 1
            {"[S]", "5"},     // S -> 5
            {"[B]", "8"},     // B -> 8
            {"[+＋]", ""},    // 移除加号
            {"[^0-9]", ""}    // 只保留数字
        };
        config.filter_pattern = "[0-9]+";  // 提取数字
        config.debug_save = true;
        config.description = "主页面 - 识别当前理智值";
        registerTask("Sanity-Current", config);
    }

    // 任务2: 龙门币数量识别（游戏主页面）
    // 基准分辨率: 1280x720
    // 全页面扫描结果: "824784" 位于 x=1701-1970, y=86-153 (2800x1260)
    // 换算到 1280x720: x≈777-900, y≈49-87
    {
        ROIConfig config;
        config.base_width = 1280;
        config.base_height = 720;
        config.roi = cv::Rect(770, 45, 140, 50);  // 覆盖龙门币显示区域
        config.preprocess_strategy = ImagePreprocessor::Strategy::NONE;  // 不预处理，保持原样
        config.replace_rules = {
            {"[oO]", "0"},    // o/O -> 0
            {"[lI|]", "1"},   // l/I/| -> 1
            {"[^0-9]", ""}    // 只保留数字
        };
        config.filter_pattern = "[0-9]+";
        config.debug_save = true;
        config.description = "主页面 - 识别龙门币数量";
        registerTask("Money", config);
    }

    // 任务3: 合成玉数量识别（游戏主页面）
    // 基准分辨率: 1280x720
    // 全页面扫描结果: "4920+" 位于 x=2018-2292, y=53-124 (2800x1260)
    // 换算到 1280x720: x≈922-1048, y≈30-71
    {
        ROIConfig config;
        config.base_width = 1280;
        config.base_height = 720;
        config.roi = cv::Rect(920, 28, 130, 50);  // 覆盖合成玉显示区域
        config.preprocess_strategy = ImagePreprocessor::Strategy::NONE;  // 不预处理，保持原样
        config.replace_rules = {
            {"[oO]", "0"},    // o/O -> 0
            {"[lI|]", "1"},   // l/I/| -> 1
            {"[+＋]", ""},    // 移除加号
            {"[^0-9]", ""}    // 只保留数字
        };
        config.filter_pattern = "[0-9]+";
        config.debug_save = true;
        config.description = "主页面 - 识别合成玉数量";
        registerTask("Orundum", config);
    }

    // 任务4: 自定义测试区域
    {
        ROIConfig config;
        config.base_width = 1280;
        config.base_height = 720;
        config.roi = cv::Rect(0, 0, 300, 100);  // 将在运行时动态设置为图像中心
        config.preprocess_strategy = ImagePreprocessor::Strategy::AUTO;
        config.replace_rules = {};
        config.filter_pattern = "";
        config.debug_save = true;
        config.description = "自定义测试区域（图像中心）";
        registerTask("Custom", config);
    }

    std::cout << "✅ 已加载 " << task_configs_.size() << " 个默认任务配置\n";
    std::cout << "📐 所有 ROI 坐标基于 1280x720 基准分辨率，会自动适配实际分辨率\n";
}
