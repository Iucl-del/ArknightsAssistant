#include "SimpleController.hpp"
#include "Config.hpp"
#include "vision/ImagePreprocessor.hpp"
#include <thread>
#include <chrono>
#include <format>
#include <opencv2/opencv.hpp>
#include "TaskExecutor.hpp"

SimpleController::SimpleController() {
    // 初始化 OCR 模块
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
    return controller_->connect(ip, port);
}

bool SimpleController::capture_screenshot(const std::string& filename) {
    if (!controller_) return false;
    return controller_->capture_screenshot(device_address_, filename);
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
    std::string cmd = std::format("input tap {} {}", pos.x, pos.y);
    controller_->shell(device_address_, cmd);
    return true;
}

bool SimpleController::swipe(const cv::Point& from, const cv::Point& to, int duration_ms) {
    if (!controller_) return false;
    std::string cmd = std::format("input swipe {} {} {} {} {}", from.x, from.y, to.x, to.y, duration_ms);
    controller_->shell(device_address_, cmd);
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
    // 提取包名（去掉 Activity 部分）
    std::string pkg = game_package_.substr(0, game_package_.find('/'));
    controller_->shell(device_address_, "am force-stop " + pkg);
    return true;
}

void SimpleController::shell(const std::string& cmd) {
    if (controller_) {
        controller_->shell(device_address_, cmd);
    }
}

bool SimpleController::detect_text(const std::string& image_path, std::string& out_text,
                                    std::optional<ROI> roi) {
    if (!vision_api_) return false;
    std::string full_path = work_dir_ + "/" + image_path;
    cv::Mat img = cv::imread(full_path);
    if (img.empty()) return false;

    cv::Mat target = img;
    if (roi.has_value()) {
        const auto& r = roi.value();
        float scale_x = static_cast<float>(img.cols) / r.base_w;
        float scale_y = static_cast<float>(img.rows) / r.base_h;
        int sx = std::max(0, std::min(static_cast<int>(r.x * scale_x), img.cols - 1));
        int sy = std::max(0, std::min(static_cast<int>(r.y * scale_y), img.rows - 1));
        int sw = std::min(static_cast<int>(r.w * scale_x), img.cols - sx);
        int sh = std::min(static_cast<int>(r.h * scale_y), img.rows - sy);
        target = img(cv::Rect(sx, sy, sw, sh));
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
            float cx = 0, cy = 0;
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
                                                      cv::Point& out_pos) {
    if (!vision_api_) return false;

    std::string full_path = work_dir_ + "/" + image_path;
    cv::Mat scene = cv::imread(full_path);
    if (scene.empty()) return false;

    // 轮询匹配多个模板，任一匹配即返回成功
    for (const auto& tpl_path : template_paths) {
        std::string tpl_full = std::string(Config::PROJECT_ROOT_DIR) + "/" + tpl_path;
        cv::Mat templ = cv::imread(tpl_full);
        if (templ.empty()) continue;

        if (vision_api_->findTemplate(scene, templ, strategy, threshold, out_pos)) {
            return true;
        }
    }
    return false;
}
