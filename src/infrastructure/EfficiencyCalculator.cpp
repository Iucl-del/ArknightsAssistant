#include "infrastructure/EfficiencyCalculator.hpp"
#include "infrastructure/InfrastructureManager.hpp"
#include <iostream>

EfficiencyCalculator::EfficiencyCalculator(const InfrastructureManager& manager)
    : manager_(manager) {
}

double EfficiencyCalculator::evaluate(const InfrastructureState& state) {
    // 创建可修改的副本
    InfrastructureState eval_state = state;

    // ===== Phase 1: 计算全局变量 =====
    compute_global_variables(eval_state);
    compute_production_lines(eval_state);

    // ===== Phase 2: 计算各设施效率 =====
    double total = 0.0;

    for (const auto& [fac_id, operators] : eval_state.assignments) {
        auto it = manager_.get_facilities().find(fac_id);
        if (it == manager_.get_facilities().end()) continue;

        const auto& facility = it->second;

        // 只计算生产设施
        if (facility.type == "贸易站" ||
            facility.type == "制造站" ||
            facility.type == "发电站") {
            total += evaluate_facility(fac_id, operators, eval_state);
        }
    }

    return total;
}

double EfficiencyCalculator::evaluate_facility(
    const std::string& facility_id,
    const std::vector<std::string>& operators,
    const InfrastructureState& state
) {
    double efficiency = 0.0;

    auto fac_it = manager_.get_facilities().find(facility_id);
    if (fac_it == manager_.get_facilities().end()) return 0.0;

    const auto& facility = fac_it->second;

    for (const auto& op_id : operators) {
        auto op_it = manager_.get_operators().find(op_id);
        if (op_it == manager_.get_operators().end()) continue;

        const auto& op = op_it->second;

        const ParsedOperatorSkill* active_skill = nullptr;
        for (const auto& skill : op.parsed_skills) {
            // 检查技能是否适用于当前设施
            if (skill.facility != facility.type) continue;

            // 检查是否解锁
            if (op.elite < skill.unlock_elite) continue;
            if (op.elite == skill.unlock_elite && op.level < skill.unlock_level) continue;

            if (!active_skill ||
                skill.unlock_elite > active_skill->unlock_elite ||
                (skill.unlock_elite == active_skill->unlock_elite &&
                 skill.unlock_level > active_skill->unlock_level)) {
                active_skill = &skill;
            }
        }

        if (!active_skill) continue;

        // 同一干员同一设施只生效最高已解锁基建技能。
        for (const auto& effect : active_skill->effects) {
            if (!effect.production_type.empty() &&
                effect.production_type != facility.production_type) {
                continue;
            }

            switch (effect.type) {
                case SkillEffectType::FLAT_BONUS:
                    efficiency += calc_flat_bonus(effect);
                    break;

                case SkillEffectType::PER_OPERATOR_BONUS:
                    efficiency += calc_per_operator_bonus(effect, operators);
                    break;

                case SkillEffectType::PER_PRODUCTION_LINE:
                    efficiency += calc_per_line_bonus(effect, state);
                    break;

                case SkillEffectType::SYNERGY_SPECIFIC:
                    efficiency += calc_synergy_specific(effect, operators);
                    break;

                case SkillEffectType::SYNERGY_GROUP:
                    efficiency += calc_synergy_group(effect, operators);
                    break;

                case SkillEffectType::VARIABLE_CONSUME:
                    efficiency += calc_variable_consume(effect, state);
                    break;

                case SkillEffectType::PER_FACILITY_GLOBAL:
                    efficiency += calc_per_facility_global(effect, state);
                    break;

                default:
                    break;
            }
        }
    }

    return efficiency;
}

double EfficiencyCalculator::calc_flat_bonus(const SkillEffect& effect) {
    return effect.value;
}

double EfficiencyCalculator::calc_per_operator_bonus(
    const SkillEffect& effect,
    const std::vector<std::string>& operators
) {
    int count = count_group_members(operators, effect.condition_group);
    if (effect.condition_count <= 0) return 0.0;
    return effect.value * (count / effect.condition_count);
}

double EfficiencyCalculator::calc_per_line_bonus(
    const SkillEffect& effect,
    const InfrastructureState& state
) {
    int lines = 0;
    if (effect.production_type == "gold") {
        lines = state.gold_lines;
    } else if (effect.production_type == "record") {
        lines = state.record_lines;
    } else if (effect.production_type == "chip") {
        lines = state.chip_lines;
    }

    if (effect.condition_count <= 0) return 0.0;
    return effect.value * (lines / effect.condition_count);
}

double EfficiencyCalculator::calc_synergy_specific(
    const SkillEffect& effect,
    const std::vector<std::string>& operators
) {
    // 检查目标干员是否在同设施
    for (const auto& op_id : operators) {
        if (is_operator_by_name(op_id, effect.condition_operator)) {
            return effect.value;
        }
    }
    return 0.0;
}

double EfficiencyCalculator::calc_synergy_group(
    const SkillEffect& effect,
    const std::vector<std::string>& operators
) {
    // 检查是否有同组织干员
    for (const auto& op_id : operators) {
        if (is_same_group(op_id, effect.condition_group)) {
            return effect.value;
        }
    }
    return 0.0;
}

double EfficiencyCalculator::calc_variable_consume(
    const SkillEffect& effect,
    const InfrastructureState& state
) {
    auto it = state.variables.find(effect.variable_name);
    if (it == state.variables.end()) return 0.0;

    double var_value = it->second;
    if (effect.condition_count <= 0) return 0.0;
    return effect.value * (var_value / effect.condition_count);
}

double EfficiencyCalculator::calc_per_facility_global(
    const SkillEffect& effect,
    const InfrastructureState& state
) {
    int matched_facilities = 0;

    for (const auto& [fac_id, operators] : state.assignments) {
        (void)fac_id;

        bool facility_matches = false;
        for (const auto& op_id : operators) {
            auto op_it = manager_.get_operators().find(op_id);
            if (op_it == manager_.get_operators().end()) continue;

            const auto& op = op_it->second;

            if (effect.facility_condition == "elite") {
                // 精英化等级大于0视为精英干员
                if (op.elite > 0) {
                    facility_matches = true;
                    break;
                }
            } else {
                if (is_same_group(op_id, effect.facility_condition)) {
                    facility_matches = true;
                    break;
                }
            }
        }

        if (facility_matches) {
            matched_facilities++;
        }
    }

    if (effect.max_count > 0 && matched_facilities > effect.max_count) {
        matched_facilities = effect.max_count;
    }

    if (effect.condition_count <= 0) return 0.0;
    return effect.value * (matched_facilities / effect.condition_count);
}

void EfficiencyCalculator::compute_global_variables(InfrastructureState& state) {
    state.variables.clear();

    // 遍历所有分配，计算变量产生
    for (const auto& [fac_id, operators] : state.assignments) {
        for (const auto& op_id : operators) {
            auto op_it = manager_.get_operators().find(op_id);
            if (op_it == manager_.get_operators().end()) continue;

            const auto& op = op_it->second;

            for (const auto& skill : op.parsed_skills) {
                // 检查是否解锁
                if (op.elite < skill.unlock_elite) continue;
                if (op.elite == skill.unlock_elite && op.level < skill.unlock_level) continue;

                for (const auto& effect : skill.effects) {
                    if (effect.type == SkillEffectType::VARIABLE_PRODUCE) {
                        state.variables[effect.variable_name] += effect.value;
                    }
                }
            }
        }
    }
}

void EfficiencyCalculator::compute_production_lines(InfrastructureState& state) {
    state.gold_lines = 0;
    state.record_lines = 0;
    state.chip_lines = 0;

    for (const auto& [fac_id, fac] : manager_.get_facilities()) {
        if (fac.type != "制造站") continue;
        if (fac.production_type == "gold") state.gold_lines++;
        else if (fac.production_type == "record") state.record_lines++;
        else if (fac.production_type == "chip") state.chip_lines++;
    }
}

bool EfficiencyCalculator::is_same_group(const std::string& op_id, const std::string& group_name) {
    auto op_it = manager_.get_operators().find(op_id);
    if (op_it == manager_.get_operators().end()) return false;

    const auto& op = op_it->second;

    // 通过 group_id 匹配
    auto name_it = NAME_TO_GROUP_ID.find(group_name);
    if (name_it != NAME_TO_GROUP_ID.end()) {
        const auto& id = name_it->second;
        return op.meta.group_id == id ||
               op.meta.nation_id == id ||
               op.meta.team_id == id;
    }

    // 直接比较内部 ID，兼容阵营、组织、小队字段
    return op.meta.group_id == group_name ||
           op.meta.nation_id == group_name ||
           op.meta.team_id == group_name;
}

bool EfficiencyCalculator::is_operator_by_name(const std::string& op_id, const std::string& target_name) {
    auto op_it = manager_.get_operators().find(op_id);
    if (op_it == manager_.get_operators().end()) return false;

    const auto& op_name = op_it->second.name;
    if (op_name == target_name) return true;

    return false;
}

int EfficiencyCalculator::count_group_members(
    const std::vector<std::string>& operators,
    const std::string& group_name
) {
    int count = 0;
    for (const auto& op_id : operators) {
        if (is_same_group(op_id, group_name)) {
            count++;
        }
    }
    return count;
}
