#pragma once
#include "TaskConfig.hpp"
#include <string>
#include <fstream>
#include <json/json.h>

// JSON 解析器
class TaskLoader {
public:
    // 从 JSON 文件加载任务
    static TaskConfig load_from_file(const std::string& path) {
        std::ifstream file(path);
        Json::Value root;
        Json::CharReaderBuilder builder;
        std::string errors;
        Json::parseFromStream(builder, file, &root, &errors);
        return parse_task(root);
    }

    // 从 JSON 字符串加载任务
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

        if (j.isMember("on_success")) {
            config.on_success = j["on_success"].asString();
        }
        if (j.isMember("on_failure")) {
            config.on_failure = j["on_failure"].asString();
        }

        if (j.isMember("steps")) {
            for (const auto& step_json : j["steps"]) {
                TaskStep step;
                step.action = step_json["action"].asString();
                step.x = step_json["x"].asInt();
                step.y = step_json["y"].asInt();
                step.x2 = step_json["x2"].asInt();
                step.y2 = step_json["y2"].asInt();
                step.duration = step_json["duration"].asInt();
                step.template_path = step_json["template_path"].asString();
                step.save_path = step_json["save_path"].asString();
                step.text = step_json["text"].asString();
                step.shell_cmd = step_json["shell_cmd"].asString();
                step.package_name = step_json["package_name"].asString();
                step.retry = step_json["retry"].asInt();
                step.timeout = step_json["timeout"].asInt();
                config.steps.push_back(step);
            }
        }
        return config;
    }
};
