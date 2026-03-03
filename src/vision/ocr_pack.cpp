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

bool OcrPack::findTemplate(const cv::Mat& scene, const cv::Mat& templ,
                           ImagePreprocessor::Strategy strategy,
                           double threshold, cv::Point& out_pos) {
    if (scene.empty() || templ.empty()) return false;
    if (templ.rows > scene.rows || templ.cols > scene.cols) return false;

    // 使用 ImagePreprocessor 对场景图和模板图做相同的预处理
    cv::Mat proc_scene = ImagePreprocessor::process(scene, strategy);
    cv::Mat proc_templ = ImagePreprocessor::process(templ, strategy);

    // 预处理后可能变为单通道，需要保证两者通道数一致
    if (proc_scene.channels() != proc_templ.channels()) {
        if (proc_scene.channels() == 1)
            cv::cvtColor(proc_scene, proc_scene, cv::COLOR_GRAY2BGR);
        if (proc_templ.channels() == 1)
            cv::cvtColor(proc_templ, proc_templ, cv::COLOR_GRAY2BGR);
    }

    cv::Mat result;
    cv::matchTemplate(proc_scene, proc_templ, result, cv::TM_CCOEFF_NORMED);

    double max_val;
    cv::Point max_loc;
    cv::minMaxLoc(result, nullptr, &max_val, nullptr, &max_loc);

    if (max_val >= threshold) {
        // 返回匹配区域的中心坐标（相对于原始 scene）
        out_pos.x = max_loc.x + templ.cols / 2;
        out_pos.y = max_loc.y + templ.rows / 2;
        return true;
    }
    return false;
}

