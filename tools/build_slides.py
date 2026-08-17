#!/usr/bin/env python3
"""build_slides.py — 把 `book/slides/*.md` 渲染成可放映的网页课件。

**为什么是 Markdown 而不是 .pptx**：课件要改、要 review、要和书稿一起演进。
二进制文件在 git 里只能看到「已变更」三个字——2026-08-16 把 19 份旧课件抽成文本
入库，就是因为这个。所以这里 Markdown 是唯一事实源，HTML 是产物，
和 `build_site.py`（书稿网页版）、`build_book_pdf.py`（学生 PDF）同一套路。

**幻灯片上的代码由闸门逐字核对**：`book/slides/*.md` 落在 `book/` 下，
`check_doc.py` 的 `BOOK.rglob("*.md")` 自动收编，于是 R3 照样生效——
课件里印的 C++ 必须来自 `code/` 下真编译真跑过的文件。讲课时投在屏幕上的代码
和学生 clone 下来跑的代码是同一份，这一点不靠自觉。

写法（`book/slides/示例.md` 有完整样例）：

    ---
    title: 第3章 栈与队列
    ---

    # 这一页的标题

    - 要点一
    - 要点二

    ```cpp file=code/ch03/array_stack/teaching.hpp#push
    ```

    <!-- 备注
    讲课时说的话。放映时按 N 显示，投影上看不见。
    -->

    ---

    # 下一页

用法:
  python3 tools/build_slides.py            # 渲染到 book/slides/site/
  python3 tools/build_slides.py --check    # 只校验产物是否最新（闸门用）
"""
import argparse
import html
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import build_site  # noqa: E402
from build_site import Context, render_blocks, render_inline  # noqa: E402
from repo import ROOT, rel_label  # noqa: E402

SLIDES = ROOT / "book" / "slides"
SITE = SLIDES / "site"
# 课件页住在 book/slides/site/，插图住在 book/assets/。不复制第二份，往上退两级。
ASSETS_HREF = "../../assets/"

DECK_TITLE = "现代 C++ 数据结构教程 · 课件"

# 分页符：整行只有三个减号。**必须在代码围栏之外**——`---` 在 C++ 注释里
# 完全可能出现，切错了会把一页代码劈成两半。
SEPARATOR = re.compile(r"^---\s*$")
FENCE = re.compile(r"^\s*```")
# 演讲者备注：`<!-- 备注 ... -->`。用 HTML 注释是为了在任何 Markdown 阅读器里都隐身，
# 这样课件源文件直接读也不会被讲稿打断。
NOTE_BLOCK = re.compile(r"<!--\s*备注\s*(.*?)-->", re.S)

# 单页超过这么多行就警告（不挡构建）。
#
# 数字是量出来的：正文字号 clamp(15px,2.5vmin,28px)、代码 clamp(11px,1.95vmin,21px)，
# 1080p 投影上代码约 21px × 1.5 行距 = 31px 一行；一页正文区约 890px，
# 于是**28 行左右就满了**，再多就要滚动——讲课时滚动很糟。
# 阈值放宽到 32 是给「整页只有一段代码」留的余地：那种页滚一下还能接受，
# 而要点页超过 32 行基本是没拆干净。
MAX_SLIDE_LINES = 32


def split_front_matter(text):
    """开头那段 `---` 包起来的元信息。没有就返回空。"""
    lines = text.splitlines()
    if not lines or not SEPARATOR.match(lines[0]):
        return {}, text
    for index in range(1, len(lines)):
        if SEPARATOR.match(lines[index]):
            meta = {}
            for line in lines[1:index]:
                key, _, value = line.partition(":")
                if value.strip():
                    meta[key.strip()] = value.strip()
            return meta, "\n".join(lines[index + 1:])
    return {}, text


def split_slides(text):
    """按 `---` 切页，但**不切代码围栏里的**。返回每页的行列表。"""
    slides, current, in_fence = [], [], False
    for line in text.splitlines():
        if FENCE.match(line):
            in_fence = not in_fence
            current.append(line)
            continue
        if not in_fence and SEPARATOR.match(line):
            slides.append(current)
            current = []
            continue
        current.append(line)
    slides.append(current)
    # 丢掉全空的页：文件首尾和连着两个 `---` 都会产生空页
    return [s for s in slides if any(line.strip() for line in s)]


def take_notes(lines):
    """摘出这一页的演讲者备注，返回 (去掉备注的行, 备注段落列表)。"""
    body = "\n".join(lines)
    notes = [m.strip() for m in NOTE_BLOCK.findall(body) if m.strip()]
    body = NOTE_BLOCK.sub("", body)
    return body.splitlines(), notes


def render_deck(path: Path, ctx):
    """一份课件 → (元信息, [(标题, 正文 HTML, [备注段落])])。"""
    meta, text = split_front_matter(path.read_text(encoding="utf-8"))
    # 课件源文件写 `../assets/`——R4 按 Markdown 自身所在目录查文件，
    # 而课件比书稿深一层。渲染时归一成 `assets/`，交给 build_site 的图片分支
    # （它按 ASSETS_HREF 加前缀，上面已经改成 `../../assets/`）。
    text = text.replace("](../assets/", "](assets/")
    slides = []
    for raw in split_slides(text):
        lines, notes = take_notes(raw)
        anchors = build_site.Anchors() if hasattr(build_site, "Anchors") else _Anchors()
        body, headings = render_blocks(lines, ctx, anchors)
        title = headings[0][1] if headings else ""
        visible = [ln for ln in lines if ln.strip()]
        if len(visible) > MAX_SLIDE_LINES and hasattr(ctx, "crowded"):
            ctx.crowded.append((path.name, title or "（无标题）", len(visible)))
        note_html = "".join(f"<p>{render_inline(n, ctx)}</p>" for n in notes)
        slides.append((title, "".join(body), note_html))
    return meta, slides


class _Anchors:
    """`render_blocks` 要一个能发锚点的对象。课件不需要页内锚点，给个最简的。"""

    def __init__(self):
        self.seen = {}

    def take(self, text):
        base = build_site.slugify(text) or "s"
        self.seen[base] = self.seen.get(base, 0) + 1
        return base if self.seen[base] == 1 else f"{base}-{self.seen[base]}"


STYLE = """
:root{--bg:#fdfdfc;--fg:#1a1a18;--muted:#6b6b66;--line:#dcdcd6;--accent:#8a4b2a;
--code-bg:#f5f4f0;--kw:#8a4b2a;--ty:#2c6b57;--st:#7a5c1e;--cm:#8a8a84;--nu:#4a5aa8}
:root[data-theme="dark"]{--bg:#16161a;--fg:#ececea;--muted:#a0a09a;--line:#33333a;
--accent:#e0a074;--code-bg:#1e1e24;--kw:#e0a074;--ty:#7fd0b0;--st:#d8c088;--cm:#7a7a84;--nu:#9ab0f0}
*{box-sizing:border-box}
html,body{margin:0;padding:0;height:100%;background:var(--bg);color:var(--fg)}
body{font-family:-apple-system,"Segoe UI","Noto Sans CJK SC","Source Han Sans SC",
"Microsoft YaHei",sans-serif;overflow:hidden}
.deck{position:relative;height:100vh}
.slide{position:absolute;inset:0;display:none;flex-direction:column;
padding:4.2vmin 6vmin 9vmin;overflow:auto}
.slide.current{display:flex}
.slide .anchor{display:none}
.slide h1{font-size:clamp(26px,4.4vmin,52px);margin:0 0 3vmin;line-height:1.25;
letter-spacing:-.01em;border-bottom:3px solid var(--accent);padding-bottom:1.6vmin}
.slide h2{font-size:clamp(20px,3.1vmin,34px);margin:2.4vmin 0 1.2vmin}
.slide h3{font-size:clamp(17px,2.5vmin,27px);margin:2vmin 0 1vmin;color:var(--muted)}
.slide p,.slide li{font-size:clamp(15px,2.5vmin,28px);line-height:1.55;margin:.7vmin 0}
.slide ul,.slide ol{margin:1vmin 0;padding-left:3.4vmin}
.slide li{margin:.9vmin 0}
.slide strong{color:var(--accent)}
.slide blockquote{margin:1.6vmin 0;padding:.6vmin 0 .6vmin 2.4vmin;
border-left:4px solid var(--line);color:var(--muted)}
.slide table{border-collapse:collapse;font-size:clamp(13px,2.1vmin,23px);margin:1.4vmin 0}
.slide th,.slide td{border:1px solid var(--line);padding:.7vmin 1.4vmin;text-align:left}
.slide th{background:var(--code-bg)}
.table-wrap{overflow-x:auto;max-width:100%}
.slide img{max-width:100%;max-height:52vh;object-fit:contain;display:block;margin:1.4vmin auto}
.codeblock{background:var(--code-bg);border:1px solid var(--line);border-radius:6px;
margin:1.4vmin 0;overflow:hidden}
.codeblock pre{margin:0;padding:1.6vmin 2vmin;overflow-x:auto}
.codeblock code{font-family:"SF Mono",Menlo,Consolas,"Noto Sans Mono CJK SC",monospace;
font-size:clamp(11px,1.95vmin,21px);line-height:1.5;white-space:pre}
.srcbar{font-size:clamp(9px,1.3vmin,13px);padding:.5vmin 2vmin;color:var(--muted);
border-bottom:1px solid var(--line);font-family:"SF Mono",Menlo,monospace}
.srcbar a{color:var(--muted);text-decoration:none}
.k{color:var(--kw)}.t{color:var(--ty)}.s{color:var(--st)}.c{color:var(--cm);font-style:italic}
.n{color:var(--nu)}.p{color:var(--fg)}
.math{font-style:italic}.math-display{text-align:center;margin:2vmin 0;
font-size:clamp(16px,2.8vmin,30px)}
.upr{font-style:normal}.bold{font-weight:700}
code{background:var(--code-bg);padding:.1em .34em;border-radius:4px;
font-family:"SF Mono",Menlo,Consolas,monospace;font-size:.86em}
hr{border:0;border-top:1px solid var(--line);margin:2vmin 0}
.chrome{position:fixed;left:0;right:0;bottom:0;height:5.4vmin;min-height:26px;
display:flex;align-items:center;gap:1.6vmin;padding:0 2vmin;
border-top:1px solid var(--line);background:var(--bg);font-size:clamp(10px,1.6vmin,15px);
color:var(--muted)}
.chrome .grow{flex:1}
.chrome b{color:var(--fg);font-weight:600}
.bar{position:fixed;left:0;bottom:0;height:3px;background:var(--accent);
transition:width .18s ease;z-index:5}
.notes{position:fixed;left:0;right:0;bottom:5.4vmin;max-height:34vh;overflow:auto;
padding:1.6vmin 2.4vmin;background:var(--code-bg);border-top:2px solid var(--accent);
font-size:clamp(13px,2vmin,20px);line-height:1.5;display:none}
.notes.on{display:block}
.notes p{margin:.5em 0}
.notes:before{content:"演讲者备注";display:block;font-size:.72em;letter-spacing:.08em;
color:var(--accent);margin-bottom:.5em}
.overview{position:fixed;inset:0;background:var(--bg);overflow:auto;padding:3vmin;
display:none;grid-template-columns:repeat(auto-fill,minmax(190px,1fr));gap:1.6vmin;z-index:20}
.overview.on{display:grid}
.thumb{border:2px solid var(--line);border-radius:6px;padding:1.2vmin;cursor:pointer;
font-size:13px;line-height:1.35;min-height:88px;overflow:hidden;background:var(--bg)}
.thumb:hover{border-color:var(--accent)}
.thumb.here{border-color:var(--accent);box-shadow:0 0 0 2px var(--accent)}
.thumb .no{color:var(--muted);font-size:11px}
.help{position:fixed;inset:0;background:rgba(0,0,0,.62);display:none;z-index:30;
align-items:center;justify-content:center}
.help.on{display:flex}
.help div{background:var(--bg);padding:3vmin 4vmin;border-radius:10px;max-width:520px;
font-size:15px;line-height:1.9}
.help kbd{background:var(--code-bg);border:1px solid var(--line);border-radius:4px;
padding:.1em .5em;font-family:monospace;margin-right:.6em}
@media print{
  html,body{height:auto;overflow:visible}
  .chrome,.bar,.notes,.overview,.help{display:none!important}
  .deck{height:auto}
  .slide{display:flex!important;position:relative;inset:auto;page-break-after:always;
  height:100vh;border:0}
}
"""

SCRIPT = """
(function(){
  var slides=[].slice.call(document.querySelectorAll('.slide'));
  var total=slides.length, at=0;
  var bar=document.querySelector('.bar'), now=document.getElementById('now');
  var notes=document.querySelector('.notes'), over=document.querySelector('.overview');
  var help=document.querySelector('.help');
  function show(n,push){
    at=Math.max(0,Math.min(total-1,n));
    slides.forEach(function(s,i){s.classList.toggle('current',i===at);});
    bar.style.width=((at+1)/total*100)+'%';
    now.textContent=(at+1)+' / '+total;
    notes.innerHTML=slides[at].getAttribute('data-notes')||'';
    document.querySelectorAll('.thumb').forEach(function(t,i){
      t.classList.toggle('here',i===at);});
    if(push!==false){history.replaceState(null,'','#'+(at+1));}
    slides[at].scrollTop=0;
  }
  function go(d){show(at+d);}
  document.addEventListener('keydown',function(e){
    if(e.metaKey||e.ctrlKey||e.altKey)return;
    var k=e.key;
    if(help.classList.contains('on')&&k!=='?'){help.classList.remove('on');return;}
    if(over.classList.contains('on')&&(k==='Escape'||k==='o'||k==='O')){
      over.classList.remove('on');return;}
    if(k==='ArrowRight'||k==='PageDown'||k===' '||k==='j'){go(1);e.preventDefault();}
    else if(k==='ArrowLeft'||k==='PageUp'||k==='k'){go(-1);e.preventDefault();}
    else if(k==='Home'){show(0);}
    else if(k==='End'){show(total-1);}
    else if(k==='n'||k==='N'){notes.classList.toggle('on');}
    else if(k==='o'||k==='O'){over.classList.toggle('on');}
    else if(k==='d'||k==='D'){
      var dark=document.documentElement.getAttribute('data-theme')==='dark';
      document.documentElement.setAttribute('data-theme',dark?'light':'dark');}
    else if(k==='f'||k==='F'){
      if(document.fullscreenElement){document.exitFullscreen();}
      else{document.documentElement.requestFullscreen();}}
    else if(k==='?'){help.classList.toggle('on');}
  });
  document.querySelectorAll('.thumb').forEach(function(t,i){
    t.addEventListener('click',function(){over.classList.remove('on');show(i);});});
  window.addEventListener('hashchange',function(){
    var n=parseInt(location.hash.slice(1),10);
    if(n>=1&&n<=total)show(n-1,false);});
  var start=parseInt(location.hash.slice(1),10);
  show(start>=1&&start<=total?start-1:0,false);
})();
"""

HELP_HTML = """<div>
<p><b>放映快捷键</b></p>
<p><kbd>→ / 空格</kbd>下一页　<kbd>←</kbd>上一页　<kbd>Home / End</kbd>首尾</p>
<p><kbd>N</kbd>演讲者备注　<kbd>O</kbd>缩略图总览　<kbd>F</kbd>全屏　<kbd>D</kbd>深色</p>
<p><kbd>?</kbd>这张帮助　<kbd>Esc</kbd>关闭</p>
<p style="color:var(--muted);font-size:.9em">
导出 PDF：浏览器打印（Ctrl/Cmd+P），每页一张幻灯片。</p>
</div>"""


def page_html(title, subtitle, slides, deck_links):
    parts = []
    for index, (heading, body, notes) in enumerate(slides):
        note_attr = html.escape(notes, quote=True)
        parts.append(f'<section class="slide" data-notes="{note_attr}">{body}</section>')
    thumbs = "".join(
        f'<div class="thumb"><div class="no">{n + 1}</div>{html.escape(h or "（无标题）")}</div>'
        for n, (h, _, _) in enumerate(slides)
    )
    return f"""<!doctype html>
<html lang="zh-CN"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>{html.escape(title)}</title>
<style>{STYLE}</style></head><body>
<div class="deck">{"".join(parts)}</div>
<div class="bar"></div>
<div class="notes"></div>
<div class="overview">{thumbs}</div>
<div class="help">{HELP_HTML}</div>
<div class="chrome">
<b>{html.escape(title)}</b><span>{html.escape(subtitle)}</span>
<span class="grow"></span>
{deck_links}
<span>按 <b>?</b> 看快捷键</span><b id="now">1 / {len(slides)}</b>
</div>
<script>{SCRIPT}</script>
</body></html>
"""


INDEX_STYLE = """
body{overflow:auto;font-family:-apple-system,"Segoe UI","Noto Sans CJK SC",sans-serif;
max-width:760px;margin:0 auto;padding:48px 24px;line-height:1.7}
h1{font-size:30px;margin:0 0 6px}
p.sub{color:var(--muted);margin:0 0 32px}
a.deck-card{display:block;padding:14px 18px;margin:8px 0;border:1px solid var(--line);
border-radius:8px;text-decoration:none;color:var(--fg)}
a.deck-card:hover{border-color:var(--accent)}
a.deck-card b{display:block;font-size:18px}
a.deck-card span{color:var(--muted);font-size:14px}
"""


def index_html(decks):
    rows = "".join(
        f'<a class="deck-card" href="{html.escape(name, quote=True)}"><b>{html.escape(title)}</b>'
        f"<span>{count} 页</span></a>"
        for name, title, count in decks
    )
    return f"""<!doctype html>
<html lang="zh-CN"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>{html.escape(DECK_TITLE)}</title>
<style>{STYLE}{INDEX_STYLE}</style></head><body>
<h1>{html.escape(DECK_TITLE)}</h1>
<p class="sub">由 <code>book/slides/*.md</code> 渲染。幻灯片上的每一段 C++
都来自 <code>code/</code> 下真编译真跑过的文件，由闸门的 R3 逐字核对。</p>
{rows}
</body></html>
"""


def build(check_only=False, out_dir=None, assets_href=None):
    if not SLIDES.is_dir():
        print("⚠️  book/slides/ 还不存在")
        return 0
    sources = sorted(p for p in SLIDES.glob("*.md") if p.name != "README.md")
    if not sources:
        print("⚠️  book/slides/ 下没有课件源文件")
        return 0

    # 图片前缀要按课件页的深度改。build_site 用模块级常量表示它，这里临时换掉。
    saved = build_site.ASSETS_HREF
    build_site.ASSETS_HREF = assets_href or ASSETS_HREF
    try:
        ctx = Context()
        ctx.crowded = []   # build_site 的 Context 没有这一项，课件自己加
        decks, pages = [], {}
        for src in sources:
            ctx.page = src.name
            meta, slides = render_deck(src, ctx)
            title = meta.get("title") or src.stem
            subtitle = meta.get("subtitle", "现代 C++ 数据结构教程")
            out_name = src.stem + ".html"
            links = '<a href="index.html" style="color:var(--muted)">目录</a>'
            pages[out_name] = page_html(title, subtitle, slides, links)
            decks.append((out_name, title, len(slides)))
        pages["index.html"] = index_html(decks)
    finally:
        build_site.ASSETS_HREF = saved

    if ctx.crowded:
        for deck, title, count in ctx.crowded:
            print(f"⚠️  {deck} 的「{title}」有 {count} 行，投影上放不下"
                  f"（超过 {MAX_SLIDE_LINES} 行就会滚动）")
    if ctx.unknown_tex:
        print(f"⚠️  {len(ctx.unknown_tex)} 个没认出来的 LaTeX 命令："
              f"{', '.join(sorted(ctx.unknown_tex)[:6])}")
    if ctx.missing_assets:
        for page, src in ctx.missing_assets:
            print(f"❌ {page}: 插图 {src} 不存在")
        return 1

    site = Path(out_dir) if out_dir else SITE
    if check_only:
        stale = [
            name for name, text in pages.items()
            if not (site / name).is_file()
            or (site / name).read_text(encoding="utf-8") != text
        ]
        extra = [p.name for p in site.glob("*.html")] if site.is_dir() else []
        extra = [n for n in extra if n not in pages]
        if stale or extra:
            print("❌ book/slides/site/ 与课件源文件已脱节，需要重新构建："
                  + ", ".join(sorted(stale + extra)))
            print("  修法：python3 tools/build_slides.py")
            return 1
        print(f"✅ book/slides/site/ 与 book/slides/*.md 一致（{len(pages)} 个页面）")
        return 0

    site.mkdir(parents=True, exist_ok=True)
    for name, text in pages.items():
        (site / name).write_text(text, encoding="utf-8")
    for leftover in site.glob("*.html"):
        if leftover.name not in pages:
            leftover.unlink()
    total = sum(count for _, _, count in decks)
    print(f"✅ {rel_label(site)}  {len(decks)} 份课件、{total} 页  "
          f"入口 {rel_label(site / 'index.html')}")
    return 0


def main():
    parser = argparse.ArgumentParser(description="把 book/slides/*.md 渲染成网页课件")
    parser.add_argument("--check", action="store_true", help="只校验产物是否最新")
    parser.add_argument("--out", help="输出目录（默认 book/slides/site/）")
    parser.add_argument("--assets-href", help="插图前缀（发布到别处时用，默认 ../../assets/）")
    opts = parser.parse_args()
    return build(check_only=opts.check, out_dir=opts.out, assets_href=opts.assets_href)


if __name__ == "__main__":
    sys.exit(main())
