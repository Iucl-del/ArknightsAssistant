#pragma once
#include "TaskConfig.hpp"
#include <string>
#include <fstream>
#include <iostream>
#include <json/json.h>

class TaskLoader {
public:
    static TaskConfig load_from_file(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "无法打开任务文件: " << path << std::endl;
            return TaskConfig{};
        }
        Json::Value root;
        Json::CharReaderBuilder builder;
        std::string errors;
        if (!Json::parseFromStream(builder, file, &root, &errors)) {
            std::cerr << "JSON 解析失败: " << errors << std::endl;
            return TaskConfig{};
        }
        return parse_task(root);
    }

    static TaskConfig load_from_string(const std::string& json_str) {
        Json::Value root;
        Json::CharReaderBuilder builder;
        std::istringstream stream(json_str);
        std::string errors;
        Json::parseFromStream(builder, stream, &root, &errors);
        return parse_task(root);
    }

private:
    static TaskConfig parse_task(const Json::Value& j) {
        TaskConfig config;
        config.name = j.get("name", "").asString();
        config.description = j.get("description", "").asString();
        config.loop = j.get("loop", false).asBool();
        config.loop_count = j.get("loop_count", 1).asInt();

        if (j.isMember("on_success")) config.on_success = j["on_success"].asString();
        if (j.isMember("on_failure")) config.on_failure = j["on_failure"].asString();

        if (j.isMember("steps")) {
            for (const auto& s : j["steps"]) {
                std::string action = s["action"].asString();

                // 基础操作
                if (action == "click" || action == "swipe" || action == "wait") {
                    BasicStep step;
                    step.action = action;
                    step.x = s["x"].asInt();
                    step.y = s["y"].asInt();
                    step.x2 = s["x2"].asInt();
                    step.y2 = s["y2"].asInt();
                    step.duration = s["duration"].asInt();
                    config.steps.push_back(step);
                }
                // 视觉操作
                else if (action == "screenshot" || action == "ocr" || action == "ocr_click" ||
                         action == "ocr_region" || action == "template") {
                    VisionStep step;
                    step.action = action;
                    step.image_name = s["save_name"].asString();
                    step.text = s["text"].asString();
                    step.template_path = s["template_path"].asString();
                    step.retry = s.get("retry", 1).asInt();
                    step.timeout = s.get("timeout", 5000).asInt();

                    if (s.isMember("roi")) {
                        ROIConfig roi;
                        const auto& r = s["roi"];
                        roi.x = r["x"].asInt();
                        roi.y = r["y"].asInt();
                        roi.width = r["width"].asInt();
                        roi.height = r["height"].asInt();
                        roi.base_width = r.get("base_width", 1280).asInt();
                        roi.base_height = r.get("base_height", 720).asInt();
                        roi.preprocess = r.get("preprocess", "auto").asString();
                        roi.filter_pattern = r.get("filter_pattern", "").asString();
                        roi.debug_save = r.get("debug_save", false).asBool();
                        step.roi = roi;
                    }
                    config.steps.push_back(step);
                }
                // 系统操作
                else if (action == "shell" || action == "start_app") {
                    SystemStep step;
                    step.action = action;
                    step.cmd = s["shell_cmd"].asString();
                    step.package_name = s["package_name"].asString();
                    config.steps.push_back(step);
                }
                else {
                    std::cerr << "未知操作: " << action << std::endl;
                }
            }
        }
        return config;
    }
};
