#!/usr/bin/env python3
"""
解析明日方舟基建技能数据
从 building_data.json 和 character_table.json 生成简化的干员技能数据
"""

import json
import re
from pathlib import Path

def clean_description(desc: str) -> str:
    """清理描述文本中的标签"""
    # 移除 <@cc.xxx> </> 等标签
    desc = re.sub(r'<@[^>]+>', '', desc)
    desc = re.sub(r'</>', '', desc)
    desc = re.sub(r'<\$[^>]+>', '', desc)
    return desc.strip()

def parse_room_type(room_type: str) -> str:
    """转换设施类型为中文"""
    mapping = {
        'CONTROL': '控制中枢',
        'MANUFACTURE': '制造站',
        'TRADING': '贸易站',
        'POWER': '发电站',
        'DORMITORY': '宿舍',
        'MEETING': '会客室',
        'WORKSHOP': '加工站',
        'TRAINING': '训练室',
        'HIRE': '办公室',
    }
    return mapping.get(room_type, room_type)

def parse_phase(phase: str) -> int:
    """转换精英化阶段"""
    mapping = {
        'PHASE_0': 0,
        'PHASE_1': 1,
        'PHASE_2': 2,
    }
    return mapping.get(phase, 0)

def extract_operator_meta(char_info: dict) -> dict:
    """提取干员组织信息"""
    return {
        'nation_id': char_info.get('nationId', '') or '',
        'group_id': char_info.get('groupId', '') or '',
        'team_id': char_info.get('teamId', '') or '',
        'profession': char_info.get('profession', '') or '',
        'sub_profession': char_info.get('subProfessionId', '') or '',
    }

def main():
    data_dir = Path(__file__).parent.parent / 'resource' / 'data'

    # 加载数据
    with open(data_dir / 'building_data.json', 'r', encoding='utf-8') as f:
        building_data = json.load(f)

    with open(data_dir / 'character_table.json', 'r', encoding='utf-8') as f:
        character_table = json.load(f)

    chars = building_data['chars']
    buffs = building_data['buffs']

    # 构建干员技能数据
    operators = {}

    for char_id, char_data in chars.items():
        # 获取干员名称
        if char_id not in character_table:
            continue

        char_info = character_table[char_id]
        name = char_info.get('name', char_id)

        # 跳过非干员（如召唤物）
        if char_info.get('profession') == 'TOKEN':
            continue

        operator = {
            'id': char_id,
            'name': name,
            'rarity': char_info.get('rarity', 'TIER_1'),
            'meta': extract_operator_meta(char_info),  # 新增：组织信息
            'skills': []
        }

        # 解析基建技能
        for buff_group in char_data.get('buffChar', []):
            for buff_data in buff_group.get('buffData', []):
                buff_id = buff_data.get('buffId')
                cond = buff_data.get('cond', {})

                if buff_id not in buffs:
                    continue

                buff_info = buffs[buff_id]

                skill = {
                    'buff_id': buff_id,
                    'name': buff_info.get('buffName', ''),
                    'facility': parse_room_type(buff_info.get('roomType', '')),
                    'description': clean_description(buff_info.get('description', '')),
                    'unlock_elite': parse_phase(cond.get('phase', 'PHASE_0')),
                    'unlock_level': cond.get('level', 1),
                }

                operator['skills'].append(skill)

        if operator['skills']:
            operators[char_id] = operator

    # 按设施类型分组统计
    facility_stats = {}
    for op in operators.values():
        for skill in op['skills']:
            facility = skill['facility']
            if facility not in facility_stats:
                facility_stats[facility] = 0
            facility_stats[facility] += 1

    # 统计组织信息
    group_stats = {}
    for op in operators.values():
        group_id = op['meta']['group_id']
        if group_id:
            group_stats[group_id] = group_stats.get(group_id, 0) + 1

    print(f"解析完成!")
    print(f"干员数量: {len(operators)}")
    print(f"\n各设施技能数量:")
    for facility, count in sorted(facility_stats.items(), key=lambda x: -x[1]):
        print(f"  {facility}: {count}")

    print(f"\n组织分布 (前10):")
    for group, count in sorted(group_stats.items(), key=lambda x: -x[1])[:10]:
        print(f"  {group}: {count}")

    # 保存结果
    output_path = data_dir / 'operator_skills.json'
    with open(output_path, 'w', encoding='utf-8') as f:
        json.dump(operators, f, ensure_ascii=False, indent=2)

    print(f"\n已保存到: {output_path}")

    # 输出示例
    print("\n=== 示例数据 ===")
    for i, (char_id, op) in enumerate(operators.items()):
        if i >= 3:
            break
        print(f"\n{op['name']} (组织: {op['meta']['group_id'] or '无'}):")
        for skill in op['skills']:
            desc = skill['description'][:50] + '...' if len(skill['description']) > 50 else skill['description']
            print(f"  - [{skill['facility']}] {skill['name']}: {desc}")

if __name__ == '__main__':
    main()
