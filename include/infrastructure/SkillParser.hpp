#pragma once
#include "SkillEffect.hpp"
#include <string>
#include <vector>
#include <optional>
#include <regex>

/**
 * @brief 技能描述解析器
 *
 * 将干员基建技能的文本描述解析为结构化的 SkillEffect 对象。
 * 使用正则表达式匹配各种技能效果模式。
 *
 * ## 支持的模式
 *
 * | 模式 | 示例文本 | 解析结果 |
 * |------|----------|----------|
 * | 固定加成 | "生产力+30%" | FLAT_BONUS, value=0.30 |
 * | 按干员数量 | "每个[格拉斯哥帮]干员+5%" | PER_OPERATOR_BONUS |
 * | 按生产线 | "每条[赤金]生产线+5%" | PER_PRODUCTION_LINE |
 * | 特定干员联动 | "与德克萨斯一起时+65%" | SYNERGY_SPECIFIC |
 * | 组织联动 | "与[莱茵生命]干员一起+25%" | SYNERGY_GROUP |
 * | 变量产生 | "人间烟火+15" | VARIABLE_PRODUCE |
 * | 变量消费 | "每1点人间烟火+1%" | VARIABLE_CONSUME |
 *
 * ## 使用方式
 *
 * ```cpp
 * SkillParser parser;
 * auto effects = parser.parse("生产力+30%，人间烟火+10", "制造站");
 * // effects 包含两个 SkillEffect: FLAT_BONUS 和 VARIABLE_PRODUCE
 * ```
 *
 * @note 一个技能描述可能包含多个效果，parse() 会返回所有识别到的效果
 */
class SkillParser {
public:
    /**
     * @brief 解析技能描述
     *
     * 尝试从描述文本中识别所有技能效果。
     *
     * @param description 技能描述文本（中文）
     * @param facility 适用设施类型
     * @return 解析出的效果列表（可能为空）
     */
    std::vector<SkillEffect> parse(const std::string& description,
                                   const std::string& facility);

private:
    // ===== 各类效果解析方法 =====

    /**
     * @brief 解析固定加成
     *
     * 匹配模式: "生产力+N%", "订单获取效率+N%"
     */
    std::optional<SkillEffect> parse_flat_bonus(const std::string& desc, const std::string& facility);

    /**
     * @brief 解析按干员数量加成
     *
     * 匹配模式: "每个[组织名]干员+N%", "每N个[组织名]干员+M%"
     */
    std::optional<SkillEffect> parse_per_operator(const std::string& desc, const std::string& facility);

    /**
     * @brief 解析按生产线数量加成
     *
     * 匹配模式: "每条[赤金/作战记录/芯片]生产线+N%"
     */
    std::optional<SkillEffect> parse_per_production_line(const std::string& desc, const std::string& facility);

    /**
     * @brief 解析特定干员联动
     *
     * 匹配模式: "与[干员名]在同一[设施]时+N%"
     */
    std::optional<SkillEffect> parse_synergy_specific(const std::string& desc, const std::string& facility);

    /**
     * @brief 解析组织联动
     *
     * 匹配模式: "与[组织名]干员一起工作时+N%"
     */
    std::optional<SkillEffect> parse_synergy_group(const std::string& desc, const std::string& facility);

    /**
     * @brief 解析变量产生
     *
     * 匹配模式: "[变量名]+N", 如"人间烟火+15"
     */
    std::optional<SkillEffect> parse_variable_produce(const std::string& desc, const std::string& facility);

    /**
     * @brief 解析变量消费
     *
     * 匹配模式: "每N点[变量名]+M%"
     */
    std::optional<SkillEffect> parse_variable_consume(const std::string& desc, const std::string& facility);

    // ===== 辅助方法 =====

    /**
     * @brief 从文本中提取百分比数值
     * @param text 包含百分比的文本
     * @return 小数形式的百分比值（如30%返回0.30）
     */
    double extract_percentage(const std::string& text);

    /**
     * @brief 提取方括号内的内容
     * @param text 包含方括号的文本
     * @return 方括号内的内容，若无则返回空字符串
     */
    std::string extract_bracket_content(const std::string& text);
};
