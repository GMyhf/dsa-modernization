// author_diff.cpp —— 与作者代码包对拍（由 tools/authorsrc.py --check 编译运行，不归 check_code 管）
//
// 作者包 ch03_StackQueue/alg3.5/arrStack.h（代码3.2）与本单元 ArrayStack 在随机操作序列上对拍。
//
// 代码包是「原书这里为什么编译不过」的第二证人：包里取栈顶叫 getTop(T*)，成员变量 int top 是 public，
// 两者不重名——**所以这个文件能编译**。原书印的是 bool top(T&)，与 int top 重名，编译不过（勘误 E05）。
// 包与原书同为 2008 年 6 月，谁先谁后不可考，只能说「印出来的与作者的代码不一致」。
//
// 行为上的两处不同都是预期内的，由断言钉住：
//   * 容量内：push/pop/getTop 的结果逐步一致；
//   * 满了以后：作者 push 打印「栈满溢出」并返回 false——包里没有算法3.3 的翻倍扩容，
//     三份 arrStack.h 都一样；本单元按算法3.3 翻倍，继续收。
// 不跑的：作者的无参构造 arrStack() 只设 top = -1，mSize 与 st 都未初始化，析构时 delete[] 一个野指针。
#include <iostream>
#include <random>
#include <sstream>
#include <string>

#include "modern.hpp"

#include "arrStack.h"

namespace {

int failures = 0;

void expect(bool ok, const std::string& what) {
    if (!ok) {
        ++failures;
        std::cerr << "  ✗ " << what << "\n";
    }
}

}  // namespace

int main() {
    constexpr int capacity = 8;
    std::mt19937 rng(20080601);
    int operations = 0;
    int refused = 0;

    std::ostringstream quiet;  // 作者的栈在容器里 cout 报错，截掉
    std::streambuf* saved = std::cout.rdbuf(quiet.rdbuf());

    for (int round = 0; round < 300; ++round) {
        arrStack<int> author(capacity);
        dsa::ArrayStack<int> ours(capacity);
        for (int step = 0; step < 40; ++step) {
            const unsigned op = rng() % 3;
            const std::string where = "第 " + std::to_string(round) + " 轮第 " + std::to_string(step) + " 步";
            if (op == 0) {
                const int value = static_cast<int>(rng() % 1000);
                const bool author_ok = author.push(value);
                const bool was_full = ours.size() == static_cast<std::size_t>(capacity);
                if (was_full) {
                    // 作者：栈满拒收；本单元：算法3.3 翻倍后照收。拒收之后两边不再同步，本轮到此为止。
                    expect(!author_ok, where + "：作者的栈满了应拒收");
                    ++refused;
                    break;
                }
                ours.push(value);
                expect(author_ok, where + "：容量内作者 push 应成功");
            } else if (op == 1) {
                int author_value = -1;
                const bool author_ok = author.pop(&author_value);
                const auto ours_value = ours.pop();
                expect(author_ok == ours_value.has_value(), where + "：pop 成败不一致");
                if (author_ok && ours_value) expect(author_value == *ours_value, where + "：pop 出的值不一致");
            } else {
                int author_value = -1;
                const bool author_ok = author.getTop(&author_value);
                const auto ours_value = ours.top();
                expect(author_ok == ours_value.has_value(), where + "：取栈顶成败不一致");
                if (author_ok && ours_value) expect(author_value == *ours_value, where + "：栈顶值不一致");
            }
            expect(author.isEmpty() == ours.empty(), where + "：判空不一致");
            expect(author.top + 1 == static_cast<int>(ours.size()), where + "：作者 public 的 top 与元素个数对不上");
            ++operations;
        }
    }
    std::cout.rdbuf(saved);

    expect(refused > 0, "随机序列应当至少撞满一次，否则「作者满了拒收」这条没被检验");
    if (failures) {
        std::cerr << "❌ 顺序栈对拍：" << failures << " 处不一致\n";
        return 1;
    }
    std::cout << "✅ 顺序栈对拍：" << operations << " 步随机操作逐步一致；其中 " << refused
              << " 次撞满，作者拒收、本单元按算法3.3 翻倍（包里没有 3.3）\n";
    return 0;
}
