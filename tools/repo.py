#!/usr/bin/env python3
"""repo.py — 各工具共用的仓库路径小工具。

存在的理由：`Path.relative_to(ROOT)` 在仓库外的路径上会抛 ValueError，
而工具经常要处理仓库外的路径——变异自检把单元复制到 /tmp 再跑，
单元测试在临时目录里造样例。这个坑在 ledger / check_doc / check_code 里
各踩了一次，所以把它收成一处。
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def rel_label(path) -> str:
    """能相对化就相对化，不能就原样返回——绝不抛异常。"""
    path = Path(path)
    try:
        return str(path.relative_to(ROOT))
    except ValueError:
        return str(path)
