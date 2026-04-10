#include "infrastructure/InfrastructureManager.hpp"
#include "infrastructure/SkillParser.hpp"
#include "infrastructure/ScheduleOptimizer.hpp"
#include "SimpleController.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>

InfrastructureManager::InfrastructureManager(SimpleController& controller)
    : controller_(controller) {
}

bool InfrastructureManager::load_skill_database(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[Infrastructure] 无法打开技能数据库: " << path << std::endl;
        return false;
    }

    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errors;

    if (!Json::parseFromStream(builder, file, &root, &errors)) {
        std::cerr << "[Infrastructure] JSON 解析失败: " << errors << std::endl;
        return false;
    }

    if (!parse_skill_json(root)) {
        return false;
    }

    std::cout << "[Infrastructure] 加载技能数据库完成，干员数量: " << skill_db_.size() << std::endl;
    return true;
}

bool InfrastructureManager::parse_skill_json(const Json::Value& root) {
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

    return true;
}

bool InfrastructureManager::import_operators_from_skland(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[Infrastructure] 无法打开森空岛数据文件: " << path << std::endl;
        return false;
    }

    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errors;

    if (!Json::parseFromStream(builder, file, &root, &errors)) {
        std::cerr << "[Infrastructure] 森空岛数据解析失败: " << errors << std::endl;
        return false;
    }

    // 清空当前干员列表
    my_operators_.clear();

    // 森空岛数据格式: { "chars": [ { "charId": "char_xxx", "evolvePhase": 2, "level": 80 }, ... ] }
    const auto& chars = root["chars"];
    if (!chars.isArray()) {
        std::cerr << "[Infrastructure] 森空岛数据格式错误: 缺少 chars 数组" << std::endl;
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

    std::cout << "[Infrastructure] 从森空岛导入干员完成" << std::endl;
    std::cout << "  导入成功: " << imported_count << " 个干员" << std::endl;
    std::cout << "  跳过: " << skipped_count << " 个（无基建技能或未知干员）" << std::endl;

    return imported_count > 0;
}

bool InfrastructureManager::use_all_operators_for_test() {
    std::cout << "[Infrastructure] 使用全部干员进行测试（满级满精英）" << std::endl;

    my_operators_ = skill_db_;

    for (auto& [id, op] : my_operators_) {
        op.elite = 2;
        op.level = 90;
        op.mood = 24.0f;
    }

    std::cout << "  加载干员: " << my_operators_.size() << " 个" << std::endl;
    return true;
}

bool InfrastructureManager::scan_infrastructure() {
    // TODO: 实现识别基建布局
    // 1. 进入基建界面
    // 2. 识别设施类型和等级
    // 3. 识别当前入驻干员

    std::cout << "[Infrastructure] scan_infrastructure() 待实现，使用默认252布局" << std::endl;

    // 默认基建布局（252配置）
    facilities_["trading_1"] = {"贸易站", 3, 3, {}};
    facilities_["trading_2"] = {"贸易站", 3, 3, {}};
    facilities_["manufact_1"] = {"制造站", 3, 3, {}};
    facilities_["manufact_2"] = {"制造站", 3, 3, {}};
    facilities_["manufact_3"] = {"制造站", 3, 3, {}};
    facilities_["manufact_4"] = {"制造站", 3, 3, {}};
    facilities_["manufact_5"] = {"制造站", 3, 3, {}};
    facilities_["power_1"] = {"发电站", 3, 1, {}};
    facilities_["power_2"] = {"发电站", 3, 1, {}};
    facilities_["power_3"] = {"发电站", 3, 1, {}};
    facilities_["control"] = {"控制中枢", 5, 5, {}};
    facilities_["dormitory_1"] = {"宿舍", 5, 5, {}};
    facilities_["dormitory_2"] = {"宿舍", 5, 5, {}};
    facilities_["dormitory_3"] = {"宿舍", 5, 5, {}};
    facilities_["dormitory_4"] = {"宿舍", 5, 5, {}};
    facilities_["meeting"] = {"会客室", 3, 2, {}};
    facilities_["workshop"] = {"加工站", 3, 1, {}};
    facilities_["training"] = {"训练室", 3, 1, {}};
    facilities_["hire"] = {"办公室", 3, 1, {}};

    return true;
}

SchedulePlan InfrastructureManager::optimize() {
    std::cout << "[Infrastructure] 开始优化排班（贪心初始化 + 模拟退火）..." << std::endl;

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

    return optimizer.optimize(config);
}

bool InfrastructureManager::apply(const SchedulePlan& plan) {
    // TODO: 实现自动换班
    // 1. 遍历每个设施
    // 2. 清空当前干员
    // 3. 添加新干员

    std::cout << "[Infrastructure] 排班方案:" << std::endl;
    std::cout << "  总效率: " << plan.total_efficiency << std::endl;
    std::cout << "  迭代次数: " << plan.iterations << std::endl;
    std::cout << "  优化耗时: " << plan.optimization_time_ms << "ms" << std::endl;
    std::cout << std::endl;

    for (const auto& assignment : plan.assignments) {
        std::cout << "  " << assignment.facility_id << " (" << assignment.facility_type << "): ";
        for (const auto& op_id : assignment.operator_ids) {
            if (my_operators_.count(op_id)) {
                std::cout << my_operators_.at(op_id).name << " ";
            }
        }
        std::cout << std::endl;
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
