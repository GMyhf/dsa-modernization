// 后缀表达式求值 —— 原书【算法3.5】的现代化实现。
//
// 本节要教的是**用栈求值**：遇操作数压栈，遇操作符弹两个算完再压回。
// 这条主线一字未改，用的还是本章自己的 ArrayStack。
// 换掉的是原书把「解析、求值、输入输出」焊在一起的结构——那让它既不能测也不能复用。
#pragma once

#include "ch03/array_stack/modern.hpp"

#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

namespace dsa {

// >>> evaluate
/// 对后缀（逆波兰）表达式求值。记号之间用空白分隔，例如 "3 4 + 2 *" → 14。
///
/// 与原书 `class Calculator` 的差别，逐条对应它的三个问题：
///
/// 1. **原书 `Run()` 直接从 `cin` 读、往 `cout` 写**，算法与终端焊死：没法写测试、
///    没法在库里复用、没法处理来自别处的表达式。这里接受一个 `string_view`、返回结果。
/// 2. **原书 `GetTwoOperands` 在第二个操作数缺失时已经把第一个弹掉了**，
///    返回 false 时栈已被破坏（legacy.md 缺陷 1）。
///    但要说准确：真正让这个 bug 无害的，**不是**下面那句"先查够不够"，
///    而是**出错即抛出、整次求值随即作废**——栈根本没有机会被下一步用到。
///    预检查只是让错误信息更早、更准，属于锦上添花。
///    原书的 bug 之所以要命，是因为它 `cerr` 一行之后**继续跑**。
/// 3. **原书出错时 `cerr` 打一行然后 `s.clear()` 继续跑**，调用方拿不到任何信号。
///    这里抛 `std::invalid_argument`（表达式不合法）或 `std::domain_error`（除零）。
[[nodiscard]] inline double evaluate_postfix(std::string_view expression) {
    ArrayStack<double> operands;
    std::size_t i = 0;

    const auto pop_two = [&operands](double& left, double& right) {
        // 先确认有两个再弹。注意这不是"修复"原书 bug 的关键（见函数注释第 2 条），
        // 而是让报错更早、更准。
        if (operands.size() < 2) {
            throw std::invalid_argument("后缀表达式：操作数不足");
        }
        right = *operands.pop();  // 先弹出的是右操作数
        left = *operands.pop();
    };

    while (i < expression.size()) {
        const char c = expression[i];
        if (c == ' ' || c == '\t' || c == '\n') {
            ++i;
            continue;
        }

        // 操作符：+ - * / 各弹两个。注意 '-' 也可能是负号，靠后面是不是数字来区分。
        const bool is_sign = (c == '-' || c == '+') && i + 1 < expression.size()
                             && (std::isdigit(static_cast<unsigned char>(expression[i + 1]))
                                 || expression[i + 1] == '.');
        if (!is_sign && (c == '+' || c == '-' || c == '*' || c == '/')) {
            double left = 0.0;
            double right = 0.0;
            pop_two(left, right);
            switch (c) {
                case '+': operands.push(left + right); break;
                case '-': operands.push(left - right); break;
                case '*': operands.push(left * right); break;
                default:
                    // 原书写 `if (operand1 == 0.0)`，正文又说这样比较浮点数不对、
                    // 该用阈值。两者都值得推敲：除以 1e-300 是**合法**的，
                    // 结果是个很大的数或 inf，用阈值把它当成错误是另一个决定。
                    // 这里只拦精确的 0.0（含 -0.0），并把这个取舍写在明面上。
                    if (right == 0.0) {
                        throw std::domain_error("后缀表达式：除数为零");
                    }
                    operands.push(left / right);
                    break;
            }
            ++i;
            continue;
        }

        // 操作数：交给标准库解析，顺便拿到它吃掉了多少字符
        std::size_t consumed = 0;
        double value = 0.0;
        try {
            value = std::stod(std::string(expression.substr(i)), &consumed);
        } catch (const std::exception&) {
            throw std::invalid_argument(std::string("后缀表达式：无法识别的记号 '") + c + "'");
        }
        operands.push(value);
        i += consumed;
    }

    if (operands.size() != 1) {
        // 空表达式、操作数多余、操作符不足，都落在这里
        throw std::invalid_argument("后缀表达式：不是一个完整的表达式");
    }
    return *operands.pop();
}
// <<< evaluate

}  // namespace dsa
