#include <iostream>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include "ADBClient.hpp"
#include <future>
#include <chrono>
#include "SimpleController.hpp"
#include "TaskExecutor.hpp"
#include "Config.hpp"

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    try
    {
        // 初始化控制器
        SimpleController controller;
        // if (!controller.connect("/tmp/adb", "192.168.3.43", "5555")) {
        //     std::cerr << "连接设备失败" << std::endl;
        //     return 1;
        // }
        //
        // // 初始化任务执行器
        // controller.get_executor()->start();
        //
        // // 投递任务
        // std::string task_path = std::string(Config::PROJECT_ROOT_DIR) + "/resource/tasks/start_arknights.json";
        // controller.get_executor()->submit(task_path, [&](){});

        InfrastructureManager infrastructure_manager(controller);
        infrastructure_manager.import_operators_from_skland();
        infrastructure_manager.scan_infrastructure();
        SchedulePlan plan = infrastructure_manager.optimize();
    }catch (std::exception& e){
        std::cout << "Exception: " << e.what() << std::endl;
    }
    getchar();
    return 0;
}