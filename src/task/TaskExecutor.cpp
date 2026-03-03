#include "task/TaskExecutor.hpp"
#include "vision/image_preprocessor.h"
#include <iostream>
#include <chrono>

namespace {
    // 将任务配置中的 method 字符串映射到 ImagePreprocessor::Strategy
    ImagePreprocessor::Strategy methodToStrategy(const std::string& method) {
        if (method == "Grayscale")  return ImagePreprocessor::Strategy::GRAYSCALE;
        if (method == "HSVCount")   return ImagePreprocessor::Strategy::ENHANCE_CONTRAST;
        if (method == "RGBCount")   return ImagePreprocessor::Strategy::ENHANCE_CONTRAST;
        // Ccoeff 及其他 — 不做预处理，直接彩色匹配
        return ImagePreprocessor::Strategy::NONE;
    }
}

TaskExecutor::TaskExecutor(SimpleController& controller) : controller_(controller) {}

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

// ============================================================
// 任务执行：顺序执行节点
// ============================================================

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
            bool result = execute_node(node);
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start);

            if (!result) {
                std::cerr << "[Node] ❌ 失败 (" << duration.count() << "ms)" << std::endl;
                return false;
            }
            std::cout << "[Node] ✅ 完成 (" << duration.count() << "ms)" << std::endl;
        }
    }


    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "[TaskExecutor] ✅ 任务完成: " << task.name << std::endl;
    std::cout << std::string(60, '=') << "\n" << std::endl;
    return true;
}

// ============================================================
// 执行单个节点：识别轮询 → pre_delay → 动作 → post_delay
// ============================================================

bool TaskExecutor::execute_node(const TaskNode& node) {
    int round = 0;

    do {
        std::string screenshot;

        if (node.recognition == "DirectHit") {
            // DirectHit 不需要识别
        } else {
            if (node.repeat_until_failed && round > 0) {
                std::cout << "  🔁 repeat_until_failed 第" << (round + 1) << "轮" << std::endl;
            }
            std::cout << "  🔍 识别: " << node.recognition << " [" << node.method << "]";
            if (!node.expected.empty()) std::cout << " \"" << node.expected << "\"";
            if (!node.template_paths.empty()) {
                std::cout << " [";
                for (size_t i = 0; i < node.template_paths.size(); ++i) {
                    if (i > 0) std::cout << ", ";
                    std::cout << node.template_paths[i];
                }
                std::cout << "]";
            }
            std::cout << " (超时: " << node.timeout << "ms)" << std::endl;

            auto start = std::chrono::steady_clock::now();
            bool found = false;
            int attempt = 0;

            while (running_.load()) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start).count();
                if (elapsed >= node.timeout) {
                    if (node.repeat_until_failed) {
                        std::cout << "  ✅ repeat_until_failed 结束 (共执行" << round << "轮)" << std::endl;
                        return true;
                    }
                    std::cerr << "  ⏰ 识别超时 (" << elapsed << "ms)" << std::endl;
                    return false;
                }

                attempt++;
                screenshot = controller_.auto_screenshot(node.recognition);
                if (screenshot.empty()) {
                    std::cerr << "  ❌ 截图失败" << std::endl;
                    return false;
                }

                found = recognize(node, screenshot);
                if (found) {
                    std::cout << "  ✅ 识别成功 (第" << attempt << "次, "
                              << std::chrono::duration_cast<std::chrono::milliseconds>(
                                     std::chrono::steady_clock::now() - start).count()
                              << "ms)" << std::endl;
                    break;
                }

                controller_.wait(node.interval);
            }

            if (!found) {
                if (node.repeat_until_failed) {
                    std::cout << "  ✅ repeat_until_failed 结束 (共执行" << round << "轮)" << std::endl;
                    return true;
                }
                return false;
            }
        }

        // pre_delay
        if (node.pre_delay > 0) {
            std::cout << "  ⏳ pre_delay " << node.pre_delay << "ms" << std::endl;
            controller_.wait(node.pre_delay);
        }

        // 执行动作
        if (!perform_action(node, screenshot)) {
            return false;
        }

        // post_delay
        if (node.post_delay > 0) {
            std::cout << "  ⏳ post_delay " << node.post_delay << "ms" << std::endl;
            controller_.wait(node.post_delay);
        }

        round++;
    } while (node.repeat_until_failed && running_.load());

    return true;
}

// ============================================================
// 识别：根据 recognition + method 分发
// ============================================================

bool TaskExecutor::recognize(const TaskNode& node, const std::string& screenshot) {
    if (node.recognition == "OCR") {
        if (node.roi.has_value()) {
            std::string text;
            if (!controller_.detect_text(screenshot, text, node.roi.value())) return false;
            return text.find(node.expected) != std::string::npos;
        } else {
            cv::Point pos;
            return controller_.find_text(screenshot, node.expected, pos);
        }
    } else if (node.recognition == "TemplateMatch") {
        cv::Point pos;
        auto strategy = methodToStrategy(node.method);
        return controller_.find_template_with_preprocess(screenshot, node.template_paths,
                                                          strategy, node.threshold, pos);
    }

    std::cerr << "  ❌ 未知识别方式: " << node.recognition << std::endl;
    return false;
}

// ============================================================
// 动作执行
// ============================================================

bool TaskExecutor::perform_action(const TaskNode& node, const std::string& screenshot) {
    if (node.action == "Click") {
        if (!node.target.empty() && node.target.size() >= 2) {
            cv::Point pos(node.target[0], node.target[1]);
            std::cout << "  🖱️  点击 (" << pos.x << ", " << pos.y << ")" << std::endl;
            return controller_.click(pos);
        } else if (!node.expected.empty() && !screenshot.empty()) {
            cv::Point pos;
            if (controller_.find_text(screenshot, node.expected, pos)) {
                std::cout << "  🖱️  OCR点击 \"" << node.expected << "\" (" << pos.x << ", " << pos.y << ")" << std::endl;
                return controller_.click(pos);
            }
            std::cerr << "  ❌ 未找到点击位置" << std::endl;
            return false;
        } else if (!node.template_paths.empty() && !screenshot.empty()) {
            cv::Point pos;
            auto strategy = methodToStrategy(node.method);
            bool found = controller_.find_template_with_preprocess(screenshot, node.template_paths,
                                                                    strategy, node.threshold, pos);
            if (found) {
                std::cout << "  🖱️  模板点击 (" << pos.x << ", " << pos.y << ")" << std::endl;
                return controller_.click(pos);
            }
            std::cerr << "  ❌ 未找到点击位置" << std::endl;
            return false;
        }
        std::cerr << "  ❌ Click 缺少 target 或识别结果" << std::endl;
        return false;
    } else if (node.action == "Swipe") {
        if (node.target.size() >= 5) {
            cv::Point from(node.target[0], node.target[1]);
            cv::Point to(node.target[2], node.target[3]);
            int dur = node.target[4];
            std::cout << "  👆 滑动 (" << from.x << "," << from.y << ") -> (" << to.x << "," << to.y << ") " << dur << "ms" << std::endl;
            return controller_.swipe(from, to, dur);
        }
        std::cerr << "  ❌ Swipe 需要 target: [x1,y1,x2,y2,duration]" << std::endl;
        return false;
    } else if (node.action == "Shell") {
        std::cout << "  💻 Shell: " << node.shell_cmd << std::endl;
        controller_.shell(node.shell_cmd);
        return true;
    } else if (node.action == "StartApp") {
        std::cout << "  📱 启动游戏" << std::endl;
        return controller_.start_app();
    } else if (node.action == "StopApp") {
        std::cout << "  📱 关闭游戏" << std::endl;
        return controller_.stop_app();
    }

    std::cerr << "  ❌ 未知动作: " << node.action << std::endl;
    return false;
}
