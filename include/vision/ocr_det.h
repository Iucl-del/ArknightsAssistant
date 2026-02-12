#pragma once
#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>
#include <vector>

struct TextBox {
    std::vector<cv::Point2f> box; ///< 文本框的四个顶点坐标
    float score;                  ///< 检测置信度分数
};

/**
 * @brief 文本检测器类，基于ONNX模型实现文本区域检测。
 *
 * 主要功能：
 *  - 加载文本检测ONNX模型
 *  - 对输入图像进行预处理
 *  - 推理并后处理，输出文本框坐标和置信度
 *  - 提供文本区域的旋转裁剪功能
 */
class TextDetector {
public:
    /**
     * @brief 构造函数，加载ONNX文本检测模型
     * @param env ONNX Runtime环境
     * @param model_path 检测模型文件路径
     */
    TextDetector(Ort::Env& env, const std::string& model_path);

    /**
     * @brief 检测输入图像中的文本区域
     * @param img 输入图像
     * @return 检测到的文本框集合
     */
    std::vector<TextBox> detect(const cv::Mat& img);

private:
    Ort::Session session_; ///< ONNX推理句柄
    Ort::AllocatorWithDefaultOptions allocator_; ///< ONNX内存分配器
    std::vector<std::string> input_name_strings_; ///< 输入节点名称字符串
    std::vector<std::string> output_name_strings_; ///< 输出节点名称字符串
    std::vector<const char*> input_names_; ///< 输入节点名称指针
    std::vector<const char*> output_names_; ///< 输出节点名称指针

    /**
     * @brief 图像预处理，调整尺寸并归一化
     * @param img 输入图像
     * @param ratio_h 输出：高度缩放比例
     * @param ratio_w 输出：宽度缩放比例
     * @return 预处理后的图像
     */
    cv::Mat preprocess(const cv::Mat& img, float& ratio_h, float& ratio_w);

    /**
     * @brief 后处理推理结果，生成文本框
     * @param pred 推理输出的概率图
     * @param ratio_h 高度缩放比例
     * @param ratio_w 宽度缩放比例
     * @return 文本框集合
     */
    std::vector<TextBox> postprocess(const cv::Mat& pred, float ratio_h, float ratio_w);

    /**
     * @brief 对文本区域进行旋转裁剪
     * @param img 原始图像
     * @param box 文本框四个顶点
     * @return 裁剪后的文本区域图像
     */
    cv::Mat getRotateCropImage(const cv::Mat& img, const std::vector<cv::Point2f>& box);
};
