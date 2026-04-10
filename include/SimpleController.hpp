#pragma once
#include <string>
#include <functional>
#include <memory>
#include <optional>
#include <atomic>

#include "adb/ADBClient.hpp"
#include "vision/OcrPack.hpp"
#include "vision/VisionTypes.hpp"
#include "vision/ImagePreprocessor.hpp"

class TaskExecutor;

class SimpleController: public std::enable_shared_from_this<SimpleController> {
public:
    using Task = std::function<void(SimpleController&)>;
    SimpleController();
    ~SimpleController();

    std::shared_ptr<TaskExecutor> get_executor(){return executor_;}

    // 连接设备（ip 和 port 分开传，与 controller 保持一致）
    bool connect(const std::string& adb_path, const std::string& ip, const std::string& port);

    // 基本操作
    bool capture_screenshot(const std::string& filename);
    bool click(const cv::Point& pos);
    void wait(int ms);
    bool swipe(const cv::Point& from, const cv::Point& to, int duration_ms);
    bool start_app();
    bool stop_app();
    void shell(const std::string& cmd);

    // 自动截图：生成唯一文件名并截图，返回文件名；失败返回空串
    std::string auto_screenshot(const std::string& hint = "auto");

    // 视觉功能：不传 roi 则识别整张图，传入 roi 则只识别指定区域
    bool detect_text(const std::string& image_path, std::string& out_text,
                     std::optional<ROI> roi = std::nullopt);
    // 文字检索：查找截图中是否有匹配文字
    bool find_text(const std::string& image_path, const std::string& target_text, cv::Point& out_pos);

    // 模板匹配（支持多模板轮询，预处理策略由 ImagePreprocessor::Strategy 指定）
    bool find_template_with_preprocess(const std::string& image_path,
                                       const std::vector<std::string>& template_paths,
                                       ImagePreprocessor::Strategy strategy,
                                       double threshold,
                                       cv::Point& out_pos);

private:
    std::unique_ptr<ADBClient> controller_;
    std::unique_ptr<OcrPack> vision_api_;
    std::shared_ptr<TaskExecutor> executor_;
    std::string device_address_;
    std::string work_dir_;
    std::string game_package_;
    std::atomic<int> screenshot_seq_{0};
};
