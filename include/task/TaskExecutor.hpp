#pragma once
#include "TaskConfig.hpp"
#include "TaskLoader.hpp"
#include "SimpleController.hpp"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>

class TaskExecutor {
public:
    explicit TaskExecutor(SimpleController& controller);
    ~TaskExecutor();

    // 启动工作线程
    void start();

    // 停止工作线程
    void stop();

    // 投递任务（JSON 路径）
    void submit(const std::string& task_path);

    // 获取队列长度
    size_t queue_size() const;

    // 是否正在运行
    bool is_running() const;

private:
    void worker_loop();
    bool execute_task(const TaskConfig& task);

    // 静态多态：函数重载执行不同类型步骤
    bool execute(const BasicStep& step);
    bool execute(const VisionStep& step);
    bool execute(const SystemStep& step);

    SimpleController& controller_;

    // 任务队列（存放 JSON 路径）
    std::queue<std::string> task_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;

    // 工作线程
    std::thread worker_thread_;
    std::atomic<bool> running_{false};
};
