#include "../../include/adb/AdbStatus.hpp"

AdbDeviceStatus stringToAdbDeviceStatus(std::string_view str) {
    if (str == "device")         return AdbDeviceStatus::DEVICE;
    if (str == "offline")        return AdbDeviceStatus::OFFLINE;
    if (str == "unauthorized")   return AdbDeviceStatus::UNAUTHORIZED;
    if (str == "no permissions") return AdbDeviceStatus::NO_PERMISSIONS;
    if (str == "bootloader")     return AdbDeviceStatus::BOOTLOADER;
    if (str == "recovery")       return AdbDeviceStatus::RECOVERY;
    if (str == "sideload")       return AdbDeviceStatus::SIDELOAD;
    if (str == "authorizing")    return AdbDeviceStatus::AUTHORIZING;
    return AdbDeviceStatus::OFFLINE;
}
