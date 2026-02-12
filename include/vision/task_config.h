#pragma once
#include <string>
#include <map>
#include <opencv2/opencv.hpp>
#include "image_preprocessor.h"

/**
 * @brief ROI配置结构体
 */
struct ROIConfig {
    cv::Rect roi;                           ///< 感兴趣区域（基于基准分辨率）
    ImagePreprocessor::Strategy preprocess_strategy; ///< 预处理策略
    std::vector<std::pair<std::string, std::string>> replace_rules; ///< 替换规则（正则表达式，替换值）
    std::string filter_pattern;             ///< 过滤模式（正则表达式，只保留匹配的内容）
    bool debug_save;                        ///< 是否保存调试图像
    std::string description;                ///< 任务描述

    // 基准分辨率（用于定义 ROI 坐标的参考分辨率）
    int base_width = 1280;
    int base_height = 720;

    ROIConfig() : roi(0, 0, 100, 50),
                  preprocess_strategy(ImagePreprocessor::Strategy::AUTO),
                  debug_save(false),
                  description("") {}

    /**
     * @brief 根据实际图片分辨率缩放 ROI
     * @param actual_width 实际图片宽度
     * @param actual_height 实际图片高度
     * @return 缩放后的 ROI
     */
    cv::Rect getScaledROI(int actual_width, int actual_height) const {
        float scale_x = static_cast<float>(actual_width) / base_width;
        float scale_y = static_cast<float>(actual_height) / base_height;

        int scaled_x = static_cast<int>(roi.x * scale_x);
        int scaled_y = static_cast<int>(roi.y * scale_y);
        int scaled_w = static_cast<int>(roi.width * scale_x);
        int scaled_h = static_cast<int>(roi.height * scale_y);

        return cv::Rect(scaled_x, scaled_y, scaled_w, scaled_h);
    }
};

/**
 * @brief 任务配置管理器
 *
 * 管理所有任务的ROI配置，支持添加、查询、列出所有任务
 */
class TaskConfigManager {
public:
    /**
     * @brief 获取单例实例
     */
    static TaskConfigManager& getInstance();

    /**
     * @brief 注册一个任务配置
     * @param task_name 任务名称
     * @param config ROI配置
     */
    void registerTask(const std::string& task_name, const ROIConfig& config);

    /**
     * @brief 获取任务配置
     * @param task_name 任务名称
     * @return ROI配置（如果任务不存在，返回空配置）
     */
    ROIConfig getTaskConfig(const std::string& task_name) const;

    /**
     * @brief 检查任务是否存在
     * @param task_name 任务名称
     * @return true 如果任务存在
     */
    bool hasTask(const std::string& task_name) const;

    /**
     * @brief 列出所有任务
     * @return 任务名称列表
     */
    std::vector<std::string> listTasks() const;

    /**
     * @brief 打印所有任务信息
     */
    void printAllTasks() const;

    /**
     * @brief 初始化默认任务配置
     */
    void initDefaultTasks();

private:
    TaskConfigManager() = default;
    std::map<std::string, ROIConfig> task_configs_;
};
