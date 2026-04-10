#pragma once
#include <map>
#include <string>

// 组织 ID 到中文名映射
inline const std::map<std::string, std::string> GROUP_ID_TO_NAME = {
    // 维多利亚
    {"glasgow", "格拉斯哥帮"},
    {"victoria", "维多利亚"},

    // 莱塔尼亚
    {"rhine", "莱茵生命"},

    // 龙门
    {"lungmen", "龙门"},
    {"lgd", "龙门近卫局"},

    // 罗德岛
    {"rhodes", "罗德岛"},
    {"elite", "罗德岛精英干员"},
    {"op_a", "行动组A"},
    {"op_b", "行动组B"},
    {"reserve_a1", "预备干员A1"},
    {"reserve_a4", "预备干员A4"},
    {"reserve_a6", "预备干员A6"},
    {"action4", "行动预备组A4"},
    {"student", "彩虹小队"},

    // 黑钢国际
    {"blacksteel", "黑钢国际"},

    // 企鹅物流
    {"penguin", "企鹅物流"},

    // 喀兰贸易
    {"karlan", "喀兰贸易"},

    // 深海猎人
    {"abyssal", "深海猎人"},

    // 乌萨斯
    {"ursus", "乌萨斯"},
    {"peterheim", "乌萨斯学生自治团"},

    // 叙拉古
    {"siracusa", "叙拉古"},
    {"chiave", "西西里帮"},

    // 炎
    {"yan", "炎"},
    {"sui", "岁"},

    // 哥伦比亚
    {"columbia", "哥伦比亚"},
    {"rim", "莱茵生命"},

    // 萨米
    {"sami", "萨米"},

    // 卡兹戴尔
    {"kazdel", "卡兹戴尔"},
    {"babel", "巴别塔"},

    // 伊比利亚
    {"iberia", "伊比利亚"},

    // 萨尔贡
    {"sargon", "萨尔贡"},

    // 米诺斯
    {"minos", "米诺斯"},

    // 玻利瓦尔
    {"bolivar", "玻利瓦尔"},

    // 雷姆必拓
    {"rim_billiton", "雷姆必拓"},

    // 怪物猎人联动
    {"mh", "怪物猎人小队"},

    // 红松骑士团
    {"knight", "红松骑士团"},
    {"pinus", "红松骑士团"},

    // 其他
    {"egir", "深海教会"},
    {"followers", "追随者"},
    {"dublinn", "都柏林"},
};

// 中文名到组织 ID 映射
inline const std::map<std::string, std::string> NAME_TO_GROUP_ID = {
    {"格拉斯哥帮", "glasgow"},
    {"维多利亚", "victoria"},
    {"莱茵生命", "rhine"},
    {"龙门", "lungmen"},
    {"龙门近卫局", "lgd"},
    {"罗德岛", "rhodes"},
    {"黑钢国际", "blacksteel"},
    {"企鹅物流", "penguin"},
    {"喀兰贸易", "karlan"},
    {"深海猎人", "abyssal"},
    {"乌萨斯", "ursus"},
    {"乌萨斯学生自治团", "peterheim"},
    {"叙拉古", "siracusa"},
    {"炎", "yan"},
    {"岁", "sui"},
    {"哥伦比亚", "columbia"},
    {"萨米", "sami"},
    {"卡兹戴尔", "kazdel"},
    {"巴别塔", "babel"},
    {"伊比利亚", "iberia"},
    {"萨尔贡", "sargon"},
    {"米诺斯", "minos"},
    {"玻利瓦尔", "bolivar"},
    {"雷姆必拓", "rim_billiton"},
    {"怪物猎人小队", "mh"},
    {"红松骑士团", "knight"},
    {"深海教会", "egir"},
    {"追随者", "followers"},
    {"都柏林", "dublinn"},
};

// 设施类型映射（英文 -> 中文）
inline const std::map<std::string, std::string> FACILITY_TYPE_TO_NAME = {
    {"CONTROL", "控制中枢"},
    {"MANUFACTURE", "制造站"},
    {"TRADING", "贸易站"},
    {"POWER", "发电站"},
    {"DORMITORY", "宿舍"},
    {"MEETING", "会客室"},
    {"WORKSHOP", "加工站"},
    {"TRAINING", "训练室"},
    {"HIRE", "办公室"},
};

// 中文设施名 -> 英文类型
inline const std::map<std::string, std::string> FACILITY_NAME_TO_TYPE = {
    {"控制中枢", "CONTROL"},
    {"制造站", "MANUFACTURE"},
    {"贸易站", "TRADING"},
    {"发电站", "POWER"},
    {"宿舍", "DORMITORY"},
    {"会客室", "MEETING"},
    {"加工站", "WORKSHOP"},
    {"训练室", "TRAINING"},
    {"办公室", "HIRE"},
};
