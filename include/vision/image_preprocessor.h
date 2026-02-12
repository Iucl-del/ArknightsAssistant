#pragma once
#include <opencv2/opencv.hpp>
#include <string>

/**
 * @brief 图像预处理工具类
 *
 * 提供各种图像预处理方法，以增强OCR识别效果
 */
class ImagePreprocessor {
public:
    /**
     * @brief 预处理策略枚举
     */
    enum class Strategy {
        NONE,           ///< 无预处理
        GRAYSCALE,      ///< 灰度化
        BINARY,         ///< 二值化
        ADAPTIVE_BINARY, ///< 自适应二值化
        DENOISE,        ///< 去噪
        ENHANCE_CONTRAST, ///< 增强对比度
        AUTO            ///< 自动选择最佳策略
    };

    /**
     * @brief 应用预处理策略
     * @param img 输入图像
     * @param strategy 预处理策略
     * @return 预处理后的图像
     */
    static cv::Mat process(const cv::Mat& img, Strategy strategy = Strategy::AUTO);

    /**
     * @brief 灰度化
     */
    static cv::Mat toGrayscale(const cv::Mat& img);

    /**
     * @brief 二值化（固定阈值）
     * @param img 输入图像
     * @param threshold 阈值（默认127）
     */
    static cv::Mat toBinary(const cv::Mat& img, int threshold = 127);

    /**
     * @brief 自适应二值化（适用于光照不均匀的情况）
     * @param img 输入图像
     * @param block_size 邻域大小（默认11）
     * @param C 常数（默认2）
     */
    static cv::Mat toAdaptiveBinary(const cv::Mat& img, int block_size = 11, int C = 2);

    /**
     * @brief 去噪（中值滤波）
     * @param img 输入图像
     * @param kernel_size 核大小（默认3）
     */
    static cv::Mat denoise(const cv::Mat& img, int kernel_size = 3);

    /**
     * @brief 增强对比度（直方图均衡化）
     * @param img 输入图像
     */
    static cv::Mat enhanceContrast(const cv::Mat& img);

    /**
     * @brief 自动预处理（组合多种方法）
     * @param img 输入图像
     */
    static cv::Mat autoProcess(const cv::Mat& img);
};
