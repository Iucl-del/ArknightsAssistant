#include "ocr_rec.h"
#include <fstream>
#include <iostream>

TextRecognizer::TextRecognizer(Ort::Env& env, const std::string& model_path,
                               const std::string& dict_path)
    : session_(env, model_path.c_str(), Ort::SessionOptions{nullptr}) {

    loadDict(dict_path);

    size_t num_input = session_.GetInputCount();
    size_t num_output = session_.GetOutputCount();

    // 保存节点名称的临时字符串
    input_name_strings_.resize(num_input);
    output_name_strings_.resize(num_output);

    for (size_t i = 0; i < num_input; i++) {
        auto name = session_.GetInputNameAllocated(i, allocator_);
        input_name_strings_[i] = name.get();
        input_names_.push_back(input_name_strings_[i].c_str());
    }
    for (size_t i = 0; i < num_output; i++) {
        auto name = session_.GetOutputNameAllocated(i, allocator_);
        output_name_strings_[i] = name.get();
        output_names_.push_back(output_name_strings_[i].c_str());
    }
}

void TextRecognizer::loadDict(const std::string& dict_path) {
    std::ifstream file(dict_path);
    if (!file.is_open()) {
        std::cerr << "无法打开字典文件: " << dict_path << std::endl;
        return;
    }

    characters_.push_back(" "); // blank
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            characters_.push_back(line);
        }
    }
    file.close();
}

cv::Mat TextRecognizer::preprocess(const cv::Mat& img) {
    int img_h = 48;
    int img_w = 320;

     cv::Mat processed = img.clone();

    // 如果图片太小，先放大
    if (img.rows < 20) {
        float scale = 20.0f / img.rows;
        cv::resize(img, processed, cv::Size(), scale, scale, cv::INTER_CUBIC);
    }

    // 转为灰度图
    cv::Mat gray;
    if (processed.channels() == 3) {
        cv::cvtColor(processed, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = processed.clone();
    }

    // 自适应直方图均衡化，增强对比度
    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(2.0, cv::Size(8, 8));
    cv::Mat enhanced;
    clahe->apply(gray, enhanced);

    // 转回3通道
    cv::Mat enhanced_bgr;
    cv::cvtColor(enhanced, enhanced_bgr, cv::COLOR_GRAY2BGR);

    // 计算缩放比例
    float ratio = enhanced_bgr.cols * 1.0f / enhanced_bgr.rows;
    int resize_w = std::min(int(img_h * ratio), img_w);

    cv::Mat resized;
    cv::resize(enhanced_bgr, resized, cv::Size(resize_w, img_h), 0, 0, cv::INTER_CUBIC);

    if (resize_w < img_w) {
        cv::copyMakeBorder(resized, resized, 0, 0, 0, img_w - resize_w,
                          cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));
    }

    resized.convertTo(resized, CV_32FC3, 1.0 / 255.0);

    cv::Mat normalized;
    cv::subtract(resized, cv::Scalar(0.5, 0.5, 0.5), normalized);
    cv::divide(normalized, cv::Scalar(0.5, 0.5, 0.5), normalized);

    return normalized;
}

std::string TextRecognizer::recognize(const cv::Mat& img) {
    cv::Mat input = preprocess(img);

    std::vector<int64_t> input_shape = {1, 3, input.rows, input.cols};
    size_t input_tensor_size = 1 * 3 * input.rows * input.cols;
    std::vector<float> input_tensor_values(input_tensor_size);

    std::vector<cv::Mat> channels(3);
    cv::split(input, channels);
    for (int c = 0; c < 3; c++) {
        std::memcpy(input_tensor_values.data() + c * input.rows * input.cols,
                   channels[c].data, input.rows * input.cols * sizeof(float));
    }

    auto memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
        memory_info, input_tensor_values.data(), input_tensor_size,
        input_shape.data(), input_shape.size());

    auto output_tensors = session_.Run(Ort::RunOptions{nullptr},
                                       input_names_.data(), &input_tensor, 1,
                                       output_names_.data(), 1);

    float* output = output_tensors[0].GetTensorMutableData<float>();
    auto output_shape = output_tensors[0].GetTensorTypeAndShapeInfo().GetShape();

    int seq_len = output_shape[1];
    int num_classes = output_shape[2];

    std::string result;
    int last_idx = 0;
    for (int i = 0; i < seq_len; i++) {
        int max_idx = 0;
        float max_val = output[i * num_classes];
        for (int j = 1; j < num_classes; j++) {
            if (output[i * num_classes + j] > max_val) {
                max_val = output[i * num_classes + j];
                max_idx = j;
            }
        }

        if (max_idx != 0 && max_idx != last_idx) {
            if (max_idx < characters_.size()) {
                result += characters_[max_idx];
            }
        }
        last_idx = max_idx;
    }

    return result;
}
