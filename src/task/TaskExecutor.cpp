#include "task/TaskExecutor.hpp"
#include <iostream>
#include <chrono>
#include <variant>

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

void TaskExecutor::submit(const std::string& task_path,TaskCallback func) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        task_queue_.push(std::make_pair(task_path, func));
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
        std::pair<std::string,TaskCallback> task;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this] {
                return !task_queue_.empty() || !running_.load();
            });

            if (!running_.load() && task_queue_.empty()) break;

            if (!task_queue_.empty()) {
                task = task_queue_.front();

                task_queue_.pop();
                std::cout << "[TaskExecutor] 📤 取出任务: " << task.first
                          << " (剩余: " << task_queue_.size() << ")" << std::endl;
            }
        }

        if (!task.first.empty()) {
            auto task_path = TaskLoader::load_from_file(task.first);
            if (!task_path.name.empty()) {
                execute_task(task_path);
                task.second();
            } else {
                std::cerr << "[TaskExecutor] ❌ 任务加载失败: " << task.first << std::endl;
            }
        }
    }
}

bool TaskExecutor::execute_task(const TaskConfig& task) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "[TaskExecutor] 🚀 开始执行任务: " << task.name << std::endl;
    std::cout << "[TaskExecutor] 📋 " << task.description << std::endl;
    std::cout << "[TaskExecutor] 📝 步骤总数: " << task.steps.size() << std::endl;
    std::cout << std::string(60, '=') << std::endl;

    int loop_count = task.loop ? task.loop_count : 1;

    for (int i = 0; i < loop_count && running_.load(); ++i) {
        if (loop_count > 1) {
            std::cout << "\n[TaskExecutor] ━━━ 第 " << (i + 1) << "/" << loop_count << " 轮 ━━━" << std::endl;
        }

        int step_index = 0;
        for (const auto& step : task.steps) {
            if (!running_.load()) {
                std::cout << "[TaskExecutor] ⏹️ 任务被中断" << std::endl;
                return false;
            }

            step_index++;
            std::cout << "\n[Step " << step_index << "/" << task.steps.size() << "] ";

            auto start = std::chrono::steady_clock::now();
            bool result = std::visit([this](const auto& s) { return execute(s); }, step);
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start);

            if (!result) {
                std::cerr << "[Step " << step_index << "] ❌ 失败 (" << duration.count() << "ms)" << std::endl;
                return false;
            }
            std::cout << "[Step " << step_index << "] ✅ 完成 (" << duration.count() << "ms)" << std::endl;
        }
    }

    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "[TaskExecutor] ✅ 任务完成: " << task.name << std::endl;
    std::cout << std::string(60, '=') << "\n" << std::endl;
    return true;
}

// ========== 静态多态：函数重载 ==========

bool TaskExecutor::execute(const BasicStep& step) {
    if (step.action == "click") {
        std::cout << "🖱️  点击 (" << step.x << ", " << step.y << ")" << std::endl;
        return controller_.click(step.x, step.y);
    } else if (step.action == "swipe") {
        std::cout << "👆 滑动 (" << step.x << ", " << step.y << ") -> ("
                  << step.x2 << ", " << step.y2 << ") " << step.duration << "ms" << std::endl;
        return controller_.swipe(step.x, step.y, step.x2, step.y2, step.duration);
    } else if (step.action == "wait") {
        std::cout << "⏳ 等待 " << step.duration << "ms" << std::endl;
        controller_.wait(step.duration);
        return true;
    }
    std::cerr << "❌ 未知操作: " << step.action << std::endl;
    return false;
}

bool TaskExecutor::execute(const VisionStep& step) {
    if (step.action == "screenshot") {
        std::cout << "📷 截图 -> " << step.image_name << std::endl;
        return controller_.capture_screenshot(step.image_name);
    } else if (step.action == "ocr_click") {
        std::cout << "🔍🖱️  OCR点击: \"" << step.text << "\"" << std::endl;
        int x, y;
        if (controller_.find_text(step.image_name, step.text, x, y)) {
            std::cout << "  ✅ 位置: (" << x << ", " << y << ")" << std::endl;
            return controller_.click(x, y);
        }
        std::cerr << "  ❌ 未找到: \"" << step.text << "\"" << std::endl;
        return false;
    } else if (step.action == "ocr_region") {
        if (!step.roi.has_value()) {
            std::cerr << "❌ ocr_region 需要配置 roi" << std::endl;
            return false;
        }
        const auto& roi = step.roi.value();
        std::cout << "🔍📐 OCR区域 (" << roi.x << ", " << roi.y << ", "
                  << roi.width << "x" << roi.height << ")" << std::endl;
        std::string text;
        if (controller_.ocr_region(step.image_name, roi.x, roi.y, roi.width, roi.height,
                                    roi.base_width, roi.base_height, text)) {
            std::cout << "  📝 结果: \"" << text << "\"" << std::endl;
            if (!step.text.empty()) {
                return text.find(step.text) != std::string::npos;
            }
            return true;
        }
        return false;
    } else if (step.action == "template") {
        std::cout << "🖼️  模板匹配: " << step.template_path << std::endl;
        int x, y;
        if (controller_.find_template(step.image_name, step.template_path, x, y)) {
            std::cout << "  ✅ 位置: (" << x << ", " << y << ")" << std::endl;
            return controller_.click(x, y);
        }
        std::cerr << "  ❌ 匹配失败" << std::endl;
        return false;
    }
    std::cerr << "❌ 未知操作: " << step.action << std::endl;
    return false;
}

bool TaskExecutor::execute(const SystemStep& step) {
    if (step.action == "shell") {
        std::cout << "💻 Shell: " << step.cmd << std::endl;
        controller_.build_cmd(step.cmd);
        return true;
    } else if (step.action == "start_app") {
        std::cout << "📱 启动: " << step.package_name << std::endl;
        controller_.build_cmd("am start -n " + step.package_name);
        return true;
    }
    std::cerr << "❌ 未知操作: " << step.action << std::endl;
    return false;
}
