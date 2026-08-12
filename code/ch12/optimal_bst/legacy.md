# 第12章高级结构

## 原书逐条核对

- 【算法12.1】把固定尺寸 `LinkNode` 从共享 `avail` 单链表取出，重载 `new`/`delete` 归还。
- 【算法12.2】以成功查找权 `a`、失败查找权 `b` 建造成本 `c`、根 `r` 和总权 `w` 三张 DP 表。
- 原书的 `static avail` 和类专属 `operator new/delete` 使所有对象共享隐式全局可变状态，生命周期
  结束时还需显式 `::delete` 回收整条链，极易与普通分配混淆。
- 现代 `ReusableNodePool` 用槽位索引和 `optional` 表示占用状态；耗尽返回 `nullopt`，重复/越界
  回收返回 `false`，不劫持 C++ 的全局分配语义。
- `optimal_bst` 保留教材的三表 DP，显式校验 `q.size() == p.size() + 1`，成本使用 `long long`。

## OCR 与编译证据

【算法12.1】的 `: :new LinkNode[ size]` 在 OCR 后既错误分隔作用域符，也把字节数 `size`
误当作结点数；`new` 的返回块不对应一枚 `LinkNode`。原文还含孤立 `1` 作为缺失花括号。

```text
$ printf 'int main(){ int value = 0; 1 }\n' | g++ -std=c++17 -x c++ -
<stdin>:1:30: error: expected ';' before '}' token
```

【算法12.2】的 `c[i][k -1] + c[k][j]；` 使用全角分号，直接进入 C++ 会报词法错误；`r[]]`
和 `w[i. j]` 同样是 OCR 损伤，不据此推断 DP 递推本身错误。

## 验证边界

测试覆盖固定空闲表耗尽、归还、复用、非法与重复归还，以及教材 4 键权重样例（总成本 57、
根为 2）、单键、空树和权重维度错误。池的索引句柄在释放后失效；它没有实现跨线程分配器，
也不替代通用内存管理器。
