#pragma once
#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>
#include <vector>
#include <string>

/**
 * @brief 文本识别器类，基于ONNX模型实现文本内容识别
 *
 * 主要功能：
 *  - 加载文本识别ONNX模型和字符字典
 *  - 对裁剪后的文本区域图像进行预处理
 *  - 推理并解码输出，返回识别的文字内容
 *
 * 工作流程：
 *  1. 接收已裁剪的单行文本图像
 *  2. 预处理：缩放、归一化、标准化
 *  3. 模型推理：输出每个位置的字符概率
 *  4. CTC解码：将概率序列转换为最终文字
 */
class TextRecognizer {
public:
    /**
     * @brief 构造函数，加载ONNX文本识别模型
     * @param env ONNX Runtime环境
     * @param model_path 识别模型文件路径
     * @param dict_path 字符字典文件路径（每行一个字符）
     * @param session_options 会话选项（可配置 CPU/GPU）
     */
    TextRecognizer(Ort::Env& env, const std::string& model_path, const std::string& dict_path, const Ort::SessionOptions& session_options);

    /**
     * @brief 识别图像中的文字内容
     * @param img 输入图像（已裁剪的单行文本区域）
     * @return 识别出的文字字符串
     */
    std::string recognize(const cv::Mat& img);

private:
    Ort::Session session_;                          ///< ONNX推理会话
    Ort::AllocatorWithDefaultOptions allocator_;    ///< ONNX内存分配器
    std::vector<std::string> input_name_strings_;   ///< 输入节点名称字符串
    std::vector<std::string> output_name_strings_;  ///< 输出节点名称字符串
    std::vector<const char*> input_names_;          ///< 输入节点名称指针
    std::vector<const char*> output_names_;         ///< 输出节点名称指针
    std::vector<std::string> characters_;           ///< 字符字典（索引→字符映射）

    /**
     * @brief 加载字符字典文件
     * @param dict_path 字典文件路径
     */
    void loadDict(const std::string& dict_path);

    /**
     * @brief 图像预处理，调整尺寸并归一化、标准化
     * @param img 输入图像
     * @return 预处理后的图像（float32格式）
     */
    cv::Mat preprocess(const cv::Mat& img);
};