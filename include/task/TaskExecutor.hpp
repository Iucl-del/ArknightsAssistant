#pragma once
#include "TaskConfig.hpp"
#include "TaskLoader.hpp"
#include "SimpleController.hpp"
#include <iostream>
#include <map>

// 任务执行器
class TaskExecutor {
public:
    explicit TaskExecutor(SimpleController& controller) : controller_(controller) {}

    // 加载任务配置文件
    void load_task(const std::string& path) {
        auto config = TaskLoader::load_from_file(path);
        tasks_[config.name] = config;
    }

    // 执行任务
    bool run(const std::string& task_name) {
        if (!tasks_.count(task_name)) {
            std::cerr << "任务不存在: " << task_name << std::endl;
            return false;
        }

        auto& task = tasks_[task_name];
        int loop_count = task.loop ? task.loop_count : 1;

        for (int i = 0; i < loop_count; ++i) {
            bool success = execute_steps(task.steps);
            if (success && task.on_success) {
                run(*task.on_success);
            } else if (!success) {
                if (task.on_failure) {
                    run(*task.on_failure);
                }
                return false;
            }
        }
        return true;
    }

private:
    bool execute_steps(const std::vector<TaskStep>& steps) {
        for (const auto& step : steps) {
            if (!execute_step(step)) {
                return false;
            }
        }
        return true;
    }

    bool execute_step(const TaskStep& step) {
        std::cout << "执行步骤: " << step.action << std::endl;
        if (step.action == "click") {
            return controller_.click(step.x, step.y);
        }
        else if (step.action == "wait") {
            controller_.wait(step.duration);
            return true;
        }
        else if (step.action == "screenshot") {
            return controller_.capture_screenshot(step.save_name);
        }
        else if (step.action == "shell") {
            controller_.build_cmd(step.shell_cmd);
            return true;
        }
        else if (step.action == "swipe") {
            return controller_.swipe(step.x, step.y, step.x2, step.y2, step.duration);
        }
        else if (step.action == "ocr") {
            for (int retry = 0; retry < step.retry; ++retry) {
                std::string text;
                if (controller_.detect_text(step.save_name, text)) {
                    if (text.find(step.text) != std::string::npos) {
                        return true;
                    }
                }
                controller_.wait(step.timeout / step.retry);
            }
            return false;
        }
        else if (step.action == "template") {
            int x, y;
            if (controller_.find_template(step.save_name, step.template_path, x, y)) {
                return controller_.click(x, y);
            }
            return false;
        }
        else if (step.action == "ocr_click") {
            int x, y;
            if (controller_.find_text(step.save_name, step.text, x, y)) {
                return controller_.click(x, y);
            }
            return false;
        }
        return false;
    }

    SimpleController& controller_;
    std::map<std::string, TaskConfig> tasks_;
};
