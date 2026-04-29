#pragma once
#include <string>
#include <vector>

/**
 * @brief 技能效果类型枚举
 *
 * 定义了基建技能的所有效果类型，用于效率计算器识别和处理不同的技能效果。
 */
enum class SkillEffectType {
    // ===== 基础类型 =====
    FLAT_BONUS,              ///< 固定加成: "生产力+30%"

    // ===== 条件类型 =====
    PER_OPERATOR_BONUS,      ///< 按干员数量: "每个[格拉斯哥帮]干员+5%"
    PER_PRODUCTION_LINE,     ///< 按生产线: "每条赤金生产线+5%"
    PER_FACILITY_GLOBAL,     ///< 按全局设施数量: "每有一间进驻精英干员的设施+2%"

    // ===== 联动类型 =====
    SYNERGY_SPECIFIC,        ///< 特定干员联动: "与德克萨斯一起工作时+65%"
    SYNERGY_GROUP,           ///< 组织联动: "与[莱茵生命]干员一起工作时+25%"
    SYNERGY_SAME_ROOM,       ///< 同设施人数: "设施内每个干员+5%"

    // ===== 全局变量类型 =====
    VARIABLE_PRODUCE,        ///< 产生变量: "人间烟火+15"
    VARIABLE_CONSUME,        ///< 消费变量: "每1点人间烟火，生产力+1%"

    // ===== 特殊类型 =====
    MOOD_MODIFIER,           ///< 心情消耗修正: "心情消耗-0.25/小时"
    CAPACITY_BONUS,          ///< 仓库容量: "仓库容量+10"

    UNKNOWN                  ///< 未识别的技能类型
};

/**
 * @brief 技能效果定义
 *
 * 描述单个技能效果的所有属性，由 SkillParser 从技能描述文本解析生成。
 * 不同的效果类型使用不同的字段组合。
 *
 * @note 效果数值 (value) 以小数表示，如 0.30 表示 +30%
 */
struct SkillEffect {
    SkillEffectType type = SkillEffectType::UNKNOWN;  ///< 效果类型

    std::string facility;           ///< 目标设施: "制造站", "贸易站", ""(全局)
    double value = 0.0;             ///< 效果数值: 0.30 = +30%

    // ===== 条件相关字段 =====
    std::string condition_group;    ///< 条件组织: "格拉斯哥帮", "莱茵生命"
    std::string condition_operator; ///< 条件干员名: "德克萨斯"
    int condition_count = 1;        ///< 每N个触发一次效果

    // ===== 变量相关字段 =====
    std::string variable_name;      ///< 变量名: "人间烟火", "感知信息"

    // ===== 生产线相关字段 =====
    std::string production_type;    ///< 生产类型: "gold"(赤金), "record"(作战记录), "chip"(芯片)

    // ===== 设施计数相关字段 =====
    std::string facility_condition; ///< 设施条件: "elite"(精英干员), 组织名等
    int max_count = 0;              ///< 最大生效数量（0=无上限）
};

/**
 * @brief 解析后的干员基建技能
 *
 * 存储干员单个基建技能的完整信息，包括解锁条件和解析后的效果列表。
 * 一个技能可能包含多个效果（如同时提供效率加成和变量产出）。
 */
struct ParsedOperatorSkill {
    std::string buff_id;            ///< 技能ID
    std::string name;               ///< 技能名称
    std::string facility;           ///< 适用设施类型
    std::string description;        ///< 原始描述文本（用于调试）
    int unlock_elite = 0;           ///< 解锁所需精英化等级
    int unlock_level = 1;           ///< 解锁所需等级

    std::vector<SkillEffect> effects;  ///< 解析后的效果列表
};

/**
 * @brief 干员组织信息
 *
 * 存储干员的阵营归属信息，用于判断组织联动技能。
 */
struct OperatorMeta {
    std::string nation_id;          ///< 国家ID: "rhodes", "lungmen", "victoria"
    std::string group_id;           ///< 组织ID: "glasgow", "rhine", "blacksteel"
    std::string team_id;            ///< 小队ID: "action4", "reserve1"
    std::string profession;         ///< 职业: "PIONEER", "CASTER", "GUARD"
    std::string sub_profession;     ///< 子职业: "sword", "artsfghter"
};
