#pragma once
#include "InfrastructureManager.hpp"
#include "InfrastructureState.hpp"
#include "EfficiencyCalculator.hpp"
#include <random>
#include <chrono>

/**
 * @brief 模拟退火优化器配置
 *
 * 控制优化算法的行为参数，包括温度调度和邻域操作权重。
 */
struct OptimizerConfig {
    // ===== 模拟退火参数 =====
    double initial_temperature = 100.0;  // 初始温度，越高越容易接受劣解
    double cooling_rate = 0.995;         // 降温系数，每次迭代乘以此值
    double min_temperature = 0.01;       // 最低温度，达到后停止优化
    int max_iterations = 50000;          // 最大迭代次数

    // ===== 邻域操作权重 =====
    // 三种操作的概率分布，总和应为1.0
    double swap_weight = 0.5;            // 交换操作：交换两个设施中的干员
    double move_weight = 0.3;            // 移动操作：将干员移动到有空位的设施
    double replace_weight = 0.2;         // 替换操作：用未分配干员替换已分配干员

    // ===== 性能选项 =====
    int plateau_threshold = 1000;        // 停滞阈值，连续多少次未改进后重新加热
};

/**
 * @brief 基建排班优化器
 *
 * 使用模拟退火算法优化基建干员排班，最大化生产效率。
 *
 * ## 算法流程
 *
 * ### 1. 贪心初始化
 * 按设施优先级（贸易站 > 制造站 > 发电站）为每个设施选择基础效率最高的干员，
 * 生成一个初始可行解。
 *
 * ### 2. 模拟退火优化
 * 从初始解出发，通过邻域操作探索解空间：
 * - **交换 (Swap)**: 随机选择两个设施，交换各自的一个干员
 * - **移动 (Move)**: 将一个干员从当前设施移动到有空位的其他设施
 * - **替换 (Replace)**: 用一个未被分配的干员替换已分配的干员
 *
 * 接受准则采用 Metropolis 准则：
 * - 若新解更优，直接接受
 * - 若新解更差，以概率 exp(Δ/T) 接受，其中 Δ 为效率差，T 为当前温度
 *
 * ### 3. 重新加热
 * 当连续多次迭代未找到更优解（停滞）时，将温度提升到初始值的一半，
 * 帮助跳出局部最优。
 *
 * ## 效率计算
 * 使用 EfficiencyCalculator 计算每个状态的总效率，支持：
 * - 固定加成 (FLAT_BONUS)
 * - 按人数加成 (PER_OPERATOR_BONUS)
 * - 按生产线加成 (PER_PRODUCTION_LINE)
 * - 特定干员联动 (SYNERGY_SPECIFIC)
 * - 组织联动 (SYNERGY_GROUP)
 * - 全局变量系统 (VARIABLE_PRODUCE/CONSUME)
 *
 * @see EfficiencyCalculator 效率计算详情
 * @see InfrastructureState 基建状态表示
 */
class ScheduleOptimizer {
public:
    explicit ScheduleOptimizer(InfrastructureManager& manager);

    /**
     * @brief 执行优化
     * @param config 优化器配置
     * @return 优化后的排班方案
     */
    SchedulePlan optimize(const OptimizerConfig& config = {});

    /**
     * @brief 优化统计信息
     */
    struct Stats {
        int total_iterations = 0;        // 总迭代次数
        int accepted_moves = 0;          // 接受的移动次数
        int rejected_moves = 0;          // 拒绝的移动次数
        double initial_score = 0.0;      // 初始解效率
        double final_score = 0.0;        // 最终解效率
        double improvement_percent = 0.0; // 效率提升百分比
        double time_ms = 0.0;            // 优化耗时（毫秒）
    };

    /**
     * @brief 获取优化统计
     * @return 统计信息
     */
    [[nodiscard]] Stats get_stats() const { return stats_; }

private:
    InfrastructureManager& manager_;     // 基建管理器引用
    EfficiencyCalculator calculator_;    // 效率计算器
    Stats stats_;                        // 统计信息
    std::mt19937 rng_;                   // 随机数生成器

    // ===== 初始解生成 =====

    /**
     * @brief 贪心初始化
     *
     * 按设施优先级为每个设施分配基础效率最高的干员。
     * 优先级: 贸易站 > 制造站 > 发电站
     *
     * @return 初始基建状态
     */
    InfrastructureState greedy_initialize();

    /**
     * @brief 粗略估计干员在设施中的潜在价值
     *
     * 用于初始解候选裁剪，避免纯联动干员因基础效率为0被排除。
     */
    double estimate_operator_potential(const OperatorInfo& op, const FacilityInfo& facility) const;

    InfrastructureState improve_facility_type_assignments(const InfrastructureState& state,
                                                          const std::string& facility_type);

    // ===== 邻域操作 =====

    /**
     * @brief 生成邻域解
     *
     * 根据配置的权重随机选择一种邻域操作生成新解。
     *
     * @param current 当前状态
     * @param config 优化器配置
     * @return 邻域状态
     */
    InfrastructureState generate_neighbor(const InfrastructureState& current,
                                          const OptimizerConfig& config);

    /**
     * @brief 交换操作
     *
     * 随机选择两个不同设施，各选一个干员进行交换。
     * 交换前检查干员是否有目标设施的技能。
     *
     * @param state 当前状态
     * @return 交换后的状态（若不合法则返回原状态）
     */
    InfrastructureState swap_operators(const InfrastructureState& state);

    /**
     * @brief 移动操作
     *
     * 将一个干员从当前设施移动到有空槽位的其他设施。
     * 移动前检查干员是否有目标设施的技能。
     *
     * @param state 当前状态
     * @return 移动后的状态（若不合法则返回原状态）
     */
    InfrastructureState move_operator(const InfrastructureState& state);

    /**
     * @brief 替换操作
     *
     * 用一个未分配的干员替换设施中已分配的干员。
     * 替换前检查新干员是否有该设施的技能。
     *
     * @param state 当前状态
     * @return 替换后的状态（若不合法则返回原状态）
     */
    InfrastructureState replace_operator(const InfrastructureState& state);

    // ===== 辅助方法 =====

    /**
     * @brief 获取未分配的干员列表
     * @param state 当前状态
     * @return 未分配干员ID列表
     */
    std::vector<std::string> get_unassigned_operators(const InfrastructureState& state);

    /**
     * @brief 验证状态合法性
     *
     * 检查：
     * - 每个设施的干员数不超过槽位数
     * - 每个干员只被分配到一个设施
     *
     * @param state 待验证状态
     * @return 是否合法
     */
    bool is_valid_state(const InfrastructureState& state);

    /**
     * @brief 构建排班方案
     * @param state 最优状态
     * @param efficiency 总效率
     * @return 排班方案
     */
    SchedulePlan build_plan(const InfrastructureState& state, double efficiency);
};
