#include "infrastructure/SkillParser.hpp"
#include <iostream>

std::vector<SkillEffect> SkillParser::parse(const std::string& desc,
                                            const std::string& facility) {
    std::vector<SkillEffect> effects;

    // 按分号分割描述，对每个子句独立解析
    // 例如: "订单获取效率+20%；当与能天使在同一个贸易站时，额外+25%"
    // 分割为两个子句分别解析
    auto clauses = split_clauses(desc);

    for (const auto& clause : clauses) {
        if (auto e = parse_flat_bonus(clause, facility)) {
            effects.push_back(*e);
        }

        if (auto e = parse_per_operator(clause, facility)) {
            effects.push_back(*e);
        }

        if (auto e = parse_per_production_line(clause, facility)) {
            effects.push_back(*e);
        }

        if (auto e = parse_per_facility_global(clause, facility)) {
            effects.push_back(*e);
        }

        if (auto e = parse_synergy_specific(clause, facility)) {
            effects.push_back(*e);
        }

        if (auto e = parse_synergy_group(clause, facility)) {
            effects.push_back(*e);
        }

        if (auto e = parse_variable_produce(clause, facility)) {
            effects.push_back(*e);
        }

        if (auto e = parse_variable_consume(clause, facility)) {
            effects.push_back(*e);
        }
    }

    return effects;
}

// 固定加成: "生产力+30%", "订单获取效率+35%", "无人机充能速度+10%"
// 排除带条件的子句（联动、按人数等由其他解析器处理）
std::optional<SkillEffect> SkillParser::parse_flat_bonus(const std::string& desc,
                                                         const std::string& facility) {
    // 子句中包含条件关键词时，不作为无条件加成
    static const std::regex condition_pattern(R"(当与|与[^】\]]*同一|每[有个条]|每\d+点)");
    if (std::regex_search(desc, condition_pattern)) {
        return std::nullopt;
    }

    // 匹配模式：生产力/效率/速度 + 数字%
    static const std::regex pattern(
        R"((生产力|订单获取效率|贸易站效率|效率|无人机充能速度|充能速度|线索搜集速度)[+＋](\d+)%)"
    );

    std::smatch match;
    if (std::regex_search(desc, match, pattern)) {
        SkillEffect effect;
        effect.type = SkillEffectType::FLAT_BONUS;
        effect.facility = facility;
        effect.value = std::stod(match[2].str()) / 100.0;
        return effect;
    }
    return std::nullopt;
}

// 每干员加成: "每个进驻的[格拉斯哥帮]干员，生产力+5%"
std::optional<SkillEffect> SkillParser::parse_per_operator(const std::string& desc,
                                                           const std::string& facility) {
    // 匹配模式：每(有/个)N个[组织名]干员...+X%
    static const std::regex pattern(
        R"(每[有个]?(\d*)[个名位]?(?:进驻[的在]?)?[【\[]([^\]】]+)[】\]][^+＋]*[+＋](\d+)%)"
    );

    std::smatch match;
    if (std::regex_search(desc, match, pattern)) {
        SkillEffect effect;
        effect.type = SkillEffectType::PER_OPERATOR_BONUS;
        effect.facility = facility;
        effect.condition_count = match[1].str().empty() ? 1 : std::stoi(match[1].str());
        effect.condition_group = match[2].str();
        effect.value = std::stod(match[3].str()) / 100.0;
        return effect;
    }
    return std::nullopt;
}

// 生产线加成: "当前制造站内每条[赤金]生产线，生产力+5%"
std::optional<SkillEffect> SkillParser::parse_per_production_line(const std::string& desc,
                                                                   const std::string& facility) {
    // 匹配模式：每(条)[赤金/作战记录]生产线...+X%
    static const std::regex pattern(
        R"(每[有条]?(\d*)条?[【\[]?(赤金|作战记录|芯片)[】\]]?生产线[^+＋]*[+＋](\d+)%)"
    );

    std::smatch match;
    if (std::regex_search(desc, match, pattern)) {
        SkillEffect effect;
        effect.type = SkillEffectType::PER_PRODUCTION_LINE;
        effect.facility = facility;
        effect.condition_count = match[1].str().empty() ? 1 : std::stoi(match[1].str());

        // 转换生产线类型
        std::string prod_type = match[2].str();
        if (prod_type == "赤金") {
            effect.production_type = "gold";
        } else if (prod_type == "作战记录") {
            effect.production_type = "record";
        } else if (prod_type == "芯片") {
            effect.production_type = "chip";
        }

        effect.value = std::stod(match[3].str()) / 100.0;
        return effect;
    }
    return std::nullopt;
}

// 特定干员联动: "与德克萨斯在同一个贸易站时，订单获取效率+65%"
std::optional<SkillEffect> SkillParser::parse_synergy_specific(const std::string& desc,
                                                                const std::string& facility) {
    // 匹配模式：与XXX(在)同一...+X%
    static const std::regex pattern(
        R"(与[【\[]?([^】\]\s]+?)[】\]]?(?:在)?同一[个]?(?:制造站|贸易站|发电站|设施|房间)?[^+＋]*[+＋](\d+)%)"
    );

    std::smatch match;
    if (std::regex_search(desc, match, pattern)) {
        SkillEffect effect;
        effect.type = SkillEffectType::SYNERGY_SPECIFIC;
        effect.facility = facility;
        effect.condition_operator = match[1].str();
        effect.value = std::stod(match[2].str()) / 100.0;
        return effect;
    }
    return std::nullopt;
}

// 组织联动: "与[莱茵生命]干员一起工作时+25%"
std::optional<SkillEffect> SkillParser::parse_synergy_group(const std::string& desc,
                                                            const std::string& facility) {
    // 匹配模式：与[组织]干员(一起工作时)...+X%
    static const std::regex pattern(
        R"(与[【\[]([^\]】]+)[】\]]干员[一同]起工作时[^+＋]*[+＋](\d+)%)"
    );

    std::smatch match;
    if (std::regex_search(desc, match, pattern)) {
        SkillEffect effect;
        effect.type = SkillEffectType::SYNERGY_GROUP;
        effect.facility = facility;
        effect.condition_group = match[1].str();
        effect.value = std::stod(match[2].str()) / 100.0;
        return effect;
    }
    return std::nullopt;
}

// 变量产生: "人间烟火+15"
std::optional<SkillEffect> SkillParser::parse_variable_produce(const std::string& desc,
                                                                const std::string& facility) {
    // 匹配模式：(变量名)+数字
    static const std::regex pattern(
        R"((人间烟火|感知信息|意识协议|干劲)[+＋](\d+))"
    );

    std::smatch match;
    if (std::regex_search(desc, match, pattern)) {
        SkillEffect effect;
        effect.type = SkillEffectType::VARIABLE_PRODUCE;
        effect.facility = facility;
        effect.variable_name = match[1].str();
        effect.value = std::stod(match[2].str());
        return effect;
    }
    return std::nullopt;
}

// 变量消费: "每1点人间烟火，生产力+1%"
std::optional<SkillEffect> SkillParser::parse_variable_consume(const std::string& desc,
                                                                const std::string& facility) {
    // 匹配模式：每N点(变量名)...+X%
    static const std::regex pattern(
        R"(每(\d+)点(人间烟火|感知信息|意识协议)[^+＋]*[+＋](\d+)%)"
    );

    std::smatch match;
    if (std::regex_search(desc, match, pattern)) {
        SkillEffect effect;
        effect.type = SkillEffectType::VARIABLE_CONSUME;
        effect.facility = facility;
        effect.condition_count = std::stoi(match[1].str());
        effect.variable_name = match[2].str();
        effect.value = std::stod(match[3].str()) / 100.0;
        return effect;
    }
    return std::nullopt;
}

double SkillParser::extract_percentage(const std::string& text) {
    static const std::regex pattern(R"((\d+)%)");
    std::smatch match;
    if (std::regex_search(text, match, pattern)) {
        return std::stod(match[1].str()) / 100.0;
    }
    return 0.0;
}

std::string SkillParser::extract_bracket_content(const std::string& text) {
    static const std::regex pattern(R"([【\[]([^\]】]+)[】\]])");
    std::smatch match;
    if (std::regex_search(text, match, pattern)) {
        return match[1].str();
    }
    return "";
}

// 全局设施计数加成: "每有一间进驻精英干员的设施，订单获取效率额外+2%（最多10间）"
std::optional<SkillEffect> SkillParser::parse_per_facility_global(const std::string& desc,
                                                                   const std::string& facility) {
    // 匹配: 每有(一/1)间...精英干员...设施...+N%...最多M间
    static const std::regex pattern(
        R"(每有[一1]间[^+＋]*(精英干员|[^\s，,]+干员)[的]?设施[^+＋]*[+＋](\d+)%)"
    );

    std::smatch match;
    if (std::regex_search(desc, match, pattern)) {
        SkillEffect effect;
        effect.type = SkillEffectType::PER_FACILITY_GLOBAL;
        effect.facility = facility;
        effect.value = std::stod(match[2].str()) / 100.0;

        // 解析条件类型
        std::string cond = match[1].str();
        if (cond == "精英干员") {
            effect.facility_condition = "elite";
        } else {
            // 其他条件如"岁干员" -> 提取组织名
            effect.facility_condition = cond.substr(0, cond.find("干员"));
        }

        // 提取最大数量限制
        static const std::regex max_pattern(R"(最多(\d+)间)");
        std::smatch max_match;
        if (std::regex_search(desc, max_match, max_pattern)) {
            effect.max_count = std::stoi(max_match[1].str());
        }

        return effect;
    }
    return std::nullopt;
}

std::vector<std::string> SkillParser::split_clauses(const std::string& desc) {
    std::vector<std::string> clauses;
    std::string current;

    for (size_t i = 0; i < desc.size(); ) {
        // 检查中文分号 "；" (UTF-8: 0xEF 0xBC 0x9B)
        if (i + 2 < desc.size() &&
            static_cast<unsigned char>(desc[i]) == 0xEF &&
            static_cast<unsigned char>(desc[i+1]) == 0xBC &&
            static_cast<unsigned char>(desc[i+2]) == 0x9B) {
            if (!current.empty()) clauses.push_back(current);
            current.clear();
            i += 3;
            continue;
        }
        // 检查英文分号 ";"
        if (desc[i] == ';') {
            if (!current.empty()) clauses.push_back(current);
            current.clear();
            i++;
            continue;
        }
        current += desc[i];
        i++;
    }
    if (!current.empty()) clauses.push_back(current);

    // 如果没有分号，返回原始描述
    if (clauses.empty()) clauses.push_back(desc);

    return clauses;
}
