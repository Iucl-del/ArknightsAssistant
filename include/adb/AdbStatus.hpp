#pragma once
#include <string>

enum class AdbDeviceStatus {
    DEVICE,
    OFFLINE,
    UNAUTHORIZED,
    NO_PERMISSIONS,
    BOOTLOADER,
    RECOVERY,
    SIDELOAD,
    AUTHORIZING
};

AdbDeviceStatus stringToAdbDeviceStatus(std::string_view str);
