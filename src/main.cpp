#include <iostream>
#include "ADBClient.hpp"
#include <future>
#include <chrono>
#include "SimpleController.hpp"
#include "TaskExecutor.hpp"
#include "Config.hpp"

int main() {
    // 初始化控制器
    SimpleController controller;
    if (!controller.connect("/tmp/adb", "192.168.3.69", "5555")) {
        std::cerr << "连接设备失败" << std::endl;
        return 1;
    }

    // 初始化任务执行器
    TaskExecutor executor(controller);
    executor.start();

    // 投递任务
    std::string task_path = std::string(Config::PROJECT_ROOT_DIR) + "/resource/tasks/start_arknights.json";
    executor.submit(task_path, [&](){});


    return 0;
}