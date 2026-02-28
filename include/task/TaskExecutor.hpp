#pragma once
#include "TaskConfig.hpp"
#include "TaskLoader.hpp"
#include "SimpleController.hpp"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>

using TaskCallback = std::function<void()>;

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

    // 执行单个节点：识别轮询 → 动作 → 返回是否成功
    bool execute_node(const TaskNode& node);

    // 识别：自动截图 + 检测，返回是否匹配成功
    bool recognize(const TaskNode& node, std::string& screenshot);

    // 动作：根据 action 类型执行
    bool perform_action(const TaskNode& node, const std::string& screenshot);

    SimpleController& controller_;

    std::queue<std::pair<std::string, TaskCallback>> task_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;

    std::thread worker_thread_;
    std::atomic<bool> running_{false};
};
