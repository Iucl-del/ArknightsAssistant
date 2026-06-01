#include "SimpleController.hpp"
#include "Config.hpp"
#include "Logger.hpp"
#include "TaskExecutor.hpp"
#include "vision/ImagePreprocessor.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <format>
#include <thread>

#include <opencv2/opencv.hpp>

namespace {
constexpr int kVisionBaseWidth = 1280;
constexpr int kVisionBaseHeight = 720;

cv::Rect make_scaled_roi(const ROI& roi, const cv::Size& image_size) {
    float scale_x = static_cast<float>(image_size.width) / static_cast<float>(roi.base_w);
    float scale_y = static_cast<float>(image_size.height) / static_cast<float>(roi.base_h);
    int sx = std::clamp(static_cast<int>(std::lround(roi.x * scale_x)), 0, image_size.width - 1);
    int sy = std::clamp(static_cast<int>(std::lround(roi.y * scale_y)), 0, image_size.height - 1);
    int sw = std::clamp(static_cast<int>(std::lround(roi.w * scale_x)), 1, image_size.width - sx);
    int sh = std::clamp(static_cast<int>(std::lround(roi.h * scale_y)), 1, image_size.height - sy);
    return cv::Rect(sx, sy, sw, sh);
}
}

SimpleController::SimpleController() {
    std::string model_dir = std::string(Config::PROJECT_ROOT_DIR) + "/models/onnx/";
    std::string dict_path = std::string(Config::PROJECT_ROOT_DIR) + "/models/ppocr_keys_v1.txt";
    vision_api_ = std::make_unique<OcrPack>(
        model_dir + "ch_ppocr_det.onnx",
        model_dir + "ch_ppocr_rec.onnx",
        dict_path,
        DeviceType::GPU
    );
}

SimpleController::~SimpleController() = default;

bool SimpleController::connect(const std::string& adb_path, const std::string& ip, const std::string& port) {
    device_address_ = ip + ":" + port;
    work_dir_ = adb_path;
    game_package_ = "com.hypergryph.arknights/com.u8.sdk.U8UnityContext";

    controller_ = std::make_unique<ADBClient>(adb_path);
    if (!controller_->connect(ip, port)) {
        return false;
    }

    executor_ = std::make_shared<TaskExecutor>(*this);
    return true;
}

bool SimpleController::capture_screenshot(const std::string& filename) {
    if (!controller_) return false;
    if (!controller_->capture_screenshot(device_address_, filename)) {
        return false;
    }

    std::string full_path = work_dir_ + "/" + filename;
    cv::Mat original = cv::imread(full_path);
    if (original.empty()) {
        return false;
    }

    input_scale_x_ = static_cast<double>(original.cols) / kVisionBaseWidth;
    input_scale_y_ = static_cast<double>(original.rows) / kVisionBaseHeight;

    if (original.cols == kVisionBaseWidth && original.rows == kVisionBaseHeight) {
        return true;
    }

    cv::Mat resized;
    cv::resize(original, resized, cv::Size(kVisionBaseWidth, kVisionBaseHeight), 0, 0, cv::INTER_AREA);
    return cv::imwrite(full_path, resized);
}

std::string SimpleController::auto_screenshot(const std::string& hint) {
    int seq = screenshot_seq_.fetch_add(1);
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count() % 1000000;
    std::string name = "_auto_" + hint + "_" + std::to_string(seq) + "_" + std::to_string(ms) + ".png";
    if (capture_screenshot(name)) {
        return name;
    }
    return "";
}

bool SimpleController::click(const cv::Point& pos) {
    if (!controller_) return false;
    int x = static_cast<int>(std::lround(pos.x * input_scale_x_));
    int y = static_cast<int>(std::lround(pos.y * input_scale_y_));
    controller_->shell(device_address_, std::format("input tap {} {}", x, y));
    return true;
}

bool SimpleController::swipe(const cv::Point& from, const cv::Point& to, int duration_ms) {
    if (!controller_) return false;
    int from_x = static_cast<int>(std::lround(from.x * input_scale_x_));
    int from_y = static_cast<int>(std::lround(from.y * input_scale_y_));
    int to_x = static_cast<int>(std::lround(to.x * input_scale_x_));
    int to_y = static_cast<int>(std::lround(to.y * input_scale_y_));
    controller_->shell(device_address_, std::format("input swipe {} {} {} {} {}", from_x, from_y, to_x, to_y, duration_ms));
    return true;
}

void SimpleController::wait(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

bool SimpleController::start_app() {
    if (!controller_) return false;
    controller_->shell(device_address_, "am start -n " + game_package_);
    return true;
}

bool SimpleController::stop_app() {
    if (!controller_) return false;
    std::string pkg = game_package_.substr(0, game_package_.find('/'));
    controller_->shell(device_address_, "am force-stop " + pkg);
    return true;
}

void SimpleController::shell(const std::string& cmd) {
    if (controller_) {
        controller_->shell(device_address_, cmd);
    }
}

bool SimpleController::detect_text(const std::string& image_path, std::string& out_text, std::optional<ROI> roi) {
    if (!vision_api_) return false;
    std::string full_path = work_dir_ + "/" + image_path;
    cv::Mat img = cv::imread(full_path);
    if (img.empty()) return false;

    cv::Mat target = img;
    if (roi.has_value()) {
        target = img(make_scaled_roi(roi.value(), img.size()));
    }

    auto results = vision_api_->recognizeAll(target);
    out_text.clear();
    for (const auto& [box, text] : results) {
        out_text += text + "\n";
    }
    return !out_text.empty();
}

bool SimpleController::find_text(const std::string& image_path, const std::string& target_text, cv::Point& out_pos) {
    if (!vision_api_) return false;
    std::string full_path = work_dir_ + "/" + image_path;
    cv::Mat img = cv::imread(full_path);
    if (img.empty()) return false;

    auto results = vision_api_->recognizeAll(img);
    for (const auto& [box, text] : results) {
        if (text.find(target_text) != std::string::npos) {
            float cx = 0.0f;
            float cy = 0.0f;
            for (const auto& pt : box.box) {
                cx += pt.x;
                cy += pt.y;
            }

            out_pos.x = static_cast<int>(cx / static_cast<float>(box.box.size()));
            out_pos.y = static_cast<int>(cy / static_cast<float>(box.box.size()));
            return true;
        }
    }
    return false;
}

bool SimpleController::find_template_with_preprocess(const std::string& image_path,
                                                      const std::vector<std::string>& template_paths,
                                                      ImagePreprocessor::Strategy strategy,
                                                      double threshold,
                                                      std::optional<ROI> roi,
                                                      cv::Point& out_pos) {
    if (!vision_api_) return false;

    std::string full_path = work_dir_ + "/" + image_path;
    auto scene_read_start = std::chrono::steady_clock::now();
    cv::Mat scene = cv::imread(full_path);
    auto scene_read_end = std::chrono::steady_clock::now();
    if (scene.empty()) return false;
    Logger::debug("[耗时][模板匹配] 读取场景={}ms 尺寸={}x{} 路径={}",
                  std::chrono::duration_cast<std::chrono::milliseconds>(scene_read_end - scene_read_start).count(),
                  scene.cols,
                  scene.rows,
                  full_path);

    cv::Point roi_offset(0, 0);
    cv::Mat search_scene = scene;
    if (roi.has_value()) {
        cv::Rect rect = make_scaled_roi(roi.value(), scene.size());
        roi_offset = rect.tl();
        search_scene = scene(rect);
        Logger::debug("[模板匹配] ROI=({}, {}, {}, {})", rect.x, rect.y, rect.width, rect.height);
    }

    for (const auto& tpl_path : template_paths) {
        std::string tpl_full = std::string(Config::PROJECT_ROOT_DIR) + "/" + tpl_path;
        auto templ_read_start = std::chrono::steady_clock::now();
        cv::Mat templ = cv::imread(tpl_full);
        auto templ_read_end = std::chrono::steady_clock::now();
        if (templ.empty()) continue;
        Logger::debug("[耗时][模板匹配] 读取模板={}ms 尺寸={}x{} 路径={}",
                      std::chrono::duration_cast<std::chrono::milliseconds>(templ_read_end - templ_read_start).count(),
                      templ.cols,
                      templ.rows,
                      tpl_full);

        auto match_start = std::chrono::steady_clock::now();
        cv::Point local_pos;
        if (vision_api_->findTemplate(search_scene, templ, strategy, threshold, local_pos)) {
            auto match_end = std::chrono::steady_clock::now();
            out_pos = local_pos + roi_offset;
            Logger::debug("[耗时][模板匹配] 当前模板总耗时={}ms",
                          std::chrono::duration_cast<std::chrono::milliseconds>(match_end - match_start).count());
            return true;
        }
        auto match_end = std::chrono::steady_clock::now();
        Logger::debug("[耗时][模板匹配] 当前模板总耗时={}ms",
                      std::chrono::duration_cast<std::chrono::milliseconds>(match_end - match_start).count());
    }
    return false;
}
