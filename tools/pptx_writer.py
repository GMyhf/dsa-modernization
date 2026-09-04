#!/usr/bin/env python3
"""pptx_writer.py — 一个只用标准库写 .pptx 的最小 OOXML 打包器。

**为什么不用 python-pptx**：`tools/` 与 `code/` 零第三方依赖是这个仓库的架构约束
（见 CLAUDE.md）。闸门要在任何一台机器上跑绿，多一个 pip 包就多一处「在我这儿是好的」。
而 .pptx 说到底是一个 zip 里装着几份 XML——`zipfile` + 字符串模板就够，
这和仓库里手写 Markdown 渲染器、手写 C++ 词法着色是同一个取舍。

本模块**只管打包，不管排版**：调用方给出「第几页、放哪些形状、形状在哪个位置」，
它负责生成 PowerPoint 和 LibreOffice 都认的那一堆 XML 部件与关系。排版在 `build_pptx.py`。

坐标单位是 EMU（English Metric Unit）：1 英寸 = 914400 EMU，1 磅 = 12700 EMU。
字号单位是百分之一磅：`sz="1800"` 是 18pt。

已验证：LibreOffice 24.x 可转 PDF；python-pptx 可解析（自测里用 zipfile 直接核 XML，
不引入依赖）。
"""
import zipfile
from xml.sax.saxutils import escape

EMU_PER_INCH = 914400
EMU_PER_POINT = 12700

# 16:9，13.333in × 7.5in——投影仪与笔记本的默认比例。
SLIDE_W = 12192000
SLIDE_H = 6858000
NOTES_W = 6858000
NOTES_H = 9144000

NS = ('xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main" '
      'xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships" '
      'xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main"')
REL = "http://schemas.openxmlformats.org/officeDocument/2006/relationships"
PKG_REL = "http://schemas.openxmlformats.org/package/2006/relationships"

LATIN = "Segoe UI"
EAST = "Noto Sans CJK SC"
MONO = "Noto Sans Mono CJK SC"

# 配色跟网页版课件同源（build_slides.py 的 STYLE）：一份课件两种介质，颜色不该各说各话。
COLORS = {
    "fg": "1A1A18", "muted": "6B6B66", "line": "DCDCD6", "accent": "8A4B2A",
    "codebg": "F5F4F0", "kw": "8A4B2A", "ty": "2C6B57", "st": "7A5C1E",
    "cm": "8A8A84", "nu": "4A5AA8", "bg": "FDFDFC",
}


def esc(text):
    return escape(str(text))


# --------------------------------------------------------------------- 文本

class Run:
    """一段字体一致的文字。`cls` 用来上代码高亮色（k/t/s/c/n）。"""

    __slots__ = ("text", "bold", "italic", "mono", "size", "color", "link")

    def __init__(self, text, bold=False, italic=False, mono=False, size=None,
                 color=None, link=None):
        self.text = text
        self.bold = bold
        self.italic = italic
        self.mono = mono
        self.size = size
        self.color = color
        self.link = link

    def xml(self, default_size, default_color, rel_id=None):
        size = self.size or default_size
        color = self.color or default_color
        latin = MONO if self.mono else LATIN
        east = MONO if self.mono else EAST
        attrs = f' sz="{size}" dirty="0"'
        if self.bold:
            attrs += ' b="1"'
        if self.italic:
            attrs += ' i="1"'
        link = f'<a:hlinkClick r:id="{rel_id}"/>' if rel_id else ""
        return (f'<a:r><a:rPr lang="zh-CN"{attrs}>'
                f'<a:solidFill><a:srgbClr val="{color}"/></a:solidFill>'
                f'<a:latin typeface="{latin}"/><a:ea typeface="{east}"/>'
                f'<a:cs typeface="{latin}"/>{link}</a:rPr>'
                f'<a:t>{esc(self.text)}</a:t></a:r>')


class Para:
    """一个段落。`bullet` 是项目符号字符，`indent` 是层级（0 起）。"""

    def __init__(self, runs, size=1800, color=None, bullet=None, indent=0,
                 align=None, space_before=0, line=100000):
        self.runs = runs
        self.size = size
        self.color = color or COLORS["fg"]
        self.bullet = bullet
        self.indent = indent
        self.align = align
        self.space_before = space_before
        self.line = line          # 行距，百分数 × 1000

    def xml(self):
        margin = 0 if self.bullet is None else 270000 + self.indent * 270000
        hang = -230000 if self.bullet is not None else 0
        bits = [f'marL="{margin}" indent="{hang}" lvl="{min(self.indent, 8)}"']
        if self.align:
            bits.append(f'algn="{self.align}"')
        props = [f'<a:lnSpc><a:spcPct val="{self.line}"/></a:lnSpc>']
        if self.space_before:
            props.append(f'<a:spcBef><a:spcPts val="{self.space_before}"/></a:spcBef>')
        if self.bullet is None:
            props.append("<a:buNone/>")
        else:
            props.append(f'<a:buClr><a:srgbClr val="{COLORS["accent"]}"/></a:buClr>'
                         f'<a:buFont typeface="Arial"/><a:buChar char="{esc(self.bullet)}"/>')
        runs = "".join(r.xml(self.size, self.color) for r in self.runs)
        if not runs:
            runs = (f'<a:endParaRPr lang="zh-CN" sz="{self.size}"/>')
        return f'<a:p><a:pPr {" ".join(bits)}>{"".join(props)}</a:pPr>{runs}</a:p>'


# --------------------------------------------------------------------- 形状

class Shape:
    """所有形状的共同部分：一个编号、一个名字、一个矩形。"""

    def __init__(self, x, y, cx, cy, name="shape"):
        self.x, self.y, self.cx, self.cy = int(x), int(y), int(cx), int(cy)
        self.name = name

    def xml(self, sid):
        raise NotImplementedError


class TextBox(Shape):
    def __init__(self, x, y, cx, cy, paras, name="文本", fill=None, line=None,
                 inset=0, anchor=None, radius=False):
        super().__init__(x, y, cx, cy, name)
        self.paras = paras
        self.fill = fill
        self.line = line
        self.inset = inset
        self.anchor = anchor
        self.radius = radius

    def xml(self, sid):
        fill = (f'<a:solidFill><a:srgbClr val="{self.fill}"/></a:solidFill>'
                if self.fill else "<a:noFill/>")
        line = (f'<a:ln w="9525"><a:solidFill><a:srgbClr val="{self.line}"/></a:solidFill></a:ln>'
                if self.line else "")
        geom = "roundRect" if self.radius else "rect"
        avlst = '<a:avLst><a:gd name="adj" fmla="val 6000"/></a:avLst>' if self.radius else "<a:avLst/>"
        anchor = f' anchor="{self.anchor}"' if self.anchor else ""
        body = "".join(p.xml() for p in self.paras) or Para([]).xml()
        return (f'<p:sp><p:nvSpPr><p:cNvPr id="{sid}" name="{esc(self.name)}"/>'
                f'<p:cNvSpPr txBox="1"/><p:nvPr/></p:nvSpPr>'
                f'<p:spPr><a:xfrm><a:off x="{self.x}" y="{self.y}"/>'
                f'<a:ext cx="{self.cx}" cy="{self.cy}"/></a:xfrm>'
                f'<a:prstGeom prst="{geom}">{avlst}</a:prstGeom>{fill}{line}</p:spPr>'
                f'<p:txBody><a:bodyPr wrap="square" lIns="{self.inset}" tIns="{self.inset}" '
                f'rIns="{self.inset}" bIns="{self.inset}"{anchor}><a:noAutofit/></a:bodyPr>'
                f'<a:lstStyle/>{body}</p:txBody></p:sp>')


class Rect(Shape):
    """纯色块，用来画标题下的那条线。"""

    def __init__(self, x, y, cx, cy, color, name="线"):
        super().__init__(x, y, cx, cy, name)
        self.color = color

    def xml(self, sid):
        return (f'<p:sp><p:nvSpPr><p:cNvPr id="{sid}" name="{esc(self.name)}"/>'
                f'<p:cNvSpPr/><p:nvPr/></p:nvSpPr>'
                f'<p:spPr><a:xfrm><a:off x="{self.x}" y="{self.y}"/>'
                f'<a:ext cx="{self.cx}" cy="{self.cy}"/></a:xfrm>'
                f'<a:prstGeom prst="rect"><a:avLst/></a:prstGeom>'
                f'<a:solidFill><a:srgbClr val="{self.color}"/></a:solidFill>'
                f'<a:ln><a:noFill/></a:ln></p:spPr>'
                f'<p:txBody><a:bodyPr/><a:lstStyle/><a:p/></p:txBody></p:sp>')


class Picture(Shape):
    def __init__(self, x, y, cx, cy, rel_id, name="插图"):
        super().__init__(x, y, cx, cy, name)
        self.rel_id = rel_id

    def xml(self, sid):
        return (f'<p:pic><p:nvPicPr><p:cNvPr id="{sid}" name="{esc(self.name)}"/>'
                f'<p:cNvPicPr><a:picLocks noChangeAspect="1"/></p:cNvPicPr><p:nvPr/></p:nvPicPr>'
                f'<p:blipFill><a:blip r:embed="{self.rel_id}"/><a:stretch><a:fillRect/></a:stretch></p:blipFill>'
                f'<p:spPr><a:xfrm><a:off x="{self.x}" y="{self.y}"/>'
                f'<a:ext cx="{self.cx}" cy="{self.cy}"/></a:xfrm>'
                f'<a:prstGeom prst="rect"><a:avLst/></a:prstGeom></p:spPr></p:pic>')


class Table(Shape):
    """真表格（a:tbl），不是画出来的格子——这样在 PowerPoint 里还能编辑。"""

    def __init__(self, x, y, widths, rows, row_heights, size=1400, name="表"):
        super().__init__(x, y, sum(widths), sum(row_heights), name)
        self.widths = [int(w) for w in widths]
        self.rows = rows                  # [[[Run, ...], ...], ...]，第一行是表头
        self.row_heights = [int(h) for h in row_heights]
        self.size = size

    def cell(self, runs, header, align):
        fill = COLORS["codebg"] if header else "FFFFFF"
        para = Para(runs, size=self.size, align=align,
                    color=COLORS["fg"], line=100000)
        border = (f'<a:lnL w="9525"><a:solidFill><a:srgbClr val="{COLORS["line"]}"/></a:solidFill></a:lnL>'
                  f'<a:lnR w="9525"><a:solidFill><a:srgbClr val="{COLORS["line"]}"/></a:solidFill></a:lnR>'
                  f'<a:lnT w="9525"><a:solidFill><a:srgbClr val="{COLORS["line"]}"/></a:solidFill></a:lnT>'
                  f'<a:lnB w="9525"><a:solidFill><a:srgbClr val="{COLORS["line"]}"/></a:solidFill></a:lnB>')
        return (f'<a:tc><a:txBody><a:bodyPr/><a:lstStyle/>{para.xml()}</a:txBody>'
                f'<a:tcPr marL="45720" marR="45720" marT="27432" marB="27432" anchor="ctr">'
                f'{border}<a:solidFill><a:srgbClr val="{fill}"/></a:solidFill></a:tcPr></a:tc>')

    def xml(self, sid):
        grid = "".join(f'<a:gridCol w="{w}"/>' for w in self.widths)
        body = []
        for index, row in enumerate(self.rows):
            cells = "".join(self.cell(runs, index == 0, align)
                            for runs, align in row)
            body.append(f'<a:tr h="{self.row_heights[index]}">{cells}</a:tr>')
        return (f'<p:graphicFrame><p:nvGraphicFramePr>'
                f'<p:cNvPr id="{sid}" name="{esc(self.name)}"/>'
                f'<p:cNvGraphicFramePr><a:graphicFrameLocks noGrp="1"/></p:cNvGraphicFramePr>'
                f'<p:nvPr/></p:nvGraphicFramePr>'
                f'<p:xfrm><a:off x="{self.x}" y="{self.y}"/>'
                f'<a:ext cx="{self.cx}" cy="{self.cy}"/></p:xfrm>'
                f'<a:graphic><a:graphicData uri="http://schemas.openxmlformats.org/drawingml/2006/table">'
                f'<a:tbl><a:tblPr firstRow="1"/><a:tblGrid>{grid}</a:tblGrid>'
                f'{"".join(body)}</a:tbl></a:graphicData></a:graphic></p:graphicFrame>')


class Slide:
    def __init__(self, shapes=None, notes=None):
        self.shapes = shapes or []
        self.notes = notes or []          # [str]
        self.images = []                  # [(rel_id, 包内路径)]

    def add(self, shape):
        self.shapes.append(shape)
        return shape


# --------------------------------------------------------------------- 打包

def _sptree(inner):
    return ('<p:cSld><p:spTree>'
            '<p:nvGrpSpPr><p:cNvPr id="1" name=""/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr>'
            '<p:grpSpPr><a:xfrm><a:off x="0" y="0"/><a:ext cx="0" cy="0"/>'
            '<a:chOff x="0" y="0"/><a:chExt cx="0" cy="0"/></a:xfrm></p:grpSpPr>'
            f'{inner}</p:spTree></p:cSld>')


CLRMAP = ('<p:clrMap bg1="lt1" tx1="dk1" bg2="lt2" tx2="dk2" accent1="accent1" '
          'accent2="accent2" accent3="accent3" accent4="accent4" accent5="accent5" '
          'accent6="accent6" hlink="hlink" folHlink="folHlink"/>')

_FILL = '<a:solidFill><a:schemeClr val="phClr"/></a:solidFill>'
_LN = ('<a:ln w="6350" cap="flat" cmpd="sng" algn="ctr">'
       '<a:solidFill><a:schemeClr val="phClr"/></a:solidFill>'
       '<a:prstDash val="solid"/></a:ln>')
_FX = "<a:effectStyle><a:effectLst/></a:effectStyle>"


def _theme():
    pairs = [("dk1", "sysClr", 'val="windowText" lastClr="000000"'),
             ("lt1", "sysClr", 'val="window" lastClr="FFFFFF"'),
             ("dk2", "srgbClr", f'val="{COLORS["fg"]}"'),
             ("lt2", "srgbClr", f'val="{COLORS["bg"]}"'),
             ("accent1", "srgbClr", f'val="{COLORS["accent"]}"'),
             ("accent2", "srgbClr", f'val="{COLORS["ty"]}"'),
             ("accent3", "srgbClr", f'val="{COLORS["st"]}"'),
             ("accent4", "srgbClr", f'val="{COLORS["nu"]}"'),
             ("accent5", "srgbClr", f'val="{COLORS["muted"]}"'),
             ("accent6", "srgbClr", f'val="{COLORS["line"]}"'),
             ("hlink", "srgbClr", f'val="{COLORS["accent"]}"'),
             ("folHlink", "srgbClr", f'val="{COLORS["muted"]}"')]
    scheme = "".join(f'<a:{n}><a:{tag} {attr}/></a:{n}>' for n, tag, attr in pairs)
    return ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n'
            '<a:theme xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main" '
            'name="dsa">'
            f'<a:themeElements><a:clrScheme name="dsa">{scheme}</a:clrScheme>'
            '<a:fontScheme name="dsa">'
            f'<a:majorFont><a:latin typeface="{LATIN}"/><a:ea typeface="{EAST}"/>'
            '<a:cs typeface=""/></a:majorFont>'
            f'<a:minorFont><a:latin typeface="{LATIN}"/><a:ea typeface="{EAST}"/>'
            '<a:cs typeface=""/></a:minorFont></a:fontScheme>'
            '<a:fmtScheme name="dsa">'
            f'<a:fillStyleLst>{_FILL}{_FILL}{_FILL}</a:fillStyleLst>'
            f'<a:lnStyleLst>{_LN}{_LN}{_LN}</a:lnStyleLst>'
            f'<a:effectStyleLst>{_FX}{_FX}{_FX}</a:effectStyleLst>'
            f'<a:bgFillStyleLst>{_FILL}{_FILL}{_FILL}</a:bgFillStyleLst>'
            '</a:fmtScheme></a:themeElements></a:theme>')


def _rels(items):
    body = "".join(f'<Relationship Id="{rid}" Type="{typ}" Target="{target}"/>'
                   for rid, typ, target in items)
    return ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n'
            f'<Relationships xmlns="{PKG_REL}">{body}</Relationships>')


def write(path, slides, title="课件", images=None):
    """把若干 Slide 写成一个 .pptx。

    `images` 是 {包内文件名: bytes}；`Picture` 的 rel_id 由本函数按 slide.images 分配。
    """
    images = images or {}
    parts = {}
    overrides = []

    parts["ppt/theme/theme1.xml"] = _theme()
    parts["ppt/slideMasters/slideMaster1.xml"] = (
        '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n'
        f'<p:sldMaster {NS}>{_sptree("")}{CLRMAP}'
        '<p:sldLayoutIdLst><p:sldLayoutId id="2147483649" r:id="rId1"/></p:sldLayoutIdLst>'
        '</p:sldMaster>')
    parts["ppt/slideMasters/_rels/slideMaster1.xml.rels"] = _rels([
        ("rId1", f"{REL}/slideLayout", "../slideLayouts/slideLayout1.xml"),
        ("rId2", f"{REL}/theme", "../theme/theme1.xml")])
    parts["ppt/slideLayouts/slideLayout1.xml"] = (
        '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n'
        f'<p:sldLayout {NS} type="blank" preserve="1">{_sptree("")}</p:sldLayout>')
    parts["ppt/slideLayouts/_rels/slideLayout1.xml.rels"] = _rels([
        ("rId1", f"{REL}/slideMaster", "../slideMasters/slideMaster1.xml")])
    parts["ppt/notesMasters/notesMaster1.xml"] = (
        '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n'
        f'<p:notesMaster {NS}>{_sptree("")}{CLRMAP}</p:notesMaster>')
    parts["ppt/notesMasters/_rels/notesMaster1.xml.rels"] = _rels([
        ("rId1", f"{REL}/theme", "../theme/theme1.xml")])

    for name, blob in images.items():
        parts[f"ppt/media/{name}"] = blob

    sld_ids = []
    for number, slide in enumerate(slides, 1):
        shapes = []
        sid = 2
        for shape in slide.shapes:
            shapes.append(shape.xml(sid))
            sid += 1
        parts[f"ppt/slides/slide{number}.xml"] = (
            '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n'
            f'<p:sld {NS}>{_sptree("".join(shapes))}</p:sld>')
        rels = [("rId1", f"{REL}/slideLayout", "../slideLayouts/slideLayout1.xml")]
        if slide.notes:
            rels.append((f"rId2", f"{REL}/notesSlide", f"../notesSlides/notesSlide{number}.xml"))
        for rid, media in slide.images:
            rels.append((rid, f"{REL}/image", f"../media/{media}"))
        parts[f"ppt/slides/_rels/slide{number}.xml.rels"] = _rels(rels)
        overrides.append((f"/ppt/slides/slide{number}.xml",
                          "application/vnd.openxmlformats-officedocument.presentationml.slide+xml"))
        if slide.notes:
            paras = "".join(Para([Run(line)], size=1200).xml() for line in slide.notes)
            body = ('<p:sp><p:nvSpPr><p:cNvPr id="2" name="备注"/>'
                    '<p:cNvSpPr><a:spLocks noGrp="1"/></p:cNvSpPr>'
                    '<p:nvPr><p:ph type="body" idx="1"/></p:nvPr></p:nvSpPr>'
                    '<p:spPr><a:xfrm><a:off x="457200" y="3886200"/>'
                    '<a:ext cx="5943600" cy="4400550"/></a:xfrm>'
                    '<a:prstGeom prst="rect"><a:avLst/></a:prstGeom></p:spPr>'
                    f'<p:txBody><a:bodyPr/><a:lstStyle/>{paras}</p:txBody></p:sp>')
            parts[f"ppt/notesSlides/notesSlide{number}.xml"] = (
                '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n'
                f'<p:notes {NS}>{_sptree(body)}</p:notes>')
            parts[f"ppt/notesSlides/_rels/notesSlide{number}.xml.rels"] = _rels([
                ("rId1", f"{REL}/notesMaster", "../notesMasters/notesMaster1.xml"),
                ("rId2", f"{REL}/slide", f"../slides/slide{number}.xml")])
            overrides.append((f"/ppt/notesSlides/notesSlide{number}.xml",
                              "application/vnd.openxmlformats-officedocument.presentationml.notesSlide+xml"))
        sld_ids.append(number)

    pres_rels = [("rId1", f"{REL}/slideMaster", "slideMasters/slideMaster1.xml")]
    slide_ids = []
    for index, number in enumerate(sld_ids):
        rid = f"rId{index + 2}"
        pres_rels.append((rid, f"{REL}/slide", f"slides/slide{number}.xml"))
        slide_ids.append(f'<p:sldId id="{256 + index}" r:id="{rid}"/>')
    tail = len(sld_ids) + 2
    pres_rels.append((f"rId{tail}", f"{REL}/notesMaster", "notesMasters/notesMaster1.xml"))
    pres_rels.append((f"rId{tail + 1}", f"{REL}/theme", "theme/theme1.xml"))

    parts["ppt/presentation.xml"] = (
        '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n'
        f'<p:presentation {NS} saveSubsetFonts="1">'
        '<p:sldMasterIdLst><p:sldMasterId id="2147483648" r:id="rId1"/></p:sldMasterIdLst>'
        f'<p:notesMasterIdLst><p:notesMasterId r:id="rId{tail}"/></p:notesMasterIdLst>'
        f'<p:sldIdLst>{"".join(slide_ids)}</p:sldIdLst>'
        f'<p:sldSz cx="{SLIDE_W}" cy="{SLIDE_H}"/>'
        f'<p:notesSz cx="{NOTES_W}" cy="{NOTES_H}"/></p:presentation>')
    parts["ppt/_rels/presentation.xml.rels"] = _rels(pres_rels)

    parts["docProps/core.xml"] = (
        '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n'
        '<cp:coreProperties '
        'xmlns:cp="http://schemas.openxmlformats.org/package/2006/metadata/core-properties" '
        'xmlns:dc="http://purl.org/dc/elements/1.1/" '
        'xmlns:dcterms="http://purl.org/dc/terms/" '
        'xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">'
        f'<dc:title>{esc(title)}</dc:title>'
        '<dc:creator>dsa-modernization</dc:creator>'
        '<cp:revision>1</cp:revision></cp:coreProperties>')
    parts["docProps/app.xml"] = (
        '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n'
        '<Properties xmlns="http://schemas.openxmlformats.org/officeDocument/2006/extended-properties" '
        'xmlns:vt="http://schemas.openxmlformats.org/officeDocument/2006/docPropsVTypes">'
        '<Application>dsa-modernization/tools/build_pptx.py</Application>'
        f'<Slides>{len(sld_ids)}</Slides></Properties>')

    fixed = [
        ("/ppt/presentation.xml",
         "application/vnd.openxmlformats-officedocument.presentationml.presentation.main+xml"),
        ("/ppt/slideMasters/slideMaster1.xml",
         "application/vnd.openxmlformats-officedocument.presentationml.slideMaster+xml"),
        ("/ppt/slideLayouts/slideLayout1.xml",
         "application/vnd.openxmlformats-officedocument.presentationml.slideLayout+xml"),
        ("/ppt/notesMasters/notesMaster1.xml",
         "application/vnd.openxmlformats-officedocument.presentationml.notesMaster+xml"),
        ("/ppt/theme/theme1.xml", "application/vnd.openxmlformats-officedocument.theme+xml"),
        ("/docProps/core.xml", "application/vnd.openxmlformats-package.core-properties+xml"),
        ("/docProps/app.xml",
         "application/vnd.openxmlformats-officedocument.extended-properties+xml"),
    ]
    body = "".join(f'<Override PartName="{name}" ContentType="{ctype}"/>'
                   for name, ctype in fixed + overrides)
    parts["[Content_Types].xml"] = (
        '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n'
        '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
        '<Default Extension="rels" '
        'ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
        '<Default Extension="xml" ContentType="application/xml"/>'
        '<Default Extension="png" ContentType="image/png"/>'
        '<Default Extension="jpg" ContentType="image/jpeg"/>'
        '<Default Extension="jpeg" ContentType="image/jpeg"/>'
        f'{body}</Types>')
    parts["_rels/.rels"] = _rels([
        ("rId1", f"{REL}/officeDocument", "ppt/presentation.xml"),
        ("rId2", f"{PKG_REL}/metadata/core-properties", "docProps/core.xml"),
        ("rId3", f"{REL}/extended-properties", "docProps/app.xml")])

    # 时间戳固定，输出才可复算——否则同样的源文件每次构建都得到不同的字节。
    stamp = (2026, 1, 1, 0, 0, 0)
    path.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as pack:
        for name in sorted(parts):
            info = zipfile.ZipInfo(name, date_time=stamp)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o600 << 16
            blob = parts[name]
            pack.writestr(info, blob.encode("utf-8") if isinstance(blob, str) else blob)
    return path
