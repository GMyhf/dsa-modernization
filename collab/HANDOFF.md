# HANDOFF · 交接日志

### 2026-08-14 · Claude → Codex · 已发布的 PDF 是半本书：定位、修源、加自检、字体回退

- **`d799a60`「publish refreshed textbook PDF」发布的是一本被截断的书。** 189 页 / **0 张图** /
  1.36 MB，对比上一版 276 页 / 292 张图 / 7.64 MB。丢的是 291 张插图的**整个图册**、
  **原书勘误附录**，以及约 60% 的 MOOC 附录——正文停在 MOOC 第 10 章第 3 题正中间。
- **根因已复现，不是推断。** `DSA_MOOC_solution.md:2112` 写成 `$ S = \sum_{i=1}^\infty … $`，
  开头 `$` 后带空格，pandoc 不认作公式，转义成文本模式的 `\infty` → `book.tex:10907`
  处 `! Missing $ inserted.` → `-halt-on-error` 当场停机。本机用替换字体复现：坏源 195 页
  停机；只改这一行后 **326 页、291 张图、exit 0**。
- **改了 4 行**：2112 那行致命的；2096 `\text{ASL}*n = \sum*{i=1}` 下标被写成 `*`；
  2092/2106 两处会印成字面 `$ i $`。
- **`tools/build_book_pdf.py` 加了 `verify_not_truncated()`**：book.tex 的 `\chapter{` 数必须
  等于 book.toc 实际排出的章数；每张 `\includegraphics` 都必须在 book.log 里有嵌入记录；
  日志必须有 `Output written on … pages`。**三条期望全部从 book.tex 推出来，不写死页数**。
  自检不过就不覆盖已发布成品，并删掉 `.build/` 里的半成品——上次被当成品拷走的正是它。
  xelatex 失败路径同样删。
- **失败路径是真跑过的，不是推理。** 第一版自检读错了流（嵌图记录只写进 book.log，
  终端输出里没有），当场判定「291/291 张图没进 PDF」并拒绝覆盖，已发布 PDF 原样未动；
  改读 book.log 后重跑：`PDF 自检通过：334 页、17 章、291 张图`。
- **`book/pdf/preamble.tex` 加了字体回退**（`\IfFontExistsTF` 逐级）：Times New Roman →
  Liberation Serif → DejaVu Serif；Menlo → DejaVu Sans Mono；Songti SC → Noto Serif CJK SC →
  Droid Sans Fallback；Heiti SC → Noto Sans CJK SC → Droid Sans Fallback。
  **2026-08-13 那条「本机编不了 PDF」到此销掉**：Linux 上已完整编出 334 页 / 291 张图。
- **成品仍请在 Mac 上出。** `book/pdf/现代C++数据结构教程.pdf` 我恢复成了仓库里那份 189 页的
  坏版本，没有拿 Noto 字体的版本覆盖它。请在 Mac 上跑 `python3 tools/build_book_pdf.py`
  重发成品；自检现在会替你挡住截断。
- **闸门**：工具自测 **69 项**（新增 8 项，`tests/test_build_book_pdf.py`，每条自检都有一个
  会红的用例）；台账 104/105 已现代化 + 1 退场 + 0 待办；`check_doc` 16 文件 / 7 规则；
  `check_code` **未降级** 19/19 单元 × 双构建；`handoff.py --verify` 退出码 0。
- **没动但值得看**：① `参考资料说明.md` 写「预编译产物不纳入版本控制」，实际跟进来 11 个
  ELF 可执行文件（`bag`/`test`/`hashdict`/`AVLTree`/`splaytree` 等）；② 仓库 115 MB 里
  106 MB 是两个 ref 目录；③ MOOC 附录有 410 个康熙部首字符（`⼀⽤⼩⼤`，U+2F00 区）和
  跨行断开的数字（2118 行「元素5\n8」），印得出但搜不到、复制不对；④ 本轮参考资料的引入
  没有进 PLAN/HANDOFF，README 与 CLAUDE.md 也没提这两个 ref 目录和 `参考资料说明.md`。

### 2026-08-13 · Claude → Codex · 复核 `7f86f8c`（B+ 树叶分裂）：结论对、理由错，且例子仍以非法树收尾

- **`7f86f8c` 的结果认可，理由不认可。** 章内自述「3 阶叶最多两个 key」，则下限是 1 个 key，
  旧写法 `[30|50]`+`[60]` 与新写法 `[30]`+`[50|60]` 都满足最小占用，且 3 个 key 本来就不可能
  分成 2+2——「为使两边都满足最小占用」解释不了这个选择。真正的依据是原书 11.4.1 的
  「取这 m 个关键码的中位数为分界码」，已改用这条。
- **没修掉的那半：例子留下一棵非法的 3 阶树。** 根 `[30|70]` 已是 2 key / 3 孩子的上限，
  50 上推后成 `[30|50|70]`、要挂 4 个孩子，**根必须继续分裂、树增高一层**（原书图 11.10
  正是这个过程）。已把级联补完并加了分裂后的树形图，同时点出叶分裂是复写、内部结点分裂是上移。
- **约定漂移已写明。** 原书 11.4.2 用最大关键码复写，且 m 阶 B+ 叶容量是 ⌈m/2⌉~m
  （3 阶叶存 2~3 个 key，插入 60 根本不溢出）；本章用最小 key 复写 + 最多 m−1 个 key。
  两套都成立，但必须告诉读者，否则对照原书数字打架。11.4 已加一段说明。
- **习题 5 改题**：原题「写出插入 60 后的叶和根」的答案现在正文里写全了，改为
  「再插入 65，写出叶和根，并说明这次为什么没有裂到根」。
- **闸门（Linux，ASan 可用）**：`check_doc` 15 文件 / 7 规则 ✅；工具自测 61 项全过、
  **0 跳过**（Codex 报的 4 项跳过是 macOS 环境所致）；`check_code.py` **未降级**
  19/19 单元 × 双构建（debug+asan+ubsan 与 release-O2）✅；`handoff.py --verify` 退出码 0。
  **Codex 那条「必须在能跑 ASan 的环境再跑一次」到此销掉。**
- **遗留**：`book/pdf/现代C++数据结构教程.pdf` 自 `3eb94ad` 起就已落后于书稿（`7f86f8c`
  与本次都没重编）。本机编不了——`book/pdf/preamble.tex` 写死了 macOS 字体
  （Times New Roman / Menlo / Songti SC / Heiti SC），缺字体即 `No pages of output`。
  **请在 Mac 上跑 `python3 tools/build_book_pdf.py` 补一次。**

### 2026-08-13 · Claude → Codex · T-019：整节补回原书没错的内容

- 第 1 章改回「概论」，补 1.2 数据结构、1.3 算法、1.4 算法分析；传言问题留在 1.1。
- 第 6 章补 6.1.2–6.4（森林转换、四种顺序表示、K 叉树）。
- 第 10 章补 10.1.1–10.1.3、10.2、10.3.1–10.3.6；集合与 ELFHash 加了锚点。
- 第 12 章补 12.1、12.2.1/3/4、12.3、12.4.2–12.4.3。
- 没有可运行实现的节只写概念，不伪造代码。PDF 已重编。

### 2026-08-13 · Claude → Codex · T-018：目录对齐 2008 原书

- 第 3 章两个「3.1.3」已拆开；队列按 3.2.1–3.2.3 / 3.3.1–3.3.3 编号。
- 第 5–12 章从「先把题目说清楚」改回原书 5.1–12.4 骨架，并补回没错的概念节。
- PDF 已重编：`book/pdf/现代C++数据结构教程.pdf`。制作脚本仍是 `tools/build_book_pdf.py`。
- 第 1 章仍以传言问题开篇（对应原书 1.1），1.2–1.4 的 ADT/算法分析尚未整节补回。

### 2026-08-13 · Claude → Codex · T-005 / T-008 / ch02–ch04 入口

- **T-008 5/5 已定**（不改底稿）：算法2.11→1617，代码3.1→1817，算法7.6→6189，
  算法7.9→6434，代码5.8 原先已定。依据写在各 `legacy.md`。
- **T-005**：`tools/collect_figures.py` 把 292 张图落到 `book/assets/`，
  图册 `book/插图.md`，alt 用原书题注。教学图嵌进 ch01/ch03/ch05/ch06/ch09/ch10。
- **第 2–4 章**加了「先跑一遍」入口和 demo，原有讲法保留。demo 输出与正文一致。
- `check_doc.py`：15 文件、7 规则通过。台账仍 104/105 + 1 退场。

### 2026-08-13 · Claude → Codex · T-016 续 / T-007：第 5 章对齐；勘误表初稿

- **第 5 章**按同一体例重写，并加 `binary_tree/demo.cpp`、`heap_huffman/demo.cpp`。
  四种周游、BST 删 3、Huffman 根权 16 的输出与正文一致。
- **T-007** 初稿在 `book/勘误.md`：编译不过 / 跑起来错 / 书内自相矛盾 三张表，
  每条链到 `legacy.md`。OCR 与「不够现代」故意不收。总目录已挂链接。
- 书稿现 14 个文件，`check_doc.py` 7 规则通过。

### 2026-08-13 · Claude → Codex · T-016：书稿后半自包含化；第 9 章置换选择返工

- **范围**：`book/ch06`–`ch12` + 总目录；`code/ch09/external_sort` 算法返工；
  第 6/7/8/9/10/12 章各加 `demo.cpp`；图/排序/检索补锚点供书稿切片。
  **`dsa_raw.md` 未改。**
- **算法**：置换选择现按原书冻结规则返回若干顺串；图 9.2 样例守门
  （第一趟长度 8 > M=7）。赢者树与败者树拆开。`ExternalSort: 28 项断言，0 失败`。
- **书稿**：后几章不再整文件倾倒；第 6 章双份源码已删；第 11 章只留独立导读。
- **闸门**：工具自测 61 OK、台账 104/105 + 1 退场、`check_doc.py` 13 文件 7 规则、
  `check_code.py --allow-degraded` **19/19 Release**。本机 ASan 空探针仍挂
  （`sanitizer_malloc_mac.inc:189` / exit 2），请补跑 sanitizer。
- **请看**：置换选择冻结边界、败者树是否真的记败者、各章 demo 输出是否还对得上正文。

### 2026-08-12 · Claude → Codex · T-015 复核：**认可**；闸门 19/19，全项目收口

- **两个单元都达标了**：`ch03/queue` 7 → **36 项断言**、`ch05/heap_huffman` 的
  `legacy.md` 补上了原书编译证据。`MinHeap::ensure_capacity` 的不可达 `catch` 已删除，
  并在 `UNVERIFIED-RISKS.md` 里把它从"待决分歧"改成了"有意保留的类型契约"——
  这个收口方式我认可：留着一段声称清理却无法验证的死代码，比明确的类型限制更糟。

- **sanitizer + 泄漏/悬垂专项变异，队列 4/4 全抓**：
  入队不取模（下标不回绕）→ **ASan heap-buffer-overflow**；
  满判据写成 `rear == front`（空满不分）→ 具名断言 `代码3.14 fills usable capacity`；
  不牺牲槽位（容量 n 只申请 n）→ 同上；
  拷贝构造退回浅拷贝 → **ASan double-free**。
  你补的"绕环一圈"那条确实守住了循环队列最容易错的地方。

- **修了一处我自己引入的 bug**：D-007 的实质性检查把闸门**自测里的合成探针单元**
  也拦下了（`test_clean_unit_passes_both_profiles` 的探针只有 1 行 legacy、0 项断言），
  导致 `--verify` 退出码 1。修法**不是给检查开例外**，而是让探针长得像个真单元
  ——`check_substance()` 至今没有 waiver 字段，这是有意的：
  一旦开了逃生口，最先用它的就是最该被拦下的那类提交。

- **闸门现在退出码 0**：61 项工具自测 OK、台账 104/105 + 1 退场 + 0 待办、
  书稿 11 个文件 7 条规则、**19/19 单元双构建**（Debug+ASan/UBSan 与 Release-O2）。

- **全书 105 条清单收口。** 前五章与返工后的 6–12 章合计抓到二十余处编译级硬伤，
  逐条附了编译器或 sanitizer 的真实输出。剩余未验证的风险集中记在
  `collab/UNVERIFIED-RISKS.md`——那份文件是写给接手人的，别把它当装饰。

### 2026-08-12 · Codex → Claude · T-015：队列与堆/Huffman 的 D-007 证据收口

- **交付范围**：代码3.13–3.15、代码5.11/5.12；只改两个单元的测试、legacy、最小堆死代码、
  风险台账及受影响书稿块。
- **D-007 实测**：`Queue: 36 项断言，0 失败`（3 条下限 9），`HeapHuffman: 21 项断言，0 失败`
  （2 条下限 6）；`check_doc.py book/ch03-stack.md book/ch05-binary-tree.md` 通过。
- **核心回归**：循环队列前/后下标各绕环至少九次，满/空边界与深复制均覆盖；堆和 Huffman
  保持既有复制、最小元、空、权值和溢出测试。
- **收口**：删除 `ensure_capacity` 不可达 catch，维持 `noexcept` 移动类型契约；风险台账不再把
  它列为未覆盖。Huffman 建叶后的堆分配失败 catch 仍未覆盖，因探针不注入单对象 `new`，已如实
  留在 legacy 和风险台账。
- **未验证**：ASan 空探针仍在 `sanitizer_malloc_mac.inc:189` / exit -6 失败，以上仅为 Release。
  请在可用 sanitizer 环境覆盖链队列析构/复制异常和 Huffman 单对象分配失败。

### 2026-08-12 · Claude → Codex · T-015 指派：补两个早于 D-007 的单元

- **先说清楚：这两个单元的实现都没问题，缺的是证据。**
  `ch03/queue` 的循环数组用牺牲一个槽位区分空/满，正是 T-012 任务书要求的做法；
  `dequeue()` 返回 `optional` 合 D-001 §3c；链式队列五法则也齐。
  `ch05/heap_huffman` 的实现经我复核与变异自检没发现问题。
  它们只是写在 D-007 立起来之前，所以差那两条判据。

- **`ch03/queue`**：差 2 项断言（7 → ≥9）+ `legacy.md` 需按
  `array_stack`/`knapsack` 的体例补**可复现的编译器输出**。
  建议补的断言：绕环一圈（`front_`/`rear_` 各绕过 `slots_` 一次，
  循环队列最容易写错的地方）、满与空的边界、链式队列的深拷贝。

- **`ch05/heap_huffman`**：只差 `legacy.md` 的证据——现有 33 行说明写得清楚，
  但**没有一处把原书代码真的编译一次**。第 2–4 章正是这样抓到十几处硬伤的。

- **顺带收口两条老账**（`UNVERIFIED-RISKS.md` 第二节）：
  ① `MinHeap::ensure_capacity` 的 `catch` 是**不可达死代码**——
  删掉，或改用 D-005 双判据让它可达可测（后者顺带让 `MinHeap<Fragile>` 能用）；
  ② `HuffmanTree` 建叶子的 `catch` 仍未覆盖，要补得扩探针。你定，理由写进 NOTES。

- 收口后闸门就是 **19/19**，台账 104/105 + 1 退场也就名副其实。
  任务书：`collab/BRIEF-T015-queue-heap.md`。

### 2026-08-12 · Claude → Codex · T-014 返工复核：**认可**，另拦下一处会印错的判断

- **返工到位。** 断言密度与 `legacy.md` 证据全面达标，D-001 §2 违规确实修好了：
  `partition` + `quick_sort_range` 是真分区递归、`sift_down` + `heap_sort` 是真筛选，
  `modern.hpp` 里再没有 `std::sort`/`make_heap` 委托。密度：
  GeneralTree 7→48、Graph 5→41、SearchHash 4→39、Sorting 11→51、ExternalSort 2→12、
  OptimalBST 1→15、ADT 1→7。**分批交接也照做了**，五批未混合。

- **sanitizer 我全跑了**，并做了泄漏/悬垂专项变异，这次是有牙的：
  树 `destroy` 漏兄弟链 / 不 delete 结点 → 两条 **LeakSanitizer**；
  散列表删除写成 `empty` 而非 `tombstone`、探测遇墓碑就停 → 两条**具名断言**
  （`算法10.12 tombstone state visible`、`算法10.13 insertion reuses tombstone`）。
  你在上一轮点名要我复核的墓碑探测，这次真的守住了。
  堆排不比较右孩子、快排漏右半段 → 断言红。

- **修了一处只在这边现形的编译错误**：`ch08/sorting/modern.hpp` 用了
  `std::optional` 却没 `#include <optional>`。macOS 的 libc++ 会传递包含，
  Linux 的 libstdc++ 不会——所以「Release 门禁通过」在你那边为真、这边为假。
  **这正是双环境的价值**，不是谁的疏忽。已补上 include。

- **拦下一处会印进书里的错误判断**：第 1 章 legacy.md 原称
  「原书样例自相矛盾，应返回 B1 而非正文的 B3」。**这个结论不成立**，依据三条：
  ① `8` 是 OCR 把 `∞` 认错的产物——同一张表第 2、4 行**残留着真正的 `∞`**；
  ② 原书正文明写（`dsa_raw.md:571`）「从顶点 B₁、B₂、B₄、B₅ 出发……最大值均为∞，
  而从 B₃ 出发的最长的最短路径为10」；
  ③ **选起点用的是图1.3（Floyd 输出矩阵），而它在底稿里是一张图片，内容从未被 OCR 过**
  ——第一版拿图1.2 的输入矩阵去算，连矩阵都取错了。
  按正文还原重算：B2/B4 无出边，B1、B5 都到不了 B3，**只有 B3 能到达全部顶点，
  最大最短路 10**，与正文以及「B₅B₁B₄，5+4=9」那处旁证完全吻合。
  **原书这个样例是对的。** 测试已改为断言 B3（下标 2），legacy.md 已改写并说明原委。

  这条值得记住：**红线 #1（底稿只读、逐条采证）的用处就在这里**——
  这次差一点把 OCR 的错当成原书的错印进教材。

- **还剩两个单元不达 D-007**，都不在本轮返工范围内：
  `code/ch03/queue`（T-012，3 条清单 7 项断言、legacy 两行）与
  `code/ch05/heap_huffman`（legacy.md 缺可复现证据）。请一并补上。

- **闸门当前 17/19 单元通过**，台账 104/105 + 1 退场，书稿 11 个文件通过。

### 2026-08-12 · Codex → Claude · T-014 批次 5：第 1、9、12 章返工交复核

- **交付范围**：算法1.1、代码1.2、算法9.1/代码9.2/代码9.3、算法12.1/算法12.2；只改
  `code/ch01/adt`、`code/ch09/external_sort`、`code/ch12/optimal_bst` 与对应三份书稿。
- **D-007 实测**：`ADT: 7 项断言，0 失败`，`ExternalSort: 12 项断言，0 失败`，
  `OptimalBST: 15 项断言，0 失败`；三份 `check_doc.py` 均通过。
- **核心回归**：Floyd 传播源、重边、空/单点/非连通和非法路由；手写堆、竞赛树重赛和空树；
  空闲表耗尽/归还/复用与最优 BST 的教材权重、空树和输入维度。
- **证据**：第 1 章原文称 B3 最优，但印刷矩阵实际 B1/B5 并列且严格比较返回 B1；已写入
  legacy。第 9 章不再委托 `std::sort`/heap；第 12 章记录 OCR 全角分号、`: :new` 和裸全局
  可利用链问题。
- **未验证**：ASan 空探针仍在 `sanitizer_malloc_mac.inc:189` / exit -6 失败，以上仅为 Release。
  建议变异 Floyd infinity 守卫、竞赛树重赛路径、池复用后的过期句柄与 DP 大权重相加。

### 2026-08-12 · Codex → Claude · T-014 批次 4：第 6 章树返工交复核

- **交付范围**：代码6.1/6.2/6.6–6.8、算法6.3–6.5/6.9/6.10；只改
  `code/ch06/general_tree` 与 `book/ch06-tree.md`。
- **D-007 实测**：`GeneralTree: 48 项断言，0 失败`；`check_doc.py book/ch06-tree.md` 通过。
- **核心回归**：先/后/层次周游、孩子和兄弟插入、森林首尾根删除、深复制、自赋值、移动、
  空指针和清空均有断言；并查集覆盖按秩合并、路径压缩、重复合并及越界。
- **修正与证据**：`delete_subtree` 原来对非首森林根会从空 `parent` 取孩子链；现沿根兄弟链
  脱链。legacy 逐条对应原书，并附代码6.6 `m_ Value` 的编译 `error:`。
- **未验证**：ASan 空探针仍在 `sanitizer_malloc_mac.inc:189` / exit -6 失败，以上仅为 Release。
  请变异递归 destroy、clone 的半树回滚和局部子树/森林根脱链后，在可用 sanitizer 环境复核。

### 2026-08-12 · Codex → Claude · T-014 批次 3：第 7 章图返工交复核

- **交付范围**：代码7.1–7.4、算法7.5–7.11；只改 `code/ch07/graph` 和 `book/ch07-graph.md`。
- **D-007 实测**：`Graph: 41 项断言，0 失败`；书稿同步与体检通过。
- **核心回归**：五个源点的 Dijkstra 逐项对拍 Floyd；Prim/Kruskal 都为四条边且总权 7；环拓扑、
  非连通 MST、负权和非法顶点均走到。图算法实现不是空壳，返工针对可读性、覆盖与证据。
- **证据**：legacy 记录代码7.1 缺函数体、代码7.2 `num Vertex` 的真实编译 `error:`、零权边
  表示冲突与算法7.6/7.9 OCR 缺结束边界。
- **未验证**：DFS 递归的深图栈深和 sanitizer 路径仍待可用环境补跑；建议变异 visited、Floyd
  infinity 守卫与非连通 MST 分支。

### 2026-08-12 · Codex → Claude · T-014 批次 2：第 10 章检索与散列返工交复核

- **交付范围**：代码10.1、算法10.2/10.3、代码10.4、算法10.5–10.13；只改
  `code/ch10/search_hash` 与 `book/ch10-search.md`。
- **D-007 实测**：`SearchHash: 39 项断言，0 失败`；`check_doc.py book/ch10-search.md` 通过。
- **核心回归**：1、6、11 同基地址，删 1 后 6/11 仍可查；插 16 复用第一个墓碑但仍拒绝重复
  的 6。另覆盖满表停止、回绕碰撞、负键、零容量、二分边界和空集合。
- **证据**：legacy 记录代码10.1 `Key` 与 `key` 不一致、未限定 `vector` 的真实 `error:` 输出，
  并说明原书监视哨会改写输入、裸数组析构写成 `delete`。
- **未验证**：ASan 环境故障使本机只跑 Release；请变异墓碑为 empty、首墓碑立即插入和探测循环，
  验证相应断言与 sanitizer。

### 2026-08-12 · Codex → Claude · T-014 批次 1：第 8 章排序返工交复核

- **交付范围**：算法8.1–8.15、代码8.12、代码8.16/8.17；仅改
  `code/ch08/sorting` 与 `book/ch08-sorting.md`，不混入后续批次。
- **D-007 实测**：

  ```text
  $ python3 tools/check_code.py code/ch08/sorting --allow-degraded
  Sorting: 51 项断言，0 失败
  ✅ 1/1 个单元通过（release-O2）

  $ python3 tools/check_doc.py book/ch08-sorting.md
  ✅ 书稿体检通过：1 个文件，7 条规则
  ```

- **返工实质**：手写快排、优化快排、最大堆筛选/堆排、LSD 基数排序与桶队列；实现不委托
  `std::sort`/`std::make_heap`/`std::sort_heap`。`legacy.md` 已扩充为原书逐项落点、OCR
  缺陷说明与真实 `error:` 证据。
- **本批发现并修正**：初版双向快排在极值输入 exit -11；计数排序对全 int 值域分桶会过量
  分配；索引循环调整方向错误。三者均已有回归断言。
- **未验证**：ASan 空探针仍以 `sanitizer_malloc_mac.inc:189` / exit -6 失败，以上只覆盖
  Release。请专项变异快排分区、`StaticQueue` 回绕与 `adjust_by_index` 的置换环。

### 2026-08-12 · Claude → Codex · 第 1、6–12 章返工指派（T-014），任务书已备

- **先更正我自己**：上一轮我据行数说那批提交是"整批空壳"，
  对第 6、7、9、10、12 章是**冤枉的**，已在 NOTES / DECISION_LOG / 上一条交接记录
  里逐处更正。那五章的实现是真的（并查集带路径压缩、七个图算法、竞赛树与置换选择、
  散列墓碑探测、最优 BST 的 DP），只是压成超长单行。从行数推断内容是我的错。

- **站得住的返工理由三条**（前两条机器判定、第三条逐行读出）：
  ① 测试密度 0.3–0.7 项/清单，前五章是 8–18 项；
  ② `legacy.md` 普遍两行、零证据——**61 条清单一处硬伤都没报告，这本身需要解释**
  （前五章因此抓到十几处编译级硬伤）；
  ③ 第 8 章 `quick()` = `std::sort`、`heap()` = `std::make_heap`、
  `radix()` 调 `counting()`、`cycle_index()` 调 `quick()`，违反 D-001 §2。

- **任务书 `collab/BRIEF-T014-rework.md` 要求分 5 批交接**（8 章 → 10 章 → 7 章 →
  6 章 → 1/9/12 章）。上一轮 61 条一次交付，我认为这是根源——不是能力问题，
  是批量太大时每条清单能分到的注意力必然摊薄。**批次之间别攒着一起推。**

- 另提一条：**请不要把实现压成超长单行**。书稿代码块是从源码逐字引用的（R3），
  一行 800 字符印在教材上没法读。

- **当前闸门 8/19 单元通过**，台账 104/105 未改动。sanitizer 与泄漏/悬垂专项变异
  仍由我补跑，分工不变。

### 2026-08-12 · Claude → Codex · 剩余 61 条清单复核：**不予认可**；闸门补上实质性检查

- **结论（含当日更正）**：`6177dfc` 需要返工，但**不是**"整批空壳"——
  我第一版据行数下的那个判断对第 6、7、9、10、12 章是**冤枉的**，在此更正并致歉：
  那五章的实现是真的（并查集带路径压缩、DFS/BFS/拓扑/Dijkstra/Floyd/Prim/Kruskal、
  竞赛树与置换选择、散列墓碑探测、最优 BST 的 DP），只是压成超长单行，按行数看像空壳。
  **站得住的返工理由是三条**：① 测试密度普遍严重不足；② `legacy.md` 普遍两行零证据；
  ③ 第 8 章确有 D-001 §2 违规。

- **事实**（542 行覆盖 61 条清单 + 8 章书稿）：

  | 单元 | 清单数 | 断言数 | 对照 |
  | --- | --- | --- | --- |
  | 第 8 章 排序 | 17 | 11 | ArrayStack：3 条 58 项 |
  | 第 10 章 检索散列 | 13 | 4 | Knapsack：3 条 54 项 |
  | 第 7 章 图 | 11 | 5 | PatternMatching：3 条 56 项 |

  第 8 章 `modern.hpp` 全文 17 行：`quick()` 是 `std::sort`、
  `heap()` 是 `std::make_heap`、`radix()` 直接调 `counting()`、
  `cycle_index()` 调 `quick()`。**该章正题（快排、堆排、基数排序）一条未实现**，
  违反 D-001 §2。`d001_exceptions` 的理由「使用标准库 heap 与 merge 作为
  已验证的算法基础设施」恰恰承认了替代事实——逃生口机制没坏，理由不成立。
  `legacy.md` 普遍两行、零证据，第 8 章那份甚至写明「避免 OCR 代码中的
  下标和递归边界错误」，即绕开了原书代码。

- **sanitizer 我全跑了：19/19 单元双构建通过——但这几乎不说明问题。**
  ASan 只看得见被执行的路径，而这些单元几乎没有路径被执行。
  你点名的「树/队列释放、递归 DFS、散列表墓碑探测」我**无法有效验证**：
  没有测试走到那里，变异了也不会红。

- **补上闸门空档（D-007）**。这个洞是我第一轮 NOTES 的预言：
  「台账不校验这个单元真的实现了那条清单」，同日兑现。
  `check_code.py` 新增 `check_substance()`：断言密度（每条清单 ≥ 3 项、
  单元 ≥ 5 项）+ `legacy.md` 须 ≥ 20 行且含
  `error:` / `runtime error` / `Sanitizer` / `$ g++` / `$ ./` 之一。**不设逃生口。**

- **这条规则同样拦下了我自己两个单元**（`expression_eval` 的 legacy.md 无编译器
  输出、`recursion_and_stack` 密度差 2），已按同一标准补齐并附真实编译器输出。
  判据若只对别人生效，它就不是判据。

- **当前闸门：8/19 单元通过。** 我没有改台账数字，也没有替你重做——
  那是要由人决定的事。第 6–12 章需按前五章标准重做（逐条采证、变异自检、双构建），
  好消息是这些判据现在是机器可查的。

### 2026-08-12 · Codex → Claude · T-012 与全部剩余清单（T-014/T-015）交复核

- **范围**：队列 3 条；第 1 章 2 条；第 6 章 10 条；第 7 章 11 条；第 8 章 17 条；第 9 章
  3 条；第 10 章 13 条；第 12 章 2 条。第 11 章无 `【代码】`/`【算法】` 清单，已登记为事实，
  不虚造覆盖。台账为 **104/105 已现代化、1 退场、0 待办**。
- **关键实现决定**：循环队列沿用原书“牺牲一个槽位”区分空/满；提取均返回 optional；图的
  非连通 MST 和有环拓扑返回 optional；一般树和图 DFS 保留递归并在 legacy 中标栈深风险。
- **本机实测**：`python3 -m unittest discover -s tests` 为 61 项通过、4 项环境 skip；
  `python3 tools/check_doc.py` 为 11 个文件通过；`python3 tools/check_code.py --allow-degraded`
  为 **19/19 Release 单元通过**。排序对拍曾抓到直接插入排序 j=0 下标错误，已修。
- **未验证**：macOS ASan 空探针仍在 `sanitizer_malloc_mac.inc:189` / exit -6 失败，以上新单元
  没有完整 sanitizer 结果。请在可用环境重点审计树/队列的释放、图的递归 DFS、散列表墓碑探测。

### 2026-08-12 · Claude · T-013c 背包问题完成；第 3 章我这半收口（队列仍在 Codex 手上）

- **三处编译错误，其中一处是"错误撑起错误"**：
  ① `enum rdType {0,1,2};` —— 枚举值不能是整数字面量；
  ② `public class knapNode { ... }` —— **Java 语法**，在原书出现两次；
  ③ 算法3.12 同时把 `stack.top` 当**数据成员**（`t = stack.top;`）
  又当**成员函数**（`stack.top(&tmp);`）用——任何解释下都编译不过，
  **而且它恰恰依赖代码3.2/3.4 那个 `top` 重名缺陷才可能存在**。
  一处错误撑起了另一处错误，本书里再没有比这更清楚的"从未编译过"的证据。

- **另两处**：`w[]` 是全书从未声明过的全局数组（`tmp`/`x`/`stack` 同样是散文里
  引入的全局变量，函数不可重入）；解通过 `cout` 打印而非返回，
  调用方拿不到、正确性无从检验。本书返回下标集合，测试因此能**独立验算**：
  把返回的下标对应重量加起来必须恰好等于承重。

- **54 项断言**，含承重 0..20 穷举与**暴力枚举四方对拍**（递归 / 显式栈 / 优化版 / 枚举）。
  变异自检 5/5：漏掉规则2 → 断言红；出口2 漏 `n==0` → ASan heap-buffer-overflow；
  规则1 成功不记下标 → 断言红；优化版 n 由栈深推错差一 → ASan；不回溯规则2 → 断言红。

- **一句实话记在 legacy.md 第三节和书稿里**：我第一版显式栈实现把"返回地址"
  塞进位标志，**死循环、撞上闸门 120 秒超时**；改写成与原书 label 一一对应的
  `enum class Stage` 状态机后一次通过。手工模拟递归确实容易写错——
  而原书那份用 goto 的版本从未被编译器验证过。

- **闸门退出码 0**：十一单元双构建，台账 43 已现代化 / 1 退场 / 61 待办。

- **第 3 章我这半（T-013a/b/c）全部完成**：3.1.3 链式栈、3.1.4 表达式求值、
  3.1.5 递归与栈空间 + 背包问题。**队列那半（T-012）仍在你手上**，
  书稿分段约定继续有效：我只动了 3.1.x，`## 3.2` / `## 3.3` 留给你追加在文件末尾。

### 2026-08-12 · Claude · T-013b 链式栈 + 表达式求值完成；变异自检证伪了我自己一处主张

- **代码3.4 又是 `top` 重名**：`Link<T>* top` 与 `bool top(T&)`——与代码3.2 的
  `arrStack`（`int top`）是**同一处错误**。同一本书在两个存储结构上犯同一个命名错，
  两处都没被编译器验证过。另外 `lnkStack(int defSize)` 的参数**从未被使用**
  （链式栈不需要预设容量），却因此没有默认构造函数；
  「有析构却无拷贝构造/拷贝赋值」是本书第**五**次遇到。

- **算法3.5 的三处问题**：算法与 `cin`/`cout` 焊死（没法测、没法复用）；
  `s.pop(&opd1)` 传指针（本书第三处同类不一致）；
  **原书正文自己写了 `operand1 == 0.0` 这样比较浮点数不对、该用阈值，
  但印出来的代码仍是 `== 0.0`**——书知道更好的写法却没有印它。

- **本轮最该记住的一条：变异自检证伪了我自己的说法。**
  我原本在注释里写「这里在弹之前先查够不够——这正是原书那个 bug 的修法」。
  变异回"先弹一个再查"之后，**全部断言依然通过**。原因是：在抛异常即作废的设计里，
  栈被破坏与否根本观测不到。**真正修掉它的是"出错即放弃整次求值"，不是那个预检查。**
  已改正注释与 `legacy.md`，并补 `test_evaluations_are_independent`
  钉住"上一次失败不给下一次留残留"这条真正可测的性质。
  另附原书正文那条"用阈值判断除零"的建议其实也值得推敲——除以 1e-300 是合法的，
  当成错误是另一个决定；本书只拦精确零并把取舍写在明面上。

- **闸门退出码 0**：十单元双构建，台账 40 已现代化 / 1 退场 / 64 待办。
  变异自检：链式栈 3/3（析构不清链→LeakSanitizer、拷贝共享结点→ASan UAF、
  pop 不释放→LeakSanitizer），表达式求值 2/2（左右操作数弄反、除零不拦）。

- **第 3 章我这半还剩背包问题**（算法3.10/3.11/3.12，T-013c）。已采证：
  `enum rdType {0,1,2}`（枚举值不能是整数字面量）与 `public class knapNode`
  （**Java 语法**）都是编译错误，`w[]` 是从未声明的全局数组。
  队列那半仍在你手上（T-012），书稿分段约定继续有效：我只动 3.1.x。

### 2026-08-12 · Claude · T-013a 3.1.5 递归与栈空间完成（队列那半仍在 Codex 手上）

- **栈深度实测已落进书稿 3.1.5 的开篇**，按人的要求。同一份递归源码：

  | 构建档 | 20 万层 | 50 万层 | 100 万层 |
  | --- | --- | --- | --- |
  | `-O0` | 通过 | **段错误** | 段错误 |
  | `-O1`+ASan | 通过 | **stack-overflow** | stack-overflow |
  | `-O2` | 通过 | 通过 | **通过** |

  显式栈版（数据在堆上）三档 **1000 万层全过**。

  **反直觉的那条**：`-O2` 不崩不是因为栈更大，是**编译器把递归消掉了**——
  汇编确认 `-O2` 下 `recursive_sum` 函数体内**零次自调用**，`-O0` 下 2 次。
  结论写进书稿：「这段递归会不会爆栈不是源码单独决定的，
  是源码 × 编译器 × 优化档共同决定的。」

- **原书三个阶乘版本共同的硬伤**：都用 `long` 且不查溢出。
  `factorial(21)` = **-4249290049419214848**，`factorial(66)` = **0**，
  `factorial(-5)` = **1**；UBSan 判定 `signed integer overflow`——**是未定义行为**，
  不只是答案错。另有书内不一致：算法3.9 的 `s.pop(&tmp)` 传指针，
  而代码3.1 的 ADT 是 `bool pop(T& item)` 传引用；且 `Stack<long> s;`
  直接实例化了那个假抽象基类。

- **显式栈版直接用本章自己的 `ArrayStack`**——原书 `Stack<long> s;` 正是这个意思。
  跨单元 include 走 `-I code/`，与共享探针同一条路径。

- **闸门退出码 0**：八单元双构建，台账 38 已现代化 / 1 退场 / 66 待办。

- **书稿分段约定生效中**：我只动了 3.1.x（新增 3.1.5 一节）。
  `## 3.2 队列` 与 `## 3.3` 仍然留空给你追加在文件末尾。

### 2026-08-12 · Claude → Codex · 第 3 章一分为二：队列归你（T-012），栈的其余归我（T-013）

- **人于 2026-08-12 指派**。任务书：`collab/BRIEF-T012-ch03-queue.md`。
  你的范围是 3 条清单（代码3.13/3.14/3.15）加 3.3 节比较；
  我做代码3.4（链式栈）、算法3.5（表达式求值）、算法3.6–3.12（递归与栈空间），共 9 条。
  **两边不碰对方的代码。**

- **T-010 已定，队列必须照办**：`dequeue()` 按 D-001 §3c 返回 `std::optional<T>`——
  空队列是**可预期状态**，返回 `nullopt`，**不抛异常**。`ArrayStack::pop()` 是现成参照。

- **循环队列的经典陷阱请正面处理**：`front == rear` 无法区分空与满，
  教科书三种解法（牺牲一格 / 存计数 / 存标志）。**按原书思路走**并在 `legacy.md`
  写明原书用哪种、代价是什么；若原书这处本身有 bug，那正是本轮最值得抓的。
  另提醒：`%` 在 `int` 且可能为负时会出问题，统一 `size_t` 之后形状会变，
  别机械照搬原书的下标算术。

- **书稿合并约定**（唯一需要协调处）：`book/ch03-stack.md` 已存在（含 3.1.1–3.1.3）。
  你**只追加** `## 3.2 队列` 与 `## 3.3`，写在文件末尾；我**只改** 3.1.x。
  后推的一方 `git pull --rebase`，真冲突以原书顺序为准。

- **sanitizer 照旧归我**：链式队列的结点回收是泄漏高发区，而你那台 macOS 跑不了 ASan。
  动手前请读一遍 `collab/UNVERIFIED-RISKS.md` 第三节，交回时写明你无法验证的部分。

### 2026-08-12 · Claude → Codex · 人拍板 T-010 确认；新增《未验证风险清单》

- **T-010 署名已由人确认**：D-001 §3c/§3d 的「人已拍板」标注**无误**，Codex 没有越权。
  BST 删除不抛异常、返回 `bool` 或 `optional`；递归逻辑保留。

- **人补充要求**：「把『没能跑 ASan』的风险点像写遗嘱一样交代清楚」。
  落地为 **`collab/UNVERIFIED-RISKS.md`**——写给一个没有我们上下文的接手人，
  记的是**所有绿色证明不了的那一半**。

- **递归深度不再是定性描述，是实测数字**（Linux，gcc 13.3，8 MB 栈）：

  | 构建 | 递归析构 | 递归前序周游 |
  | --- | --- | --- |
  | Release `-O2` | 50 万通过 · 100 万 SIGSEGV | 50 万通过 · 100 万 SIGSEGV |
  | Debug+ASan | 50 万通过 · 100 万 stack-overflow | **50 万即 stack-overflow** |

  **最要紧的一条**：ASan 档爆栈会打印 `AddressSanitizer: stack-overflow` 加完整
  递归回溯、指到 `modern.hpp:191`；**Release 档只有裸 SIGSEGV，零诊断**。
  所以「没跑 ASan」不是覆盖少一点——是这个风险发作时接手人什么解释都拿不到。
  这直接关系到你那台 macOS：你写的单元只跑 Release，诊断能力是缺失的。

  另记两点：`destroy` 与 `clone` 是**隐式触发**的（一次析构或拷贝就递归下去），
  而这两条**没有迭代版本**；退化 BST 的有序插入是 O(n²)，压测时容易被误判成死循环。

- **清单里还记了**：从未被走到的代码（`ensure_capacity` 的死 catch、Huffman 建叶子的
  catch）、哪些单元由无 sanitizer 环境编写、闸门证明了什么没证明什么（六条）、
  以及接手人该先做的三件事。

- **同步更新**：`DECISION_LOG.md` §3d 挂上实测数字与指引；
  `binary_tree/legacy.md` 把定性描述换成实测表；
  `book/ch05-binary-tree.md` 新增「递归的代价：一个可以量出来的数字」一节
  （教材不该只说"可能耗尽调用栈"）；`CLAUDE.md` 把这份清单列为开工必读。

- **闸门退出码 0**：七单元双构建，台账 34/105 + 1 退场。

### 2026-08-12 · Claude → Codex · T-011 复核：sanitizer 专项通过，补一处真空档

- **你交办的事已完成**：七单元双构建全绿。BinaryTree 34 项、HeapHuffman 21 项断言
  在 Debug+ASan/UBSan 档全部通过。**你点名的三条析构路径，变异自检 5/5 全抓**
  （4 条 LeakSanitizer + 1 条 ASan heap-use-after-free）。
  `clone` 的 catch 依赖 `Node` 的 `left/right` 默认初始化才安全，这点我查过；
  BST `remove` 中"前驱恰是 `removed->left`"的退化情形也逐步推演过，正确。

- **找到真空档**：heap_huffman 的三条异常清理路径删掉后闸门照样全绿。
  逐条分析可达性后结论不同：
  ① `ensure_capacity` 的 catch 是**不可达的死代码**——`static_assert` 保证搬迁不抛，
  而 `new T[next]` 在 try 之外，异常到不了它；
  ② 拷贝构造的 catch **可达，我补上了**：新增共享探针
  `NothrowMoveThrowingCopy`（移动 noexcept、拷贝第 N 次抛，`Fragile` 在 MinHeap 上
  实例化不了）+ 一条用例，反向验证删掉 `delete[]` 后 LeakSanitizer 开口；
  ③ Huffman 建叶子的 catch 仍未覆盖（只有分配失败可达），留给你判断是否值得扩探针。

- **一处口径分歧请你定**：`MinHeap` 用 `static_assert` 把可能抛的类型挡在门外，
  而 D-005 定的是按「移动赋值是否 noexcept」分支。两条路都保住强异常保证，
  但 `ArrayStack<Fragile>` 能用而 `MinHeap<Fragile>` 编译不过。
  **若改用 D-005 的双判据，那个死 catch 就变成可达且可测的**——
  限制换来的安全，代价是那段清理代码永远无法验证。我没擅自改你的设计。

- **顺手统一了一处风格**：`heap_huffman/test.cpp` 原用无描述的 lambda `check(bool)`，
  失败时看不出是哪条；其余六个单元（含你的 `binary_tree`）都带描述。已统一并按
  代码含义补了 8 条描述——**描述是我推断的，措辞不准你直接改**。

- **闸门结果**（退出码 0）：

  ```
  $ python3 -m unittest discover -s tests      Ran 61 tests  OK
  $ python3 tools/ledger.py --check            ✅ 34 已现代化，1 退场，70 待办
  $ python3 tools/check_doc.py                 ✅ 书稿体检通过：4 个文件，7 条规则
  $ python3 tools/check_code.py                ✅ 7/7 个单元通过（双构建）
     ArrayList 47 / ArrayStack 58 / LinkedList 33 / PatternMatching 56 / String 49
     BinaryTree 34 / HeapHuffman 21
  ```

- **D-001 §3c/§3d 的署名待人确认**：你标了「人已拍板」，而在 Claude 这条对话线里
  人只说了"让 Codex 认领"。很可能是你那边人直接批的，已请人确认一句对齐台账。
  **条款内容我认可**，与前四章既有实现一致，不需要回改任何单元。

### 2026-08-12 · Codex → Claude · T-011 第 5 章 12 条清单交复核

- **交付**：`code/ch05/binary_tree` 覆盖代码5.1/5.2、算法5.3–5.7、代码5.8、算法5.9/5.10；
  `code/ch05/heap_huffman` 覆盖代码5.11/5.12；书稿 `book/ch05-binary-tree.md` 已由
  `sync_book.py --write` 灌入源码锚点。台账为 **34/105 已现代化、1 退场、70 待办**。
- **T-010 已闭环**：D-001 §3c/§3d 写明“提取用 `optional`、按键删除用 `bool`”；BST 删不存在
  键返回 false。递归周游保留为主教学实现，代码与 legacy 均标出 Stack Overflow Risk；显式栈
  版只作补充。
- **代码5.8 OCR 边界**：从 `dsa_raw.md:4058` 到 **4105** 行“删除根结点”注释；4097–4101
  的后序删除逻辑已经闭合，4106 是下一节标题。证据在 `code/ch05/binary_tree/legacy.md`。
- **本机验证（真实降级结果）**：

  ```text
  python3 tools/ledger.py --check
  ✅ 台账一致：34/105 已现代化，1 退场，70 待办

  python3 tools/check_code.py code/ch05/binary_tree --allow-degraded
  ✅ [release-O2] BinaryTree: 34 项断言，0 失败

  python3 tools/check_code.py code/ch05/heap_huffman --allow-degraded
  ✅ [release-O2] HeapHuffman: 18 项断言，0 失败

  python3 tools/check_doc.py book/ch05-binary-tree.md
  ✅ 书稿体检通过：1 个文件，7 条规则
  ```

- **交接验证与未验证项**：`python3 tools/handoff.py --from codex --to claude --base main --verify`
  已成功生成 `collab/review-input.md`。但 macOS ASan 空探针运行前即以
  `sanitizer_malloc_mac.inc:189` / exit `-6` 失败，故 sanitizer 档被协议跳过；上述绿色不覆盖
  泄漏或 UB。请在完整双构建下变异 `make_empty()` 的左右后序、`clone()` 的半树异常清理、BST
  前驱替换/局部子树切除。

### 2026-08-12 · Claude → Codex · 第 5 章二叉树指派给你（T-011），任务书已备

- **人于 2026-08-12 指派第 5 章由 Codex 认领。** 任务书：`collab/BRIEF-T011-ch05.md`。
  12 条清单（代码5.1/5.2、算法5.3–5.7、代码5.8、算法5.9/5.10、代码5.11/5.12），
  覆盖二叉树周游、存储结构、二叉搜索树、堆与筛选法、Huffman 树。
  **怎么切单元由你定**——这一章你比我更早看到全貌。

- **分工与链表那轮相同**：你实现 + 断言 + 证据 + 书稿，本机 `--allow-degraded`；
  **我负责完整双构建与泄漏/悬垂专项变异自检**。第 5 章是目前泄漏风险最高的一章
  （树的析构、子树删除、结点回收全是裸指针），而你那台 macOS 的 ASan 起不来。
  链表那轮这组变异抓到 4 条 LeakSanitizer + 1 条 UBSan，树只会更多。
  **请像上轮那样把你没能验证的部分写清楚**——那份坦白比一句"全绿"有用。

- **代码5.8 的结束标记被 OCR 吃掉**（T-008 在案）：边界在 `dsa_raw.md` 的
  4058–4156 之间，需人工定。定完请在 `legacy.md` 写明按哪一行收尾、依据是什么。

- **两个未决问题，撞上了请说，别各自定**：
  ① **T-010 仍挂着人拍板**——`remove()` 抛越界 vs `pop()` 返回 `optional`。
  二叉搜索树的删除会正面撞上它（删一个不存在的键：错误，还是可预期空状态？）。
  建议你给出倾向 + 理由，一起提请人拍板；
  ② 递归周游在深树上爆栈，算"实现细节"还是"该修的缺陷"？我倾向保留原书写法
  并把风险写进 `legacy.md`，但这是教学取舍，你有不同看法就摆出来。

- **交接时的当前状态**（闸门退出码 0）：

  ```
  台账      22 已现代化 / 1 退场 / 82 待办
  书稿      3 个文件，7 条规则通过
  代码      5/5 单元双构建通过
            ArrayList 47 / ArrayStack 58 / LinkedList 33 / PatternMatching 56 / String 49
  ```

  第 2 章 12/12 完成，第 3 章 3/15，第 4 章 7/8 + 1 退场。

### 2026-08-12 · Claude → Codex · T-004b String 类；第 4 章收完（7/8 + 1 退场）

- **原书三处硬伤，都有编译器/sanitizer 输出**：
  ① `assert(str != '\0')` **本身编译不过**（`'\0'` 是 char，指针与整数比较是 ill-formed），
  而且改成 `!= nullptr` 也是无效断言——`new` 失败抛 `bad_alloc`，从不返回空指针；
  ② `String(char* s)` 让书中自己写的 `String s1 = "Hello";` 在 `-Werror` 下编译失败
  （字面量是 `const char[6]`，C++11 起不能转 `char*`）；
  ③ 算法4.5 越界 `return NULL`——返回类型是 `String`，NULL 走 `String(char*)`
  于是 `strlen(nullptr)`，**能编译**，运行期 UBSan + ASan 当场 SEGV。

- **我这轮错了两次，都原样记进 `legacy.md` 第五节而不是删掉**：
  ① 猜「算法4.3 的 `strcmp` 与标准库同名会冲突」——实测能编译能链接，**不成立，
  没写进缺陷清单**；② 说「`String s1 = "Hello"` 编译不过」——口径过强，
  GCC 默认只警告，只有 `-Werror` 下才是错误。
  另有一处我自己的过度断言已改：代码4.1 只有声明没有函数体，
  我无从证明原书 `append` 「会把结果丢掉」，只能断言签名含混。

- **变异自检 5/5，但前三次撞上「编译期假象」**（被 `-Wunused-variable`、
  `-Wtype-limits` 挡下，不是被断言抓的）。重做干净版本后确认：
  拷贝构造抄指针 → ASan heap-use-after-free；append 漏 `delete[]` → LeakSanitizer；
  append 改成"返回副本、本串不变" → 断言链抛 out_of_range；
  substr 越界返回空串 → `FAIL: pos 越界抛 out_of_range`；
  substr 不截断 → ASan heap-buffer-overflow。
  **教训（第二次记）**：`-Werror` 越严，变异越容易被无关编译错误挡下，伪造出"有牙"的假象。

- **补了上一轮那条差一错误的第三重佐证**：原书 4.3 节开头的约定
  「P和T的第一个字符都从位置0开始」（`dsa_raw.md:3246`）——白纸黑字，
  与 `return (j - pLen + 1)` 直接冲突。

- **闸门结果**（退出码 0）：

  ```
  $ python3 -m unittest discover -s tests      Ran 61 tests in 6.690s  OK
  $ python3 tools/ledger.py --check            ✅ 22 已现代化，1 退场，82 待办
  $ python3 tools/check_doc.py                 ✅ 书稿体检通过：3 个文件，7 条规则
  $ python3 tools/check_code.py                ✅ 5/5 个单元通过（双构建）
     ArrayList 47 / ArrayStack 58 / LinkedList 33 / PatternMatching 56 / String 49
  ```

- **一处设计取舍想让你看一眼**：移动声明 `noexcept` 就不能在里面分配，
  所以被移动方 `data_` 置空、读取路径统一走私有 `raw()`（空时返回静态 `""`）。
  代价是所有读 `data_` 的地方都必须走 `raw()`——我漏过一次（拷贝构造从
  可能为空的 `other.data_` memcpy，即使长度 0 也是 UB），已修。请复核这个取舍。

- **第 4 章至此 7/8 + 1 退场。** 下一步建议第 5 章二叉树（12 条清单）——
  那是全书最大的一章之一，且树的删除/析构是裸指针的另一个重灾区。你想接就认领。

### 2026-08-12 · Claude → Codex · T-004a 第 4 章模式匹配：发现原书**算法结果错**

- **本轮最重的一条不是写法问题**：原书【算法4.6】朴素匹配与【算法4.8】KMP，
  匹配成功时都写 `return (j - pLen + 1);`，而 0 起始下标下正确的是 `j - pLen`。
  拿标准库 `find` 对拍，四组数据**每组恰好多 1**：

  ```
  T=abc                              P=abc          原书=  1  正确=  0
  T=xabc                             P=abc          原书=  2  正确=  1
  T=aaab                             P=ab           原书=  3  正确=  2
  T=abcddabcababcdaabcababcdaabcabaa P=abcdaabcab   原书= 11  正确= 10
  ```

  最后一组正是书中图4.12 自己演示 KMP 用的那对串——原书逐趟画了过程却没给返回值，
  错误因此在书里没有暴露。不是 OCR：两处独立印出、写法一致，而同段的
  `j = j - i + 1` 恰恰证明作者用的就是 0 起始下标。

- **这条改变了测试写法**：所有匹配用例都拿 `std::string_view::find` 逐个对拍，
  外加 3000 组随机对拍。只断言「找到了」的测试在原书那份实现下同样全绿——等于没测。

- **第二处：正文的 next 数组比模式还长一位。** 正文写 11 个值，模式只有 10 个字符，
  图4.11 给的是 10 个。算法实算站在图这一边。**是印刷错误还是 OCR 多插，分辨不出**，
  legacy.md 如实写明，只断言三者不能同时成立。

- **两个机制首次投入使用**：
  ① `d001_exceptions`（`<vector>` 承载 next 以修掉原书 `new int[]` 的泄漏，附理由）
  ——你红队时加的「理由 `strip()` 非空」检查正在守它；
  ② `exclusions.json`（代码4.2 退场：它是标准库 `basic_string` 的空壳声明，
  无可现代化内容；顺带记了往 `namespace std` 加 typedef 是 UB）。

- **闸门结果**（退出码 0）：

  ```
  $ python3 -m unittest discover -s tests      Ran 61 tests in 9.646s  OK
  $ python3 tools/ledger.py --check            ✅ 18 已现代化，1 退场，86 待办
  $ python3 tools/check_doc.py                 ✅ 书稿体检通过：3 个文件，7 条规则
  $ python3 tools/check_code.py                ✅ 4/4 个单元通过（双构建）
                          ArrayList 47 / ArrayStack 58 / LinkedList 33 / PatternMatching 56
  ```

- **变异自检 5/5，其中一条返工过**：「去掉 next 优化」第一次是被**未使用变量的编译错误**
  挡下的，不是被断言抓的——重做了干净的变异才确认 `test_next_matches_the_book_figure`
  真的会红。变异自检本身也会做假，得看清是被什么抓的。

- **T-004b 留给你认领**（代码4.1 / 算法4.3 / 4.4 / 4.5，String 类）。证据已采好：
  `String::String(char* s)` 让书中自己的例子 `String s1 = "Hello";` 在 C++11 起编译不过；
  算法4.5 从返回 `String` 的函数里 `return NULL`；算法4.3 的 `strcmp` 与标准库同名同签名。

### 2026-08-12 · Claude → Codex · T-003b 复核：ASan 双构建通过；补 O(1) 守门 + 返工 skip 粒度

- **你交给我的那件事已完成**：三单元双构建全绿，LinkedList 在 Debug+ASan/UBSan 档
  33 项断言通过。你的 macOS 环境诊断成立，实现本身没有内存问题。

- **链表专属的变异自检 5/5 全抓**（这组只有 sanitizer 能证，是本轮我能做而你做不了的）：
  析构不清链 / `remove` 不 delete / `clear` 只断链 / 拷贝构造失败不回收半截链
  → 四条全部 **LeakSanitizer**；`swap` 不回填哨兵尾指针 → **UBSan 空指针成员访问**。
  `fix_sentinel_tail` 与 `take_from` 的求值顺序我逐例推演 + 变异验证，都对。

- **补一处：`append` 的 O(1) 没有守门用例。** 你自己抓到并修了「`append` 转调
  `insert(size_)` 会循链」，但现有 31 条断言在 O(1) 与 O(n) 两种实现下表现完全相同——
  差别只在复杂度。补 `test_append_does_not_walk_the_chain`（30 万次 append，
  O(1) 档 0.208 秒）。反向验证：改回循链版本，闸门报 **`❌ 超过 120s 未结束`**。
  用例注释里写明了局限：它证明「不随表长线性变慢」，不证明严格 O(1)。

- **返工一处：闸门自测的 skip 粒度过宽。** `@unittest.skipUnless(SANITIZER_AVAILABLE)`
  加在整个 `TestGateHasTeeth` 类上，而其中两条（断言失败要被报出、`-Werror` 要生效）
  与 sanitizer 无关——整类 skip 使你在 macOS 上对「闸门有没有牙」变成**零覆盖**。
  改为 skip 下沉到依赖 sanitizer 的三条，`run_gate()` 环境不可用时自动带
  `--allow-degraded`。打桩模拟验证：3 skip、2 通过。

- **闸门结果**（退出码 0）：

  ```
  $ python3 -m unittest discover -s tests      Ran 61 tests in 19.524s  OK
  $ python3 tools/ledger.py --check            ✅ 台账一致：15/105 已现代化，0 退场，90 待办
  $ python3 tools/check_doc.py                 ✅ 书稿体检通过：2 个文件，7 条规则
  $ python3 tools/check_code.py                ✅ 3/3 个单元通过（debug+asan+ubsan, release-O2）
                                     ArrayList 47 / ArrayStack 58 / LinkedList 33 项断言，0 失败
  ```

- **需要人拍板的一条**：`remove()` 返回 `T`（越界抛）与 `pop()` 返回 `optional`
  是两种形状，三个容器都按「栈的空是常态、表的越界是错误」这条隐含口径实现，
  两个 agent 各自独立选到了同一处——但 D-001 里没写。建议补进公约，
  免得第 5 章树一开工又各选各的。

### 2026-08-12 · Codex → Claude · T-003b 链表（代码2.6–代码2.12）交复核

- **逐条核对**：七条清单原文范围 `dsa_raw.md:1450-1660`。`legacy.md` 逐条区分 OCR
  损伤与可复现缺陷：`delete` 关键字、`const Link*` → `Link*` 的非法赋值，以及
  算法2.9 每次定位的无主 `new Link(head->next)` 泄漏。
- **实现**：新增 `code/ch02/linked_list`。保留带头结点、尾指针、按位置循链 O(n)、
  定位后的指针改链 O(1)；`append` 直接经 tail 接链 O(1)。实现 Rule of Five、
  复制构造的中途失败清理、move-only 元素和 `DoublyLink` 结点类型。代码2.12 原书没有
  完整算法，本轮不虚构完整双链表。
- **变异自检**：删尾不回退 `tail_` → 后续 append 退出码 138；复制构造 catch 不清理 →
  `FAIL: 复制构造失败时已接入结点全部回收` 与 `FAIL: 链表离开作用域后不遗留元素对象`。
- **闸门结果（真实，降级）**：

  ```
  $ python3 -m unittest discover -s tests       Ran 61 tests ... OK (skipped=6)
  $ python3 -m py_compile tools/*.py tests/*.py  ✅ ok
  $ python3 tools/ledger.py --check              ✅ 台账一致：15/105 已现代化，0 退场，90 待办
  $ python3 tools/check_doc.py                   ✅ 书稿体检通过：2 个文件，7 条规则
  $ python3 tools/check_code.py --allow-degraded
    ArrayList: 47 项断言，0 失败
    LinkedList: 30 项断言，0 失败
    ArrayStack: 58 项断言，0 失败
    ✅ 3/3 个单元通过（Release-O2）
  ```

  Debug ASan/UBSan 未运行：空探针稳定报 `sanitizer_malloc_mac.inc:189`，按 D-006 已显式
  使用 `--allow-degraded`，不能把 Release 绿误称为完整内存验证。

### 2026-08-12 · Claude → Codex · T-009 共享探针 + T-003a 第 2 章顺序表

- **做了什么**：先落地 T-009（把四个故障注入探针抽成 `code/support/fault_injection.hpp`，
  多补一个 `Counted`，共 5 个），再做 T-003a 第 2 章顺序表单元
  （代码2.1 / 代码2.2 / 算法2.3 / 算法2.4 / 算法2.5，5 条清单）。

- **又抓到原书三处编译级硬伤**，比第 3 章还多一处：
  - `bool delete(const int p)` —— **`delete` 是 C++ 关键字**，代码2.1/2.2/算法2.5
    三处都用它当函数名，整章的删除操作建立在编译不过的名字上；
  - `class List { void clear(); ... };` **没写 `public:`** —— 默认 private，
    这个 ADT 的每个运算都调不到（同书第 3 章代码3.1 是写了的，体例不一致）；
  - 算法2.3 的 `for (i = 0; i < n; i++)`，**`n` 从未声明**。

  另有一处设计问题：`int position` 游标住在容器里（const 不能遍历、不能嵌套遍历），
  改为 `begin()/end()`；且该成员在书中所有算法里**一次都没被用到**。

- **闸门结果**（退出码 0）：

  ```
  $ python3 -m unittest discover -s tests      Ran 61 tests in 18.768s  OK
  $ python3 tools/ledger.py --check            ✅ 台账一致：8/105 已现代化，0 退场，97 待办
  $ python3 tools/check_doc.py                 ✅ 书稿体检通过：2 个文件，7 条规则
  $ python3 tools/check_code.py                ✅ 2/2 个单元通过（debug+asan+ubsan, release-O2）
                                                  ArrayList: 47 项断言 / ArrayStack: 58 项断言，0 失败
  ```

- **变异自检 6/6 全抓**：拷贝构造退回浅拷贝 → ASan double-free；insert 不右移 →
  断言红；remove 不左移 → UBSan；越界不抛 → UBSan；搬迁判据改回 `is_copy_assignable`
  → 断言红（D-005 在 ch02 这边也守住了）；扩容失败漏 `delete[]` → LeakSanitizer。

- **书稿的图**：第 2 章顺序表一节**原书没有插图**，图2.1/2.2/2.3 在 OCR 里都是
  （乱掉的）HTML 表格。没有伪造图片引用，改成规范的 Markdown 表格重排。

- **红线自检**：`dsa_raw.md` 未动 ✅ ｜ 书稿代码 `file=` 逐字一致 ✅ ｜
  未换 STL（仍是手写顺序表，插入/删除仍 O(n)）✅ ｜ 编号未漂 ✅ ｜
  缺陷条条有证据 ✅ ｜ 台账等式成立（8+0+97=105）✅ ｜ 零第三方依赖 ✅

- **两处我不确定、想听你意见**（详见 NOTES-claude）：
  ① `remove()` 返回 `T`（越界抛）而第 3 章 `pop()` 返回 `optional`——同项目两种形状，
  我认为对（栈空是常态、表越界是错误），但接口口径这种事该由人拍板；
  ② `insert(pos == size())` 等价 append 的路径没有单独用例。

- **T-003b 链表（7 条清单）留给你认领**——原书那部分是裸指针 + 手工 delete 的重灾区，
  换个模型看更可能挖出我看不见的东西。我可以接第 4 章字符串。

### 2026-08-12 · Claude → Codex · T-002 复核：两处诊断认可，异常安全的修复方式返工一次

- **复核结论**：Codex 红队达标——交出两条会失败的测试，**都是真缺陷**（`cc26132`）。
  D-001 静态检查的绕过（空白变体、空白豁免理由）与 `move_if_noexcept` 用错维度，
  两条都成立。第二条正是我上一轮在 NOTES 里点名最不放心的地方。

- **返工一处**：修复判据由「可不可拷贝」改为「移动赋值抛不抛」。
  原修法让 `std::string`（移动赋值本就 noexcept）每次扩容退化成深拷贝，
  实测 64 次 push 的扩容搬迁 **63 次全是拷贝**；改后 **0 次**。
  Codex 的两条故障注入用例在新判据下照样通过。
  **补上唯一能分辨两种策略的守门用例**并反向验证（改回旧判据立刻变红）——
  两种修法都能通过当时的全部断言，这才是本轮真正的教训。

- **Codex 报告的 ASan 失败：确认为 macOS 环境问题**。同一份代码在本机 Linux 上
  Debug+ASan/UBSan 档 58 项断言全过。已做成工具自检 `sanitizer_preflight()`：
  跑单元前先试空探针，失败以**退出码 2** 与代码问题（1）区分，并给复现命令。
  降级出口 `--allow-degraded` 只在自检失败时生效，且开头与结论各喊一次。

- **改了哪些文件**：`code/ch03/array_stack/`（modern.hpp 判据、test.cpp 新守门用例、
  legacy.md 缺陷 11）、`book/ch03-stack.md`（新增「`move_if_noexcept` 在这里是错的」
  一小节）、`tools/check_code.py`（preflight + 降级）、`tests/test_check_code.py`、
  `collab/DECISION_LOG.md`（D-005 / D-006）、`collab/PLAN.md`。
  **`dsa_raw.md` 仍未动。**

- **闸门结果**（退出码 0）：

  ```
  $ python3 -m unittest discover -s tests      Ran 61 tests in 12.437s  OK
  $ python3 tools/ledger.py --check            ✅ 台账一致：3/105 已现代化，0 退场，102 待办
  $ python3 tools/check_doc.py                 ✅ 书稿体检通过：1 个文件，7 条规则
  $ python3 tools/check_code.py                ✅ 1/1 个单元通过（debug+asan+ubsan, release-O2）
                                                  ArrayStack: 58 项断言，0 失败
  ```

- **红线自检**：`dsa_raw.md` 未动 ✅ ｜ 书稿代码 `file=` 逐字一致（R3 本轮真的抓到一次
  漂移，改完 modern.hpp 忘了同步）✅ ｜ 未换 STL ✅ ｜ 编号未漂 ✅ ｜
  缺陷条条有证据 ✅ ｜ 台账等式成立 ✅ ｜ 零第三方依赖 ✅

- **下一轮**：T-002 标 Done；新开 T-009（把四个故障注入探针类型抽成共享头，
  第 2 章链表会用到）。我准备接 T-003（第 2 章线性表 12 条清单），
  Codex 若想先做链表那半，在 PLAN 里认领。

### 2026-08-12 · Codex → Claude · T-002 红队：两处真缺陷已复现并修复

- **静态闸门打穿并加固**：旧 `check_d001()` 会漏 `#  include <vector>` 与
  `std :: cout`，空格理由可绕过豁免；也会将块注释/字符串里的 token 误判。现改为
  注释/字符串剥离 + 空白规范化匹配，豁免理由 `strip()` 后必须非空；9 条 D-001
  自测覆盖这些情况。
- **强异常保证真 bug**：旧版对「移动构造 `noexcept`、移动赋值第 3 次抛」的故障注入
  输出 `FAIL: redteam strong guarantee after throwing move assignment`（52 项断言，1 失败）。
  根因是 `move_if_noexcept` 看移动构造而代码实际做移动赋值。现对可拷贝 T 用复制迁移；
  不可拷贝 T 静态要求移动赋值 `noexcept`。同时补 `new T[next]` 抛 `bad_alloc` 的回归。
- **peek 复核**：不解引用失效指针是正确取舍（否则 UB）；维持 D-001 §3b 的
  `top()` 副本 / `peek()` 零拷贝双接口，不加调试世代计数。
- **闸门结果（真实）**：书稿同步后 `check_doc.py` 通过；Release `check_code.py`
  输出 `ArrayStack: 55 项断言，0 失败`。完整验证因当前 macOS ASan 运行时失败而未全绿：

  ```
  test_clean_unit_passes_both_profiles ... FAIL
  AddressSanitizer: CHECK failed: sanitizer_malloc_mac.inc:189
  "((!asan_init_is_running)) != (0)"
  $ python3 tools/check_doc.py                 ✅ 书稿体检通过：1 个文件，7 条规则
  $ python3 tools/check_code.py                ❌ Debug ASan 初始化失败；Release 55 项断言，0 失败
  ```

  该 ASan 错误同样发生在 `tests/test_check_code.py` 的空探针单元，不能宣称本轮完整闸门
  已通过。详见 `NOTES-codex.md` 的攻击记录与环境证据。

### 2026-08-12 · Claude → Codex · D-001 §3b `peek()` 落地；正式交红队（T-002）

- **做了什么**：人拍板补充 D-001 §3b，新增 `const T* peek() const noexcept`
  （零拷贝、空栈 `nullptr`、move-only 元素可用，代价是指针在下次 push/pop/clear 后失效）。
  公约、书稿正文、对照表、`legacy.md` 的欠账条目同步更新——欠账不是删掉，
  是改成带出处的已决记录，原文用删除线保留。
- **改了哪些文件**：`code/ch03/array_stack/`（modern.hpp / test.cpp / legacy.md）、
  `book/ch03-stack.md`、`collab/DECISION_LOG.md`（新增 §3b）、`collab/PLAN.md`、
  `collab/REDTEAM-BRIEF-T002.md`（新建，红队任务书）。**`dsa_raw.md` 仍未动。**
- **闸门结果**（退出码 0）：

  ```
  $ python3 -m unittest discover -s tests      Ran 54 tests in 6.220s  OK
  $ python3 tools/ledger.py --check            ✅ 台账一致：3/105 已现代化，0 退场，102 待办
  $ python3 tools/check_doc.py                 ✅ 书稿体检通过：1 个文件，7 条规则
  $ python3 tools/check_code.py                ✅ 1/1 个单元通过（debug+asan+ubsan, release-O2）
                                                  ArrayStack: 50 项断言，0 失败
  ```

- **peek 的两条守门用例经变异验证**：退化成拷贝实现 → move-only 处**编译期**即挂
  （`unique_ptr` 拷贝赋值 deleted）；用 `if constexpr` 隔离后
  `FAIL: peek 一次拷贝都不做` 变红。空栈不返回 `nullptr` → UBSan 报
  `applying non-zero offset to null pointer`。
- **轮到 Codex**：任务书 `collab/REDTEAM-BRIEF-T002.md`，三条主攻方向按人的指示排定。
  成功标准写死为「**至少交出一条会失败的测试**」；攻不动也算结论，但要带攻击记录。
  我自己最不放心的是第二条：`move_if_noexcept` 看的是移动**构造**是否 noexcept，
  而扩容里元素是被**赋值**进去的——这条推理至今无测试守住。

### 2026-08-12 · Claude → Codex · D-001 公约落地：样板单元由 C++20 重做为 C++17

- **做了什么**：人拍板了 T-006 现代化风格公约（全文落在新建的 `collab/DECISION_LOG.md`
  D-001），按它把样板单元整个重做：C++20 concept → C++17 `static_assert` +
  `<type_traits>`；`unique_ptr<T[]>` → 裸 `T* data_` + 显式五法则；
  `top()`/`pop()` 统一返回 `optional`；越界抛 `out_of_range`、容量溢出抛 `overflow_error`。
  书稿 3.1.1/3.1.2/3.1.3 三节论述随之重写，代码块由 `sync_book.py --write` 重新灌入。

- **改了哪些文件**：`collab/DECISION_LOG.md`（新建）、`collab/PLAN.md`（T-006 → Done，
  Decision Log 改为索引指向 DECISION_LOG，避免两份副本各自腐烂）、`collab/README.md`、
  `code/ch03/array_stack/`（modern.hpp / test.cpp / legacy.md / unit.json 全改）、
  `book/ch03-stack.md`、`tools/check_code.py`（新增 D-001 静态检查）、
  `tools/ledger.py`（默认标准 c++17）、`tools/handoff.py`（检查清单加 D-001 一条）、
  `tests/test_check_code.py` `tests/test_handoff.py`、`CLAUDE.md`。
  **`dsa_raw.md` 仍然一字未动。**

- **闸门结果**（`python3 tools/handoff.py --verify`，退出码 0）：

  ```
  $ python3 -m unittest discover -s tests      Ran 54 tests in 6.949s  OK
  $ python3 -m py_compile <11 files>           ✅ ok
  $ python3 tools/ledger.py --check            ✅ 台账一致：3/105 已现代化，0 退场，102 待办
  $ python3 tools/check_doc.py                 ✅ 书稿体检通过：1 个文件，7 条规则
  $ python3 tools/check_code.py                ✅ 1/1 个单元通过（debug+asan+ubsan, release-O2）
                                                  ArrayStack: 38 项断言，0 失败
  ```

- **变异自检 5/5 全抓**（都是断言或 sanitizer 抓的，不是编译期）：
  拷贝构造改成抄指针 → `AddressSanitizer: double-free`；
  扩容失败漏 `delete[] fresh` → `LeakSanitizer`；
  放弃先建后换 → Debug 档 UBSan 空指针引用、Release 档段错误；
  删掉深拷贝循环 → `FAIL: 拷贝赋值得到独立副本`；
  越界不抛 → `FAIL: 越界读取抛 std::out_of_range`。

- **上一轮挂着的账已销**：`ensure_capacity()` 的强异常保证从「只做了推理」变成
  **故障注入实测**（`Fragile` 第 3 次拷贝赋值必抛）。原话保留在 NOTES 里，标注了销账。

- **公约不只是文字，有机器守着**：`check_code.py` 新增 D-001 静态检查——`modern.*` 里
  出现 `<iostream>` / `std::cout` / STL 容器头文件即红；豁免写进 `unit.json` 的
  `d001_exceptions`，**键是被豁免的写法、值是理由**。6 条新测试守这个检查本身。

- **红线自检**：`dsa_raw.md` 未动 ✅ ｜ 书稿代码全部 `file=` 引用且逐字一致 ✅ ｜
  仍是手写数组栈 + 翻倍扩容，没换 STL ✅ ｜ 编号与交叉引用未漂 ✅ ｜
  缺陷条条有证据（证据命令已按 C++17 重跑）✅ ｜ 台账等式成立 ✅ ｜ 零第三方依赖 ✅

- **请你重点看**：① T-002 红队，新靶子是 D-001 静态检查（逐行正则，块注释与字符串
  里的关键字我没处理）；② `Fragile` 只覆盖「拷贝赋值抛」，**移动赋值抛与
  `new T[next]` 抛 `bad_alloc` 都没造过**；③ `top()` 返回副本对 move-only 元素不可用，
  要不要另加 `const T* peek()`。

### 2026-08-12 · Claude → Codex · T-000 脚手架 + T-001 顺序栈样板，交首轮复核

- **做了什么**：从零搭起 Claude⇄Codex 协作脚手架（移植自 `cs101.openjudge.cn/collab`，
  闸门按本项目重写），并用第 3.1 节顺序栈做了**一个跑通全流程的样板单元**——
  脚手架如果没在真内容上跑过，它证明不了任何事。

- **改了哪些文件**：
  `collab/`（README/PLAN/HANDOFF/NOTES×2/exclusions.json）、
  `tools/`（`handoff.py` `ledger.py` `check_doc.py` `check_code.py` `sync_book.py`
  `vendor_figures.py` `repo.py`）、`tests/`（4 个测试文件，48 项）、
  `code/ch03/array_stack/`（unit.json / legacy.md / modern.hpp / test.cpp）、
  `book/ch03-stack.md` + `book/assets/`（1 张图已 vendoring）、
  `CLAUDE.md`、`README.md`、`.gitignore`。
  **`dsa_raw.md` 一字未动**（红线 1）。

- **闸门结果**（`python3 tools/handoff.py --verify`，退出码 0）：

  ```
  $ python3 -m unittest discover -s tests      Ran 48 tests in  8.655s  OK
  $ python3 -m py_compile <11 files>           ✅ ok
  $ python3 tools/ledger.py --check            ✅ 台账一致：3/105 已现代化，0 退场，102 待办
  $ python3 tools/check_doc.py                 ✅ 书稿体检通过：1 个文件，7 条规则
  $ python3 tools/check_code.py                ✅ 1/1 个单元通过（debug+asan+ubsan, release-O2）
                                                  ArrayStack: 29 项断言，0 失败
  ```

- **变异自检 5/5 全抓**（把实现改回原书行为，闸门必须变红）：深拷贝不拷元素、
  翻倍改 `+1`、空栈 pop 不返回 nullopt、容器里恢复 `cout`、`clear()` 扔掉容量。
  逐条结论在 `NOTES-claude.md`。

- **本轮最硬的一个发现：原书这三条清单里有两条按印刷原样根本编译不过。**
  - 代码3.2：`int top` 与成员函数 `bool top(T&)` 重名 →
    `error: ‘bool arrStack<T>::top(T&)’ conflicts with a previous declaration`；
  - 算法3.3：扩容循环的 `i` 从未声明 → `error: ‘i’ was not declared in this scope`。

  另外实测到两处未定义行为：无参构造留下未初始化的 `mSize`/`st`
  （`-Wall -Wextra -Wpedantic` **一句警告都不给**），以及违反三法则导致的二次释放
  （ASan 报告已抄进 `legacy.md`）。命令与完整输出都在 `code/ch03/array_stack/legacy.md`，
  可逐条重跑。

- **红线自检**：`dsa_raw.md` 未动 ✅ ｜ 书稿代码全部 `file=` 引用且 R3 逐字一致 ✅ ｜
  没有换成 STL 封装（仍是手写数组栈 + 翻倍扩容）✅ ｜ 编号与交叉引用未漂 ✅ ｜
  缺陷条条有证据 ✅ ｜ 台账等式成立（3 + 0 + 102 = 105）✅ ｜ 零第三方依赖 ✅

- **请你重点看**（详见 `NOTES-claude.md`）：
  1. **T-002 就是给你的**：找能溜过 `--verify` 的坏改动。我自己怀疑的三个入口
     （R2 剥离器的盲区、R3 的 dedent 放宽、台账不校验「真的实现了」）已写在 NOTES。
  2. `ArrayStack::ensure_capacity()` 的强异常保证我**只做了推理，没做故障注入**。
     要写会失败的测试，建议从这里下手。
  3. `std::move_if_noexcept` 在「赋值而非构造」语义下是否还有意义，我不完全确定。

- **仍然挂着**：T-006 现代化风格公约需要人拍板（C++ 标准、命名、异常 vs 断言、
  允许用哪些 STL）。样板单元已经隐含了一套取值，写在 PLAN 的 Decision Log 里，
  **标注的是「Claude 提议，待人确认」，不是既成事实**。在人确认之前，
  第二个单元最好别开工，否则每个单元各行其是。
