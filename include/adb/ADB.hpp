#pragma once

#include "AdbStatus.hpp"
#include <deque>
#include <vector>
#include <map>



/**
 * @brief ADB类用于管理Android设备的ADB操作
 */
class ADB {
public:
    /**
     * @brief 构造函数
     * @param work_dir ADB工作目录，所有产出文件存放于此，默认为当前目录
     */
    explicit ADB(const std::string& work_dir = "adb");

    /**
     * @brief 通过IP和端口连接设备
     * @param ip 设备IP地址
     * @param port 连接端口
     * @return 连接是否成功
     */
    bool connect(std::string_view ip, std::string_view port);

    /**
     * @brief 断开指定设备的连接
     * @param ip 设备IP地址
     * @param port 连接端口
     * @return 断开是否成功
     */
    bool disconnect(std::string_view ip, std::string_view port);

    /**
     * @brief 执行adb命令，返回命令输出
     * @param cmd 要执行的命令
     * @return 命令输出结果
     */
    std::string exec_cmd(const std::string& cmd);

    /**
     * @brief 返回每一行的字符串容器，自动分割
     * @param cmd 要执行的命令
     * @return 按行分割的输出结果
     */
    std::deque<std::string> exec_cmd_lines(const std::string& cmd);

    /**
     * @brief 对指定设备进行截图，保存到构造时指定的目录
     * @param device_id 设备ID
     * @param filename 截图文件名（可选，默认使用时间戳命名）
     * @return 截图保存的完整路径，失败返回空字符串
     */
    std::string capture_screenshot(std::string_view device_id, std::string_view filename = "");

private:
    std::string work_dir_;             ///< ADB工作目录
};
