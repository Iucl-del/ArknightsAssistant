//
// Created by zzk on 26-2-12.
//
#include "ADBClient.hpp"
#include "../../include/adb/AdbStatus.hpp"
#include <format>
#include <sstream>
#include <fstream>
#include <filesystem>

ADBClient::ADBClient(std::string_view work_dir)
    : io_context_()
    , resolver_(io_context_)
    , work_dir_(work_dir)
    , work_dir_created_(false)
{
    std::filesystem::path dir(work_dir_);
    if (!dir.empty() && !std::filesystem::exists(dir)) {
        std::filesystem::create_directories(dir);
        work_dir_created_ = true;
    }
}

ADBClient::~ADBClient() {
    if (work_dir_created_) {
        // 目录是自己创建的，直接整个删除（截图也在其中）
        std::filesystem::remove_all(work_dir_);
    } else {
        // 目录是已有的，只删除运行过程中产生的截图文件
        for (const auto& path : screenshot_paths_) {
            std::filesystem::remove(path);
        }
    }
}

tcp::socket ADBClient::connect_to_server(std::string_view host, std::string_view port) {
    tcp::socket socket(io_context_);
    auto endpoints = resolver_.resolve(host, port);
    net::connect(socket, endpoints);
    return socket;
}

std::string ADBClient::send_command(std::string_view command, std::string_view host, std::string_view port) {
    try {
        auto socket = connect_to_server(host, port);

        std::string request = std::format("{:04x}{}", command.length(), command);
        net::write(socket, net::buffer(request));

        char status[4];
        net::read(socket, net::buffer(status, 4));

        std::string result;
        if (std::string_view(status, 4) == "OKAY") {
            char len_buf[4];
            net::read(socket, net::buffer(len_buf, 4));
            int len = std::stoi(std::string(len_buf, 4), nullptr, 16);

            result.resize(len);
            net::read(socket, net::buffer(result.data(), len));
        }

        return result;
    } catch (const std::exception&) {
        return "";
    }
}

std::string ADBClient::send_device_command(std::string_view device_id, std::string_view command, std::string_view host, std::string_view port) {
    try {
        auto socket = connect_to_server(host, port);

        std::string transport_cmd = std::format("host:transport:{}", device_id);
        std::string request = std::format("{:04x}{}", transport_cmd.length(), transport_cmd);
        net::write(socket, net::buffer(request));

        char status[4];
        net::read(socket, net::buffer(status, 4));
        if (std::string_view(status, 4) != "OKAY") {
            return "";
        }

        request = std::format("{:04x}{}", command.length(), command);
        net::write(socket, net::buffer(request));

        net::read(socket, net::buffer(status, 4));
        if (std::string_view(status, 4) != "OKAY") {
            return "";
        }

        std::string result;
        char buffer[4096];
        boost::system::error_code ec;
        while (true) {
            size_t n = socket.read_some(net::buffer(buffer), ec);
            if (ec == net::error::eof || n == 0) break;
            if (ec) break;
            result.append(buffer, n);
        }

        return result;
    } catch (const std::exception&) {
        return "";
    }
}



bool ADBClient::connect(std::string_view ip, std::string_view port) {
    std::string cmd = std::format("host:connect:{}:{}", ip, port);
    std::string response = send_command(cmd);
    if (response.find("connected") != std::string::npos) {
        return true;
    }
    return false;
}

bool ADBClient::disconnect(std::string_view ip, std::string_view port) {
    std::string cmd = std::format("host:disconnect:{}:{}", ip, port);
    std::string response = send_command(cmd);
    if (response.find("disconnected") != std::string::npos) {
        return true;
    }
    return false;
}

std::string ADBClient::shell(std::string_view device_id, std::string_view command) {
    std::string shell_cmd = std::format("shell:{}", command);
    return send_device_command(device_id, shell_cmd);
}

std::deque<std::string> ADBClient::shell_lines(std::string_view device_id, std::string_view command) {
    std::string output = shell(device_id, command);
    std::deque<std::string> lines;
    std::istringstream iss(output);
    std::string line;
    while (std::getline(iss, line)) {
        // 移除行尾的\r（Android shell输出可能带\r\n）
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!line.empty()) {
            lines.push_back(std::move(line));
        }
    }
    return lines;
}

bool ADBClient::capture_screenshot(std::string_view device_id, std::string_view filename) {
    // 优先用 exec-out:screencap -p 获取原始 PNG 数据
    std::string png_data = send_device_command(device_id, "exec-out:screencap -p");

    // 如果 exec-out 不可用，再尝试 shell（兼容性兜底，但可能损坏）
    if (png_data.empty()) {
        png_data = shell(device_id, "screencap -p");
    }

    if (png_data.empty()) {
        return false;
    }

    // 生成完整保存路径
    std::filesystem::path save_path = std::filesystem::path(work_dir_) / std::string(filename);

    // 保存到本地文件
    std::ofstream file(save_path, std::ios::binary);
    if (!file) {
        return false;
    }
    file.write(png_data.data(), static_cast<std::streamsize>(png_data.size()));
    bool ok = file.good();
    file.close();

    // 记录截图路径，析构时统一清理
    if (ok) {
        screenshot_paths_.push_back(save_path.string());
    }

    return ok;
}

bool ADBClient::pull(std::string_view device_id, std::string_view remote_path, std::string_view local_path) {
    // 使用shell的cat命令读取文件内容
    std::string cmd = std::format("cat {}", remote_path);
    std::string content = shell(device_id, cmd);

    if (content.empty()) {
        return false;
    }

    std::ofstream file(std::string(local_path), std::ios::binary);
    if (!file) {
        return false;
    }
    file.write(content.data(), static_cast<std::streamsize>(content.size()));
    return file.good();
}

bool ADBClient::push(std::string_view device_id, std::string_view local_path, std::string_view remote_path) {
    // 读取本地文件
    std::ifstream file(std::string(local_path), std::ios::binary);
    if (!file) {
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    // 使用shell写入
    // 注意：这种方式对大文件效率较低，完整实现应使用sync协议
    std::string cmd = std::format("cat > {}", remote_path);

    try {
        auto socket = connect_to_server();

        std::string transport_cmd = std::format("host:transport:{}", device_id);
        std::string request = std::format("{:04x}{}", transport_cmd.length(), transport_cmd);
        net::write(socket, net::buffer(request));

        char status[4];
        net::read(socket, net::buffer(status, 4));
        if (std::string_view(status, 4) != "OKAY") {
            return false;
        }

        std::string shell_cmd = std::format("shell:{}", cmd);
        request = std::format("{:04x}{}", shell_cmd.length(), shell_cmd);
        net::write(socket, net::buffer(request));

        net::read(socket, net::buffer(status, 4));
        if (std::string_view(status, 4) != "OKAY") {
            return false;
        }

        net::write(socket, net::buffer(content));
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

