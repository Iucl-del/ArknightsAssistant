#pragma once
#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>
#include <memory>
#include <string>
#include <vector>
#include "ocr_det.h"
#include "ocr_rec.h"

/**
 * @brief 旋转裁剪图像，用于提取检测到的文本区域
 * @param img 原始图像
 * @param box 四个角点坐标
 * @return 裁剪并校正后的图像
 */
cv::Mat getRotateCropImage(const cv::Mat& img, const std::vector<cv::Point2f>& box);

/**
 * @brief OCR引擎封装类，统一管理检测和识别模型
 *
 * 提供简化的接口，隐藏底层的模型加载和调用细节
 */
class OcrPack {
public:
    /**
     * @brief 构造函数，初始化OCR引擎
     * @param det_model_path 检测模型路径
     * @param rec_model_path 识别模型路径
     * @param dict_path 字典文件路径
     */
    OcrPack(const std::string& det_model_path,
            const std::string& rec_model_path,
            const std::string& dict_path);

    /**
     * @brief 对图像进行完整的OCR识别（检测+识别）
     * @param img 输入图像
     * @return 检测到的文本框和对应识别文字
     */
    std::vector<std::pair<TextBox, std::string>> recognizeAll(const cv::Mat& img);

    /**
     * @brief 仅对图像进行文本识别（不检测，适用于已裁剪的ROI）
     * @param img 输入图像（已裁剪的文本区域）
     * @return 识别出的文字
     */
    std::string recognizeText(const cv::Mat& img);

    /**
     * @brief 检测图像中的文本区域
     * @param img 输入图像
     * @return 检测到的文本框列表
     */
    std::vector<TextBox> detectTextRegions(const cv::Mat& img);

private:
    std::unique_ptr<Ort::Env> env_;           ///< ONNX Runtime环境
    std::unique_ptr<TextDetector> detector_;   ///< 文本检测器
    std::unique_ptr<TextRecognizer> recognizer_; ///< 文本识别器
};
