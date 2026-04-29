"""
森空岛明日方舟干员数据提取脚本
用法: python get_operators.py
"""

import hashlib
import hmac
import json
import sys
import time
from pathlib import Path
from urllib.parse import urlparse

import requests


# 加载同目录下的 config.env
_CONFIG_PATH = Path(__file__).parent / "config.env"


def _load_config() -> dict:
    config = {}
    if _CONFIG_PATH.exists():
        for line in _CONFIG_PATH.read_text(encoding="utf-8").splitlines():
            line = line.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            k, _, v = line.partition("=")
            config[k.strip()] = v.strip()
    return config


BASE_AS = "https://as.hypergryph.com"
BASE_SK = "https://zonai.skland.com"

# 请求头基础配置（模拟 Skland APP）
REQUEST_HEADERS_BASE = {
    "User-Agent": "Skland/1.45.1 (com.hypergryph.skland; build:104501004; Android 34; ) Okhttp/4.11.0",
    "Accept-Encoding": "gzip",
    "Connection": "close",
    "Origin": "https://www.skland.com",
    "Referer": "https://www.skland.com/",
    "Content-Type": "application/json; charset=UTF-8",
    "manufacturer": "Xiaomi",
    "os": "34",
    "vname": "1.45.1",
    "vcode": "104501004",
    "platform": "1",
    "dId": "",
}

# 签名头基础配置
SIGN_HEADERS_BASE = {
    "platform": "1",
    "timestamp": "",
    "dId": "",
    "vName": "1.45.1",
}


def generate_signature(token: str, path: str, body_or_query: str, timestamp: str) -> tuple[str, dict]:
    """
    森空岛签名算法:
    1. 构造 sign_headers = SIGN_HEADERS_BASE + timestamp
    2. 待签名串 = path + body_or_query + timestamp + json(sign_headers)
    3. HMAC-SHA256(token, 待签名串) -> hex
    4. MD5(hex) -> 最终签名
    """
    header_ca = SIGN_HEADERS_BASE.copy()
    header_ca["timestamp"] = timestamp
    header_ca_str = json.dumps(header_ca, separators=(",", ":"))

    s = path + body_or_query + timestamp + header_ca_str
    hex_s = hmac.new(token.encode("utf-8"), s.encode("utf-8"), hashlib.sha256).hexdigest()
    md5 = hashlib.md5(hex_s.encode("utf-8")).hexdigest()
    return md5, header_ca


def get_signed_headers(url: str, method: str, body: dict | None,
                       cred: str, sign_token: str) -> dict:
    """构造带签名的完整请求头"""
    headers = REQUEST_HEADERS_BASE.copy()
    headers["cred"] = cred
    timestamp = str(int(time.time()))
    parsed = urlparse(url)

    if method == "get":
        sign, header_ca = generate_signature(sign_token, parsed.path, parsed.query or "", timestamp)
    else:
        body_str = json.dumps(body, separators=(",", ":")) if body else ""
        sign, header_ca = generate_signature(sign_token, parsed.path, body_str, timestamp)

    headers["sign"] = sign
    for k, v in header_ca.items():
        headers[k] = v
    return headers


# ─── 登录流程 ───────────────────────────────────────────────

def step1_get_token(phone: str, password: str) -> str:
    """用手机号+密码登录鹰角通行证，获取 token"""
    resp = requests.post(
        f"{BASE_AS}/user/auth/v1/token_by_phone_password",
        json={"phone": phone, "password": password},
        timeout=15,
    )
    data = resp.json()
    if data.get("status") != 0:
        raise RuntimeError(f"登录失败: {data.get('msg') or data.get('message')}")
    return data["data"]["token"]


def step2_get_oauth_code(token: str) -> str:
    """用 token 换取 OAuth2 授权码"""
    resp = requests.post(
        f"{BASE_AS}/user/oauth2/v2/grant",
        json={"token": token, "appCode": "4ca99fa6b56cc2ba", "type": 0},
        timeout=15,
    )
    data = resp.json()
    if data.get("status") != 0:
        raise RuntimeError(f"OAuth2 授权失败: {data.get('msg') or data.get('message')}")
    return data["data"]["code"]


def step3_get_cred(code: str) -> tuple[str, str]:
    """用授权码换取森空岛 cred 和 sign_token"""
    resp = requests.post(
        f"{BASE_SK}/web/v1/user/auth/generate_cred_by_code",
        json={"kind": 1, "code": code},
        headers=REQUEST_HEADERS_BASE,
        timeout=15,
    )
    data = resp.json()
    if data.get("code") != 0:
        raise RuntimeError(f"获取 cred 失败: {data.get('message')}")
    cred = data["data"]["cred"]
    token = data["data"].get("token", "")
    return cred, token


# ─── 数据接口 ───────────────────────────────────────────────

def get_binding_list(cred: str, sign_token: str) -> list:
    """获取绑定的游戏角色列表"""
    url = f"{BASE_SK}/api/v1/game/player/binding"
    headers = get_signed_headers(url, "get", None, cred, sign_token)
    resp = requests.get(url, headers=headers, timeout=15)
    data = resp.json()
    if data.get("code") != 0:
        raise RuntimeError(f"获取绑定列表失败: {data.get('message')}")
    return data["data"]["list"]


def get_player_info(cred: str, sign_token: str, uid: str) -> dict:
    """获取指定 UID 的完整玩家数据（含干员）"""
    url = f"{BASE_SK}/api/v1/game/player/info?uid={uid}"
    headers = get_signed_headers(url, "get", None, cred, sign_token)
    resp = requests.get(url, headers=headers, timeout=30)
    data = resp.json()
    if data.get("code") != 0:
        raise RuntimeError(f"获取玩家数据失败: {data.get('message')}")
    return data["data"]


# ─── 主流程 ─────────────────────────────────────────────────

def main():
    print("=== 森空岛干员数据提取 ===\n")

    cfg = _load_config()
    phone = cfg.get("SKLAND_PHONE") or input("鹰角通行证手机号: ").strip()
    password = cfg.get("SKLAND_PASSWORD") or input("密码: ").strip()

    print("\n[1/3] 登录鹰角通行证...")
    token = step1_get_token(phone, password)

    print("[2/3] 获取 OAuth2 授权...")
    code = step2_get_oauth_code(token)

    print("[3/3] 获取森空岛凭证...")
    cred, sign_token = step3_get_cred(code)
    print(f"      cred 获取成功: {cred[:8]}...")
    if sign_token:
        print(f"      token 获取成功: {sign_token[:8]}...\n")
    else:
        print("      警告: 未获取到 token，签名可能失败\n")

    print("正在获取绑定角色列表...")
    binding_list = get_binding_list(cred, sign_token)

    # 找明日方舟的角色
    ak_chars = []
    for game in binding_list:
        if game.get("appCode") == "arknights":
            ak_chars.extend(game.get("bindingList", []))

    # 兼容：没匹配到 appCode 时把所有角色列出来
    if not ak_chars:
        for game in binding_list:
            ak_chars.extend(game.get("bindingList", []))

    if not ak_chars:
        print("未找到绑定的游戏角色，请先在森空岛绑定明日方舟账号。")
        sys.exit(1)

    # 选择角色
    if len(ak_chars) == 1:
        selected = ak_chars[0]
    else:
        print("找到以下绑定角色：")
        for i, c in enumerate(ak_chars):
            print(f"  [{i}] UID={c.get('uid')}  昵称={c.get('nickName')}  服务器={c.get('channelName', '')}")
        idx = int(input("请选择角色编号: ").strip())
        selected = ak_chars[idx]

    uid = selected["uid"]
    print(f"\n正在获取 UID={uid} 的干员数据，请稍候...")

    player_data = get_player_info(cred, sign_token, uid)
    chars = player_data.get("chars", [])
    print(f"获取成功！共 {len(chars)} 名干员。")

    # 保存完整玩家数据
    data_dir = Path(__file__).resolve().parent.parent / "resource" / "data"
    data_dir.mkdir(parents=True, exist_ok=True)

    full_path = data_dir / f"player_full_{uid}.json"
    with open(full_path, "w", encoding="utf-8") as f:
        json.dump(player_data, f, ensure_ascii=False, indent=2)
    print(f"\n完整玩家数据已保存到: {full_path}")

    # 打印前3个干员预览
    print("\n--- 前3名干员预览 ---")
    for c in chars[:3]:
        print(json.dumps(c, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
