#include <iostream>
#include "ADBClient.hpp"
#include <future>
#include <chrono>
#include "SimpleController.hpp"
#include "TaskExecutor.hpp"

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
    if (!controller.connect("adb", "192.168.3.69:5555")) {
        std::cerr << "连接设备失败" << std::endl;
        return 1;
    }

    // 初始化任务执行器
    TaskExecutor executor(controller);

    // 加载任务配置
    executor.load_task("resource/tasks/start_arknights.json");

    // 执行任务
    if (executor.run("start_arknights")) {
        std::cout << "任务执行成功" << std::endl;
    } else {
        std::cout << "任务执行失败" << std::endl;
    }

    return 0;
}