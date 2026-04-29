#pragma once
#include <string>
#include <vector>
#include <map>
#include <set>
#include <json/json.h>
#include "SkillEffect.hpp"

/**
 * @brief 干员信息
 *
 * 存储干员的基本信息、组织归属、技能数据和运行时状态。
 * 技能数据在加载时通过 SkillParser 解析为结构化的 SkillEffect。
 */
struct OperatorInfo {
    std::string id;                 // 干员ID: char_xxx_xxx
    std::string name;               // 干员名称
    std::string rarity;             // 稀有度: TIER_1 ~ TIER_6

    OperatorMeta meta;              // 组织信息: 国家、组织、小队、职业

    std::vector<ParsedOperatorSkill> parsed_skills;  // 解析后的基建技能列表

    // 运行时状态（由 OCR 识别更新）
    int elite = 0;                  // 当前精英化等级 (0-2)
    int level = 1;                  // 当前等级 (1-90)
    float mood = 24.0f;             // 当前心情值 (0-24)
    bool in_facility = false;       // 是否正在设施中工作
    std::string current_facility;   // 当前所在设施ID
};

/**
 * @brief 设施信息
 *
 * 存储基建设施的类型、等级和当前入驻干员。
 */
struct FacilityInfo {
    std::string type;               // 设施类型: 贸易站|制造站|发电站|控制中枢|宿舍|...
    std::string production_type;    // 生产类型: "gold"(赤金)|"record"(作战记录)|"chip"(芯片)|"lmd"(龙门币)|""(无)
    int level;                      // 设施等级 (1-3)
    int slots;                      // 干员槽位数
    std::vector<std::string> operators;  // 当前入驻干员ID列表
};

/**
 * @brief 排班方案
 *
 * 优化器输出的完整排班计划，包含每个设施的干员分配和效率统计。
 */
struct SchedulePlan {
    /**
     * @brief 单个设施的排班分配
     */
    struct Assignment {
        std::string facility_id;                // 设施ID
        std::string facility_type;              // 设施类型
        std::vector<std::string> operator_ids;  // 分配的干员ID列表

        double base_efficiency = 0.0;           // 基础效率（不含联动）
        double synergy_efficiency = 0.0;        // 联动加成效率
        double total_efficiency = 0.0;          // 总效率
    };

    std::vector<Assignment> assignments;        // 所有设施的分配方案
    double total_efficiency = 0.0;              // 全基建总效率

    // 优化统计
    int iterations = 0;                         // 优化迭代次数
    double optimization_time_ms = 0.0;          // 优化耗时（毫秒）
};

class SimpleController;

/**
 * @brief 基建管理器
 *
 * 负责管理明日方舟基建系统，包括：
 * - 加载和管理干员技能数据库
 * - 扫描识别当前基建状态和干员
 * - 使用贪心初始化+模拟退火算法生成最优排班方案
 * - 执行自动换班操作
 *
 * @note 优化算法支持以下技能类型的效率计算：
 *       - 固定加成（如 +30%）
 *       - 按人数加成（如 每个格拉斯哥帮干员+5%）
 *       - 联动加成（如 与德克萨斯一起时+65%）
 *       - 全局变量（如 人间烟火系统）
 */
class InfrastructureManager {
public:
    explicit InfrastructureManager(SimpleController& controller);
    ~InfrastructureManager() = default;

    /**
     * @brief 从森空岛导入干员数据
     *
     * 从森空岛导出的JSON文件加载用户拥有的干员及其等级信息。
     * 森空岛数据包含干员ID、精英化等级、等级等信息。
     * @return 是否导入成功
     */
    bool import_operators_from_skland();

    /**
     * @brief 使用全部干员进行测试
     *
     * 将技能数据库中所有干员作为拥有干员，设置为满级满精英。
     * 仅用于测试目的。
     *
     * @return 是否成功
     */
    bool use_all_operators_for_test();

    /**
     * @brief 扫描当前基建布局
     * @return 是否扫描成功
     * @note TODO: 待实现OCR识别，当前使用默认243布局
     */
    bool scan_infrastructure();

    /**
     * @brief 生成最优排班方案
     *
     * 使用两阶段优化算法：
     * 1. 贪心初始化：按设施优先级选择基础效率最高的干员
     * 2. 模拟退火优化：通过邻域搜索寻找更优解，支持联动效率计算
     *
     * @return 优化后的排班方案
     */

    SchedulePlan optimize();

    /**
     * @brief 执行排班方案
     * @param plan 要执行的排班方案
     * @return 是否执行成功
     * @note TODO: 待实现自动换班操作
     */
    bool apply(const SchedulePlan& plan);

    // 数据访问接口
    [[nodiscard]] const std::map<std::string, OperatorInfo>& get_operators() const { return my_operators_; }
    [[nodiscard]] const std::map<std::string, FacilityInfo>& get_facilities() const { return facilities_; }

    /**
     * @brief 检查干员是否有指定设施的已解锁技能
     * @param op 干员信息
     * @param facility_type 设施类型
     * @return 是否有可用技能
     */
    [[nodiscard]] bool has_skill_for_facility(const OperatorInfo& op, const std::string& facility_type) const;

    /**
     * @brief 计算干员在指定设施的基础效率
     *
     * 只计算 FLAT_BONUS 类型的固定加成，不考虑联动效果。
     * 用于贪心初始化阶段快速筛选高效率干员。
     *
     * @param op 干员信息
     * @param facility_type 设施类型
     * @return 基础效率值
     */
    double calculate_base_efficiency(const OperatorInfo& op, const std::string& facility_type);

private:
    /**
     * @brief 加载干员技能数据库
     *
     * 从固定路径 resource/data/operator_skills.json 加载并解析。
     * 在构造函数中自动调用。
     *
     * @return 是否加载成功
     */
    bool load_skill_database();

    SimpleController& controller_;
    std::map<std::string, OperatorInfo> skill_db_;      // 技能数据库（所有干员）
    std::map<std::string, OperatorInfo> my_operators_;  // 当前账号拥有的干员
    std::map<std::string, FacilityInfo> facilities_;    // 当前基建设施

};
