#include "task/TaskExecutor.hpp"
#include "Logger.hpp"
#include "vision/ImagePreprocessor.hpp"

#include <chrono>
#include <ranges>

namespace {
ImagePreprocessor::Strategy method_to_strategy(const std::string& method) {
    if (method == "Grayscale" || method == "Gray") {
        return ImagePreprocessor::Strategy::GRAYSCALE;
    }
    if (method == "Binary") {
        return ImagePreprocessor::Strategy::BINARY;
    }
    if (method == "AdaptiveBinary") {
        return ImagePreprocessor::Strategy::ADAPTIVE_BINARY;
    }
    if (method == "Denoise") {
        return ImagePreprocessor::Strategy::DENOISE;
    }
    if (method == "EnhanceContrast") {
        return ImagePreprocessor::Strategy::ENHANCE_CONTRAST;
    }
    if (method == "Auto") {
        return ImagePreprocessor::Strategy::AUTO;
    }
    return ImagePreprocessor::Strategy::NONE;
}
}

TaskExecutor::TaskExecutor(SimpleController& controller) : controller_(controller) {
    register_handlers();
}

TaskExecutor::~TaskExecutor() {
    stop();
}

void TaskExecutor::register_handlers() {
    recognizers_["OCR"] = [this](const TaskNode& node, const std::string& screenshot) -> std::optional<cv::Point> {
        if (node.roi.has_value()) {
            std::string text;
            if (!controller_.detect_text(screenshot, text, node.roi.value())) {
                return std::nullopt;
            }
            bool found = std::ranges::any_of(node.expected, [&](const std::string& e) {
                return text.find(e) != std::string::npos;
            });
            if (found) {
                const auto& roi = node.roi.value();
                return cv::Point(roi.x + roi.w / 2, roi.y + roi.h / 2);
            }
            return std::nullopt;
        }

        cv::Point pos;
        for (const auto& e : node.expected) {
            if (controller_.find_text(screenshot, e, pos)) {
                return pos;
            }
        }
        return std::nullopt;
    };

    recognizers_["TemplateMatch"] = [this](const TaskNode& node, const std::string& screenshot) -> std::optional<cv::Point> {
        cv::Point pos;
        if (controller_.find_template_with_preprocess(
                screenshot,
                node.template_paths,
                method_to_strategy(node.method),
                node.threshold,
                node.roi,
                pos)) {
            return pos;
        }
        return std::nullopt;
    };

    actions_["Click"] = [this](const TaskNode& node, const std::optional<cv::Point>& pos) -> bool {
        if (!node.target.empty() && node.target.size() >= 2) {
            cv::Point p(node.target[0], node.target[1]);
            Logger::info("  点击 ({}, {})", p.x, p.y);
            return controller_.click(p);
        }
        if (pos.has_value()) {
            Logger::info("  点击识别位置 ({}, {})", pos->x, pos->y);
            return controller_.click(pos.value());
        }
        Logger::error("  Click 缺少 target 或识别结果");
        return false;
    };

    actions_["Swipe"] = [this](const TaskNode& node, const std::optional<cv::Point>&) -> bool {
        if (node.target.size() >= 5) {
            cv::Point from(node.target[0], node.target[1]);
            cv::Point to(node.target[2], node.target[3]);
            int dur = node.target[4];
            Logger::info("  滑动 ({}, {}) -> ({}, {}) {}ms", from.x, from.y, to.x, to.y, dur);
            return controller_.swipe(from, to, dur);
        }
        Logger::error("  Swipe 需要 target: [x1,y1,x2,y2,duration]");
        return false;
    };

    actions_["Shell"] = [this](const TaskNode& node, const std::optional<cv::Point>&) -> bool {
        Logger::info("  执行 Shell: {}", node.shell_cmd);
        controller_.shell(node.shell_cmd);
        return true;
    };

    actions_["StartApp"] = [this](const TaskNode&, const std::optional<cv::Point>&) -> bool {
        Logger::info("  启动应用");
        return controller_.start_app();
    };

    actions_["StopApp"] = [this](const TaskNode&, const std::optional<cv::Point>&) -> bool {
        Logger::info("  停止应用");
        return controller_.stop_app();
    };
}

void TaskExecutor::start() {
    if (running_.load()) return;
    running_ = true;
    worker_thread_ = std::thread(&TaskExecutor::worker_loop, this);
    Logger::info("[TaskExecutor] 工作线程已启动");
}

void TaskExecutor::stop() {
    if (!running_.load()) return;
    running_ = false;
    queue_cv_.notify_all();
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    Logger::info("[TaskExecutor] 工作线程已停止");
}

void TaskExecutor::submit(const std::string& task_path, TaskCallback func) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        task_queue_.emplace(task_path, std::move(func));
        Logger::info("[TaskExecutor] 任务已提交: {} (队列长度: {})", task_path, task_queue_.size());
    }
    queue_cv_.notify_one();
}

size_t TaskExecutor::queue_size() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return task_queue_.size();
}

bool TaskExecutor::is_running() const {
    return running_.load();
}

void TaskExecutor::worker_loop() {
    while (running_.load()) {
        std::pair<std::string, TaskCallback> task;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this] {
                return !task_queue_.empty() || !running_.load();
            });

            if (!running_.load() && task_queue_.empty()) break;

            if (!task_queue_.empty()) {
                task = std::move(task_queue_.front());
                task_queue_.pop();
                Logger::info("[TaskExecutor] 取出任务: {} (剩余: {})", task.first, task_queue_.size());
            }
        }

        if (!task.first.empty()) {
            auto task_config = TaskLoader::load_from_file(task.first);
            if (!task_config.name.empty()) {
                execute_task(task_config);
                if (task.second) task.second();
            } else {
                Logger::error("[TaskExecutor] 任务加载失败: {}", task.first);
            }
        }
    }
}

bool TaskExecutor::execute_task(const TaskConfig& task) {
    Logger::info("{}", std::string(60, '='));
    Logger::info("[TaskExecutor] 开始任务: {}", task.name);
    Logger::info("[TaskExecutor] 节点数量: {}", task.nodes.size());
    Logger::info("{}", std::string(60, '='));

    int loop_count = task.loop ? task.loop_count : 1;

    for (int i = 0; i < loop_count && running_.load(); ++i) {
        if (loop_count > 1) {
            Logger::info("[TaskExecutor] 第 {}/{} 轮", i + 1, loop_count);
        }

        for (size_t idx = 0; idx < task.nodes.size() && running_.load(); ++idx) {
            const auto& node = task.nodes[idx];
            Logger::info("[Node {}/{}]", idx + 1, task.nodes.size());

            auto start = std::chrono::steady_clock::now();
            auto result = execute_node(node);
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start);

            if (result == NodeResult::FAILED) {
                Logger::error("[Node] 失败 ({}ms)", duration.count());
                return false;
            }
            if (result == NodeResult::JUMP) {
                Logger::info("[Node] 跳转到节点 {} ({}ms)", node.on_fail_jump + 1, duration.count());
                idx = static_cast<size_t>(node.on_fail_jump) - 1;
                continue;
            }
            Logger::info("[Node] 完成 ({}ms)", duration.count());
        }
    }

    Logger::info("{}", std::string(60, '='));
    Logger::info("[TaskExecutor] 任务完成: {}", task.name);
    Logger::info("{}", std::string(60, '='));
    return true;
}

NodeResult TaskExecutor::execute_node(const TaskNode& node) {
    int round = 0;
    auto node_start = std::chrono::steady_clock::now();
    using DurationMs = std::chrono::duration<double, std::milli>;

    auto timed_out = [&]() {
        return DurationMs(std::chrono::steady_clock::now() - node_start).count() >= node.timeout;
    };

    do {
        if (timed_out()) {
            Logger::warning("  节点超时，跳过下一轮");
            return NodeResult::SUCCESS;
        }

        if (node.pre_delay > 0) {
            controller_.wait(node.pre_delay);
            if (timed_out()) {
                Logger::warning("  pre_delay 后节点已超时");
                return NodeResult::SUCCESS;
            }
        }

        std::optional<cv::Point> match_pos = std::nullopt;

        if (node.recognition != "DirectHit") {
            if (node.repeat_until_failed && round > 0) {
                Logger::info("  repeat_until_failed 第 {} 轮", round + 1);
            }

            Logger::info("  识别方式: {} (超时: {}ms)", node.recognition, node.timeout);

            auto recognizer = recognizers_.find(node.recognition);
            if (recognizer == recognizers_.end()) {
                Logger::error("  未知识别方式: {}", node.recognition);
                return NodeResult::FAILED;
            }

            int attempt = 0;
            while (running_.load() && !timed_out()) {
                attempt++;
                auto attempt_start = std::chrono::steady_clock::now();
                auto screenshot_start = std::chrono::steady_clock::now();
                std::string screenshot = controller_.auto_screenshot(node.recognition);
                auto screenshot_end = std::chrono::steady_clock::now();
                if (screenshot.empty()) {
                    Logger::error("  截图失败");
                    return NodeResult::FAILED;
                }

                if (timed_out()) {
                    Logger::warning("  截图后节点已超时");
                    return NodeResult::SUCCESS;
                }

                auto recognize_start = std::chrono::steady_clock::now();
                match_pos = recognizer->second(node, screenshot);
                auto recognize_end = std::chrono::steady_clock::now();
                Logger::debug("[Timing][Attempt {}] screenshot={}ms recognize={}ms total={}ms file={}",
                              attempt,
                              std::chrono::duration_cast<std::chrono::milliseconds>(screenshot_end - screenshot_start).count(),
                              std::chrono::duration_cast<std::chrono::milliseconds>(recognize_end - recognize_start).count(),
                              std::chrono::duration_cast<std::chrono::milliseconds>(recognize_end - attempt_start).count(),
                              screenshot);

                if (timed_out()) {
                    Logger::warning("  识别后节点已超时");
                    return NodeResult::SUCCESS;
                }

                if (match_pos.has_value()) {
                    Logger::info("  识别成功 (第 {} 次)", attempt);
                    break;
                }

                controller_.wait(node.interval);
            }

            if (!match_pos.has_value()) {
                if (node.repeat_until_failed) {
                    Logger::info("  repeat_until_failed 结束 (共 {} 轮)", round);
                    return NodeResult::SUCCESS;
                }
                if (node.on_fail_jump >= 0) {
                    Logger::info("  识别失败，执行跳转");
                    return NodeResult::JUMP;
                }
                if (node.optional) {
                    Logger::info("  可选节点未匹配，已跳过");
                    return NodeResult::SUCCESS;
                }
                if (timed_out()) {
                    Logger::warning("  识别超时");
                    return NodeResult::FAILED;
                }
                Logger::warning("  识别失败");
                return NodeResult::FAILED;
            }
        }

        if (timed_out()) {
            Logger::warning("  执行动作前节点已超时");
            return NodeResult::SUCCESS;
        }

        auto action = actions_.find(node.action);
        if (action == actions_.end()) {
            Logger::error("  未知动作: {}", node.action);
            return NodeResult::FAILED;
        }
        if (!action->second(node, match_pos)) {
            return NodeResult::FAILED;
        }

        if (node.post_delay > 0) {
            controller_.wait(node.post_delay);
        }

        round++;
    } while (node.repeat_until_failed && running_.load() && !timed_out());

    if (timed_out()) {
        Logger::warning("  节点超时，进入后续流程");
    }
    return NodeResult::SUCCESS;
}
