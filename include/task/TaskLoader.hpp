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
                node.expected = n.get("expected", "").asString();
                node.template_path = n.get("template", "").asString();
                node.timeout = n.get("timeout", 20000).asInt();
                node.interval = n.get("interval", 1000).asInt();

                if (n.isMember("roi")) {
                    const auto& r = n["roi"];
                    ROI roi;
                    roi.x = r["x"].asInt();
                    roi.y = r["y"].asInt();
                    roi.w = r["w"].asInt();
                    roi.h = r["h"].asInt();
                    roi.base_w = r.get("base_w", 1280).asInt();
                    roi.base_h = r.get("base_h", 720).asInt();
                    node.roi = roi;
                }

                // 动作
                node.action = n.get("action", "Click").asString();
                if (n.isMember("target")) {
                    for (const auto& v : n["target"]) {
                        node.target.push_back(v.asInt());
                    }
                }

                // 延迟
                node.pre_delay = n.get("pre_delay", 0).asInt();
                node.post_delay = n.get("post_delay", 500).asInt();

                config.nodes.emplace_back(std::move(node));
            }
        }


        return config;
    }
};
