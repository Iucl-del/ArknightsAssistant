//
// Created by zzk on 26-2-3.
//
#include "../inclide/ADB.hpp"
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <array>
#include <boost/process.hpp>
#include <sstream>
#include <ranges>


ADB::ADB(const std::string& adb_path) : adb_path(adb_path) {
    std::string cmd = "adb devices";
    devices = exec_cmd(cmd);
    devices.erase(devices.begin(), devices.begin()+2);
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
std::vector<std::string> ADB::exec_cmd_lines(const std::string& cmd) {
    std::vector<std::string> lines;
    boost::process::ipstream pipe_stream;
    boost::process::child c(cmd, boost::process::std_out > pipe_stream);
    std::string line;
    while (pipe_stream && std::getline(pipe_stream, line)) {
        lines.push_back(std::move(line));
    }
    c.wait();
    return std::move(lines);
}

bool ADB::connect(const std::string& ip, const std::string& port) {
    std::string cmd = "adb connect " + ip + ":" + port;
    std::string result = exec_cmd(cmd);
    // 判断输出内容
    if (result.find("connected") != std::string::npos || result.find("already connected") != std::string::npos) {
        return true;
    }
    return false;
}

bool ADB::disconnect(const std::string& device_id) {
    std::string cmd = "adb disconnect " + device_id;
    std::string result = exec_cmd(cmd);
    // adb disconnect 输出通常包含 "disconnected" 字样
    return result.find("disconnected") != std::string::npos;
}
