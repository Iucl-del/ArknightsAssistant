#include "SimpleController.hpp"
#include "Config.hpp"
#include <thread>
#include <chrono>
#include <fstream>
#include <format>
#include <json/json.h>
#include <opencv2/opencv.hpp>

SimpleController::SimpleController() {
    // 初始化 OCR 模块
    std::string model_dir = std::string(Config::PROJECT_ROOT_DIR) + "/models/onnx/";
    std::string dict_path = std::string(Config::PROJECT_ROOT_DIR) + "/models/ppocr_keys_v1.txt";
    vision_api_ = std::make_unique<OcrPack>(
        model_dir + "ch_ppocr_det.onnx",
        model_dir + "ch_ppocr_rec.onnx",
        dict_path
    );
}

SimpleController::~SimpleController() = default;

bool SimpleController::connect(const std::string& adb_path, const std::string& address, const std::string& config_path) {
    adb_path_ = adb_path;
    device_address_ = address;
    config_path_ = config_path;
    work_dir_ = adb_path;
    game_package_ = "com.hypergryph.arknights/com.u8.sdk.U8UnityContext";

    // 如果提供了配置文件，从中读取 game_package
    if (!config_path.empty()) {
        std::ifstream cfg(config_path);
        if (cfg.is_open()) {
            Json::Value root;
            Json::CharReaderBuilder builder;
            std::string errors;
            if (Json::parseFromStream(builder, cfg, &root, &errors)) {
                game_package_ = root.get("game_package", game_package_).asString();
            }
        }
    }

    adb_client_ = std::make_unique<ADBClient>(adb_path);

    return adb_client_->connect(address.substr(0, address.find(':')), address.substr(address.find(':')+1));
}

bool SimpleController::capture_screenshot(const std::string& filename) {
    if (!adb_client_) return false;
    return adb_client_->capture_screenshot(device_address_, filename);
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

bool SimpleController::click(int x, int y) {
    if (!adb_client_) return false;
    std::string cmd = std::format("input tap {} {}", x, y);
    adb_client_->shell(device_address_, cmd);
    return true;
}

bool SimpleController::swipe(int x1, int y1, int x2, int y2, int duration_ms) {
    if (!adb_client_) return false;
    std::string cmd = std::format("input swipe {} {} {} {} {}", x1, y1, x2, y2, duration_ms);
    adb_client_->shell(device_address_, cmd);
    return true;
}

void SimpleController::wait(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

bool SimpleController::start_app() {
    if (!adb_client_) return false;
    adb_client_->shell(device_address_, "am start -n " + game_package_);
    return true;
}

bool SimpleController::stop_app() {
    if (!adb_client_) return false;
    // 提取包名（去掉 Activity 部分）
    std::string pkg = game_package_.substr(0, game_package_.find('/'));
    adb_client_->shell(device_address_, "am force-stop " + pkg);
    return true;
}

void SimpleController::shell(const std::string& cmd) {
    if (adb_client_) {
        adb_client_->shell(device_address_, cmd);
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

bool SimpleController::find_template(const std::string& image_path, const std::string& template_path, int& out_x, int& out_y) {
    std::string full_image_path = work_dir_ + "/" + image_path;
    std::string full_template_path = std::string(Config::PROJECT_ROOT_DIR) + "/" + template_path;
    cv::Mat img = cv::imread(full_image_path);
    cv::Mat templ = cv::imread(full_template_path);
    if (img.empty() || templ.empty()) return false;

    cv::Mat result;
    cv::matchTemplate(img, templ, result, cv::TM_CCOEFF_NORMED);

    double minVal, maxVal;
    cv::Point minLoc, maxLoc;
    cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);

    if (maxVal > 0.8) {  // 匹配阈值
        out_x = maxLoc.x + templ.cols / 2;
        out_y = maxLoc.y + templ.rows / 2;
        return true;
    }
    return false;
}

bool SimpleController::find_text(const std::string& image_path, const std::string& target_text, int& out_x, int& out_y) {
    if (!vision_api_) return false;
    std::string full_path = work_dir_ + "/" + image_path;
    cv::Mat img = cv::imread(full_path);
    if (img.empty()) return false;

    auto results = vision_api_->recognizeAll(img);
    for (const auto& [box, text] : results) {
        if (text.find(target_text) != std::string::npos) {
            // 计算文本框中心点
            float cx = 0, cy = 0;
            for (const auto& pt : box.box) {
                cx += pt.x;
                cy += pt.y;
            }
            out_x = static_cast<int>(cx / static_cast<float>(box.box.size()));
            out_y = static_cast<int>(cy / static_cast<float>(box.box.size()));
            return true;
        }
    }
    return false;
}



