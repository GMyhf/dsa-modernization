// author_diff.cpp —— 与作者代码包对拍（由 tools/authorsrc.py --check 编译运行，不归 check_code 管）
//
// 作者包 ch02_LinearList/alg2.6-11/lnkList.h 的 setPos 与原书算法2.9 **印的是同一句**：
//     Link<T>* p = new Link<T>(head->next);
// legacy.md 缺陷 3 原先只把它记成「每次定位泄漏一个结点」。跑起来才看清它还是**定位错误**：
// p 起步就是那个新结点（它的 next 才是首结点），所以 setPos(0) 返回的不是首结点，而是这个游离结点；
// setPos(k) 返回的是第 k-1 个结点。于是 insert(i, v)（内部调 setPos(i-1)）：
//     i == 0   → setPos(-1) 走特判返回 head，插对了；
//     i == 1   → 插在游离结点后面，**链表根本没变**，调用却返回 true；
//     i >= 2   → 插在第 i-2 个结点后面，**比该在的位置早一格**。
// 这里用随机插入序列对拍：作者的结果恰好等于按上面三条规则在本单元 LinkedList 上重放的结果。
//
// 只跑插入。作者包的 del 在删尾结点时先 delete q、再落进 if (q != NULL) 分支 p->next = q->next;
// delete q——释放后使用加二次释放（原书算法2.11 用 else if 写对了）；非尾结点经 setPos 同样错位，
// 会把仍挂在链上的结点释放掉。这些在 ASan 下必然中止，不能放进一个要求退出码 0 的对拍，
// 证据留在 collab/authorsrc.json 的 findings 里（按源码逐字核对）。
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "modern.hpp"

#include "lnkList.h"

namespace {

int failures = 0;

void expect(bool ok, const std::string& what) {
    if (!ok) {
        ++failures;
        std::cerr << "  ✗ " << what << "\n";
    }
}

// 作者的 lnkList 没有遍历接口，只有往 cout 打印的 print()：截下来再解析。
// 输出形如 "begin\n9999 1 2 3 \nend\n"，第一个数是头结点里的占位值 9999。
std::vector<int> contents(lnkList<int>& list) {
    std::ostringstream captured;
    std::streambuf* saved = std::cout.rdbuf(captured.rdbuf());
    list.print();
    std::cout.rdbuf(saved);
    std::istringstream in(captured.str());
    std::string word;
    in >> word;  // begin
    std::vector<int> out;
    int value = 0;
    bool first = true;
    while (in >> word && word != "end") {
        value = std::stoi(word);
        if (first) {
            first = false;  // 跳过头结点的 9999
            continue;
        }
        out.push_back(value);
    }
    return out;
}

// 本单元的迭代器是最简前向迭代器（没有 iterator_traits），逐个拷出来
std::vector<int> contents(const dsa::LinkedList<int>& list) {
    std::vector<int> out;
    for (int value : list) out.push_back(value);
    return out;
}

std::string show(const std::vector<int>& v) {
    std::string s = "[";
    for (std::size_t i = 0; i < v.size(); ++i) s += (i ? " " : "") + std::to_string(v[i]);
    return s + "]";
}

}  // namespace

int main() {
    // 例：先在表头插出 [1 2 3]，再 insert(1, 42) 与 insert(2, 7)
    {
        lnkList<int> author(0);
        dsa::LinkedList<int> ours;
        for (int v : {3, 2, 1}) {
            author.insert(0, v);
            ours.insert(0, v);
        }
        expect(contents(author) == std::vector<int>({1, 2, 3}), "表头插入应当正确");

        std::ostringstream quiet;
        std::streambuf* saved = std::cout.rdbuf(quiet.rdbuf());
        const bool accepted = author.insert(1, 42);
        std::cout.rdbuf(saved);
        ours.insert(1, 42);
        expect(accepted, "作者 insert(1, 42) 返回 true");
        expect(contents(author) == std::vector<int>({1, 2, 3}),
               "作者 insert(1, 42) 应当静默丢失（插进了 setPos 新分配的游离结点后面）");
        expect(contents(ours) == std::vector<int>({1, 42, 2, 3}),
               "本单元 insert(1, 42) 应得 [1 42 2 3]");

        author.insert(2, 7);
        expect(contents(author) == std::vector<int>({1, 7, 2, 3}),
               "作者 insert(2, 7) 应当早一格落在 [1 7 2 3]，实际 " + show(contents(author)));
    }

    // 随机插入序列：作者的结果 == 按「i=0 照插、i=1 丢失、i>=2 插到 i-1」在 LinkedList 上重放
    std::mt19937 rng(20080601);
    int operations = 0;
    for (int round = 0; round < 200; ++round) {
        lnkList<int> author(0);
        dsa::LinkedList<int> model;
        std::ostringstream quiet;
        std::streambuf* saved = std::cout.rdbuf(quiet.rdbuf());
        for (int step = 0; step < 12; ++step) {
            const int size = static_cast<int>(model.size());
            const int i = static_cast<int>(rng() % static_cast<unsigned>(size + 1));  // 合法位置 0..size
            const int value = static_cast<int>(rng() % 100);
            author.insert(i, value);
            if (i == 0) {
                model.insert(0, value);
            } else if (i >= 2) {
                model.insert(static_cast<std::size_t>(i - 1), value);
            }
            ++operations;
        }
        std::cout.rdbuf(saved);
        const std::vector<int> expected = contents(model);
        expect(contents(author) == expected,
               "随机插入第 " + std::to_string(round) + " 轮：作者 " + show(contents(author)) +
                   "，按错位规则预测 " + show(expected));
    }

    if (failures) {
        std::cerr << "❌ 单链表对拍：" << failures << " 处不一致\n";
        return 1;
    }
    std::cout << "✅ 单链表对拍：" << operations
              << " 次随机插入，作者 lnkList 的结果与「setPos 错一位」的预测逐项一致"
                 "（insert(1, v) 静默丢失、insert(i≥2, v) 早一格）\n";
    return 0;
}
