#pragma once
#include "TaskConfig.hpp"
#include "TaskLoader.hpp"
#include "SimpleController.hpp"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <functional>
#include <map>
#include <opencv2/core.hpp>

using TaskCallback = std::function<void()>;

// 节点执行结果
enum class NodeResult {
    SUCCESS,    // 识别成功并执行了动作
    FAILED,     // 识别失败，任务应中止
    JUMP        // 识别失败，需要跳转到 on_fail_jump 指定的节点
};

// 识别回调：(node, screenshot_path) -> 匹配位置
using RecognizeHandler = std::function<std::optional<cv::Point>(const TaskNode&, const std::string&)>;

// 动作回调：(node, match_pos) -> 是否成功
using ActionHandler = std::function<bool(const TaskNode&, const std::optional<cv::Point>&)>;

class TaskExecutor {
public:
    explicit TaskExecutor(SimpleController& controller);
    ~TaskExecutor();

    void start();
    void stop();
    void submit(const std::string& task_path, TaskCallback func);
    size_t queue_size() const;
    bool is_running() const;

private:
    void worker_loop();
    bool execute_task(const TaskConfig& task);
    NodeResult execute_node(const TaskNode& node);
    void register_handlers();

    SimpleController& controller_;

    std::queue<std::pair<std::string, TaskCallback>> task_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;

    std::thread worker_thread_;
    std::atomic<bool> running_{false};

    // 回调映射
    std::map<std::string, RecognizeHandler> recognizers_;
    std::map<std::string, ActionHandler> actions_;
};
