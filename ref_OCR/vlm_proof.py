#!/usr/bin/env python3
"""vlm_proof.py — 让多模态模型对照扫描页，逐页找出 GLM-OCR 转写与原图不一致的地方（T-093）。

只产出待人工核对的清单，不改任何文本。模型会把原文「改通顺」，所以每条结果都要过三道检查：
1. 定位：模型摘出的「转写」片段必须能在这一页的 GLM 文本里原样找到，找不到的丢弃并计数。
2. 旁证：模型说的「原图」文字，是否也出现在 `dsa_raw.md` 或 Qwen 转写的同一页里。
3. 改通顺嫌疑：只是「性→度」「码→词」这类已知的 Qwen 改写，单独标出。

在 sulab 上运行（纯标准库）：
  python3 vlm_proof.py 端口[,端口...] 工作目录 [页码范围 a-b]
工作目录里要有：glm.md、qwen.md、dsa_raw.md、book.pdf；结果写到 工作目录/proof/pNNN.json。
汇总：python3 vlm_proof.py --report 工作目录 输出.md 输出.json
合并两个模型：python3 vlm_proof.py --merge 工作目录 proof.json proof_gemma.json 输出.md 输出.json
"""
import base64, concurrent.futures as cf, itertools, json, os, re, subprocess, sys, threading, time, urllib.request

DPI = 220
# 换模型：环境变量 VLM_MODEL=gemma VLM_OUT=proof_gemma（默认 qwen、proof）；采样参数按各模型推荐值
MODEL = os.environ.get('VLM_MODEL', 'qwen'); OUT = os.environ.get('VLM_OUT', 'proof')
SAMPLE = {'qwen': {'temperature': 0.6, 'top_p': 0.95, 'top_k': 20}, 'gemma': {'temperature': 1.0, 'top_p': 0.95, 'top_k': 64}}[MODEL]
PROMPT = '''你在做教材扫描件的校对。图片是《数据结构与算法》（2008 年）的一页扫描，下面是这一页的 OCR 转写。
请逐字对照图片，找出转写和图片**不一致**的地方。

规则：
- 只报告图片里明确看得到的差异：错字、漏字、多字、标点、数字、符号、代码字符、公式。
- 以图片为准，原书怎么印就怎么写。**不要**按常识或语法去“改正”原书，**不要**把原文改通顺，不要统一术语。
- 不报告：空白、换行、缩进、Markdown 格式（#、*、```）、LaTeX 写法差异（只要数学内容相同）、全角半角标点的区别、页眉页码、插图内部的文字。
- “转写”必须从下面的转写里**原样复制**一段，包含出错处和前后各几个字，保证能唯一定位；“原图”是图片上同一范围的实际文字。
- 没有差异就输出 []。

只输出一个 JSON 数组，不要解释：
[{"转写": "...", "原图": "...", "类型": "错字|漏字|多字|标点|数字|符号|代码|公式|其他", "把握": "高|中"}]

OCR 转写：
<<<
{text}
>>>'''

def pages(path):
    s = open(path, encoding='utf8').read()
    P = re.split(r'<!-- 第 (\d+) 页 -->', s)
    return {int(P[i]): P[i + 1].strip() for i in range(1, len(P), 2)}

def han(s):
    return re.sub(r'\s+', '', s)

def bare(s):  # 去掉空白和 LaTeX 记号后比较，只差这些的不算差异（提示词已要求不报）
    s = re.sub(r'\\(left|right|mathrm|text|operatorname)', '', s)
    return re.sub(r'[\s$\\{}]', '', s).translate(str.maketrans('，。：；（）', ',.:;()'))

def extract(c):
    c = re.sub(r'^.*?</think>', '', c or '', flags=re.S).strip()
    # 模型有时先给一个数组、再“修正”成另一个，取最后一个能解析的数组
    D, last = json.JSONDecoder(), None
    for i, ch in enumerate(c):
        if ch == '[':
            try: v, _ = D.raw_decode(c, i)
            except Exception: continue
            if isinstance(v, list) and all(isinstance(x, dict) for x in v): last = v
    return last

def run(ports, W, rng):
    G = pages(f'{W}/glm.md'); os.makedirs(f'{W}/png', exist_ok=True); os.makedirs(f'{W}/{OUT}', exist_ok=True)
    todo = [p for p in sorted(G) if (not rng or rng[0] <= p <= rng[1]) and not os.path.exists(f'{W}/{OUT}/p{p:03d}.json')]
    C = itertools.cycle(ports); L = threading.Lock(); print('待做', len(todo), '页', flush=True)
    def one(p):
        f = f'{W}/png/p{p:03d}'
        if not os.path.exists(f + '.png'):
            subprocess.run(['pdftoppm', '-r', str(DPI), '-f', str(p), '-l', str(p), '-png', '-singlefile', '-gray', f'{W}/book.pdf', f], check=True)
        b = base64.b64encode(open(f + '.png', 'rb').read()).decode()
        with L: port = next(C)
        body = {'model': MODEL, 'max_tokens': 16000, **SAMPLE,
                'messages': [{'role': 'user', 'content': [{'type': 'image_url', 'image_url': {'url': 'data:image/png;base64,' + b}},
                                                          {'type': 'text', 'text': PROMPT.replace('{text}', G[p])}]}]}
        t = time.time()
        for k in range(3):
            try:
                r = json.load(urllib.request.urlopen(urllib.request.Request(f'http://127.0.0.1:{port}/v1/chat/completions', json.dumps(body).encode(), {'Content-Type': 'application/json'}), timeout=3600))
                c = r['choices'][0]['message']; out = extract(c.get('content'))
                if out is not None or k == 2: break
            except Exception as e:
                c, out, r = {'content': repr(e)}, None, {}
        json.dump({'page': p, 'items': out, 'raw': c.get('content'), 'finish': (r.get('choices') or [{}])[0].get('finish_reason'),
                   'tokens': (r.get('usage') or {}).get('completion_tokens'), 'sec': round(time.time() - t, 1)},
                  open(f'{W}/{OUT}/p{p:03d}.json', 'w', encoding='utf8'), ensure_ascii=False, indent=1)
        return p
    with cf.ThreadPoolExecutor(16) as ex:
        for i, p in enumerate(ex.map(one, todo), 1):
            if i % 20 == 0: print(i, '/', len(todo), time.strftime('%H:%M'), flush=True)
    print('PROOFDONE', flush=True)

SMOOTH = [('性', '度'), ('码', '词')]  # Qwen 已知的“改通顺”（见 README）

def report(W, out_md, out_json):
    G, Q = pages(f'{W}/glm.md'), pages(f'{W}/qwen.md'); R = han(open(f'{W}/dsa_raw.md', encoding='utf8').read())
    items, bad, same, fail, n_pages = [], 0, 0, [], 0
    for f in sorted(os.listdir(f'{W}/{OUT}')):
        j = json.load(open(f'{W}/{OUT}/{f}', encoding='utf8')); p = j['page']; n_pages += 1
        j['items'] = extract(j.get('raw')) if j.get('raw') else j['items']
        if j['items'] is None: fail.append(p); continue
        g = G[p]; q = han(Q.get(p, ''))
        for it in j['items']:
            if not isinstance(it, dict): bad += 1; continue
            a, o = str(it.get('转写', '')), str(it.get('原图', ''))
            if not a or (a not in g and han(a) not in han(g)): bad += 1; continue
            if bare(a) == bare(o): same += 1; continue
            d = [(x, y) for x, y in zip(a, o) if x != y] if len(a) == len(o) else None
            items.append({'page': p, 'book_page': p - 14, '转写': a, '原图': o, '类型': it.get('类型'), '把握': it.get('把握'),
                          'raw旁证': bool(han(o)) and han(o) in R, 'qwen旁证': bool(han(o)) and han(o) in q,
                          '疑似改通顺': bool(d) and all(x in SMOOTH for x in d)})
    json.dump({'pages': n_pages, 'failed_pages': fail, 'dropped_unlocatable': bad, 'dropped_same': same, 'items': items},
              open(out_json, 'w', encoding='utf8'), ensure_ascii=False, indent=1)
    both = [i for i in items if i['raw旁证'] and i['qwen旁证'] and not i['疑似改通顺']]
    one = [i for i in items if (i['raw旁证'] ^ i['qwen旁证']) and not i['疑似改通顺']]
    none = [i for i in items if not i['raw旁证'] and not i['qwen旁证'] and not i['疑似改通顺']]
    sm = [i for i in items if i['疑似改通顺']]
    esc = lambda s: s.replace('|', '\\|').replace('\n', '⏎')
    L = [f'# GLM-OCR 逐页校对清单（模型初筛）', '',
         f'{time.strftime("%Y-%m-%d")} 由 `vlm_proof.py` 生成（T-093）。Qwen3.8-27B-FP8 对照 {DPI}dpi 扫描页，逐页列出 GLM 转写与原图不一致处。**未经人工核对，不能直接据此改文本。**', '',
         f'- 处理 {n_pages} 页；模型输出无法解析 {len(fail)} 页{("：" + "、".join(map(str, fail))) if fail else ""}。',
         f'- 保留 {len(items)} 条；丢弃 {bad} 条“转写”在 GLM 原文中找不到的（模型编造或改写了引文），{same} 条去掉空白和 LaTeX 记号后两边相同的（模型误报）。',
         f'- 旁证：`dsa_raw.md` 全书和 Qwen 转写同页里能否找到模型说的“原图”文字。两份都有 {len(both)} 条，只有一份 {len(one)} 条，都没有 {len(none)} 条，疑似改通顺 {len(sm)} 条。',
         '- 两份都旁证的最可信，建议先核；都没有旁证的多半是模型误报，但也可能三份 OCR 错得一样。页码是 PDF 页码，书上页码 = PDF 页码 − 14。', '']
    for title, S in (('两份旁证', both), ('一份旁证', one), ('无旁证', none), ('疑似改通顺（多半误报）', sm)):
        L += [f'## {title}（{len(S)} 条）', '', '| 页 | 转写 | 原图 | 类型 | 把握 | 旁证 |', '|---|---|---|---|---|---|']
        L += [f"| {i['page']} | {esc(i['转写'])} | {esc(i['原图'])} | {i['类型']} | {i['把握']} | {'raw' if i['raw旁证'] else ''}{' qwen' if i['qwen旁证'] else ''} |" for i in S]
        L.append('')
    open(out_md, 'w', encoding='utf8').write('\n'.join(L))
    print(n_pages, '页', len(items), '条', '丢弃', bad, '相同', same, '失败页', fail, '两份', len(both), '一份', len(one), '无', len(none), '改通顺', len(sm))

def cores(a, o):
    """转写→原图的差异片段集合（去空白和 LaTeX 记号后），用来判断两个模型是否报了同一处。"""
    import difflib
    a, o = bare(a), bare(o)
    return {(a[i1:i2], o[j1:j2]) for t, i1, i2, j1, j2 in difflib.SequenceMatcher(None, a, o, autojunk=False).get_opcodes() if t != 'equal'}

GLYPH = {('{', '|'), ('}', '|'), ('l', '1'), ('1', 'l'), ('[', '|'), (']', '|')}  # 扫描件代码字体里形近，两个模型会错得一样

def glyph_only(i):
    import difflib
    a, o = han(i['转写']), han(i['原图'])  # 只去空白，保留花括号
    c = {(a[i1:i2], o[j1:j2]) for t, i1, i2, j1, j2 in difflib.SequenceMatcher(None, a, o, autojunk=False).get_opcodes() if t != 'equal'}
    return bool(c) and all(len(a) == len(b) and all((x, y) in GLYPH for x, y in zip(a, b)) for a, b in c)

def merge(W, qj, gj, out_md, out_json):
    Q, G = json.load(open(qj, encoding='utf8')), json.load(open(gj, encoding='utf8'))
    gc = {}
    for i in G['items']: gc.setdefault(i['page'], []).append((cores(i['转写'], i['原图']), i))
    for i in Q['items']:
        c = cores(i['转写'], i['原图']); i['gemma同意'] = any(c & d for d, _ in gc.get(i['page'], []))
    used = {id(g) for i in Q['items'] for d, g in gc.get(i['page'], []) if cores(i['转写'], i['原图']) & d}
    extra = [g for p in gc for d, g in gc[p] if id(g) not in used and g['raw旁证'] and g['qwen旁证'] and not g['疑似改通顺']]
    json.dump({'qwen': Q, 'gemma_failed_pages': G['failed_pages'], 'gemma_extra': extra}, open(out_json, 'w', encoding='utf8'), ensure_ascii=False, indent=1)
    I = Q['items']; esc = lambda s: s.replace('|', '\\|').replace('\n', '⏎')
    row = lambda i: f"| {i['page']} | {esc(i['转写'])} | {esc(i['原图'])} | {i['类型']} | {'raw' if i['raw旁证'] else ''}{' qwen' if i['qwen旁证'] else ''} |"
    hd = ['| 页 | 转写 | 原图 | 类型 | 旁证 |', '|---|---|---|---|---|']
    gly = [i for i in I if glyph_only(i)]; I = [i for i in I if not glyph_only(i)]
    # Gemma 常把代码注释整段判为原图没有；这类删除的旁证没有意义（短串到处都有），不收
    extra = [g for g in extra if not glyph_only(g) and not ('//' in g['转写'] and '//' not in g['原图'])]
    agree = [i for i in I if i['gemma同意']]
    tiers = [('两份旁证', lambda i: i['raw旁证'] and i['qwen旁证']), ('一份旁证', lambda i: i['raw旁证'] ^ i['qwen旁证']), ('无旁证', lambda i: not i['raw旁证'] and not i['qwen旁证'])]
    L = ['# GLM-OCR 逐页校对清单（模型初筛）', '',
         f'{time.strftime("%Y-%m-%d")} 由 `vlm_proof.py` 生成（T-093）。主清单是 Qwen3.8-27B-FP8 对照 {DPI}dpi 扫描页逐页找出的 GLM 转写差异；Gemma 4 31B-it（4 比特 QAT）独立再做一遍，作为第二意见。**未经人工核对，不能直接据此改文本。**', '',
         f'- Qwen：{Q["pages"]} 页，保留 {len(I)} 条；丢弃引文在 GLM 原文里找不到的 {Q["dropped_unlocatable"]} 条、去掉空白和 LaTeX 记号后两边相同的 {Q["dropped_same"]} 条。',
         f'- Gemma 噪声大（同口径丢弃 {G["dropped_same"]} 条只差空白的，{len(G["failed_pages"])} 页输出不是合法 JSON），所以只用来：标出 Qwen 的哪些条目它也报了（{len(agree)} 条）；补充它独有、且 `dsa_raw.md` 和 Qwen 转写都旁证的条目（{len(extra)} 条）。',
         '- 旁证：`dsa_raw.md` 全书和 Qwen 转写同页里能否找到“原图”文字。页码是 PDF 页码，书上页码 = PDF 页码 − 14。',
         '- 建议核对顺序：两个模型一致 → 两份旁证 → 一份旁证 → Gemma 补充 → 无旁证。无旁证的不能整体丢掉：公式和上标常常三份 OCR 错得一样。', '',
         '<!-- 抽查 -->', '',
         f'## 一、两个模型一致（{len(agree)} 条）', ''] + hd + [row(i) for i in agree] + ['']
    for k, (t, f) in enumerate(tiers):
        S = [i for i in I if not i['gemma同意'] and f(i)]
        L += [f'## {"二三四"[k]}、只有 Qwen 报，{t}（{len(S)} 条）', ''] + hd + [row(i) for i in S] + ['']
        if k == 1:
            L += [f'## 补充、只有 Gemma 报，两份旁证（{len(extra)} 条）', ''] + hd + [row(i) for i in extra] + ['']
    L += [f'## 附、疑似字形误报（{len(gly)} 条）', '', '只差 `{`/`}`/`[`/`]` 与 `|`、或 `l` 与 `1`。扫描件代码字体里这几对形近，模型常常错得一样，代码里原书多半是 `{`、`}` 和变量 `l`。', ''] + hd + [row(i) for i in gly] + ['']
    open(out_md, 'w', encoding='utf8').write('\n'.join(L))
    print('Qwen', len(I), '两模型一致', len(agree), 'Gemma 补充', len(extra), '字形', len(gly))

if __name__ == '__main__':
    if sys.argv[1] == '--report': report(*sys.argv[2:5])
    elif sys.argv[1] == '--merge': merge(*sys.argv[2:7])
    else: run([int(x) for x in sys.argv[1].split(',')], sys.argv[2], [int(x) for x in sys.argv[3].split('-')] if len(sys.argv) > 3 else None)
