#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""快速版面自查：把一份 pptx 渲染成 PDF，逐页找越界与压字。

用法：python3 checklayout.py 03 [04 ...]

verify.py 第 7 项做的是同一件事，但它每次跑全部章节；写新章的时候
只想看刚建好的那一份，所以单拎出来。判据两边共用 verify.py 里的函数，
不另写一套 —— 两套判据迟早会漂。
"""
import re
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import build_all                                    # noqa: E402
import verify                                       # noqa: E402


def main(argv):
    if not argv:
        raise SystemExit('用法：python3 checklayout.py 03 [04 ...]')
    bad = 0
    with tempfile.TemporaryDirectory() as tmp:
        pptxs = [HERE / (build_all.CHAPTERS[c.zfill(2)] + '.pptx') for c in argv]
        subprocess.run(['soffice', '--headless', '--convert-to', 'pdf',
                        '--outdir', tmp] + [str(p) for p in pptxs],
                       capture_output=True, timeout=1800)
        for pdf in sorted(Path(tmp).glob('*.pdf')):
            bbox = subprocess.run(['pdftotext', '-bbox', str(pdf), '-'],
                                  capture_output=True, text=True).stdout
            pages = re.findall(
                r'<page width="([\d.]+)" height="([\d.]+)">(.*?)</page>',
                bbox, re.S)
            for pno, (pw, ph, body) in enumerate(pages, start=1):
                pw, ph = float(pw), float(ph)
                sy = 540.0 / ph
                words = []
                for x, y, x2, y2, text in re.findall(
                        r'<word xMin="([\d.]+)" yMin="([\d.]+)" '
                        r'xMax="([\d.]+)" yMax="([\d.]+)">([^<]*)', body):
                    x, y, x2, y2 = float(x), float(y), float(x2), float(y2)
                    words.append((x, y, x2, y2, text))
                    if verify.intrudes_into_footer(y * sy, y2 * sy):
                        print(f'❌ {pdf.stem} 第 {pno} 页：正文侵入页脚区 {text!r}')
                        bad += 1
                        break
                hits = verify.overlapping_words(words)
                if hits:
                    print(f'❌ {pdf.stem} 第 {pno} 页：两段文字压在一起 {hits[0]}')
                    bad += 1
            print(f'{pdf.stem}：{len(pages)} 页')
    print('全部通过' if not bad else f'{bad} 处问题')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
