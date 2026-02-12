#include "ocr_pack.h"
#include <iostream>

// 旋转裁剪图像函数
cv::Mat getRotateCropImage(const cv::Mat& img, const std::vector<cv::Point2f>& box) {
    std::vector<cv::Point2f> pts = box;

    // 排序：左上、右上、右下、左下
    std::sort(pts.begin(), pts.end(), [](const cv::Point2f& a, const cv::Point2f& b) {
        return a.y < b.y;
    });

    std::vector<cv::Point2f> top2 = {pts[0], pts[1]};
    std::vector<cv::Point2f> bottom2 = {pts[2], pts[3]};

    std::sort(top2.begin(), top2.end(), [](const cv::Point2f& a, const cv::Point2f& b) {
        return a.x < b.x;
    });
    std::sort(bottom2.begin(), bottom2.end(), [](const cv::Point2f& a, const cv::Point2f& b) {
        return a.x < b.x;
    });

    std::vector<cv::Point2f> sorted_pts = {top2[0], top2[1], bottom2[1], bottom2[0]};

    float width1 = cv::norm(sorted_pts[0] - sorted_pts[1]);
    float width2 = cv::norm(sorted_pts[2] - sorted_pts[3]);
    float width = std::max(width1, width2);

    float height1 = cv::norm(sorted_pts[0] - sorted_pts[3]);
    float height2 = cv::norm(sorted_pts[1] - sorted_pts[2]);
    float height = std::max(height1, height2);

    std::vector<cv::Point2f> dst_pts = {
        cv::Point2f(0, 0),
        cv::Point2f(width, 0),
        cv::Point2f(width, height),
        cv::Point2f(0, height)
    };

    cv::Mat M = cv::getPerspectiveTransform(sorted_pts, dst_pts);
    cv::Mat warped;
    cv::warpPerspective(img, warped, M, cv::Size(width, height));

    return warped;
}


OcrPack::OcrPack(const std::string& det_model_path,
                 const std::string& rec_model_path,
                 const std::string& dict_path) {
    // 初始化 ONNX Runtime 环境
    env_ = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "OcrPack");

    // 初始化检测器和识别器
    detector_ = std::make_unique<TextDetector>(*env_, det_model_path);
    recognizer_ = std::make_unique<TextRecognizer>(*env_, rec_model_path, dict_path);

    std::cout << "✅ OcrPack 初始化成功" << std::endl;
}

std::vector<std::pair<TextBox, std::string>> OcrPack::recognizeAll(const cv::Mat& img) {
    std::vector<std::pair<TextBox, std::string>> results;

    // 1. 检测文本区域
    std::vector<TextBox> boxes = detector_->detect(img);

    // 2. 对每个区域进行识别
    for (const auto& box : boxes) {
        cv::Mat crop = getRotateCropImage(img, box.box);
        std::string text = recognizer_->recognize(crop);
        results.push_back({box, text});
    }

    return results;
}

std::string OcrPack::recognizeText(const cv::Mat& img) {
    return recognizer_->recognize(img);
}

std::vector<TextBox> OcrPack::detectTextRegions(const cv::Mat& img) {
    return detector_->detect(img);
}
