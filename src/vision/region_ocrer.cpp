#include "region_ocrer.h"
#include "task_config.h"
#include <iostream>
#include <fstream>
#include <sys/stat.h>

RegionOCRer::RegionOCRer(std::shared_ptr<OcrPack> ocr_pack, const std::string& config_path)
    : ocr_pack_(ocr_pack) {
    if (!config_path.empty()) {
        loadConfig(config_path);
    }
    std::cout << "✅ RegionOCRer 初始化成功" << std::endl;
}

void RegionOCRer::registerTask(const std::string& task_name, const ROIConfig& config) {
    TaskConfigManager::getInstance().registerTask(task_name, config);
}

void RegionOCRer::listAllTasks() const {
    TaskConfigManager::getInstance().printAllTasks();
}

std::string RegionOCRer::recognize(const std::string& task_name, const cv::Mat& screen_img) {
    // 从 TaskConfigManager 获取任务配置
    if (!TaskConfigManager::getInstance().hasTask(task_name)) {
        std::cerr << "❌ 任务未找到: " << task_name << std::endl;
        return "";
    }

    const ROIConfig config = TaskConfigManager::getInstance().getTaskConfig(task_name);

    // 获取实际图片分辨率
    int actual_width = screen_img.cols;
    int actual_height = screen_img.rows;

    // 根据实际分辨率缩放 ROI
    cv::Rect scaled_roi = config.getScaledROI(actual_width, actual_height);

    std::cout << "📐 分辨率适配:\n";
    std::cout << "   基准分辨率: " << config.base_width << "x" << config.base_height << "\n";
    std::cout << "   实际分辨率: " << actual_width << "x" << actual_height << "\n";
    std::cout << "   原始 ROI: (" << config.roi.x << ", " << config.roi.y
              << ", " << config.roi.width << ", " << config.roi.height << ")\n";
    std::cout << "   缩放 ROI: (" << scaled_roi.x << ", " << scaled_roi.y
              << ", " << scaled_roi.width << ", " << scaled_roi.height << ")\n";

    // 1. 提取ROI
    cv::Mat roi_img = extractROI(screen_img, scaled_roi);
    if (roi_img.empty()) {
        std::cerr << "❌ ROI提取失败" << std::endl;
        return "";
    }

    // 2. 预处理
    cv::Mat processed_img = ImagePreprocessor::process(roi_img, config.preprocess_strategy);

    // 3. 保存调试图像（如果启用）
    if (config.debug_save) {
        saveDebugImage(task_name, roi_img, processed_img);
    }

    // 4. OCR识别
    std::string raw_text = ocr_pack_->recognizeText(processed_img);

    // 5. 后处理
    std::string final_text = applyPostProcess(raw_text, config);

    std::cout << "🔍 [" << task_name << "] 原始: \"" << raw_text
              << "\" -> 处理后: \"" << final_text << "\"" << std::endl;

    return final_text;
}

std::string RegionOCRer::recognizeROI(const cv::Mat& screen_img,
                                     const cv::Rect& roi,
                                     ImagePreprocessor::Strategy preprocess_strategy) {
    // 提取ROI
    cv::Mat roi_img = extractROI(screen_img, roi);
    if (roi_img.empty()) {
        return "";
    }

    // 预处理
    cv::Mat processed_img = ImagePreprocessor::process(roi_img, preprocess_strategy);

    // OCR识别
    return ocr_pack_->recognizeText(processed_img);
}

void RegionOCRer::loadConfig(const std::string& config_path) {
    // TODO: 实现JSON配置文件加载
    // 这里可以使用第三方JSON库（如nlohmann/json）来解析配置文件
    std::cout << "⚠️  配置文件加载功能待实现: " << config_path << std::endl;
}

cv::Mat RegionOCRer::extractROI(const cv::Mat& screen_img, const cv::Rect& roi) {
    // 检查ROI是否在图像范围内
    if (roi.x < 0 || roi.y < 0 ||
        roi.x + roi.width > screen_img.cols ||
        roi.y + roi.height > screen_img.rows) {
        std::cerr << "❌ ROI超出图像范围: " << roi << std::endl;
        return cv::Mat();
    }

    return screen_img(roi).clone();
}

std::string RegionOCRer::applyPostProcess(const std::string& raw_text, const ROIConfig& config) {
    std::string result = raw_text;

    // 1. 应用替换规则
    for (const auto& rule : config.replace_rules) {
        try {
            std::regex pattern(rule.first);
            result = std::regex_replace(result, pattern, rule.second);
        } catch (const std::regex_error& e) {
            std::cerr << "❌ 正则表达式错误: " << e.what() << std::endl;
        }
    }

    // 2. 应用过滤模式（只保留匹配的内容）
    if (!config.filter_pattern.empty()) {
        try {
            std::regex pattern(config.filter_pattern);
            std::smatch match;
            if (std::regex_search(result, match, pattern)) {
                result = match.str();
            } else {
                result = ""; // 没有匹配，返回空字符串
            }
        } catch (const std::regex_error& e) {
            std::cerr << "❌ 正则表达式错误: " << e.what() << std::endl;
        }
    }

    return result;
}

void RegionOCRer::saveDebugImage(const std::string& task_name,
                                const cv::Mat& roi_img,
                                const cv::Mat& processed_img) {
    // 创建调试目录
    std::string debug_dir = "debug_roi";
    mkdir(debug_dir.c_str(), 0755);

    // 生成时间戳
    time_t now = time(nullptr);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", localtime(&now));

    // 保存原始ROI
    std::string roi_path = debug_dir + "/" + task_name + "_" + timestamp + "_roi.jpg";
    cv::imwrite(roi_path, roi_img);

    // 保存预处理后的图像
    std::string processed_path = debug_dir + "/" + task_name + "_" + timestamp + "_processed.jpg";
    cv::imwrite(processed_path, processed_img);

    std::cout << "💾 调试图像已保存: " << roi_path << ", " << processed_path << std::endl;
}
