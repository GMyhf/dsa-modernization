// 上机题 2：用 C++ 类声明定义「复数」这个抽象数据类型。
//
//   c++ -std=c++17 -Wall -Wextra -Werror complex_adt.cpp -o /tmp/complex-adt
//   /tmp/complex-adt
//
// 这道题练的是「自己定义一个类型」，所以不用 std::complex。
// 按 D-001：ADT 内部不打印任何东西，真错误抛标准异常，输出交给 operator<<。

#include <cassert>
#include <cmath>
#include <iostream>
#include <ostream>
#include <stdexcept>

namespace dsa::ch01 {

// >>> complex
class Complex {
public:
    // 题目要的是三个**重载**的构造函数，不是一个带默认参数的。
    Complex() : real_(0.0), imag_(0.0) {}
    // explicit：否则 `Complex c = 3.0;` 这种隐式转换会悄悄发生。
    explicit Complex(double real) : real_(real), imag_(0.0) {}
    Complex(double real, double imag) : real_(real), imag_(imag) {}

    [[nodiscard]] double real() const { return real_; }
    [[nodiscard]] double imag() const { return imag_; }
    void set_real(double value) { real_ = value; }
    void set_imag(double value) { imag_ = value; }

    [[nodiscard]] double norm() const { return real_ * real_ + imag_ * imag_; }

private:
    double real_;
    double imag_;
};

// 四则运算写成自由函数而不是成员函数：这样 `2.0 + c` 也能成立
// —— 成员版的左操作数必须是 Complex，写不出对称的接口。
inline Complex operator+(const Complex& a, const Complex& b) {
    return {a.real() + b.real(), a.imag() + b.imag()};
}

inline Complex operator-(const Complex& a, const Complex& b) {
    return {a.real() - b.real(), a.imag() - b.imag()};
}

inline Complex operator*(const Complex& a, const Complex& b) {
    return {a.real() * b.real() - a.imag() * b.imag(),
            a.real() * b.imag() + a.imag() * b.real()};
}

inline Complex operator/(const Complex& a, const Complex& b) {
    const double d = b.norm();
    if (d == 0.0) {
        throw std::invalid_argument("complex division by zero");
    }
    return {(a.real() * b.real() + a.imag() * b.imag()) / d,
            (a.imag() * b.real() - a.real() * b.imag()) / d};
}

// 重载的流函数只负责格式化，不做任何判断。
inline std::ostream& operator<<(std::ostream& os, const Complex& c) {
    os << c.real() << (c.imag() < 0 ? " - " : " + ")
       << std::abs(c.imag()) << 'i';
    return os;
}
// <<< complex

}  // namespace dsa::ch01

namespace {

bool close(double a, double b) { return std::abs(a - b) < 1e-9; }

}  // namespace

int main() {
    using dsa::ch01::Complex;

    const Complex zero;                 // 默认构造
    const Complex real_only(3.0);       // 只给实部
    const Complex a(1.0, 2.0);
    const Complex b(3.0, -4.0);

    assert(close(zero.real(), 0.0) && close(zero.imag(), 0.0));
    assert(close(real_only.real(), 3.0) && close(real_only.imag(), 0.0));

    assert(close((a + b).real(), 4.0) && close((a + b).imag(), -2.0));
    assert(close((a - b).real(), -2.0) && close((a - b).imag(), 6.0));
    // (1+2i)(3-4i) = 3 - 4i + 6i - 8i² = 11 + 2i
    assert(close((a * b).real(), 11.0) && close((a * b).imag(), 2.0));
    // (1+2i)/(3-4i) = (1+2i)(3+4i)/25 = (-5+10i)/25 = -0.2 + 0.4i
    assert(close((a / b).real(), -0.2) && close((a / b).imag(), 0.4));

    // 除以 0 抛异常，而不是打印一行提示后返回垃圾值
    bool threw = false;
    try {
        (void)(a / zero);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);

    Complex c = a;                      // 三个 double 成员，编译器生成的拷贝就够用
    c.set_imag(-7.5);
    assert(close(c.real(), 1.0) && close(c.imag(), -7.5));

    std::cout << "a       = " << a << '\n';
    std::cout << "b       = " << b << '\n';
    std::cout << "a * b   = " << a * b << '\n';
    std::cout << "a / b   = " << a / b << '\n';
    std::cout << "a / 0   -> 抛出 std::invalid_argument\n";
}
