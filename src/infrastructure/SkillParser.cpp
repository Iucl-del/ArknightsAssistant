#include "infrastructure/SkillParser.hpp"
#include <iostream>

std::vector<SkillEffect> SkillParser::parse(const std::string& desc,
                                            const std::string& facility) {
    std::vector<SkillEffect> effects;

    // 按优先级尝试解析各种模式
    // 一个描述可能包含多个效果，如："生产力+25%，人间烟火+10"

    if (auto e = parse_flat_bonus(desc, facility)) {
        effects.push_back(*e);
    }

    if (auto e = parse_per_operator(desc, facility)) {
        effects.push_back(*e);
    }

    if (auto e = parse_per_production_line(desc, facility)) {
        effects.push_back(*e);
    }

    if (auto e = parse_synergy_specific(desc, facility)) {
        effects.push_back(*e);
    }

    if (auto e = parse_synergy_group(desc, facility)) {
        effects.push_back(*e);
    }

    if (auto e = parse_variable_produce(desc, facility)) {
        effects.push_back(*e);
    }

    if (auto e = parse_variable_consume(desc, facility)) {
        effects.push_back(*e);
    }

    return effects;
}

// 固定加成: "生产力+30%", "订单获取效率+35%", "无人机充能速度+10%"
std::optional<SkillEffect> SkillParser::parse_flat_bonus(const std::string& desc,
                                                         const std::string& facility) {
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
