#include "task/TaskExecutor.hpp"
#include "vision/ImagePreprocessor.hpp"
#include <iostream>
#include <chrono>


TaskExecutor::TaskExecutor(SimpleController& controller) : controller_(controller) {
    register_handlers();
}

void TaskExecutor::register_handlers() {
    // ========== 识别回调 ==========
    recognizers_["OCR"] = [this](const TaskNode& node, const std::string& screenshot) -> std::optional<cv::Point> {
        if (node.roi.has_value()) {
            std::string text;
            if (!controller_.detect_text(screenshot, text, node.roi.value())) return std::nullopt;
            bool found = std::ranges::any_of(node.expected,
                [&](const std::string& e) { return text.find(e) != std::string::npos; });
            if (found) {
                const auto& roi = node.roi.value();
                return cv::Point(roi.x + roi.w / 2, roi.y + roi.h / 2);
            }
            return std::nullopt;
        } else {
            cv::Point pos;
            for (const auto& e : node.expected) {
                if (controller_.find_text(screenshot, e, pos)) {
                    return pos;
                }
            }
            return std::nullopt;
        }
    };

    recognizers_["TemplateMatch"] = [this](const TaskNode& node, const std::string& screenshot) -> std::optional<cv::Point> {
        cv::Point pos;
        if (controller_.find_template_with_preprocess(screenshot, node.template_paths,
                ImagePreprocessor::Strategy::NONE, node.threshold, pos)) {
            return pos;
        }
        return std::nullopt;
    };

    // ========== 动作回调 ==========
    actions_["Click"] = [this](const TaskNode& node, const std::optional<cv::Point>& pos) -> bool {
        if (!node.target.empty() && node.target.size() >= 2) {
            cv::Point p(node.target[0], node.target[1]);
            std::cout << "  🖱️  点击 (" << p.x << ", " << p.y << ")" << std::endl;
            return controller_.click(p);
        }
        if (pos.has_value()) {
            std::cout << "  🖱️  点击识别位置 (" << pos->x << ", " << pos->y << ")" << std::endl;
            return controller_.click(pos.value());
        }
        std::cerr << "  ❌ Click 缺少 target 或识别结果" << std::endl;
        return false;
    };

    actions_["Swipe"] = [this](const TaskNode& node, const std::optional<cv::Point>&) -> bool {
        if (node.target.size() >= 5) {
            cv::Point from(node.target[0], node.target[1]);
            cv::Point to(node.target[2], node.target[3]);
            int dur = node.target[4];
            std::cout << "  👆 滑动 (" << from.x << "," << from.y << ") -> (" << to.x << "," << to.y << ") " << dur << "ms" << std::endl;
            return controller_.swipe(from, to, dur);
        }
        std::cerr << "  ❌ Swipe 需要 target: [x1,y1,x2,y2,duration]" << std::endl;
        return false;
    };

    actions_["Shell"] = [this](const TaskNode& node, const std::optional<cv::Point>&) -> bool {
        std::cout << "  💻 Shell: " << node.shell_cmd << std::endl;
        controller_.shell(node.shell_cmd);
        return true;
    };

    actions_["StartApp"] = [this](const TaskNode&, const std::optional<cv::Point>&) -> bool {
        std::cout << "  📱 启动游戏" << std::endl;
        return controller_.start_app();
    };

    actions_["StopApp"] = [this](const TaskNode&, const std::optional<cv::Point>&) -> bool {
        std::cout << "  📱 关闭游戏" << std::endl;
        return controller_.stop_app();
    };
}

TaskExecutor::~TaskExecutor() {
    stop();
}

void TaskExecutor::start() {
    if (running_.load()) return;
    running_ = true;
    worker_thread_ = std::thread(&TaskExecutor::worker_loop, this);
    std::cout << "[TaskExecutor] ✅ 工作线程已启动" << std::endl;
}

void TaskExecutor::stop() {
    if (!running_.load()) return;
    running_ = false;
    queue_cv_.notify_all();
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    std::cout << "[TaskExecutor] ⏹️ 工作线程已停止" << std::endl;
}

void TaskExecutor::submit(const std::string& task_path, TaskCallback func) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        task_queue_.emplace(task_path, std::move(func));
        std::cout << "[TaskExecutor] 📥 任务已投递: " << task_path
                  << " (队列长度: " << task_queue_.size() << ")" << std::endl;
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
                std::cout << "[TaskExecutor] 📤 取出任务: " << task.first
                          << " (剩余: " << task_queue_.size() << ")" << std::endl;
            }
        }

        if (!task.first.empty()) {
            auto task_config = TaskLoader::load_from_file(task.first);
            if (!task_config.name.empty()) {
                execute_task(task_config);
                if (task.second) task.second();
            } else {
                std::cerr << "[TaskExecutor] ❌ 任务加载失败: " << task.first << std::endl;
            }
        }
    }
}

bool TaskExecutor::execute_task(const TaskConfig& task) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "[TaskExecutor] 🚀 开始执行任务: " << task.name << std::endl;
    std::cout << "[TaskExecutor] 📝 节点总数: " << task.nodes.size() << std::endl;
    std::cout << std::string(60, '=') << std::endl;

    int loop_count = task.loop ? task.loop_count : 1;

    for (int i = 0; i < loop_count && running_.load(); ++i) {
        if (loop_count > 1) {
            std::cout << "\n[TaskExecutor] ━━━ 第 " << (i + 1) << "/" << loop_count << " 轮 ━━━" << std::endl;
        }

        for (size_t idx = 0; idx < task.nodes.size() && running_.load(); ++idx) {
            const auto& node = task.nodes[idx];
            std::cout << "\n[Node " << (idx + 1) << "/" << task.nodes.size() << "]" << std::endl;

            auto start = std::chrono::steady_clock::now();
            auto result = execute_node(node);
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start);

            if (result == NodeResult::FAILED) {
                std::cerr << "[Node] ❌ 失败 (" << duration.count() << "ms)" << std::endl;
                return false;
            } else if (result == NodeResult::JUMP) {
                std::cout << "[Node] ⏭️  跳转到节点 " << (node.on_fail_jump + 1) << " (" << duration.count() << "ms)" << std::endl;
                idx = static_cast<size_t>(node.on_fail_jump) - 1;
                continue;
            }
            std::cout << "[Node] ✅ 完成 (" << duration.count() << "ms)" << std::endl;
        }
    }

    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "[TaskExecutor] ✅ 任务完成: " << task.name << std::endl;
    std::cout << std::string(60, '=') << "\n" << std::endl;
    return true;
}

// 简化后的 execute_node
NodeResult TaskExecutor::execute_node(const TaskNode& node) {
    int round = 0;

    do {
        // pre_delay
        if (node.pre_delay > 0) {
            controller_.wait(node.pre_delay);
        }

        std::optional<cv::Point> match_pos = std::nullopt;

        // 识别阶段
        if (node.recognition != "DirectHit") {
            if (node.repeat_until_failed && round > 0) {
                std::cout << "  🔁 repeat_until_failed 第" << (round + 1) << "轮" << std::endl;
            }

            std::cout << "  🔍 识别: " << node.recognition << " (超时: " << node.timeout << "ms)" << std::endl;

            // 查找识别回调
            auto it = recognizers_.find(node.recognition);
            if (it == recognizers_.end()) {
                std::cerr << "  ❌ 未知识别方式: " << node.recognition << std::endl;
                return NodeResult::FAILED;
            }

            // 轮询识别
            auto start = std::chrono::steady_clock::now();
            int attempt = 0;
            using DurationMs = std::chrono::duration<double, std::milli>;

            while (running_.load() && DurationMs(std::chrono::steady_clock::now() - start).count() < node.timeout) {
                attempt++;
                std::string screenshot = controller_.auto_screenshot(node.recognition);
                if (screenshot.empty()) {
                    std::cerr << "  ❌ 截图失败" << std::endl;
                    return NodeResult::FAILED;
                }

                match_pos = it->second(node, screenshot);
                if (match_pos.has_value()) {
                    std::cout << "  ✅ 识别成功 (第" << attempt << "次)" << std::endl;
                    break;
                }
                controller_.wait(node.interval);
            }

            // 识别失败处理
            if (!match_pos.has_value()) {
                if (node.repeat_until_failed) {
                    std::cout << "  ✅ repeat_until_failed 结束 (共" << round << "轮)" << std::endl;
                    return NodeResult::SUCCESS;
                }
                if (node.on_fail_jump >= 0) {
                    std::cout << "  ⏭️  识别失败，准备跳转" << std::endl;
                    return NodeResult::JUMP;
                }
                if (node.optional) {
                    std::cout << "  ⏭️  optional 节点未匹配，跳过" << std::endl;
                    return NodeResult::SUCCESS;
                }
                std::cerr << "  ⏰ 识别超时" << std::endl;
                return NodeResult::FAILED;
            }
        }

        // 动作阶段
        auto it = actions_.find(node.action);
        if (it == actions_.end()) {
            std::cerr << "  ❌ 未知动作: " << node.action << std::endl;
            return NodeResult::FAILED;
        }
        if (!it->second(node, match_pos)) {
            return NodeResult::FAILED;
        }

        // post_delay
        if (node.post_delay > 0) {
            std::cout << "  ⏳ post_delay " << node.post_delay << "ms" << std::endl;
            controller_.wait(node.post_delay);
        }

        round++;
    } while (node.repeat_until_failed && running_.load());

    return NodeResult::SUCCESS;
}
