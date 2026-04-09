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
        config.loop = j.get("loop", false).asBool();
        config.loop_count = j.get("loop_count", 1).asInt();

        if (j.isMember("nodes")) {
            for (const auto& n : j["nodes"]) {
                TaskNode node;

                // 识别
                node.recognition = n.get("recognition", "DirectHit").asString();

                if (n.isMember("expected")) {
                    const auto& e = n["expected"];
                    if (e.isArray()) {
                        for (const auto& v : e) node.expected.push_back(v.asString());
                    } else {
                        node.expected.push_back(e.asString());
                    }
                }

                if (n.isMember("template")) {
                    const auto& t = n["template"];
                    if (t.isArray()) {
                        for (const auto& v : t) node.template_paths.push_back(v.asString());
                    } else {
                        node.template_paths.push_back(t.asString());
                    }
                }

                node.threshold = n.get("threshold", 0.8).asDouble();
                node.timeout = n.get("timeout", 10000).asInt();
                node.interval = n.get("interval", 100).asInt();

                // ROI: [x, y, w, h] 或 [x, y, w, h, base_w, base_h]
                if (n.isMember("roi")) {
                    const auto& r = n["roi"];
                    if (r.isArray() && r.size() >= 4) {
                        ROI roi;
                        roi.x = r[0].asInt();
                        roi.y = r[1].asInt();
                        roi.w = r[2].asInt();
                        roi.h = r[3].asInt();
                        roi.base_w = r.size() > 4 ? r[4].asInt() : 1280;
                        roi.base_h = r.size() > 5 ? r[5].asInt() : 720;
                        node.roi = roi;
                    }
                }

                // 动作
                node.action = n.get("action", "Click").asString();
                if (n.isMember("target")) {
                    for (const auto& v : n["target"]) {
                        node.target.push_back(v.asInt());
                    }
                }
                node.shell_cmd = n.get("shell_cmd", "").asString();

                // 延迟
                node.pre_delay = n.get("pre_delay", 0).asInt();
                node.post_delay = n.get("post_delay", 500).asInt();

                // 失败策略
                node.repeat_until_failed = n.get("repeat_until_failed", false).asBool();
                node.optional = n.get("optional", false).asBool();
                node.on_fail_jump = n.get("on_fail_jump", -1).asInt();

                config.nodes.emplace_back(std::move(node));
            }
        }

        return config;
    }
};