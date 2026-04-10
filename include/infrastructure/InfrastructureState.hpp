#pragma once
#include <map>
#include <vector>
#include <string>
#include <set>

/**
 * @brief 基建状态快照
 *
 * 表示某一时刻基建的完整状态，包括干员分配、全局变量和生产线配置。
 * 用于优化算法中状态的表示和效率计算。
 *
 * ## 主要用途
 *
 * 1. **优化器状态表示**: 模拟退火算法中的当前解和邻域解
 * 2. **效率计算输入**: 传递给 EfficiencyCalculator 进行效率评估
 * 3. **全局变量存储**: 存储"人间烟火"等跨设施变量系统的值
 *
 * @see ScheduleOptimizer 优化器使用此结构表示解
 * @see EfficiencyCalculator 计算器使用此结构评估效率
 */
struct InfrastructureState {
    /**
     * @brief 设施干员分配
     *
     * 映射: 设施ID -> 入驻干员ID列表
     * 例: {"trading_1" -> ["char_102_texas", "char_140_whitew"]}
     */
    std::map<std::string, std::vector<std::string>> assignments;

    /**
     * @brief 全局变量池
     *
     * 存储跨设施的变量值，如"人间烟火"系统。
     * 由 EfficiencyCalculator::compute_global_variables() 计算填充。
     * 映射: 变量名 -> 变量值
     * 例: {"人间烟火" -> 45.0}
     */
    std::map<std::string, double> variables;

    // ===== 生产线统计 =====
    int gold_lines = 0;         ///< 赤金生产线数量
    int record_lines = 0;       ///< 作战记录生产线数量
    int chip_lines = 0;         ///< 芯片生产线数量

    // ===== 辅助方法 =====

    /**
     * @brief 获取指定设施的干员列表
     * @param fac_id 设施ID
     * @return 干员ID列表，若设施不存在返回空列表
     */
    [[nodiscard]] std::vector<std::string> get_operators_in_facility(const std::string& fac_id) const {
        auto it = assignments.find(fac_id);
        if (it != assignments.end()) {
            return it->second;
        }
        return {};
    }

    /**
     * @brief 获取所有已分配干员列表
     * @return 所有设施中干员ID的合并列表
     */
    [[nodiscard]] std::vector<std::string> get_all_assigned_operators() const {
        std::vector<std::string> result;
        for (const auto& [fac_id, ops] : assignments) {
            result.insert(result.end(), ops.begin(), ops.end());
        }
        return result;
    }

    /**
     * @brief 检查干员是否已被分配
     * @param op_id 干员ID
     * @return 是否已分配到某设施
     */
    [[nodiscard]] bool is_operator_assigned(const std::string& op_id) const {
        for (const auto& [fac_id, ops] : assignments) {
            for (const auto& id : ops) {
                if (id == op_id) return true;
            }
        }
        return false;
    }

    /**
     * @brief 获取干员所在设施
     * @param op_id 干员ID
     * @return 设施ID，若未分配返回空字符串
     */
    [[nodiscard]] std::string get_operator_facility(const std::string& op_id) const {
        for (const auto& [fac_id, ops] : assignments) {
            for (const auto& id : ops) {
                if (id == op_id) return fac_id;
            }
        }
        return "";
    }

    /**
     * @brief 获取所有已分配干员的集合
     * @return 干员ID集合（用于快速查找）
     */
    [[nodiscard]] std::set<std::string> get_assigned_set() const {
        std::set<std::string> result;
        for (const auto& [fac_id, ops] : assignments) {
            result.insert(ops.begin(), ops.end());
        }
        return result;
    }
};
