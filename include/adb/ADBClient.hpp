#pragma once

#include "AdbStatus.hpp"
#include <deque>
#include <map>
#include <boost/asio.hpp>

// ADB 客户端，Socket 直连 ADB Server，支持常用设备管理与文件操作
class ADBClient {
public:
    /**
     * @brief 构造函数
     * @param work_dir ADB工作目录，所有adb产生的文件存放于此，默认为当前目录
     */
    explicit ADBClient(std::string_view work_dir = "adb");
    ~ADBClient();

    std::map<std::string, AdbDeviceStatus> list_devices();
    bool connect(std::string_view ip, std::string_view port);
    bool disconnect(std::string_view ip, std::string_view port);
    std::string shell(std::string_view device_id, std::string_view command);
    std::deque<std::string> shell_lines(std::string_view device_id, std::string_view command);
    bool capture_screenshot(std::string_view device_id, std::string_view save_path);
    bool pull(std::string_view device_id, std::string_view remote_path, std::string_view local_path);
    bool push(std::string_view device_id, std::string_view local_path, std::string_view remote_path);

private:
    // 连接到 ADB Server
    boost::asio::ip::tcp::socket connect_to_server(std::string_view host = "127.0.0.1", std::string_view port = "5037");
    // 发送 ADB 协议命令
    std::string send_command(std::string_view command, std::string_view host = "127.0.0.1", std::string_view port = "5037");
    // 选择设备并发送命令
    std::string send_device_command(std::string_view device_id, std::string_view command, std::string_view host = "127.0.0.1", std::string_view port = "5037");

    boost::asio::io_context io_context_;  // ASIO IO上下文
    boost::asio::ip::tcp::resolver resolver_; // TCP解析器
    std::string work_dir_; // ADB工作目录
};
