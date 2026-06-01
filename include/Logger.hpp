#pragma once

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <print>
#include <string>
#include <utility>

enum class LogLevel {
    Debug = 0,
    Info = 1,
    Warning = 2,
    Error = 3,
    Off = 4
};

class Logger {
public:
    static void set_level(LogLevel level) {
        level_ref().store(level, std::memory_order_relaxed);
    }

    static bool set_level_from_string(std::string level) {
        std::ranges::transform(level, level.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (level == "debug") {
            set_level(LogLevel::Debug);
            return true;
        }
        if (level == "info" || level == "log") {
            set_level(LogLevel::Info);
            return true;
        }
        if (level == "warning" || level == "warn") {
            set_level(LogLevel::Warning);
            return true;
        }
        if (level == "error") {
            set_level(LogLevel::Error);
            return true;
        }
        if (level == "off" || level == "none") {
            set_level(LogLevel::Off);
            return true;
        }
        return false;
    }

    static bool should_log(LogLevel level) {
        return static_cast<int>(level) >= static_cast<int>(level_ref().load(std::memory_order_relaxed));
    }

    template <typename... Args>
    static void print(LogLevel level, std::format_string<Args...> fmt, Args&&... args) {
        if (!should_log(level)) {
            return;
        }

        if (level == LogLevel::Error) {
            std::println(stderr, "[{}] {}", level_name(level), std::format(fmt, std::forward<Args>(args)...));
        } else {
            std::println("[{}] {}", level_name(level), std::format(fmt, std::forward<Args>(args)...));
        }
    }

    template <typename... Args>
    static void debug(std::format_string<Args...> fmt, Args&&... args) {
        print(LogLevel::Debug, fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void info(std::format_string<Args...> fmt, Args&&... args) {
        print(LogLevel::Info, fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void warning(std::format_string<Args...> fmt, Args&&... args) {
        print(LogLevel::Warning, fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void error(std::format_string<Args...> fmt, Args&&... args) {
        print(LogLevel::Error, fmt, std::forward<Args>(args)...);
    }

private:
    static std::atomic<LogLevel>& level_ref() {
        static std::atomic<LogLevel> level{LogLevel::Info};
        return level;
    }

    static const char* level_name(LogLevel level) {
        switch (level) {
            case LogLevel::Debug: return "Debug";
            case LogLevel::Info: return "Info";
            case LogLevel::Warning: return "Warning";
            case LogLevel::Error: return "Error";
            case LogLevel::Off: return "Off";
        }
        return "Log";
    }
};
