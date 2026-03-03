#pragma once

#include "AdbStatus.hpp"
#include <deque>
#include <map>
#include <vector>
#include <boost/asio.hpp>

namespace net = boost::asio;
using tcp = net::ip::tcp;

// ADB 客户端，Socket 直连 ADB Server，支持常用设备管理与文件操作
class ADBClient {
public:
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
    tcp::socket connect_to_server(std::string_view host = "127.0.0.1", std::string_view port = "5037");
    std::string send_command(std::string_view command, std::string_view host = "127.0.0.1", std::string_view port = "5037");
    std::string send_device_command(std::string_view device_id, std::string_view command, std::string_view host = "127.0.0.1", std::string_view port = "5037");

    net::io_context io_context_;
    tcp::resolver resolver_;
    std::string work_dir_;
    bool work_dir_created_;
    std::vector<std::string> screenshot_paths_;
};
