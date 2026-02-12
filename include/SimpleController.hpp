#pragma once
#include <string>
#include <functional>
#include <map>
#include <memory>
#include "adb/ADBClient.hpp"
#include "vision/ocr_pack.h"


class SimpleController {
public:
    using Task = std::function<void(SimpleController&)>;
    SimpleController();
    ~SimpleController();
    // 连接设备
    bool connect(const std::string& adb_path, const std::string& address, const std::string& config_path = "");

    // 基本操作
    bool capture_screenshot(const std::string& filename);
    bool click(int x, int y);
    void wait(int ms);
    std::string build_cmd(const std::string& cmd);
    bool swipe(int x1, int y1, int x2, int y2, int duration_ms);

    // 视觉功能
    bool detect_text(const std::string& image_path, std::string& out_text);
    bool find_template(const std::string& image_path, const std::string& template_path, int& out_x, int& out_y);

    // 任务注册与调度
    void register_task(const std::string& name, Task task);
    bool run_task(const std::string& name);

private:
    std::unique_ptr<ADBClient> adb_client_;
    std::unique_ptr<OcrPack> vision_api_;
    std::map<std::string, Task> task_map_;
    std::string device_address_;
    std::string adb_path_;
    std::string config_path_;
};
