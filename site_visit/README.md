# site_visit · 2025 秋期末机考的「考场可用资料」

来源：<http://dsa.openjudge.cn/2025final/> 的考场说明。那一页列了四个文件，**考场里可以使用**：

| 考场说明里的文件 | 本目录 | 入库 |
| --- | --- | --- |
| `DSCode_ZhangWangZhao2008_06.zip`（本书作者的 C++ 代码包） | `DSCode_ZWZ200806_CPP/` | 可读源文件入库 |
| `DScodeCversion.rar`（C 语言代码包） | `DScode_ZWZ_C/` | 入库 |
| `201810cplusplus.zip`（cppreference 离线手册，69MB） | `201810cplusplus/` | **不入库** |
| `DSAlgoWeissMark.pdf`（Weiss 英文教材） | `DSAlgoWeissMark.pdf` | **不入库**（有版权） |

不入库的规则写在根目录 `.gitignore`。作者包里的 `a.out` 等 23 个 ELF 可执行文件、VC6 工程文件
（`.dsp`/`.dsw`）、编辑器备份（`*~`）和空输出文件同样不入库，与
`ref_数据结构与算法A 2021秋/SourceCodes/` 的收录口径一致。

## 作者 C++ 代码包与仓库里已有那份是同一套代码

`ref_数据结构与算法A 2021秋/SourceCodes/` 早就在仓库里（第 11、12 章几个单元的 `legacy.md` 引用过它）。
2026-09-18 按 sha256 逐文件比对：去掉编译产物和工程文件后，**两份源文件只有一个不同**——

```text
ch07_Graph/Graph_Dijkstra/Graph_matrix.h（考场版） vs chap7_Graph/Graph_Dijkstra/Graph_matrix.h（2021 版）
10c10
< 	Edge(){};                       // 2021 版：to 未初始化
> 	Edge(){to=-1;};                 // 考场版
107c107
< 		//  myEdge.to = -1;
> 		myEdge.to = -1;
```

考场版修掉了一处「成员未初始化」。以考场版为准：它更新，也是学生考试时手里那一份。

## 编码

作者包是 **GBK + CRLF**（2008 年 VC6 的默认），直接 `cat` 中文注释会乱码：

```bash
iconv -f GBK -t UTF-8 site_visit/DSCode_ZWZ200806_CPP/ch03_StackQueue/alg3.5/arrStack.h | tr -d '\r'
```

## 它在本项目里的地位

**它不是「原书印了什么」的凭据**——那是扫描件（`tools/pdfref.py`），`dsa_raw.md` 是扫描件的 OCR。
代码包是作者**印刷前**的工程版本，和印出来的清单会有出入（例：代码3.2 印的是与成员变量 `top`
同名的 `top(T&)`，编译不过；代码包里叫 `getTop(T*)`，编译得过）。所以它的用处是**第二证人**：
帮我们分辨一处错误是作者写错、排印时引入，还是 OCR 造成的。
