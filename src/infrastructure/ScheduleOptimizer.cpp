#include "infrastructure/ScheduleOptimizer.hpp"
#include "Logger.hpp"
#include <algorithm>
#include <cmath>
#include <functional>

ScheduleOptimizer::ScheduleOptimizer(InfrastructureManager& manager)
    : manager_(manager)
    , calculator_(manager)
    , rng_(std::random_device{}()) {
}

SchedulePlan ScheduleOptimizer::optimize(const OptimizerConfig& config) {
    auto start_time = std::chrono::steady_clock::now();

    // ===== Step 1: 生成初始解 =====
    InfrastructureState current = greedy_initialize();
    double current_score = calculator_.evaluate(current);

    InfrastructureState best = current;
    double best_score = current_score;

    stats_ = Stats{};
    stats_.initial_score = current_score;

    Logger::info("[Optimizer] 初始分数: {}", current_score);

    // ===== Step 2: 模拟退火 =====
    double temperature = config.initial_temperature;
    int plateau_count = 0;

    std::uniform_real_distribution<> uniform(0.0, 1.0);

    for (int iter = 0; iter < config.max_iterations; ++iter) {
        // 检查温度
        if (temperature < config.min_temperature) break;

        // 检查停滞
        if (plateau_count > config.plateau_threshold) {
            // 重新加热
            temperature = config.initial_temperature * 0.5;
            plateau_count = 0;
            Logger::info("[Optimizer] 在第 {} 次迭代重新加热", iter);
        }

        // 生成邻域解
        InfrastructureState neighbor = generate_neighbor(current, config);

        // 验证合法性
        if (!is_valid_state(neighbor)) continue;

        // 评估
        double neighbor_score = calculator_.evaluate(neighbor);
        double delta = neighbor_score - current_score;

        // 接受准则
        bool accept = false;
        if (delta > 0) {
            accept = true;  // 更优解，直接接受
        } else {
            // Metropolis 准则
            double prob = std::exp(delta / temperature);
            accept = uniform(rng_) < prob;
        }

        if (accept) {
            current = neighbor;
            current_score = neighbor_score;
            stats_.accepted_moves++;

            if (current_score > best_score) {
                best = current;
                best_score = current_score;
                plateau_count = 0;
            } else {
                plateau_count++;
            }
        } else {
            stats_.rejected_moves++;
            plateau_count++;
        }

        // 降温
        temperature *= config.cooling_rate;
        stats_.total_iterations = iter + 1;

        // 进度输出
        if ((iter + 1) % 10000 == 0) {
            Logger::info("[Optimizer] 迭代 {}, 当前={}, 最佳={}, 温度={}", iter + 1, current_score, best_score, temperature);
        }
    }

    // ===== Step 3: 确定性局部改良 =====
    for (const auto& facility_type : {"制造站", "贸易站", "发电站"}) {
        InfrastructureState improved = improve_facility_type_assignments(best, facility_type);
        double improved_score = calculator_.evaluate(improved);
        if (improved_score > best_score) {
            best = improved;
            best_score = improved_score;
        }
    }

    // ===== Step 4: 记录统计 =====
    auto end_time = std::chrono::steady_clock::now();
    stats_.final_score = best_score;
    if (stats_.initial_score > 0) {
        stats_.improvement_percent = (best_score - stats_.initial_score) / stats_.initial_score * 100;
    }
    stats_.time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    Logger::info("[Optimizer] 完成: 初始={}, 最终={}, 提升={}%, 耗时={}ms, 迭代={}, 接受={}, 拒绝={}", stats_.initial_score, stats_.final_score, stats_.improvement_percent, stats_.time_ms, stats_.total_iterations, stats_.accepted_moves, stats_.rejected_moves);

    return build_plan(best, best_score);
}

InfrastructureState ScheduleOptimizer::greedy_initialize() {
    InfrastructureState state;
    std::set<std::string> assigned;

    // 按设施优先级排序（贸易站、制造站优先）
    std::vector<std::pair<std::string, const FacilityInfo*>> facility_order;
    for (const auto& [fac_id, fac] : manager_.get_facilities()) {
        if (fac.type == "贸易站" || fac.type == "制造站" || fac.type == "发电站") {
            facility_order.emplace_back(fac_id, &fac);
        }
    }

    // 按类型排序：贸易站 > 制造站 > 发电站
    std::sort(facility_order.begin(), facility_order.end(),
        [](const auto& a, const auto& b) {
            auto priority = [](const std::string& type) {
                if (type == "贸易站") return 0;
                if (type == "制造站") return 1;
                if (type == "发电站") return 2;
                return 3;
            };
            return priority(a.second->type) < priority(b.second->type);
        });

    // 对每个设施，枚举高潜力候选组合，避免错过组合型联动收益。
    for (const auto& [fac_id, fac_ptr] : facility_order) {
        const auto& facility = *fac_ptr;

        // 获取该设施的候选干员及潜力分
        std::vector<std::pair<std::string, double>> candidates;

        for (const auto& [op_id, op] : manager_.get_operators()) {
            if (assigned.count(op_id)) continue;
            if (!manager_.has_skill_for_facility(op, facility.type)) continue;

            double eff = estimate_operator_potential(op, facility);
            candidates.emplace_back(op_id, eff);
        }

        // 按潜力排序，只枚举前若干名控制组合数量。
        std::sort(candidates.begin(), candidates.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });

        constexpr int max_candidates = 40;
        if (static_cast<int>(candidates.size()) > max_candidates) {
            candidates.resize(max_candidates);
        }

        std::vector<std::string> best_ops;
        double best_score = -1.0;
        std::vector<std::string> current_ops;
        const int target_size = std::min(facility.slots, static_cast<int>(candidates.size()));

        std::function<void(int)> dfs = [&](int start) {
            if (static_cast<int>(current_ops.size()) == target_size) {
                InfrastructureState trial = state;
                trial.assignments[fac_id] = current_ops;
                double score = calculator_.evaluate(trial);
                if (score > best_score) {
                    best_score = score;
                    best_ops = current_ops;
                }
                return;
            }

            int remaining = target_size - static_cast<int>(current_ops.size());
            for (int i = start; i <= static_cast<int>(candidates.size()) - remaining; ++i) {
                current_ops.push_back(candidates[i].first);
                dfs(i + 1);
                current_ops.pop_back();
            }
        };

        if (target_size > 0) {
            dfs(0);
        }

        for (const auto& op_id : best_ops) {
            state.assignments[fac_id].push_back(op_id);
            assigned.insert(op_id);
        }
    }

    return state;
}

double ScheduleOptimizer::estimate_operator_potential(
    const OperatorInfo& op,
    const FacilityInfo& facility
) const {
    double potential = 0.0;
    const ParsedOperatorSkill* active_skill = nullptr;

    for (const auto& skill : op.parsed_skills) {
        if (skill.facility != facility.type) continue;

        if (op.elite < skill.unlock_elite) continue;
        if (op.elite == skill.unlock_elite && op.level < skill.unlock_level) continue;

        if (!active_skill ||
            skill.unlock_elite > active_skill->unlock_elite ||
            (skill.unlock_elite == active_skill->unlock_elite &&
             skill.unlock_level > active_skill->unlock_level)) {
            active_skill = &skill;
        }
    }

    if (!active_skill) return potential;

    for (const auto& effect : active_skill->effects) {
        if (!effect.production_type.empty() &&
            effect.production_type != facility.production_type) {
            continue;
        }

        switch (effect.type) {
            case SkillEffectType::FLAT_BONUS:
            case SkillEffectType::PER_OPERATOR_BONUS:
            case SkillEffectType::PER_PRODUCTION_LINE:
            case SkillEffectType::PER_FACILITY_GLOBAL:
            case SkillEffectType::SYNERGY_SPECIFIC:
            case SkillEffectType::SYNERGY_GROUP:
            case SkillEffectType::VARIABLE_CONSUME:
                potential += effect.value;
                break;
            default:
                break;
        }
    }

    return potential;
}

InfrastructureState ScheduleOptimizer::improve_facility_type_assignments(
    const InfrastructureState& state,
    const std::string& facility_type
) {
    std::vector<std::string> facility_ids;
    for (const auto& [fac_id, fac] : manager_.get_facilities()) {
        if (fac.type == facility_type) {
            facility_ids.push_back(fac_id);
        }
    }

    if (facility_ids.size() < 2) return state;

    std::set<std::string> locked_ops = state.get_assigned_set();
    std::set<std::string> scoped_ops;
    for (const auto& fac_id : facility_ids) {
        auto it = state.assignments.find(fac_id);
        if (it == state.assignments.end()) continue;
        for (const auto& op_id : it->second) {
            scoped_ops.insert(op_id);
            locked_ops.erase(op_id);
        }
    }

    std::vector<std::string> candidate_ops(scoped_ops.begin(), scoped_ops.end());
    std::vector<std::pair<std::string, double>> extras;
    for (const auto& [op_id, op] : manager_.get_operators()) {
        if (locked_ops.count(op_id) || scoped_ops.count(op_id)) continue;
        if (!manager_.has_skill_for_facility(op, facility_type)) continue;

        double best_potential = 0.0;
        for (const auto& fac_id : facility_ids) {
            auto fac_it = manager_.get_facilities().find(fac_id);
            if (fac_it == manager_.get_facilities().end()) continue;
            best_potential = std::max(best_potential, estimate_operator_potential(op, fac_it->second));
        }
        if (best_potential > 0.0) {
            extras.emplace_back(op_id, best_potential);
        }
    }

    std::sort(extras.begin(), extras.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    constexpr int max_extra_candidates = 12;
    for (int i = 0; i < static_cast<int>(extras.size()) && i < max_extra_candidates; ++i) {
        candidate_ops.push_back(extras[i].first);
    }

    struct Combo {
        std::string fac_id;
        std::vector<std::string> ops;
        double score = 0.0;
    };

    std::map<std::string, std::vector<Combo>> combos_by_facility;
    for (const auto& fac_id : facility_ids) {
        auto fac_it = manager_.get_facilities().find(fac_id);
        if (fac_it == manager_.get_facilities().end()) continue;

        const auto& facility = fac_it->second;
        std::vector<std::string> current;

        std::function<void(int)> dfs = [&](int start) {
            if (static_cast<int>(current.size()) == facility.slots) {
                InfrastructureState trial = state;
                trial.assignments[fac_id] = current;
                double score = calculator_.evaluate_facility(fac_id, current, trial);
                combos_by_facility[fac_id].push_back({fac_id, current, score});
                return;
            }

            int remaining = facility.slots - static_cast<int>(current.size());
            for (int i = start; i <= static_cast<int>(candidate_ops.size()) - remaining; ++i) {
                current.push_back(candidate_ops[i]);
                dfs(i + 1);
                current.pop_back();
            }
        };

        dfs(0);
        auto& combos = combos_by_facility[fac_id];
        std::sort(combos.begin(), combos.end(),
                  [](const Combo& a, const Combo& b) { return a.score > b.score; });
        constexpr int max_combos_per_facility = 120;
        if (static_cast<int>(combos.size()) > max_combos_per_facility) {
            combos.resize(max_combos_per_facility);
        }
    }

    InfrastructureState best_state = state;
    double best_score = calculator_.evaluate(state);
    InfrastructureState current_state = state;
    std::set<std::string> used;

    std::function<void(int)> assign_facility = [&](int index) {
        if (index == static_cast<int>(facility_ids.size())) {
            double score = calculator_.evaluate(current_state);
            if (score > best_score) {
                best_score = score;
                best_state = current_state;
            }
            return;
        }

        const auto& fac_id = facility_ids[index];
        for (const auto& combo : combos_by_facility[fac_id]) {
            bool conflict = false;
            for (const auto& op_id : combo.ops) {
                if (used.count(op_id)) {
                    conflict = true;
                    break;
                }
            }
            if (conflict) continue;

            for (const auto& op_id : combo.ops) used.insert(op_id);
            current_state.assignments[fac_id] = combo.ops;
            assign_facility(index + 1);
            for (const auto& op_id : combo.ops) used.erase(op_id);
        }
    };

    assign_facility(0);
    return best_state;
}

InfrastructureState ScheduleOptimizer::generate_neighbor(
    const InfrastructureState& current,
    const OptimizerConfig& config
) {
    std::uniform_real_distribution<> dist(0.0, 1.0);
    double r = dist(rng_);

    double threshold1 = config.swap_weight;
    double threshold2 = threshold1 + config.move_weight;

    if (r < threshold1) {
        return swap_operators(current);
    } else if (r < threshold2) {
        return move_operator(current);
    } else {
        return replace_operator(current);
    }
}

InfrastructureState ScheduleOptimizer::swap_operators(const InfrastructureState& state) {
    InfrastructureState result = state;

    // 收集所有非空设施
    std::vector<std::string> facilities;
    for (const auto& [fac_id, ops] : state.assignments) {
        if (!ops.empty()) facilities.push_back(fac_id);
    }

    if (facilities.size() < 2) return result;

    std::uniform_int_distribution<> fac_dist(0, static_cast<int>(facilities.size()) - 1);
    int idx1 = fac_dist(rng_);
    int idx2 = fac_dist(rng_);
    while (idx2 == idx1) idx2 = fac_dist(rng_);

    const std::string& fac1 = facilities[idx1];
    const std::string& fac2 = facilities[idx2];

    auto& ops1 = result.assignments[fac1];
    auto& ops2 = result.assignments[fac2];

    if (ops1.empty() || ops2.empty()) return state;

    std::uniform_int_distribution<> op_dist1(0, static_cast<int>(ops1.size()) - 1);
    std::uniform_int_distribution<> op_dist2(0, static_cast<int>(ops2.size()) - 1);

    int op_idx1 = op_dist1(rng_);
    int op_idx2 = op_dist2(rng_);

    // 检查交换后是否合法
    auto fac1_it = manager_.get_facilities().find(fac1);
    auto fac2_it = manager_.get_facilities().find(fac2);
    if (fac1_it == manager_.get_facilities().end() || fac2_it == manager_.get_facilities().end()) {
        return state;
    }

    auto op1_it = manager_.get_operators().find(ops1[op_idx1]);
    auto op2_it = manager_.get_operators().find(ops2[op_idx2]);
    if (op1_it == manager_.get_operators().end() || op2_it == manager_.get_operators().end()) {
        return state;
    }

    if (!manager_.has_skill_for_facility(op1_it->second, fac2_it->second.type)) return state;
    if (!manager_.has_skill_for_facility(op2_it->second, fac1_it->second.type)) return state;

    // 执行交换
    std::swap(ops1[op_idx1], ops2[op_idx2]);

    return result;
}

InfrastructureState ScheduleOptimizer::move_operator(const InfrastructureState& state) {
    InfrastructureState result = state;

    // 收集所有设施
    std::vector<std::string> non_empty_facilities;
    std::vector<std::string> has_space_facilities;

    for (const auto& [fac_id, ops] : state.assignments) {
        if (!ops.empty()) non_empty_facilities.push_back(fac_id);

        auto fac_it = manager_.get_facilities().find(fac_id);
        if (fac_it != manager_.get_facilities().end()) {
            if (static_cast<int>(ops.size()) < fac_it->second.slots) {
                has_space_facilities.push_back(fac_id);
            }
        }
    }

    if (non_empty_facilities.empty() || has_space_facilities.empty()) return state;

    // 随机选择一个干员移出
    std::uniform_int_distribution<> src_dist(0, static_cast<int>(non_empty_facilities.size()) - 1);
    const std::string& src_fac = non_empty_facilities[src_dist(rng_)];
    auto& src_ops = result.assignments[src_fac];

    if (src_ops.empty()) return state;

    std::uniform_int_distribution<> op_dist(0, static_cast<int>(src_ops.size()) - 1);
    int op_idx = op_dist(rng_);
    std::string op_id = src_ops[op_idx];

    // 随机选择一个有空位的目标设施
    std::uniform_int_distribution<> dst_dist(0, static_cast<int>(has_space_facilities.size()) - 1);
    const std::string& dst_fac = has_space_facilities[dst_dist(rng_)];

    if (src_fac == dst_fac) return state;

    // 检查干员是否可以进入目标设施
    auto dst_fac_it = manager_.get_facilities().find(dst_fac);
    auto op_it = manager_.get_operators().find(op_id);
    if (dst_fac_it == manager_.get_facilities().end() || op_it == manager_.get_operators().end()) {
        return state;
    }

    if (!manager_.has_skill_for_facility(op_it->second, dst_fac_it->second.type)) return state;

    // 执行移动
    src_ops.erase(src_ops.begin() + op_idx);
    result.assignments[dst_fac].push_back(op_id);

    return result;
}

InfrastructureState ScheduleOptimizer::replace_operator(const InfrastructureState& state) {
    InfrastructureState result = state;

    // 获取未分配的干员
    auto unassigned = get_unassigned_operators(state);
    if (unassigned.empty()) return result;

    // 收集所有已分配的 (设施, 索引) 对
    std::vector<std::pair<std::string, int>> assigned_ops;
    for (const auto& [fac_id, ops] : state.assignments) {
        for (int i = 0; i < static_cast<int>(ops.size()); ++i) {
            assigned_ops.emplace_back(fac_id, i);
        }
    }

    if (assigned_ops.empty()) return result;

    std::uniform_int_distribution<> assigned_dist(0, static_cast<int>(assigned_ops.size()) - 1);
    auto [fac_id, op_idx] = assigned_ops[assigned_dist(rng_)];

    // 获取设施信息
    auto fac_it = manager_.get_facilities().find(fac_id);
    if (fac_it == manager_.get_facilities().end()) return state;

    // 从未分配干员中选择一个兼容的
    std::vector<std::string> compatible;
    for (const auto& op_id : unassigned) {
        auto op_it = manager_.get_operators().find(op_id);
        if (op_it != manager_.get_operators().end()) {
            if (manager_.has_skill_for_facility(op_it->second, fac_it->second.type)) {
                compatible.push_back(op_id);
            }
        }
    }

    if (compatible.empty()) return result;

    std::uniform_int_distribution<> compat_dist(0, static_cast<int>(compatible.size()) - 1);
    std::string new_op = compatible[compat_dist(rng_)];

    // 执行替换
    result.assignments[fac_id][op_idx] = new_op;

    return result;
}

std::vector<std::string> ScheduleOptimizer::get_unassigned_operators(const InfrastructureState& state) {
    std::set<std::string> assigned = state.get_assigned_set();
    std::vector<std::string> unassigned;

    for (const auto& [op_id, op] : manager_.get_operators()) {
        if (assigned.find(op_id) == assigned.end()) {
            unassigned.push_back(op_id);
        }
    }

    return unassigned;
}

bool ScheduleOptimizer::is_valid_state(const InfrastructureState& state) {
    std::set<std::string> seen;

    for (const auto& [fac_id, ops] : state.assignments) {
        // 检查槽位限制
        auto fac_it = manager_.get_facilities().find(fac_id);
        if (fac_it == manager_.get_facilities().end()) return false;
        if (static_cast<int>(ops.size()) > fac_it->second.slots) return false;

        // 检查干员唯一性
        for (const auto& op_id : ops) {
            if (seen.count(op_id)) return false;
            seen.insert(op_id);
        }
    }

    return true;
}

SchedulePlan ScheduleOptimizer::build_plan(const InfrastructureState& state, double efficiency) {
    SchedulePlan plan;
    plan.total_efficiency = efficiency;
    plan.iterations = stats_.total_iterations;
    plan.optimization_time_ms = stats_.time_ms;

    // 构建带全局变量的评估状态
    InfrastructureState eval_state = state;
    calculator_.evaluate(eval_state);  // 填充 variables、production lines

    for (const auto& [fac_id, ops] : state.assignments) {
        auto fac_it = manager_.get_facilities().find(fac_id);
        if (fac_it == manager_.get_facilities().end()) continue;

        SchedulePlan::Assignment assignment;
        assignment.facility_id = fac_id;
        assignment.facility_type = fac_it->second.type;
        assignment.operator_ids = ops;
        assignment.total_efficiency = calculator_.evaluate_facility(fac_id, ops, eval_state);

        plan.assignments.push_back(assignment);
    }

    return plan;
}
