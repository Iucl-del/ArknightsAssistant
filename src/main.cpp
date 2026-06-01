#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <chrono>
#include <future>
#include <exception>

#include "ADBClient.hpp"
#include "Config.hpp"
#include "Logger.hpp"
#include "SimpleController.hpp"
#include "TaskExecutor.hpp"

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    Logger::set_level(LogLevel::Info);

    try {
        SimpleController controller;
        if (!controller.connect("D:/ArknightsAssistant/screenshot", "127.0.0.1", "16384")) {
            Logger::error("连接设备失败");
            return 1;
        }

        auto executor = controller.get_executor();
        executor->start();

        std::promise<void> done;
        auto future = done.get_future();

        std::string task_path = std::string(Config::PROJECT_ROOT_DIR) + "/resource/tasks/start_arknights.json";
        executor->submit(task_path, [&]() {
            done.set_value();
        });

        future.wait();
        getchar();
    } catch (std::exception& e) {
        Logger::error("异常: {}", e.what());
    }
    return 0;
}
