#include <iostream>
#include "ADBClient.hpp"
#include <future>
#include <chrono>
#include "SimpleController.hpp"
#include "TaskExecutor.hpp"
#include "Config.hpp"

void socket_adb() {
    ADBClient adb("/tmp/adb");
    if (!adb.connect("192.168.3.69","5555")) {
        std::cout<<"连接失败"<<std::endl;
        return;
    }
    adb.capture_screenshot("192.168.3.69:5555","123.png");
    if (!adb.disconnect("192.168.3.69","5555")) {
        std::cout<<"断开连接失败"<<std::endl;
        return;
    }
}

int main() {
    // 初始化控制器
    SimpleController controller;
    if (!controller.connect("/tmp/adb", "192.168.3.69:5555")) {
        std::cerr << "连接设备失败" << std::endl;
        return 1;
    }

    // 初始化任务执行器
    TaskExecutor executor(controller);
    executor.start();
    // 使用项目根目录拼接任务配置路径
    std::string task_path = std::string(Config::PROJECT_ROOT_DIR) + "/resource/tasks/start_arknights.json";
    executor.submit(task_path,[&](){});


    return 0;
}