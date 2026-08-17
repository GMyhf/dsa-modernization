# 派生产物新鲜度清单（2026-08-17）

核查基线：`05fab23`。这里把两个问题分开记录：

- **当前一致**：现有产物是否由当前源文件生成，或能否用结构化证据确认没有脱节；
- **持续防漂移**：以后源文件变化而产物没有更新时，闸门能否自动报红。

`绿色` 表示当前一致且有自动闸门；`黄色` 表示本次核查未发现不一致，但缺少完整的自动新鲜度契约。

| 产物 | 源文件 / 依赖 | 当前结果 | 持续防漂移 | 本次证据 |
| --- | --- | --- | --- | --- |
| 网站 `book/site/` | `book/*.md`、`book/assets/`、PDF sidecar、`tools/build_site.py` | **绿色：一致** | `python3 tools/build_site.py --check`；`handoff.py --verify` 已纳入。Pages 会现场重建，但仓库副本过期目前只发 warning | 16 个页面逐字重渲染比较通过；334 个书稿本地图片引用全部存在；网站测试覆盖 180 个 C++ 代码块渲染前后逐字一致 |
| 课件网页 `book/slides/site/` | `book/slides/*.md`、课件引用的源码和图片、`tools/build_slides.py` | **绿色：渲染产物一致** | `python3 tools/build_slides.py --check`；`handoff.py --verify` 已纳入 | 13 个页面与 12 份课件 Markdown 重渲染结果一致；课件图片均可解析，密度闸门通过 |
| 课件内容 `book/slides/*.md` | 12 章正文与配套源码 | **黄色：抽样覆盖通过，不是全文同构** | 有章节补课的定点回归测试，但没有正文到课件的完整映射或内容哈希；课件本来就是教学改写，不能用逐字比较 | `test_slides_cover_sections_added_to_the_book` 覆盖已补小节与后半本演算；本轮相关测试通过。以后新增正文仍可能没有同步进课件而不报红 |
| PDF `book/pdf/现代C++数据结构教程.pdf` | 总目录、12 章正文、习题答案、勘误、插图集、所有被引用本地图片、LaTeX preamble、构建脚本 | **绿色：一致** | `python3 tools/build_book_pdf.py --check`；sidecar 的 `source_sha256` 覆盖文本、构建输入和 292 张被引用图片；CI 与 handoff 均阻断陈旧 PDF | 当前为 436 页、17 个章级条目、12 章正文、292 张图，摘要 `17721fd71ef0...`；改一个字符或替换一张图的变异测试均会报红 |
| 习题答案 `book/习题与参考答案.md` | 各章正文、`code/`、课程答案、`ref_DSA/` 等登记来源 | **黄色：当前结构完整，语义新鲜度不可机器证明** | 没有独立生成器、sidecar 或 `--check`；它是手工维护的书稿附录。网站和 PDF 闸门只能证明“当前这份答案已被收录”，不能证明答案随正文或代码语义同步 | 12 章齐全，共 271 行，来源标记 111 处；`check_doc.py` 的 13 条规则通过，交叉引用受书稿规则管辖；网站 `exercises.html` 和 PDF 摘要都包含当前文件 |
| 插图集 `book/插图.md` + `book/assets/` | `dsa_raw.md` 中的远程图片与题注、`tools/collect_figures.py` | **黄色：当前结构与底稿一致，缺只读闸门** | `collect_figures.py` 只有写入和 `--dry-run`，没有 `--check` 或 sidecar；重新生成还依赖网络下载 | 底稿收集结果 292 张，图册实际 292 个且顺序题注逐项一致；292 个引用互不重复、无缺图；292 个文件名均等于文件内容 SHA-256 前 16 位；题注恢复测试 14 项相关行为通过 |

## 结论

当前没有发现需要重建的陈旧产物。网站 HTML、课件 HTML 和 PDF 的“生成结果新鲜度”可以机器证明；风险集中在两个语义层：

1. **习题答案与正文/实现的对应关系**没有可重生成规格，当前只能靠来源登记、书稿规则和人工复核；
2. **课件是否覆盖正文新增内容**只有定点回归，不是完整映射；
3. **插图集虽在本次逐项一致**，但 `collect_figures.py` 缺少无网络、只读的 `--check`，底稿变化不会自动报红。

若继续补闸门，优先顺序应为：给插图集增加可离线的 metadata sidecar 与 `--check`；给习题答案建立“章/题/来源/实现或测试”清单；最后再把正文小节到课件页做显式覆盖登记。网站和 PDF 已不需要重复造新机制。

## 复核命令

```bash
python3 tools/build_site.py --check
python3 tools/build_slides.py --check
python3 tools/build_book_pdf.py --check
python3 tools/check_doc.py
python3 -m unittest \
  tests.test_build_site \
  tests.test_build_slides \
  tests.test_build_book_pdf \
  tests.test_collect_figures -v
```

本轮上述命令均通过，共运行 82 项相关自测。未跟踪的参考资料、历史工程文件和本地构建残留不属于派生产物，也未纳入本次判断。
