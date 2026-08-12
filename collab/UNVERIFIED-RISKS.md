# 未验证风险清单

> **这份文件写给一个没有我们上下文的人。**
>
> 仓库里所有的绿——闸门退出码 0、七个单元双构建通过、几百项断言——
> 都只证明了「被测试走到的那些路径，在这台机器上、这次构建里没出问题」。
> 这份文件记的是**另一半**：哪些东西没被验证过，以及一旦出事你会看到什么。
>
> 人于 2026-08-12 指示：递归实现保留，但「没能跑 ASan 的风险点要像写遗嘱一样交代清楚」。
> 本文件即为此而写。每条都附了可复现的命令和实测数字，不写"可能有风险"这种话。

---

## 一、递归深度：最要紧的一条

第 5 章按 D-001 §3d 保留了递归实现（递归结构本身是教学内容）。
代价是**树高受进程调用栈限制**，而这个代价 sanitizer 也救不了你——它只能在事后告诉你发生了什么。

### 实测数字

环境：Linux，gcc 13.3.0，`ulimit -s` = 8192（8 MB 栈）。
用 `BinaryTree::create_tree` 以 O(n) 造一条纯左链（深度 = 结点数），再触发相应操作。

| 构建档 | 递归析构 `destroy` | 递归前序周游 |
| --- | --- | --- |
| Release `-O2` | 50 万 通过 · **100 万 SIGSEGV** | 50 万 通过 · **100 万 SIGSEGV** |
| Debug `+ASan/UBSan` | 50 万 通过 · **100 万 stack-overflow** | **50 万即 stack-overflow** |

Debug 档比 Release 先崩，因为 sanitizer 让栈帧变胖；周游又比析构先崩，
因为访问者路径的帧更大。**换一台机器、换一个 `ulimit -s`、换一个编译器版本，
这些数字都会变**——所以下面给了复现方法，别把上表当常数用。

### 出事的时候你会看到什么（这一条才是重点）

**Debug + ASan 档**——诊断清清楚楚：

```
==2588525==ERROR: AddressSanitizer: stack-overflow on address 0x7fff05c3bff8
    #0 dsa::BinaryTree<int>::destroy(Node*) code/ch05/binary_tree/modern.hpp:191
    #1 dsa::BinaryTree<int>::destroy(Node*) code/ch05/binary_tree/modern.hpp:192
    #2 dsa::BinaryTree<int>::destroy(Node*) code/ch05/binary_tree/modern.hpp:192
    ...
```

**Release `-O2` 档**——什么都没有：

```
$ ./chain_O2 1000000 destroy
建链完成 深度=1000000
Segmentation fault (core dumped)
```

退出码 139，一行诊断也没有。

> **所以「没跑 ASan」不是「覆盖少一点」这么轻。**
> 这个风险一旦发作，只跑 Release 的人拿到的是一个无法解释的段错误；
> 跑得起 ASan 的人拿到的是精确到行号的递归回溯。
> 第 5 章的作者（Codex）在 macOS 上**跑不起 ASan**——这就是为什么它的每一轮
> 都由另一方补跑 sanitizer，也是为什么这条必须写下来。

### 怎么复现

把下面这段存成 `chain.cpp`：

```cpp
#include "modern.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>
int main(int argc, char** argv) {
    const int n = std::atoi(argv[1]);
    const std::string what = argv[2];
    dsa::BinaryTree<int> t;
    for (int i = 0; i < n; ++i) {           // O(n) 造左链，深度 = n
        dsa::BinaryTree<int> parent;
        parent.create_tree(i, std::move(t), dsa::BinaryTree<int>{});
        t = std::move(parent);
    }
    std::printf("建链完成 深度=%d\n", n);
    std::fflush(stdout);
    if (what == "traverse") { long s = 0; t.preorder([&](int v){ s += v; }); std::printf("%ld\n", s); }
    return 0;   // 递归析构在这里
}
```

```bash
g++ -std=c++17 -O2 -Icode/ch05/binary_tree chain.cpp -o chain_O2
g++ -std=c++17 -O1 -g -fsanitize=address,undefined -Icode/ch05/binary_tree chain.cpp -o chain_ASan
./chain_O2   1000000 destroy    # 期望：SIGSEGV，无诊断
./chain_ASan 1000000 destroy    # 期望：AddressSanitizer: stack-overflow + 回溯
ulimit -s                       # 先看看你的栈有多大，数字全跟着它变
```

### 哪些代码是递归的

`code/ch05/binary_tree/modern.hpp`：
`destroy`（析构与 `make_empty` 都走它）、`clone`（拷贝构造/拷贝赋值都走它）、
`preorder_impl` / `inorder_impl` / `postorder_impl`。

`BinarySearchTree`：`destroy`、`clone`、`insert_impl`、`remove_impl`、查找路径。

**注意 `destroy` 与 `clone` 是隐式触发的**：一个普通的析构或一次拷贝就会走到，
调用方看不到任何"我正在递归"的迹象。周游至少还是显式调用。

同一文件里提供了 `preorder_iterative` / `inorder_iterative` / `postorder_iterative`
三个显式栈版本，按 §3d 作为**补充**而非替换。**但析构与拷贝没有迭代版本**——
真要处理病态深树，这两条才是先撞墙的。

### 顺带一条：退化 BST 的插入是 O(n²)

按有序序列插入会退化成一条链，第 i 次插入要走 i 层。测量时 40 万个结点
在 90 秒内没插完——这不是崩，是慢。做压测的人容易把它误判成死循环。

---

## 二、从未被任何测试走到的代码

以下位置删掉之后闸门**照样全绿**，也就是说没有任何用例走到过它们。

| 位置 | 状态 |
| --- | --- |
| `heap_huffman/modern.hpp` `MinHeap::ensure_capacity` 的 `catch (...) { delete[] fresh; throw; }` | **不可达的死代码**。`static_assert` 保证搬迁循环不抛，而 `new T[next]` 写在 `try` 之外——异常永远到不了这个 catch |
| `heap_huffman/modern.hpp` `HuffmanTree` 建叶子的 `catch (...) { delete leaf; throw; }` | 可达但未覆盖。只有 `new Node` 分配失败才走得到；现有探针 `AllocationFailure` 是按 `operator new[]` 设计的，覆盖不到单对象 `new` |

`MinHeap` 拷贝构造的 catch 原本也在这张表上，2026-08-12 已补测试
（探针 `NothrowMoveThrowingCopy` + `test_copy_constructor_cleans_up_on_throw`），
反向验证：删掉 `delete[] data_` 后 LeakSanitizer 立刻开口。

**第一条值得单独说**：`MinHeap` 用 `static_assert` 把可能抛的元素类型挡在门外，
而 D-005 定的是按「移动赋值是否 `noexcept`」分支（noexcept 就移动，否则拷贝）。
两条路都能保住强异常保证，但前者的代价是那段清理代码**永远无法被验证**。
`ArrayStack<Fragile>` 能用而 `MinHeap<Fragile>` 编译不过，也是这个差异的表现。
这条分歧尚未收口。

---

## 三、按环境划分：哪些代码在作者机器上从没跑过 sanitizer

Codex 所在的 macOS 环境，ASan 连**空探针程序**都起不来
（`sanitizer_malloc_mac.inc:189`，`!asan_init_is_running`）。
`tools/check_code.py` 的 `sanitizer_preflight()` 会检出这个情况并以**退出码 2**
（区别于代码问题的 1）报告，`--allow-degraded` 可以只跑 Release 并大声记录降级。

因此以下单元由 Codex 编写时**只跑过 Release**，sanitizer 是事后由另一方补跑的：

- `code/ch02/linked_list`（T-003b）
- `code/ch05/binary_tree`、`code/ch05/heap_huffman`（T-011）

补跑的结果都是通过，且各做了一轮泄漏/悬垂专项变异自检（链表 5 条、树 5 条，全被抓）。
**但这依赖"有人记得补跑"这件事**。如果将来某一轮没有人补，
`--allow-degraded` 的输出里会有两处大声提示，别忽略它们。

---

## 四、闸门证明了什么、没证明什么

**证明了**：被测试走到的路径上，在 `-Wall -Wextra -Wpedantic -Werror` 下编译干净；
Debug+ASan/UBSan 与 Release `-O2` 两种构建下断言全过；ASan/LSan 未报告
内存错误或泄漏；书稿里印的每一段 C++ 与 `code/` 下的源码逐字一致；
105 条清单的账是平的。

**没有证明**：

1. **没走到的路径。** sanitizer 是运行期工具，它只看得见执行过的代码。
   上面第二节那两处就是例子。
2. **栈深度。** 见第一节。ASan 能在事发后指出 stack-overflow，但没有任何测试
   会去逼近那个边界（逼近就意味着让闸门崩）。
3. **别的编译器与平台。** 全部结论来自 Linux + gcc 13.3。clang、MSVC、
   32 位、不同 `ulimit -s`，都没验过。
4. **并发。** 所有容器都不是线程安全的，也没有任何测试涉及并发。
5. **性能。** 只有两处规模守门（链表 `append` 的 O(1)、KMP 的线性性），
   靠的是"退化实现会撞上 120 秒超时"，不是真正的基准测试。
6. **教学正确性。** 闸门管代码，不管讲法。书稿的文字是否讲清楚了，
   只有人能判断。

---

## 五、如果你接手，先做这三件

1. **在你自己的机器上跑一遍第一节的复现**，把表里的数字换成你的。
   数字跟着栈大小、编译器、优化档走，照抄别人的没有意义。
2. **决定 `MinHeap` 那条分歧**（第二节末）：是收紧到 D-005 的双判据
   让死代码变可测，还是保留 `static_assert` 并**删掉**那段永远走不到的 catch。
   两者都比现状好——现状是留着一段声称提供保证、却无法验证的代码。
3. **如果你要引入新的递归结构**（第 6 章树、第 7 章图都会有），
   先回到第一节看一眼数字，再决定递归还是显式栈；
   决定完把结论写进 `DECISION_LOG.md`，别让它变成又一处口头约定。
