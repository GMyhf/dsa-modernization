# -*- coding: utf-8 -*-
"""生成《数据结构与算法》第 1–12 章的课件 PPTX。

用法：
    python3 build_all.py           # 生成全部（尚未写内容的章会跳过并列出）
    python3 build_all.py 01 03     # 只生成指定章
"""

import importlib
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE / 'content'))

import deck  # noqa: E402

# 章号 -> 输出文件名（与 Markdown 讲义同名，便于对照）
CHAPTERS = {
    '01': 'DSA_CH01_Overview_ADT_Complexity',
    '02': 'DSA_CH02_Linear_List',
    '03': 'DSA_CH03_Stack_Queue',
    '04': 'DSA_CH04_String',
    '05': 'DSA_CH05_Binary_Tree',
    '06': 'DSA_CH06_Tree',
    '07': 'DSA_CH07_Graph',
    '08': 'DSA_CH08_Internal_Sorting',
    '09': 'DSA_CH09_File_External_Sorting',
    '10': 'DSA_CH10_Searching',
    '11': 'DSA_CH11_Indexing',
    '12': 'DSA_CH12_Advanced_Structures',
}


def main(argv):
    wanted = argv or sorted(CHAPTERS)
    missing = []
    for ch in wanted:
        if ch not in CHAPTERS:
            raise SystemExit(f'未知章号：{ch}（可用：{" ".join(sorted(CHAPTERS))}）')
        if not (HERE / 'content' / f'ch{ch}.py').exists():
            missing.append(ch)
            continue
        mod = importlib.import_module(f'ch{ch}')
        out = HERE / (CHAPTERS[ch] + '.pptx')
        pages = deck.build(mod.META, mod.SLIDES, str(out))
        print(f'{out.name}  ({pages} slides)')
    if missing:
        print('尚未编写内容，已跳过：' + ' '.join('第%s章' % c.lstrip('0') for c in missing))


if __name__ == '__main__':
    main(sys.argv[1:])
