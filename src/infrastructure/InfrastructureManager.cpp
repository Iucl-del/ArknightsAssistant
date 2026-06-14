#include "infrastructure/InfrastructureManager.hpp"
#include "infrastructure/SkillParser.hpp"
#include "infrastructure/ScheduleOptimizer.hpp"
#include "Logger.hpp"
#include "SimpleController.hpp"
#include "Config.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <array>
#include <map>
#include <regex>
#include <set>
#include <string_view>

#include <opencv2/core.hpp>

namespace {
std::string normalize_ocr_text(std::string text) {
    text.erase(std::remove_if(text.begin(), text.end(), [](unsigned char ch) {
        return ch == '\r' || ch == '\n' || ch == ' ' || ch == '\t';
    }), text.end());
    return text;
}

int slots_for_facility(std::string_view type) {
    if (type == "控制中枢" || type == "宿舍") return 5;
    if (type == "贸易站" || type == "制造站") return 3;
    if (type == "会客室") return 2;
    return 1;
}

std::string id_prefix_for_facility(std::string_view type) {
    if (type == "贸易站") return "trading";
    if (type == "制造站") return "manufact";
    if (type == "发电站") return "power";
    if (type == "会客室") return "meeting";
    if (type == "加工站") return "workshop";
    if (type == "办公室") return "hire";
    if (type == "训练室") return "training";
    return "facility";
}

std::string production_type_for_facility(std::string_view type, int index, int manufact_count) {
    if (type == "贸易站") return "lmd";
    if (type != "制造站") return "";

    int gold_count = std::clamp(manufact_count / 2, 1, manufact_count);
    return index <= gold_count ? "gold" : "record";
}

std::string facility_id(std::string_view type, int index) {
    const std::string prefix = id_prefix_for_facility(type);
    if (type == "控制中枢") return "control";
    if (type == "宿舍") return "dormitory_" + std::to_string(index);
    if (type == "会客室" || type == "加工站" || type == "办公室" || type == "训练室") return prefix;
    return prefix + "_" + std::to_string(index);
}

int parse_facility_index(const std::string& value) {
    if (value.empty()) return 1;

    for (auto it = value.rbegin(); it != value.rend(); ++it) {
        if (*it >= '0' && *it <= '9') {
            return std::max(1, *it - '0');
        }
    }

    return 1;
}

std::string production_type_from_text(std::string_view type, std::string_view text) {
    if (text.find("龙门商法") != std::string_view::npos ||
        text.find("龙门币") != std::string_view::npos ||
        text.find("订单") != std::string_view::npos) {
        return "lmd";
    }
    if (text.find("开采协力") != std::string_view::npos ||
        text.find("源石") != std::string_view::npos) {
        return "originium";
    }
    if (text.find("赤金") != std::string_view::npos) {
        return "gold";
    }
    if (text.find("作战记录") != std::string_view::npos ||
        text.find("录像") != std::string_view::npos ||
        text.find("经验") != std::string_view::npos) {
        return "record";
    }
    if (text.find("芯片") != std::string_view::npos) {
        return "chip";
    }
    return type == "贸易站" ? "lmd" : "";
}

void add_facility(std::map<std::string, FacilityInfo>& facilities,
                  std::string_view type,
                  int index,
                  const std::string& production_type) {
    facilities[facility_id(type, index)] = {
        std::string(type),
        production_type,
        type == "控制中枢" || type == "宿舍" ? 5 : 3,
        slots_for_facility(type),
        {}
    };
}

std::set<std::string> parse_facility_page(const std::string& ocr_text,
                                          std::map<std::string, FacilityInfo>& facilities) {
    std::set<std::string> found_ids;
    const std::string text = normalize_ocr_text(ocr_text);
    static const std::regex facility_pattern(
        "(贸易站|制造站|发电站|控制中枢|会客室|加工站|办公室|训练室)([0-9]?)");

    std::vector<std::smatch> matches;
    for (auto it = std::sregex_iterator(text.begin(), text.end(), facility_pattern);
         it != std::sregex_iterator();
         ++it) {
        matches.push_back(*it);
    }

    for (size_t i = 0; i < matches.size(); ++i) {
        const auto& match = matches[i];
        const std::string type = match[1].str();
        const int index = parse_facility_index(match[2].str());
        const size_t start = static_cast<size_t>(match.position());
        const size_t next = i + 1 < matches.size()
            ? static_cast<size_t>(matches[i + 1].position())
            : std::min(text.size(), start + static_cast<size_t>(80));
        const std::string segment = text.substr(start, next - start);
        const std::string production_type = production_type_from_text(type, segment);
        const std::string id = facility_id(type, index);

        add_facility(facilities, type, index, production_type);
        found_ids.insert(id);
    }

    return found_ids;
}
}

InfrastructureManager::InfrastructureManager(SimpleController& controller)
    : controller_(controller) {
    load_skill_database();
}

bool InfrastructureManager::load_skill_database() {
    std::string path = std::string(Config::PROJECT_ROOT_DIR) + "/resource/data/operator_skills.json";
    std::ifstream file(path);
    if (!file.is_open()) {
        Logger::error("[Infrastructure] 无法打开技能数据库: {}", path);
        return false;
    }

    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errors;

    if (!Json::parseFromStream(builder, file, &root, &errors)) {
        Logger::error("[Infrastructure] JSON 解析失败: {}", errors);
        return false;
    }

    SkillParser parser;

    for (const auto& char_id : root.getMemberNames()) {
        const auto& char_data = root[char_id];

        OperatorInfo op;
        op.id = char_id;
        op.name = char_data.get("name", "").asString();
        op.rarity = char_data.get("rarity", "TIER_1").asString();

        // 解析组织信息
        if (char_data.isMember("meta")) {
            const auto& meta_data = char_data["meta"];
            op.meta.nation_id = meta_data.get("nation_id", "").asString();
            op.meta.group_id = meta_data.get("group_id", "").asString();
            op.meta.team_id = meta_data.get("team_id", "").asString();
            op.meta.profession = meta_data.get("profession", "").asString();
            op.meta.sub_profession = meta_data.get("sub_profession", "").asString();
        }

        // 解析技能
        const auto& skills = char_data["skills"];
        for (const auto& skill_data : skills) {
            ParsedOperatorSkill parsed;
            parsed.buff_id = skill_data.get("buff_id", "").asString();
            parsed.name = skill_data.get("name", "").asString();
            parsed.facility = skill_data.get("facility", "").asString();
            parsed.unlock_elite = skill_data.get("unlock_elite", 0).asInt();
            parsed.unlock_level = skill_data.get("unlock_level", 1).asInt();

            std::string description = skill_data.get("description", "").asString();
            parsed.effects = parser.parse(description, parsed.facility);

            op.parsed_skills.push_back(parsed);
        }

        skill_db_[char_id] = op;
    }

    Logger::info("[Infrastructure] 加载技能数据库完成，干员数量: {}", skill_db_.size());
    return true;
}

bool InfrastructureManager::import_operators_from_skland() {
    // 调用 Python 脚本获取干员数据
    std::string root_dir = Config::PROJECT_ROOT_DIR;
    std::string script = root_dir + "/scripts/get_operators.py";

    if (!std::filesystem::exists(script)) {
        Logger::error("[Infrastructure] 脚本不存在: {}", script);
        return false;
    }

    std::string command = "python \"" + script + "\"";
    Logger::info("[Infrastructure] 执行脚本: {}", command);

    int ret = std::system(command.c_str());
    if (ret != 0) {
        Logger::error("[Infrastructure] 脚本执行失败，返回码: {}", ret);
        return false;
    }

    // 查找 resource/data 下最新的 player_full_*.json
    std::filesystem::path data_dir = std::filesystem::path(root_dir) / "resource" / "data";
    if (!std::filesystem::exists(data_dir)) {
        Logger::error("[Infrastructure] 数据目录不存在: {}", data_dir.string());
        return false;
    }

    std::filesystem::path latest;
    std::filesystem::file_time_type latest_time{};

    for (const auto& entry : std::filesystem::directory_iterator(data_dir)) {
        auto filename = entry.path().filename().string();
        if (filename.starts_with("player_full_") && filename.ends_with(".json")) {
            auto ftime = entry.last_write_time();
            if (latest.empty() || ftime > latest_time) {
                latest = entry.path();
                latest_time = ftime;
            }
        }
    }

    if (latest.empty()) {
        Logger::error("[Infrastructure] 未找到生成的干员数据文件");
        return false;
    }

    Logger::info("[Infrastructure] 干员数据文件: {}", latest.string());

    // 解析干员数据
    std::ifstream file(latest);
    if (!file.is_open()) {
        Logger::error("[Infrastructure] 无法打开森空岛数据文件: {}", latest.string());
        return false;
    }

    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errors;

    if (!Json::parseFromStream(builder, file, &root, &errors)) {
        Logger::error("[Infrastructure] 森空岛数据解析失败: {}", errors);
        return false;
    }

    // 清空当前干员列表
    my_operators_.clear();

    // 森空岛数据格式: { "chars": [ { "charId": "char_xxx", "evolvePhase": 2, "level": 80 }, ... ] }
    const auto& chars = root["chars"];
    if (!chars.isArray()) {
        Logger::error("[Infrastructure] 森空岛数据格式错误: 缺少 chars 数组");
        return false;
    }

    int imported_count = 0;
    int skipped_count = 0;

    for (const auto& char_data : chars) {
        std::string char_id = char_data.get("charId", "").asString();
        if (char_id.empty()) continue;

        // 在技能数据库中查找该干员
        auto it = skill_db_.find(char_id);
        if (it == skill_db_.end()) {
            // 干员不在技能数据库中（可能是新干员或无基建技能）
            skipped_count++;
            continue;
        }

        // 复制干员数据并更新等级信息
        OperatorInfo op = it->second;
        op.elite = char_data.get("evolvePhase", 0).asInt();
        op.level = char_data.get("level", 1).asInt();
        op.mood = 24.0f;  // 默认满心情

        my_operators_[char_id] = op;
        imported_count++;
    }

    Logger::info("[Infrastructure] 从森空岛导入干员完成");
    Logger::info("[Infrastructure]   导入成功: {} 个干员", imported_count);
    Logger::info("[Infrastructure]   跳过: {} 个（无基建技能或未知干员）", skipped_count);

    return imported_count > 0;
}

bool InfrastructureManager::use_all_operators_for_test() {
    Logger::info("[Infrastructure] 使用全部干员进行测试（满级满精英）");

    my_operators_ = skill_db_;

    for (auto& [id, op] : my_operators_) {
        op.elite = 2;
        op.level = 90;
        op.mood = 24.0f;
    }

    Logger::info("[Infrastructure]   加载干员: {} 个", my_operators_.size());
    return true;
}

bool InfrastructureManager::scan_infrastructure() {
    Logger::info("[Infrastructure] 从基建浏览页面进入进驻总览...");

    std::string screenshot = controller_.auto_screenshot("infrastructure_overview");
    if (screenshot.empty()) {
        Logger::error("[Infrastructure] 基建截图失败");
        return false;
    }

    cv::Point overview_pos;
    if (controller_.find_text(screenshot, "进驻总览", overview_pos)) {
        controller_.click(overview_pos);
        controller_.wait(800);
    } else {
        Logger::error("[Infrastructure] 未找到进驻总览入口，请确认当前处于基建浏览页面");
        return false;
    }

    facilities_.clear();
    std::set<std::string> seen_ids;
    int stagnant_pages = 0;
    constexpr int min_scan_pages = 6;
    constexpr int max_scan_pages = 14;

    // 仅截取左侧设施名称栏；坐标以 SimpleController 统一的 1280x720 视觉基准表示。
    const ROI list_roi{420, 70, 300, 610, 1280, 720};
    for (int page = 0; page < max_scan_pages && stagnant_pages < 4; ++page) {
        screenshot = controller_.auto_screenshot("infrastructure_roster");
        if (screenshot.empty()) {
            Logger::error("[Infrastructure] 进驻总览截图失败");
            return false;
        }

        std::string text;
        if (!controller_.detect_text(screenshot, text, list_roi)) {
            Logger::error("[Infrastructure] 进驻总览 OCR 识别失败: {}", screenshot);
            return false;
        }

        auto page_ids = parse_facility_page(text, facilities_);
        int new_count = 0;
        for (const auto& id : page_ids) {
            if (seen_ids.insert(id).second) {
                ++new_count;
            }
        }

        Logger::debug("[Infrastructure] 第 {} 页 OCR: {}", page + 1, normalize_ocr_text(text));
        Logger::debug("[Infrastructure] 第 {} 页新增设施: {}，累计: {}", page + 1, new_count, seen_ids.size());

        if (new_count == 0 && !seen_ids.empty() && page + 1 >= min_scan_pages) {
            stagnant_pages++;
        } else {
            stagnant_pages = 0;
        }
        if (stagnant_pages >= 4) break;

        controller_.swipe(cv::Point(1030, 570), cv::Point(1030, 500), 180);
        controller_.wait(450);
    }

    if (facilities_.empty()) {
        Logger::error("[Infrastructure] 未从进驻总览识别到任何设施");
        return false;
    }

    const int manufact_count = static_cast<int>(std::count_if(
        facilities_.begin(),
        facilities_.end(),
        [](const auto& item) { return item.second.type == "制造站"; }));
    for (auto& [id, facility] : facilities_) {
        if (facility.type == "制造站" && facility.production_type.empty()) {
            const auto pos = id.find_last_of('_');
            const int index = pos == std::string::npos ? 1 : std::atoi(id.substr(pos + 1).c_str());
            facility.production_type = production_type_for_facility(facility.type, index, manufact_count);
        }
    }

    for (int i = 1; i <= 4; ++i) {
        add_facility(facilities_, "宿舍", i, "");
    }

    std::map<std::string, int> counts;
    for (const auto& [id, facility] : facilities_) {
        counts[facility.type]++;
        Logger::debug("[Infrastructure] 设施: {} type={} production={} slots={}",
                      id, facility.type, facility.production_type, facility.slots);
    }

    if (counts["贸易站"] == 0 || counts["制造站"] == 0 || counts["发电站"] == 0) {
        Logger::error("[Infrastructure] 生产设施识别不完整，请确认进驻总览列表已完整扫描");
        return false;
    }

    return true;
}

SchedulePlan InfrastructureManager::optimize() {
    Logger::info("[Infrastructure] 开始优化排班（贪心初始化 + 模拟退火）...");

    ScheduleOptimizer optimizer(*this);

    OptimizerConfig config;
    config.max_iterations = 50000;
    config.initial_temperature = 100.0;
    config.cooling_rate = 0.995;
    config.min_temperature = 0.01;
    config.swap_weight = 0.5;
    config.move_weight = 0.3;
    config.replace_weight = 0.2;
    config.plateau_threshold = 1000;

    auto plan = optimizer.optimize(config);

    // 打印排班结果
    Logger::info("{}", std::string(60, '='));
    Logger::info("[Infrastructure] 排班优化完成");
    Logger::info("[Infrastructure]   迭代次数: {}  耗时: {}ms  总效率: {}",
                 plan.iterations, plan.optimization_time_ms, plan.total_efficiency);
    Logger::info("{}", std::string(60, '-'));

    for (const auto& assignment : plan.assignments) {
        std::string line = "  " + assignment.facility_type + " [" + assignment.facility_id + "]";

        // 显示生产类型
        auto fac_it = facilities_.find(assignment.facility_id);
        if (fac_it != facilities_.end() && !fac_it->second.production_type.empty()) {
            const auto& pt = fac_it->second.production_type;
            std::string pt_name = pt == "gold" ? "赤金" : pt == "record" ? "作战记录" : pt == "chip" ? "芯片" : pt == "lmd" ? "龙门币" : pt;
            line += " (" + pt_name + ")";
        }

        Logger::info("{}  效率: +{}%", line, assignment.total_efficiency * 100);

        std::string operators_line = "    干员:";
        for (size_t i = 0; i < assignment.operator_ids.size(); ++i) {
            const auto& op_id = assignment.operator_ids[i];
            if (my_operators_.contains(op_id)) {
                operators_line += (i > 0 ? ", " : " ");
                operators_line += my_operators_.at(op_id).name;
            }
        }
        Logger::info("{}", operators_line);
    }

    Logger::info("{}", std::string(60, '='));

    return plan;
}

bool InfrastructureManager::apply(const SchedulePlan& plan) {
    // TODO: 实现自动换班
    // 1. 遍历每个设施
    // 2. 清空当前干员
    // 3. 添加新干员

    Logger::info("[Infrastructure] 排班方案:");
    Logger::info("[Infrastructure]   总效率: {}", plan.total_efficiency);
    Logger::info("[Infrastructure]   迭代次数: {}", plan.iterations);
    Logger::info("[Infrastructure]   优化耗时: {}ms", plan.optimization_time_ms);

    for (const auto& assignment : plan.assignments) {
        std::string line = "  " + assignment.facility_id + " (" + assignment.facility_type + "):";
        for (const auto& op_id : assignment.operator_ids) {
            if (my_operators_.count(op_id)) {
                line += " " + my_operators_.at(op_id).name;
            }
        }
        Logger::info("{}", line);
    }

    return true;
}

bool InfrastructureManager::has_skill_for_facility(const OperatorInfo& op, const std::string& facility_type) const {
    for (const auto& skill : op.parsed_skills) {
        if (skill.facility == facility_type) {
            // 检查是否已解锁
            if (op.elite > skill.unlock_elite) {
                return true;
            }
            if (op.elite == skill.unlock_elite && op.level >= skill.unlock_level) {
                return true;
            }
        }
    }
    return false;
}

double InfrastructureManager::calculate_base_efficiency(const OperatorInfo& op, const std::string& facility_type) {
    double efficiency = 0.0;

    for (const auto& skill : op.parsed_skills) {
        if (skill.facility != facility_type) continue;

        // 检查是否已解锁
        if (op.elite < skill.unlock_elite) continue;
        if (op.elite == skill.unlock_elite && op.level < skill.unlock_level) continue;

        // 只计算基础加成（不考虑联动）
        for (const auto& effect : skill.effects) {
            if (effect.type == SkillEffectType::FLAT_BONUS) {
                efficiency += effect.value;
            }
        }
    }

    return efficiency;
}
