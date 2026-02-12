#pragma once
#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>
#include <vector>
#include <string>

class TextRecognizer {
public:
    TextRecognizer(Ort::Env& env, const std::string& model_path, const std::string& dict_path);
    std::string recognize(const cv::Mat& img);

private:
    Ort::Session session_;
    Ort::AllocatorWithDefaultOptions allocator_;
    std::vector<std::string> input_name_strings_;
    std::vector<std::string> output_name_strings_;
    std::vector<const char*> input_names_;
    std::vector<const char*> output_names_;
    std::vector<std::string> characters_;

    void loadDict(const std::string& dict_path);
    cv::Mat preprocess(const cv::Mat& img);
};
