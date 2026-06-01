#include "OcrPack.hpp"
#include "Logger.hpp"
#include <algorithm>
#include <chrono>
#ifdef USE_CUDA
#include <cuda_runtime.h>
#endif

static bool hasCudaExecutionProvider() {
#ifndef USE_CUDA
    return false;
#else
    int device_count = 0;
    if (cudaGetDeviceCount(&device_count) != cudaSuccess || device_count <= 0) {
        return false;
    }

    const auto providers = Ort::GetAvailableProviders();
    return std::find(providers.begin(), providers.end(), "CUDAExecutionProvider") != providers.end();
#endif
}

static Ort::SessionOptions createSessionOptions(DeviceType device) {
    Ort::SessionOptions options;

    if (device == DeviceType::GPU) {
#ifdef USE_CUDA
        OrtCUDAProviderOptions cuda_options{};
        cuda_options.device_id = 0;
        options.AppendExecutionProvider_CUDA(cuda_options);
        Logger::info("[OcrPack] 使用运行时推理后端");
#else
        Logger::warning("[OcrPack] CUDA 不可用，回退到 CPU");
#endif
    } else {
        Logger::info("[OcrPack] 使用运行时推理后端");
    }

    return options;
}

cv::Mat getRotateCropImage(const cv::Mat& img, const std::vector<cv::Point2f>& box) {
    std::vector<cv::Point2f> pts = box;

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
                 const std::string& dict_path,
                 DeviceType device) {
    env_ = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_ERROR, "OcrPack");
    DeviceType actual_device = device;
    if (device == DeviceType::GPU && !hasCudaExecutionProvider()) {
        Logger::warning("[OcrPack] CUDA 不可用，回退到 CPU");
        actual_device = DeviceType::CPU;
    }

    Ort::SessionOptions session_options = createSessionOptions(actual_device);

    detector_ = std::make_unique<TextDetector>(*env_, det_model_path, session_options);
    recognizer_ = std::make_unique<TextRecognizer>(*env_, rec_model_path, dict_path, session_options);
}

std::vector<std::pair<TextBox, std::string>> OcrPack::recognizeAll(const cv::Mat& img) {
    std::vector<std::pair<TextBox, std::string>> results;

    std::vector<TextBox> boxes = detector_->detect(img);
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

    auto total_start = std::chrono::steady_clock::now();
    auto scene_process_start = std::chrono::steady_clock::now();
    cv::Mat proc_scene = ImagePreprocessor::process(scene, strategy);
    auto scene_process_end = std::chrono::steady_clock::now();
    Logger::debug("[耗时][模板匹配] 场景预处理={}ms 场景={}x{} 模板={}x{}",std::chrono::duration_cast<std::chrono::milliseconds>(scene_process_end - scene_process_start).count(),scene.cols,scene.rows,templ.cols,templ.rows);

    double best_val = -1.0;
    cv::Point best_loc;
    cv::Size best_size;
    const std::vector<double> scales = {0.75, 0.9, 1.0, 1.1, 1.25, 1.5};

    for (double scale : scales) {
        auto scale_start = std::chrono::steady_clock::now();
        cv::Mat scaled_templ;
        cv::resize(templ, scaled_templ, cv::Size(), scale, scale, cv::INTER_AREA);
        auto resize_end = std::chrono::steady_clock::now();
        if (scaled_templ.empty()) continue;
        if (scaled_templ.rows > scene.rows || scaled_templ.cols > scene.cols) continue;

        cv::Mat proc_templ = ImagePreprocessor::process(scaled_templ, strategy);
        auto templ_process_end = std::chrono::steady_clock::now();
        cv::Mat match_scene = proc_scene;

        if (match_scene.channels() != proc_templ.channels()) {
            if (match_scene.channels() == 1) {
                cv::cvtColor(match_scene, match_scene, cv::COLOR_GRAY2BGR);
            }
            if (proc_templ.channels() == 1) {
                cv::cvtColor(proc_templ, proc_templ, cv::COLOR_GRAY2BGR);
            }
        }

        cv::Mat result;
        cv::matchTemplate(match_scene, proc_templ, result, cv::TM_CCOEFF_NORMED);
        auto match_end = std::chrono::steady_clock::now();

        double max_val = 0.0;
        cv::Point max_loc;
        cv::minMaxLoc(result, nullptr, &max_val, nullptr, &max_loc);
        auto minmax_end = std::chrono::steady_clock::now();
        Logger::debug("[耗时][模板匹配][缩放={}] 调整模板={}ms 模板预处理={}ms 匹配={}ms 取最大值={}ms 分数={} 尺寸={}x{} 位置=({}, {})",scale,std::chrono::duration_cast<std::chrono::milliseconds>(resize_end - scale_start).count(),std::chrono::duration_cast<std::chrono::milliseconds>(templ_process_end - resize_end).count(),std::chrono::duration_cast<std::chrono::milliseconds>(match_end - templ_process_end).count(),std::chrono::duration_cast<std::chrono::milliseconds>(minmax_end - match_end).count(),max_val,scaled_templ.cols,scaled_templ.rows,max_loc.x,max_loc.y);
        if (max_val > best_val) {
            best_val = max_val;
            best_loc = max_loc;
            best_size = scaled_templ.size();
        }
    }

    Logger::debug("[模板匹配] 最佳分数={} 阈值={} 最佳尺寸={}x{} 总耗时={}ms",best_val,threshold,best_size.width,best_size.height,std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - total_start).count());

    if (best_val >= threshold) {
        out_pos.x = best_loc.x + best_size.width / 2;
        out_pos.y = best_loc.y + best_size.height / 2;
        return true;
    }
    return false;
}
