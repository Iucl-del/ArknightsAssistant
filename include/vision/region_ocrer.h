#pragma once
#include <opencv2/opencv.hpp>
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <regex>
#include "ocr_pack.h"
#include "image_preprocessor.h"
#include "task_config.h"

/**
 * @brief 区域OCR识别器
 *
 * 根据任务名称加载ROI配置，对特定区域进行优化的OCR识别
 */
class RegionOCRer {
public:
    /**
     * @brief 构造函数
     * @param ocr_pack OCR引擎实例
     * @param config_path 配置文件路径（可选）
     */
    RegionOCRer(std::shared_ptr<OcrPack> ocr_pack, const std::string& config_path = "");

    /**
     * @brief 注册一个OCR任务配置
     * @param task_name 任务名称（如 "UsingMedicine-SanityMax"）
     * @param config ROI配置
     */
    void registerTask(const std::string& task_name, const ROIConfig& config);

    /**
     * @brief 列出所有已注册的任务
     */
    void listAllTasks() const;

    /**
     * @brief 执行OCR识别
     * @param task_name 任务名称
     * @param screen_img 全屏截图
     * @return 识别结果（经过后处理的文字）
     */
    std::string recognize(const std::string& task_name, const cv::Mat& screen_img);

    /**
     * @brief 执行OCR识别（使用默认ROI）
     * @param screen_img 全屏截图
     * @param roi 感兴趣区域
     * @param preprocess_strategy 预处理策略（可选）
     * @return 识别结果
     */
    std::string recognizeROI(const cv::Mat& screen_img,
                            const cv::Rect& roi,
                            ImagePreprocessor::Strategy preprocess_strategy = ImagePreprocessor::Strategy::AUTO);

    /**
     * @brief 加载配置文件
     * @param config_path 配置文件路径（JSON格式）
     */
    void loadConfig(const std::string& config_path);

private:
    std::shared_ptr<OcrPack> ocr_pack_;     ///< OCR引擎

    /**
     * @brief 从全屏图像中提取ROI
     */
    cv::Mat extractROI(const cv::Mat& screen_img, const cv::Rect& roi);

    /**
     * @brief 应用后处理规则
     */
    std::string applyPostProcess(const std::string& raw_text, const ROIConfig& config);

    /**
     * @brief 保存调试图像
     */
    void saveDebugImage(const std::string& task_name, const cv::Mat& roi_img, const cv::Mat& processed_img);
};
