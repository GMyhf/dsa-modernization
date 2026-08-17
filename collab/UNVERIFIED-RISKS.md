# 未验证风险清单

> **这份文件写给一个没有我们上下文的人。**
>
> 仓库里所有的绿——闸门退出码 0、七个单元双构建通过、几百项断言——
> 都只证明了「被测试走到的那些路径，在这台机器上、这次构建里没出问题」。
> 这份文件记的是**另一半**：哪些东西没被验证过，以及一旦出事你会看到什么。
>
> 人于 2026-08-12 指示：递归实现保留，但「没能跑 ASan 的风险点要像写遗嘱一样交代清楚」。
> 本文件即为此而写。每条都附了可复现的命令和实测数字，不写"可能有风险"这种话。
>
> **与 README 的分工**：仓库根目录的 `README.md` 有一节「闸门证明不了什么」，
> 把下面这些结论摘了出去——那是给落到首页的人看的，免得只看见一行绿色。
> **这份文件是它的完整版**：多出来的是可复现的测量程序、逐条的环境划分，
> 以及"如果你接手先做什么"。两边若有出入，以本文件为准。

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

**注意 `destroy` 与 `clone` 曾经是隐式触发的递归**：一个普通的析构或一次拷贝就会走到，
调用方看不到任何"我正在递归"的迹象。周游至少还是显式调用。

**2026-08-14 更新：这一条已经修掉。** `destroy` 改成「右旋拉直后沿右链逐个删」
（额外空间 O(1)，不分配内存，保持 `noexcept`），`clone` 改成堆上的显式栈。
同一台机器、同样 8 MB 栈，实测：

| | 递归版（旧） | 迭代版（现在） |
| --- | --- | --- |
| `destroy` | `-O2` 100 万段错误；ASan 档 100 万段错误 | 500 万通过 |
| `clone` | `-O2` 50 万段错误；ASan 档 **40 万**段错误 | 500 万通过 |

`code/ch05/binary_tree/test.cpp` 有一个百万深左链的用例守着这条：改回递归就报
`ERROR: AddressSanitizer: stack-overflow`，退出码 1。

**周游仍然是递归为主实现**，按 §3d 只提供迭代版作补充——那是教学内容，不能抹掉。
所以「病态深树上做周游」依然有栈风险，上面那张实测表对周游仍然有效。

### 2026-08-14 实测：`unique_ptr` 串链的析构深度

`code/ch02/ownership` 把「换成智能指针会怎样」量了出来。本机 8 MB 栈：

| 构建档 | `RecursiveChain`（`unique_ptr<Node> next`） | `IterativeChain`（迭代释放） |
| --- | --- | --- |
| `-O0 -g` | 约 57,625 安全，58,601 段错误 | 500 万无恙 |
| `-O2` | 约 523,329 安全，524,306 段错误 | 500 万无恙 |

**两档差九倍**：`-O2` 能把递归转成循环，`-O0` 不能。也就是说这类崩溃在 debug 与 release
之间不可比——这正是不把链式结构改成 `unique_ptr` 的理由（D-001 §2b）。

数字随 `ulimit -s`、编译器和平台变；复现命令在 `code/ch02/ownership/legacy.md`。
**测试里没有也不会有崩溃用例**——段错误写不成断言。

### 2026-08-14 新增单元的递归情况

`code/ch12/gen_list`：`release()` **沿表尾迭代、只对表头递归**，所以栈深度跟**嵌套层数**
走、不跟表长走；测试有 30000 元素的长表用例。但 `depth_of` / `atoms_of` / `to_string`
仍然对表头递归，病态深嵌套（上万层 `((((…))))`）会撞同一堵墙，**没有实测数字**。

`code/ch12/trie`：`keys_with_prefix` 的 `collect` 与 Patricia 的析构对树高递归。
Trie 的树高等于最长关键码长度，实际不会深；Patricia 的树高最坏等于关键码位数。
**都没有做深度压测。**

`code/ch11/bplus_tree`：`insert_into` / `erase_from` / `check_node` 对树高递归，
树高是 $O(\log_m n)$，不构成栈风险。这一条是**推理**，不是实测——但和二叉树的线性深度
不是一个量级的问题。

### 顺带一条：退化 BST 的插入是 O(n²)

按有序序列插入会退化成一条链，第 i 次插入要走 i 层。测量时 40 万个结点
在 90 秒内没插完——这不是崩，是慢。做压测的人容易把它误判成死循环。

---

## 二、从未被任何测试走到的代码

以下位置删掉之后闸门**照样全绿**，也就是说没有任何用例走到过它们。

| 位置 | 状态 |
| --- | --- |
| `heap_huffman/modern.hpp` `HuffmanTree` 建叶子的 `catch (...) { delete leaf; throw; }` | 可达但未覆盖。只有 `new Node` 分配失败才走得到；现有探针 `AllocationFailure` 是按 `operator new[]` 设计的，覆盖不到单对象 `new` |

`MinHeap` 拷贝构造的 catch 原本也在这张表上，2026-08-12 已补测试
（探针 `NothrowMoveThrowingCopy` + `test_copy_constructor_cleans_up_on_throw`），
反向验证：删掉 `delete[] data_` 后 LeakSanitizer 立刻开口。

`MinHeap::ensure_capacity` 的不可达 `catch` 已在 T-015 删除：静态断言仍把可能抛的移动元素
挡在门外，分配失败在迁移前直接传播，迁移循环本身不能抛。这样 `MinHeap<Fragile>` 仍编译不过，
但不再保留一段声称清理、却无法验证的死代码；这是有意保留的类型契约，不是待决分歧。

---

## 二a、教学版在异常路径上会漏内存（2026-08-17 Codex 复查发现）

**闸门是绿的，但它只证明了正常路径。** D-012 的教学版（`teaching.hpp`）为了讲清
三法则而砍掉了 `try/catch`，于是**元素类型 `T` 的拷贝一旦抛异常，已经申请的内存就漏了**。

三处，都用裸指针：

| 位置 | 漏什么 |
| --- | --- |
| `code/ch03/array_stack/teaching.hpp` 的 `grow()` | 搬到一半抛 → 新缓冲区 `fresh` |
| `code/ch03/linked_stack/teaching.hpp` 的 `copy_from()` | 拷到一半抛 → 已建好但尚未挂接的结点 + 半截链 |
| `code/ch05/binary_tree/teaching.hpp` 的 `clone()` | 递归到一半抛 → 已经建好的那半棵子树 |

**复现**（探针在 `collab/probe-teaching-leak.cpp`，用一个「第 N 次拷贝必抛」的元素类型）：

```console
$ g++ -std=c++17 -I code -O1 -g -fsanitize=address -fno-sanitize-recover=all \
      collab/probe-teaching-leak.cpp -o /tmp/probe && /tmp/probe
==...==ERROR: LeakSanitizer: detected memory leaks

Direct leak of 32 byte(s):
    #? in LinkedStack<Throwing>::copy_from(LinkedStack<Throwing> const&)
       code/ch03/linked_stack/teaching.hpp:99

Direct leak of 24 byte(s):
    #? in BinaryTree<Throwing>::clone(BinaryTree<Throwing>::Node const*)
       code/ch05/binary_tree/teaching.hpp:146

Direct leak of 16 byte(s):
    #? in ArrayStack<Throwing>::grow()
       code/ch03/array_stack/teaching.hpp:93

SUMMARY: AddressSanitizer: 88 byte(s) leaked in 5 allocation(s).
```

（三条 leak 的 `#1` 帧正好指到上表那三个位置。探针里的 `printf` 是缓冲的，
所以「抛出: copy assign」那几行会排在 LSan 报告之后——不影响结论。）

**为什么不修**：补失败清理就要在教学代码里加 `try/catch`，而那恰恰是各章
「进阶（选读）」要讲的强异常保证——提前塞进来就把 D-012 的分层抵消了。
**决定是改口径不改代码**（D-012 已追加更正）：书稿前言与各章进阶节开头现在写的是
「在分配和 `T` 的拷贝都不抛异常时是正确的」，并写明抛出时会漏在哪。

**这一条对读者的实际影响接近于零**——教材里的元素类型是 `int`、`std::string`、
指针，它们的拷贝不抛。但**「教学版经得起 ASan」这句话本身是错的**，
所以它必须记在这份清单里，而不是留在 commit message 里。

**如果你接手要修**：正确的做法不是给教学版加 `try/catch`，
而是在各章进阶节里把工程版的对应实现**并排印出来**，让读者自己看见差别。
`ch03/array_stack` 的 3.1.2a 已经是这个形状，另外两处还没有。

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

### 2026-08-14 新增单元没有覆盖的边界

- **第 11 章全部是内存里的页模拟。** `page_reads()` / `page_writes()` 按「访问一个结点 =
  访问一页」计，**没有任何真实磁盘 I/O、页缓存或并发控制**。任何把这些数字当成真实文件系统
  访外次数的结论都是错的。
- `code/ch11/bplus_tree` 的删除路径第一版是错的（合并时把过期分界码搬进孩子），
  **是随机对拍 + `validate()` 抓到的，不是看出来的**。这说明这类结构的正确性极难靠阅读保证；
  改动它之后必须重跑随机用例，别只跑书上那几个例子。
- `code/ch12/gen_list` **不支持循环表**。构造接口造不出环，所以引用计数够用；一旦有人加上
  能造环的接口，就会漏内存，而现在的测试**发现不了**。
- `code/ch11/bitmap_index` 的签名假阳性率只在两组固定语料上看过，**没有做参数扫描**；
  换语料换位数，结论可能不同。
- Patricia **没有实现删除**；Trie 的字母表**只支持 a..z**，不是通用字节 Trie。

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
