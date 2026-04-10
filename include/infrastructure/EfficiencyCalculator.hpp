#pragma once
#include "InfrastructureState.hpp"
#include "SkillEffect.hpp"
#include "GroupMapping.hpp"
#include <map>
#include <string>
#include <vector>

class InfrastructureManager;

/**
 * @brief 基建效率计算器
 *
 * 计算给定基建状态下的总生产效率，支持各种技能效果类型。
 *
 * ## 计算流程
 *
 * 1. **全局变量计算**: 遍历所有干员，累加 VARIABLE_PRODUCE 效果产生的变量值
 * 2. **生产线统计**: 统计赤金、作战记录、芯片等生产线数量
 * 3. **设施效率计算**: 对每个生产设施，计算所有入驻干员的技能效果
 *
 * ## 支持的效果类型
 *
 * | 类型 | 示例 | 计算方式 |
 * |------|------|----------|
 * | FLAT_BONUS | 生产力+30% | 直接加值 |
 * | PER_OPERATOR_BONUS | 每个格拉斯哥帮+5% | 统计同设施内符合条件的干员数 |
 * | PER_PRODUCTION_LINE | 每条赤金线+5% | 统计全基建的生产线数量 |
 * | SYNERGY_SPECIFIC | 与德克萨斯一起+65% | 检查目标干员是否在同设施 |
 * | SYNERGY_GROUP | 与莱茵生命干员一起+25% | 检查是否有同组织干员 |
 * | VARIABLE_CONSUME | 每1点人间烟火+1% | 消耗全局变量池中的值 |
 *
 * @note 效率值以小数表示，如 0.30 表示 +30%
 */
class EfficiencyCalculator {
public:
    explicit EfficiencyCalculator(const InfrastructureManager& manager);

    /**
     * @brief 评估整体效率
     *
     * 计算给定状态下所有生产设施（贸易站、制造站、发电站）的总效率。
     *
     * @param state 基建状态
     * @return 总效率值
     */
    double evaluate(const InfrastructureState& state);

    /**
     * @brief 评估单个设施效率
     *
     * @param facility_id 设施ID
     * @param operators 入驻干员ID列表
     * @param state 基建状态（用于访问全局变量）
     * @return 该设施的效率值
     */
    double evaluate_facility(const std::string& facility_id,
                            const std::vector<std::string>& operators,
                            const InfrastructureState& state);

private:
    const InfrastructureManager& manager_;

    // ===== 效果计算方法 =====

    /**
     * @brief 计算固定加成
     * @param effect 技能效果
     * @return 效率值
     */
    double calc_flat_bonus(const SkillEffect& effect);

    /**
     * @brief 计算按干员数量加成
     *
     * 示例: "每个进驻的[格拉斯哥帮]干员，生产力+5%"
     *
     * @param effect 技能效果
     * @param operators 同设施干员列表
     * @return 效率值
     */
    double calc_per_operator_bonus(const SkillEffect& effect,
                                   const std::vector<std::string>& operators);

    /**
     * @brief 计算按生产线数量加成
     *
     * 示例: "每条[赤金]生产线，生产力+5%"
     *
     * @param effect 技能效果
     * @param state 基建状态（包含生产线统计）
     * @return 效率值
     */
    double calc_per_line_bonus(const SkillEffect& effect,
                               const InfrastructureState& state);

    /**
     * @brief 计算特定干员联动
     *
     * 示例: "与德克萨斯在同一贸易站时，订单获取效率+65%"
     *
     * @param effect 技能效果
     * @param operators 同设施干员列表
     * @return 效率值（触发返回value，否则返回0）
     */
    double calc_synergy_specific(const SkillEffect& effect,
                                 const std::vector<std::string>& operators);

    /**
     * @brief 计算组织联动
     *
     * 示例: "与[莱茵生命]干员一起工作时+25%"
     *
     * @param effect 技能效果
     * @param operators 同设施干员列表
     * @return 效率值（触发返回value，否则返回0）
     */
    double calc_synergy_group(const SkillEffect& effect,
                              const std::vector<std::string>& operators);

    /**
     * @brief 计算变量消耗效果
     *
     * 示例: "每1点人间烟火，生产力+1%"
     *
     * @param effect 技能效果
     * @param state 基建状态（包含全局变量池）
     * @return 效率值
     */
    double calc_variable_consume(const SkillEffect& effect,
                                 const InfrastructureState& state);

    // ===== 全局状态计算 =====

    /**
     * @brief 计算全局变量
     *
     * 遍历所有干员的 VARIABLE_PRODUCE 效果，累加到变量池。
     * 例如: 多个干员产生"人间烟火"变量。
     *
     * @param state 基建状态（会被修改）
     */
    void compute_global_variables(InfrastructureState& state);

    /**
     * @brief 计算生产线数量
     *
     * 统计各类生产线数量（赤金、作战记录、芯片）。
     *
     * @param state 基建状态（会被修改）
     * @note TODO: 当前使用默认252配置硬编码值
     */
    void compute_production_lines(InfrastructureState& state);

    // ===== 辅助方法 =====

    /**
     * @brief 检查干员是否属于指定组织
     * @param op_id 干员ID
     * @param group_name 组织名称（中文或group_id）
     * @return 是否属于该组织
     */
    bool is_same_group(const std::string& op_id, const std::string& group_name);

    /**
     * @brief 检查干员是否为指定名称
     * @param op_id 干员ID
     * @param target_name 目标干员名称
     * @return 是否匹配
     */
    bool is_operator_by_name(const std::string& op_id, const std::string& target_name);

    /**
     * @brief 统计同设施内指定组织的干员数量
     * @param operators 干员ID列表
     * @param group_name 组织名称
     * @return 符合条件的干员数量
     */
    int count_group_members(const std::vector<std::string>& operators,
                           const std::string& group_name);
};
