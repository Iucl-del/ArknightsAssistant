#pragma once
#include <string>
#include <vector>

// ADB类用于管理Android设备的ADB操作
class ADB {
public:
    // 构造函数，指定adb工具路径
    explicit ADB(const std::string& adb_path = "adb");
    // 获取已连接设备列表
    std::vector<std::string> list_devices(){return devices;}
    // 通过IP和端口连接设备
    bool connect(const std::string& ip, const std::string& port);
    // 断开指定设备的连接
    bool disconnect(const std::string& device_id);
    // 执行adb命令，返回命令输出
    std::string exec_cmd(const std::string& cmd);
    // 返回每一行的字符串容器，自动分割
    std::vector<std::string> exec_cmd_lines(const std::string& cmd);
    // 对指定设备进行截图，并保存到指定路径
    bool capture_screenshot(const std::string& device_id, const std::string& save_path);

private:
    // adb工具路径
    std::string adb_path;
    // 已连接设备列表
    std::vector<std::string> devices;
};
