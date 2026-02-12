//
// Created by zzk on 26-2-3.
//
#include "ADB.hpp"
#include "../../include/adb/AdbStatus.hpp"
#include <memory>
#include <boost/process.hpp>
#include <ranges>
#include <format>
#include <fstream>
#include <chrono>

ADB::ADB(const std::string& work_dir) : work_dir_(work_dir) {
}

std::string ADB::exec_cmd(const std::string& cmd) {
    std::string result;
    boost::process::ipstream pipe_stream;
    boost::process::child c(cmd, boost::process::std_out > pipe_stream);
    std::string line;
    while (pipe_stream && std::getline(pipe_stream, line)) {
        result += line + "\n";
    }
    c.wait();
    return result;
}

// 返回每一行的字符串容器，自动分割
std::deque<std::string> ADB::exec_cmd_lines(const std::string& cmd) {
    std::deque<std::string> lines;
    boost::process::ipstream pipe_stream;
    boost::process::child c(cmd, boost::process::std_out > pipe_stream);
    std::string line;
    while (pipe_stream && std::getline(pipe_stream, line)) {
        lines.push_back(std::move(line));
    }
    c.wait();
    return std::move(lines);
}

bool ADB::connect(std::string_view ip, std::string_view port) {
    std::string cmd = std::format("adb connect {}:{}", ip, port);
    std::string result = exec_cmd(cmd);
    // 判断输出内容
    if (result.find("connected") != std::string::npos || result.find("already connected") != std::string::npos) {
        return true;
    }
    return false;
}

bool ADB::disconnect(std::string_view ip, std::string_view port) {
    std::string cmd = std::format("adb disconnect {}:{}", ip, port);
    std::string result = exec_cmd(cmd);
    // adb disconnect 输出通常包含 "disconnected" 字样
    return result.find("disconnected") != std::string::npos;
}



std::string ADB::capture_screenshot(std::string_view device_id, std::string_view filename) {
    // 生成文件名
    std::string save_filename;
    if (filename.empty()) {
        // 使用时间戳命名
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        save_filename = std::format("screenshot_{}.png", timestamp);
    } else {
        save_filename = std::string(filename);
        // 确保有 .png 后缀
        if (save_filename.find(".png") == std::string::npos) {
            save_filename += ".png";
        }
    }

    // 完整路径
    std::string save_path = std::format("{}/{}", work_dir_, save_filename);

    // 使用 adb exec-out 直接获取 PNG 数据
    std::string cmd = std::format("adb -s {} exec-out screencap -p", device_id);

    boost::process::ipstream pipe_stream;
    boost::process::child c(cmd, boost::process::std_out > pipe_stream);

    // 读取二进制 PNG 数据
    std::string png_data((std::istreambuf_iterator<char>(pipe_stream)),
                          std::istreambuf_iterator<char>());
    c.wait();

    if (png_data.empty() || c.exit_code() != 0) {
        return "";
    }

    // 保存到文件
    std::ofstream file(save_path, std::ios::binary);
    if (!file) {
        return "";
    }
    file.write(png_data.data(), static_cast<std::streamsize>(png_data.size()));

    return file.good() ? save_path : "";
}
